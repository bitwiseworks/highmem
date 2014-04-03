/*

   Copyright 2012-14 Yuri Dario <yd@os2power.com>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INCL_BASE
#define INCL_ERRORS
#include <os2.h>

#define FOR_EXEHDR 1
#include <exe386.h>
#define OBJHIMEM 0x00010000L  /* Object is loaded above 512MB if available */

#include "getopt.h"


/*  */
#pragma pack(1)
struct StubHeader_t
{
	USHORT usID;			  /* == 0x5a4d */
	USHORT ausContents1[11];
	USHORT usRelOff;		  /* == 30 or > 32 */
	USHORT ausContents2[17];
	ULONG ulNewHeaderOff;
};
#pragma pack()


/* global definitions */
#define MAX_EXCLUDE_ENTRIES 1024
char*	exclude[MAX_EXCLUDE_ENTRIES];
ULONG	ulCheckMask = 0;
ULONG	ulCheckPattern = ~0;
ULONG	ulModifyMask = 0;
ULONG	ulModifyPattern = 0;
int		quiet;
int		verbose;

/* program options */
static const char *shortoptions = "?vqcdbux:";
static const struct option longoptions[] = {
	{"help",	no_argument,	   (int *) 0, '?' },
	{"verbose",	no_argument,	   (int *) 0, 'v' },
	{"quiet",	no_argument,	   (int *) 0, 'q' },
	{"code",	no_argument,	   (int *) 0, 'c' },
	{"data",	no_argument,	   (int *) 0, 'd' },
	{"both",	no_argument,	   (int *) 0, 'b' },
	{"unmark",	no_argument,	   (int *) 0, 'u' },
	{"exclude",	required_argument, (int *) 0, 'x' },
	{(char *)0,	no_argument,	   (int *) 0, 0 }
};


