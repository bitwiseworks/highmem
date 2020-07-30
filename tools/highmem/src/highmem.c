/* ABOVE512.c  LX format 32-bit DLL module 'loading above 512 MiB' marking utility,
     version 0.01d (internal/experimental use only)
   Copyright 2004 Takayuki 'January June' Suwa. */

#pragma strings(readonly)
#include <stdio.h>
#define INCL_BASE
#define INCL_ERRORS
#include <os2.h>
typedef USHORT WORD;
typedef ULONG DWORD;
#define FOR_EXEHDR 1
#include <exe386.h>
#define OBJHIMEM 0x00010000L  /* Object is loaded above 512 MiB if available */

/*  */
extern APIRET APIENTRY DosReplaceModule(PSZ pszOldModule,
                                        PSZ pszNewModule,
                                        PSZ pszBackupModule);

/*  */
#pragma pack(1)
struct StubHeader_t
{
    USHORT usID;              /* == 0x5a4d */
    USHORT ausContents1[11];
    USHORT usRelOff;          /* == 30 or > 32 */
    USHORT ausContents2[17];
    ULONG ulNewHeaderOff;
};
#pragma pack()

/*  */
int main(int argc,
         char* argv[])
{
    ULONG ulIndex;
    ULONG ulOptions;
    unsigned long ulCheckMask, ulCheckPattern, ulModifyMask, ulModifyPattern;
    PSZ pszModule;
    ULONG aulSysInfo[2];
    HFILE hfModule;
    ULONG ulWork;
    FILESTATUS3 xfs3;
    PVOID pvBuffer;
    struct e32_exe* pxLXHeader;
    BOOL bModified;
    struct o32_obj* pxObjTable;
    static const PSZ apszObjectTypes[] = {"swappable", "permanent", "resident", "contiguous", "long lockable", "", "swappable", ""};

    ulOptions = (ULONG)0;
    ulCheckMask = (unsigned long)0;
    ulCheckPattern = (unsigned long)~0;
    ulModifyMask = (unsigned long)0;
    ulModifyPattern = (unsigned long)0;
    pszModule = (PSZ)NULL;
    for(ulIndex = (ULONG)1;
        ulIndex < (ULONG)argc;
        ulIndex++)
        if(argv[ulIndex][0] == '/' ||
           argv[ulIndex][0] == '-')
            switch(argv[ulIndex][1])
            {
                case 'C':
                case 'c':
                    ulOptions |= (ULONG)1;
                    ulCheckMask = OBJEXEC | OBJRSRC | OBJINVALID | OBJALIAS16 | OBJBIGDEF | OBJHIMEM;
                    ulCheckPattern = OBJEXEC | OBJBIGDEF;
                    ulModifyMask = (unsigned long)~0;
                    ulModifyPattern = OBJHIMEM;
                    break;
                case 'D':
                case 'd':
                    ulOptions |= (ULONG)1;
                    ulCheckMask = OBJEXEC | OBJRSRC | OBJINVALID | OBJALIAS16 | OBJBIGDEF | OBJHIMEM;
                    ulCheckPattern = OBJBIGDEF;
                    ulModifyMask = (unsigned long)~0;
                    ulModifyPattern = OBJHIMEM;
                    break;
                case 'B':
                case 'b':
                    ulOptions |= (ULONG)1;
                    ulCheckMask = OBJRSRC | OBJINVALID | OBJALIAS16 | OBJBIGDEF | OBJHIMEM;
                    ulCheckPattern = OBJBIGDEF;
                    ulModifyMask = (unsigned long)~0;
                    ulModifyPattern = OBJHIMEM;
                    break;
                case 'P':
                case 'p':
                    ulOptions |= (ULONG)1;
                    ulCheckMask = OBJRSRC | OBJPRELOAD | OBJINVALID | OBJALIAS16 | OBJBIGDEF | OBJHIMEM;
                    ulCheckPattern = OBJPRELOAD | OBJBIGDEF;
                    ulModifyMask = ~OBJPRELOAD;
                    ulModifyPattern = OBJHIMEM;
                    break;
                case 'U':
                case 'u':
                    ulOptions |= (ULONG)1;
                    ulCheckMask = OBJRSRC | OBJINVALID | OBJALIAS16 | OBJBIGDEF | OBJHIMEM;
                    ulCheckPattern = OBJBIGDEF | OBJHIMEM;
                    ulModifyMask = ~OBJHIMEM;
                    ulModifyPattern = (unsigned long)0;
                    break;
                case 'Q':
                case 'q':
                    ulOptions |= (ULONG)2;
                    break;
                case '!':
                    ulOptions |= (ULONG)4;
                    break;
            }
        else
            pszModule = (PSZ)argv[ulIndex];
    if(pszModule == (PSZ)NULL)
    {
        (VOID)DosQuerySysInfo(QSV_MAXPRMEM,
                              QSV_MAXSHMEM,
                              (PVOID)&aulSysInfo[0],
                              (ULONG)sizeof(aulSysInfo));
        (void)printf("\n"
                     "ABOVE512.exe  LX format 32-bit DLL module 'loading above 512 MiB' marking\n"
                     "  utility, version 0.01d (internal/experimental use only)\n"
                     "Copyright 2004 Takayuki 'January June' Suwa.\n"
                     "\n"
                     "usage: ABOVE512 {DLL module file} [-options]\n"
                     "  without options, ABOVE512 shows current DLL object information.\n"
                     "options: -q  quiets (no message)\n"
                     "         -c  marks pure 32-bit code objects as 'loading above 512 MiB'\n"
                     "         -d  marks pure 32-bit data objects as so\n"
                     "         -b  marks both of pure 32-bit code and data objects\n"
                     "         -p  marks pure 32-bit preloaded code/data objects and removes preload\n"
                     "         -u  unmarks 'loading above 512 MiB' pure 32-bit code/data objects\n"
                     "         -!  unlocks the DLL module before open\n"
                     "current free virtual address space in KiB (private / shared):\n"
                     "  %u / %u below 512 MiB line",
                     (aulSysInfo[0] + (ULONG)512) / (ULONG)1024,
                     (aulSysInfo[1] + (ULONG)512) / (ULONG)1024);
        if(DosQuerySysInfo(QSV_MAXHPRMEM,
                           QSV_MAXHSHMEM,
                           (PVOID)&aulSysInfo[0],
                           (ULONG)sizeof(aulSysInfo)) == NO_ERROR)
            (void)printf(", %u / %u above 512 MiB line\n",
                         (aulSysInfo[0] + (ULONG)512) / (ULONG)1024,
                         (aulSysInfo[1] + (ULONG)512) / (ULONG)1024);
        else
            (void)printf("\n");
        return -1;
    }
    if((ulOptions & (ULONG)4) != (ULONG)0)
        (VOID)DosReplaceModule(pszModule,
                               (PSZ)NULL,
                               (PSZ)NULL);
    if(DosOpen(pszModule,
               &hfModule,
               &ulWork,
               (ULONG)0,
               FILE_NORMAL,
               OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
               (ulOptions & (ULONG)1) != (ULONG)0 ? OPEN_FLAGS_FAIL_ON_ERROR | OPEN_FLAGS_SEQUENTIAL | OPEN_FLAGS_NOINHERIT | OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_READWRITE
                                                  : OPEN_FLAGS_FAIL_ON_ERROR | OPEN_FLAGS_SEQUENTIAL | OPEN_FLAGS_NOINHERIT | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READONLY,
               (PEAOP2)NULL) != NO_ERROR)
    {
        (void)printf("ABOVE512: \"%s\" cannot be opened.\n",
                     (const char*)pszModule);
        return -1;
    }
    (VOID)DosQueryFileInfo(hfModule,
                           FIL_STANDARD,
                           (PVOID)&xfs3,
                           (ULONG)sizeof(xfs3));
    (VOID)DosAllocMem(&pvBuffer,
                      xfs3.cbFile,
                      PAG_READ | PAG_WRITE | PAG_COMMIT);
    if(DosRead(hfModule,
               pvBuffer,
               xfs3.cbFile,
               &ulWork) != NO_ERROR ||
       ulWork != xfs3.cbFile)
    {
        (void)printf("ABOVE512: \"%s\" cannot be read.\n",
                     (const char*)pszModule);
        (VOID)DosFreeMem(pvBuffer);
        (VOID)DosClose(hfModule);
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
    if(pxLXHeader == (struct e32_exe*)NULL ||
       pxLXHeader->e32_level != E32LEVEL ||
       pxLXHeader->e32_cpu < E32CPU386 ||
       (pxLXHeader->e32_mflags & (E32NOLOAD | E32MODMASK)) != E32MODDLL)
    {
        (void)printf("ABOVE512: \"%s\" is not LX format 32-bit DLL module.\n",
                     (const char*)pszModule);
        (VOID)DosFreeMem(pvBuffer);
        (VOID)DosClose(hfModule);
        return -1;
    }
    if((ulOptions & (ULONG)2) == (ULONG)0)
        (void)printf("module : %s\n",
                     (const char*)pszModule);
    bModified = (BOOL)FALSE;
    for(ulIndex = (ULONG)0, pxObjTable = (struct o32_obj*)&((PUCHAR)pxLXHeader)[pxLXHeader->e32_objtab];
        ulIndex < (ULONG)pxLXHeader->e32_objcnt;
        ulIndex++, pxObjTable++)
    {
        if((ulOptions & (ULONG)2) == (ULONG)0)
            (void)printf(" object %u : base 0x%08x, size 0x%08x, flags 0x%08x\n"
                         "  %sreadable, %swriteable, %sexecutable, %sresource, %sdiscardable, %sshared,\n"
                         "  %spreload, %sinvalid, %s, %s16:16 alias, %sconforming,\n"
                         "  %s32-bit, %sIOPL, %shimem\n",
                         (unsigned int)ulIndex,
                         (unsigned int)pxObjTable->o32_base,
                         (unsigned int)pxObjTable->o32_size,
                         (unsigned int)pxObjTable->o32_flags,
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
                         (pxObjTable->o32_flags & OBJIOPL) != (unsigned long)0 ? "" : "!",
                         (pxObjTable->o32_flags & OBJHIMEM) != (unsigned long)0 ? "" : "!");
        if(((pxObjTable->o32_flags & ulCheckMask) ^ ulCheckPattern) == (unsigned long)0)
        {
            pxObjTable->o32_flags = (pxObjTable->o32_flags & ulModifyMask) ^ ulModifyPattern;
            if((ulOptions & (ULONG)2) == (ULONG)0)
                (void)printf("   modified,\n"
                             "    %sreadable, %swriteable, %sexecutable, %sresource, %sdiscardable, %sshared,\n"
                             "    %spreload, %sinvalid, %s, %s16:16 alias, %sconforming,\n"
                             "    %s32-bit, %sIOPL, %shimem\n",
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
                             (pxObjTable->o32_flags & OBJIOPL) != (unsigned long)0 ? "" : "!",
                             (pxObjTable->o32_flags & OBJHIMEM) != (unsigned long)0 ? "" : "!");
            bModified = (BOOL)TRUE;
        }
    }
    if(bModified != (BOOL)FALSE)
    {
        (VOID)DosSetFilePtr(hfModule,
                            (LONG)0,
                            FILE_BEGIN,
                            &ulWork);
        if(DosWrite(hfModule,
                    pvBuffer,
                    xfs3.cbFile,
                    &ulWork) != NO_ERROR ||
           ulWork != xfs3.cbFile)
        {
            (void)printf("ABOVE512: \"%s\" cannot be written.\n",
                         (const char*)pszModule);
            (VOID)DosFreeMem(pvBuffer);
            (VOID)DosClose(hfModule);
            return -1;
        }
    }
    (VOID)DosFreeMem(pvBuffer);
    (VOID)DosClose(hfModule);
    if(bModified != (BOOL)FALSE &&
       DosSetPathInfo(pszModule,
                      FIL_STANDARD,
                      (PVOID)&xfs3,
                      (ULONG)sizeof(xfs3),
                      (ULONG)0) != NO_ERROR)
    {
        (void)printf("ABOVE512: \"%s\" cannot be restored date/time/attributes.\n",
                     (const char*)pszModule);
        return -1;
    }

    return 0;
}

