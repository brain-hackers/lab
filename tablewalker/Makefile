
CC=/opt/cegcc/bin/arm-mingw32ce-gcc
CXX=/opt/cegcc/bin/arm-mingw32ce-g++
LD=/opt/cegcc/bin/arm-mingw32ce-g++
STRIP=/opt/cegcc/bin/arm-mingw32ce-strip
DLLTOOL=/opt/cegcc/bin/arm-mingw32ce-dlltool
AS=/opt/cegcc/bin/arm-mingw32ce-as
NM=/opt/cegcc/bin/arm-mingw32ce-nm
WINDRES=/opt/cegcc/bin/arm-mingw32ce-windres

OUTPUT=ResetKit.dll ResetKitHelper.dll SoftReset.exe

CXXFLAGS= -DEV_PLATFORM_WIN32 -DUNICODE -D_UNICODE -DEV_UNSAFE_SWPRINTF -mwin32 \
-Os -mcpu=arm926ej-s -D_WIN32_WCE=0x600 -D_LARGEFILE_SOURCE=1 -D_LARGEFILE64_SOURCE=1 \
-D_FILE_OFFSET_BITS=64 -static

DLLFLAGS=-DEV_PLATFORM_WIN32 -DUNICODE -D_UNICODE -DEV_UNSAFE_SWPRINTF -mwin32 \
-Os -mcpu=arm926ej-s -D_WIN32_WCE=0x600 -D_LARGEFILE_SOURCE=1 -D_LARGEFILE64_SOURCE=1 \
-D_FILE_OFFSET_BITS=64 -DNDEBUG -Wall -static \
 -Wl,--image-base,0x100000 \
 -shared
 
DRVFLAGS= -DEV_PLATFORM_WIN32 -DUNICODE -D_UNICODE -DEV_UNSAFE_SWPRINTF -mwin32 \
-O0 -mcpu=arm926ej-s -D_WIN32_WCE=0x600 -D_LARGEFILE_SOURCE=1 -D_LARGEFILE64_SOURCE=1 \
-D_FILE_OFFSET_BITS=64 -DNDEBUG -Wall -static \
 -Wl,--image-base,0x100000 \
 -nostdlib -lcoredll -shared

.PHONY:		all clean

all:				$(OUTPUT)

clean:			
				rm -f $(OUTPUT)

ResetKit.dll:	ResetKit.cpp
				$(CXX) ResetKit.cpp -o ResetKit.dll $(DLLFLAGS)
				$(STRIP) ResetKit.dll
				
ResetKitHelper.dll:	ResetKitHelper.cpp
				$(CXX) ResetKitHelper.cpp -o ResetKitHelper.dll $(DRVFLAGS)
				$(STRIP) ResetKitHelper.dll
                

SoftReset.exe:	SoftReset.cpp
				$(CXX) SoftReset.cpp -o SoftReset.exe $(CXXFLAGS)
				$(STRIP) SoftReset.exe