/*
* mark a single executable
*/
int mark( char* pszModule)
{
	APIRET rc;
	HFILE hfModule;
	ULONG ulWork;
	FILESTATUS3 xfs3;
	PVOID pvBuffer;
	struct e32_exe* pxLXHeader;
	BOOL bModified;
	struct o32_obj* pxObjTable;
	static const PSZ apszObjectTypes[] = {"swappable", "permanent", "resident", "contiguous", "long lockable", "", "swappable", ""};
	ULONG ulIndex;

	rc = DosOpen(pszModule, &hfModule, &ulWork, (ULONG)0, FILE_NORMAL,
			OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
			OPEN_FLAGS_FAIL_ON_ERROR | OPEN_FLAGS_SEQUENTIAL 
				| OPEN_FLAGS_NOINHERIT | 
				(ulCheckMask != 0 
					? OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_READWRITE
					: OPEN_SHARE_DENYNONE | OPEN_ACCESS_READONLY),
			(PEAOP2)NULL);
	if (rc) {
		printf("Error: \"%s\" cannot be opened, rc=%d.\n", pszModule, rc);
		return -1;
	}

	rc = DosQueryFileInfo( hfModule, FIL_STANDARD, (PVOID)&xfs3, sizeof(xfs3));
	pvBuffer = malloc( xfs3.cbFile);
	if (!pvBuffer) {
		printf("Error: not enough memory, rc=%d.\n", rc);
		return -1;
	}

	if((rc = DosRead( hfModule, pvBuffer, xfs3.cbFile, &ulWork)) != NO_ERROR ||
	   ulWork != xfs3.cbFile)
	{
		printf("Error: \"%s\" cannot be read, rc=%d.\n", pszModule, rc);
		free(pvBuffer);
		DosClose(hfModule);
		return -1;
	}
	
	pxLXHeader = (struct e32_exe*)NULL;
	switch(((PUSHORT)pvBuffer)[0])
	{
	case (USHORT)0x5a4d:  /* 'MZ' */
		if(((struct StubHeader_t*)pvBuffer)->usRelOff >= (USHORT)sizeof(struct StubHeader_t))
		{
			pxLXHeader = (struct e32_exe*)&((PUCHAR)pvBuffer)[((struct StubHeader_t*)pvBuffer)->ulNewHeaderOff];
			if(((PUSHORT)pxLXHeader)[0] != (USHORT)0x584c)  /* 'LX' */
				pxLXHeader = (struct e32_exe*)NULL;
		}
		break;
	case (USHORT)0x584c:  /* 'LX' */
		pxLXHeader = (struct e32_exe*)pvBuffer;
		break;
	}
	if (pxLXHeader == (struct e32_exe*)NULL ||
	   pxLXHeader->e32_level != E32LEVEL ||
	   pxLXHeader->e32_cpu < E32CPU386 ||
	   ((pxLXHeader->e32_mflags & (E32NOLOAD | E32MODMASK)) != E32MODDLL
	    && (pxLXHeader->e32_mflags & (E32NOLOAD | E32MODMASK)) != E32MODEXE) ) {
		printf("Error: \"%s\" is not LX format 32bit EXE/DLL module.\n",
					 pszModule);
		free(pvBuffer);
		DosClose(hfModule);
		return -1;
	}

	if (!quiet)
		printf("Processing module : %s", pszModule);

	bModified = (BOOL)FALSE;
	for(ulIndex = (ULONG)0, pxObjTable = (struct o32_obj*)&((PUCHAR)pxLXHeader)[pxLXHeader->e32_objtab];
		ulIndex < (ULONG)pxLXHeader->e32_objcnt;
		ulIndex++, pxObjTable++)
	{
		if (!quiet) {
			if (verbose > 0)
				printf("\n object %u : base 0x%08x, size 0x%08x, flags 0x%08x, %s, %shimem",
					(unsigned int)ulIndex,
					(unsigned int)pxObjTable->o32_base,
					(unsigned int)pxObjTable->o32_size,
					(unsigned int)pxObjTable->o32_flags,
					(pxObjTable->o32_flags & OBJEXEC) != (unsigned long)0 ? "executable" : "data",
					(pxObjTable->o32_flags & OBJHIMEM) != (unsigned long)0 ? "" : "!");
			if (verbose > 1)
				printf("\n  %sreadable, %swriteable, %sexecutable, %sresource, %sdiscardable, %sshared,\n"
					"  %spreload, %sinvalid, %s, %s16:16 alias, %sconforming,\n"
					"  %s32bit, %sIOPL",
					(pxObjTable->o32_flags & OBJREAD) != (unsigned long)0 ? "" : "!",
					(pxObjTable->o32_flags & OBJWRITE) != (unsigned long)0 ? "" : "!",
					(pxObjTable->o32_flags & OBJEXEC) != (unsigned long)0 ? "" : "!",
					(pxObjTable->o32_flags & OBJRSRC) != (unsigned long)0 ? "" : "!",
					(pxObjTable->o32_flags & OBJDISCARD) != (unsigned long)0 ? "" : "!",
					(pxObjTable->o32_flags & OBJSHARED) != (unsigned long)0 ? "" : "!",
					(pxObjTable->o32_flags & OBJPRELOAD) != (unsigned long)0 ? "" : "!",
					(pxObjTable->o32_flags & OBJINVALID) != (unsigned long)0 ? "" : "!",
					(const char*)apszObjectTypes[(pxObjTable->o32_flags & OBJTYPEMASK) >> 8],
					(pxObjTable->o32_flags & OBJALIAS16) != (unsigned long)0 ? "" : "!",
					(pxObjTable->o32_flags & OBJCONFORM) != (unsigned long)0 ? "" : "!",
					(pxObjTable->o32_flags & OBJBIGDEF) != (unsigned long)0 ? "" : "!",
					(pxObjTable->o32_flags & OBJIOPL) != (unsigned long)0 ? "" : "!");
		}

		if(((pxObjTable->o32_flags & ulCheckMask) ^ ulCheckPattern) == (unsigned long)0)
		{
			pxObjTable->o32_flags = (pxObjTable->o32_flags & ulModifyMask) ^ ulModifyPattern;
			
			if (!quiet) {
				if (verbose == 0)
					printf("  MODIFIED");
				if (verbose > 0)
					printf("\n  MODIFIED: base 0x%08x, size 0x%08x, flags 0x%08x, %s, %shimem",
						(unsigned int)pxObjTable->o32_base,
						(unsigned int)pxObjTable->o32_size,
						(unsigned int)pxObjTable->o32_flags,
						(pxObjTable->o32_flags & OBJEXEC) != (unsigned long)0 ? "executable" : "data",
						(pxObjTable->o32_flags & OBJHIMEM) != (unsigned long)0 ? "" : "!");
				if (verbose > 1)
					printf("\n  %sreadable, %swriteable, %sexecutable, %sresource, %sdiscardable, %sshared,\n"
						"  %spreload, %sinvalid, %s, %s16:16 alias, %sconforming,\n"
						"  %s32bit, %sIOPL",
						(pxObjTable->o32_flags & OBJREAD) != (unsigned long)0 ? "" : "!",
						(pxObjTable->o32_flags & OBJWRITE) != (unsigned long)0 ? "" : "!",
						(pxObjTable->o32_flags & OBJEXEC) != (unsigned long)0 ? "" : "!",
						(pxObjTable->o32_flags & OBJRSRC) != (unsigned long)0 ? "" : "!",
						(pxObjTable->o32_flags & OBJDISCARD) != (unsigned long)0 ? "" : "!",
						(pxObjTable->o32_flags & OBJSHARED) != (unsigned long)0 ? "" : "!",
						(pxObjTable->o32_flags & OBJPRELOAD) != (unsigned long)0 ? "" : "!",
						(pxObjTable->o32_flags & OBJINVALID) != (unsigned long)0 ? "" : "!",
						(const char*)apszObjectTypes[(pxObjTable->o32_flags & OBJTYPEMASK) >> 8],
						(pxObjTable->o32_flags & OBJALIAS16) != (unsigned long)0 ? "" : "!",
						(pxObjTable->o32_flags & OBJCONFORM) != (unsigned long)0 ? "" : "!",
						(pxObjTable->o32_flags & OBJBIGDEF) != (unsigned long)0 ? "" : "!",
						(pxObjTable->o32_flags & OBJIOPL) != (unsigned long)0 ? "" : "!");
			}
			bModified = (BOOL)TRUE;
		}
	}

	// new line
	printf( "\n");

	if (bModified == (BOOL)FALSE) {
		free(pvBuffer);
		rc = DosClose(hfModule);
		return 0;
	}

	rc = DosSetFilePtr(hfModule,  (LONG)0, FILE_BEGIN, &ulWork);
	rc = DosWrite(hfModule, pvBuffer, xfs3.cbFile, &ulWork);
	if (rc != NO_ERROR || ulWork != xfs3.cbFile) {
		printf("Error: \"%s\" cannot be written, rc=%d.\n", pszModule, rc);
		free(pvBuffer);
		rc = DosClose(hfModule);
		return -1;
	}

	free(pvBuffer);
	rc = DosClose(hfModule);

	rc=DosSetPathInfo(pszModule, FIL_STANDARD, (PVOID)&xfs3, (ULONG)sizeof(xfs3),
			(ULONG)0);
	if (rc != NO_ERROR) {
		printf("Error: \"%s\" cannot be restored date/time/attributes, rc=%d.\n",
			pszModule, rc);
		return -1;
	}

	/* done */
	return 0;
}


