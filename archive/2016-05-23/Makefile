APP      = srmodldr
EXEFILE  = $(APP).exe
OBJFILES = main.obj crc32.obj asprintf.obj
RESFILES = RESOURCES.res
LIBFILES = jansson sqlite3
DEFFILE  =

.AUTODEPEND
BCC32   = bcc32
ILINK32 = ilink32
BRC32   = brc32

CFLAGS  = -c -tWM- -w -w-par -w-inl -W -a1 -Od -k -v -w-8060 -w-8080 -w-8084 -M
LFLAGS  = -aa -V4.0 -c -Gn -x
RFLAGS  = -X -R 
STDOBJS = c0w32.obj
STDLIBS = import32.lib cw32.lib

$(EXEFILE) : $(OBJFILES) $(RESFILES)
   $(ILINK32) $(LFLAGS) $(OBJFILES) $(STDOBJS), $(EXEFILE), , \
      $(LIBFILES) $(STDLIBS), $(DEFFILE), $(RESFILES)

clean:
   del *.obj *.res *.tds *.map