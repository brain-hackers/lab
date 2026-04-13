export CFLAGS = -pg                          # Use gprof
export CFLAGS += -static                     # Static link
export CFLAGS += -O0                         # Heavy optimization
export CFLAGS += -Wl,--defsym,_mcount=mcount # Link _mcount to mcount
export CFLAGS += -mapcs-frame                # Use standard calling convention
#export CFLAGS += -fno-omit-frame-pointer     # Don't omit fp (might not necessary to enable this)

AppMain.exe:
	@arm-mingw32ce-gcc -o AppMain $(CFLAGS) profile.c

AppMain.mitigated.s:
	@arm-mingw32ce-gcc -O0 -S -fno-omit-frame-pointer -mapcs-frame profile.c
	@mv profile.s AppMain.mitigated.s

.PHONY:
clean:
	@rm -f AppMain.exe AppMain*.s