/*
* print help
*/
void usage( void)
{
	(void)printf("\n"
	"HighMem, a LX format 32bit DLL module 'loading above 512MB' marking utility,\n"
	"Version 1.0.0\n"
	"(C) 2012 Yuri Dario <yd@os2power.com>.\n"
	"    Partially based on ABOVE512 (C) 2004 Takayuki 'January June' Suwa.\n"
	"\n"
	"usage: HIGHMEM [-options] {DLL module file} ...\n"
	"Without options, current DLL object information are dumped.\n"
	"Options:\n"
	" --quiet   -q  quiets (no message)\n"
	" --verbose -v  verbose output (-v -v even more verbose)\n"
	" --code    -c  marks pure 32bit code objects as 'loading above 512MB'\n"
	" --data    -d  marks pure 32bit data objects as so\n"
	" --both    -b  marks both of pure 32bit code and data objects\n"
	" --unmark  -u  unmarks 'loading above 512MB' pure 32bit code/data objects\n"
	" --exclude -x file  list of files to be excluded (max 1024 entries\n"
	" --help    -?  show this help\n"
	);
}


/*
* analyze options
*/
int main(int argc, char* argv[])
{
	int 	i, c;
	int 	helpflag = 0;
	int		option_index;
	FILE* 	excl;
	int		exclude_entries = 0;
	char	buffer[_MAX_PATH];

	while((c = getopt_long( argc, argv, shortoptions, longoptions,
								&option_index)) != -1) {
		switch (c) {
		case 'V':
			helpflag = TRUE;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'v':
			verbose++;
			break;
		case 'c':
			ulCheckMask = OBJEXEC | OBJRSRC | OBJINVALID | OBJALIAS16 | OBJBIGDEF | OBJHIMEM;
			ulCheckPattern = OBJEXEC | OBJBIGDEF;
			ulModifyMask = (unsigned long)~0;
			ulModifyPattern = OBJHIMEM;
			break;
		case 'd':
			ulCheckMask = OBJEXEC | OBJRSRC | OBJINVALID | OBJALIAS16 | OBJBIGDEF | OBJHIMEM;
			ulCheckPattern = OBJBIGDEF;
			ulModifyMask = (unsigned long)~0;
			ulModifyPattern = OBJHIMEM;
			break;
		case 'b':
			ulCheckMask = OBJRSRC | OBJINVALID | OBJALIAS16 | OBJBIGDEF | OBJHIMEM;
			ulCheckPattern = OBJBIGDEF;
			ulModifyMask = (unsigned long)~0;
			ulModifyPattern = OBJHIMEM;
			break;
		case 'u':
			ulCheckMask = OBJRSRC | OBJINVALID | OBJALIAS16 | OBJBIGDEF | OBJHIMEM;
			ulCheckPattern = OBJBIGDEF | OBJHIMEM;
			ulModifyMask = ~OBJHIMEM;
			ulModifyPattern = (unsigned long)0;
			break;
		case 'x':
			excl = fopen( optarg, "r");
			if (excl == NULL) {
				printf("Error: \"%s\" not readable, errno=%d.\n", optarg, errno);
				helpflag++;
				break;
			} else {
				exclude_entries = 0;
				while( fgets( buffer, sizeof(buffer), excl) != NULL)
					exclude[exclude_entries++] = strdup( buffer);
				fclose( excl);
			}
			break;
		case '?':
		default:
			helpflag++;
			break;
		}
	}

	if (helpflag || !argv[optind]) {
		usage();
		if (helpflag)
			exit(0);
		else
			exit(1);
	}

	// loop over arguments to apply changes
	while( argv[optind]) {
		int found = 0;
		// check exclusion list first
		for( i=0; i<exclude_entries; i++)
			if (strstr( argv[optind], exclude[i]) != NULL) {
				found = 1;
				break;
			}
		// mark file
		if (!found)
			mark( argv[optind]);
		optind++;
	}
}

