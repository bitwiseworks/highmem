HighMem, a LX format 32bit DLL module 'loading above 512MB' marking utility,
Version 1.0.3

(C) 2012-2021 Yuri Dario <yd@os2power.com>.
    Partially based on ABOVE512 (C) 2004 Takayuki 'January June' Suwa.

usage: HIGHMEM [-options] {DLL module file} ...
Without options, current DLL object information are dumped.
Options:
 --quiet   -q  quiets (no message)
 --verbose -v  verbose output (-v -v even more verbose)
 --code    -c  marks pure 32bit code objects as 'loading above 512MB'
 --data    -d  marks pure 32bit data objects as so
 --both    -b  marks both of pure 32bit code and data objects
 --unmark  -u  unmarks 'loading above 512MB' pure 32bit code/data objects
 --exclude -x file  list of files to be excluded (max 1024 entries
 --help    -?  show this help
