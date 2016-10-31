#
#  File        : Makefile
#                ( Makefile for GNU 'make' utility )
#
#  Description : Makefile for compiling :
#
#                 . the G'MIC command line tool (type 'make cli').
#                 . the G'MIC plug-in for GIMP (type 'make gimp').
#                 . the G'MIC interpreter, as a C++ library (type 'make lib').
#                 . the G'MIC C library (type 'make libc' or 'make staticlibc').
#                 . the ZArt software (type 'make zart').
#                 . To compile all of them, just type 'make', or 'make all').
#
#                ( http://gmic.eu )
#
#  Copyright   : David Tschumperle
#                ( http://tschumperle.users.greyc.fr/ )
#
#  License     : CeCILL v2.0
#                ( http://www.cecill.info/licences/Licence_CeCILL_V2-en.html )
#
#  This software is governed by the CeCILL  license under French law and
#  abiding by the rules of distribution of free software.  You can  use,
#  modify and/ or redistribute the software under the terms of the CeCILL
#  license as circulated by CEA, CNRS and INRIA at the following URL
#  "http://www.cecill.info".
#
#  As a counterpart to the access to the source code and  rights to copy,
#  modify and redistribute granted by the license, users are provided only
#  with a limited warranty  and the software's author,  the holder of the
#  economic rights,  and the successive licensors  have only  limited
#  liability.
#
#  In this respect, the user's attention is drawn to the risks associated
#  with loading,  using,  modifying and/or developing or reproducing the
#  software by the user in light of its specific status of free software,
#  that may mean  that it is complicated to manipulate,  and  that  also
#  therefore means  that it is reserved for developers  and  experienced
#  professionals having in-depth computer knowledge. Users are therefore
#  encouraged to load and test the software's suitability as regards their
#  requirements in conditions enabling the security of their systems and/or
#  data to be ensured and,  more generally, to use and operate it in the
#  same conditions as regards security.
#
#  The fact that you are presently reading this means that you have had
#  knowledge of the CeCILL license and that you accept its terms.
#

#---------------------------
# Set OS-specific variables.
#---------------------------

# Try to auto-detect target OS.
OS := $(shell uname)
USR = /usr
LIB = lib
BIN = bin
INCLUDE = include
SO = .so
PIC = -fPIC
EXE =
WGET = wget --no-check-certificate --quiet -O
PLUGINDIR = `gimptool-2.0 --gimpplugindir`/plug-ins

ifeq ($(OS),Linux)
OS = Unix
endif

ifeq ($(OS),GNU/kFreeBSD)
OS = Unix
endif

ifeq ($(OS),GNU)
OS = Unix
endif

ifeq ($(OS),FreeBSD)
OS = Unix
USR = /usr/local
endif

ifeq ($(OS),DragonFly)
OS = Unix
USR = /usr/local
endif

ifeq ($(OS),NetBSD)
OS = Unix
endif

ifneq (, $(findstring MINGW, $(OS)))
OS = Windows
endif

ifeq ($(OS),Darwin)
PLUGINDIR = ~/Library/Application\ Support/Gimp/plug-ins
ifeq (,$(wildcard /opt/local))
USR = /usr/local
else
USR = /opt/local
endif
WGET = curl -k -L --silent -o
endif

ifeq ($(OS),Windows)
EXE = .exe
SO = .dll
PIC =
endif

ifdef NOSTRIP
STRIP = echo skip strip
else
STRIP = strip
endif

#-------------------------------------------------
# Define custom flags for each different features.
#-------------------------------------------------

# Flags set to describe a 'prerelease' version.
PRERELEASE_CFLAGS = -Dgmic_prerelease="\\\"`date +%m%d%y`\\\""

# Minimal set of flags that is mandatory to compile G'MIC.
MANDATORY_CFLAGS = -Dgmic_build -Dcimg_use_zlib `pkg-config --cflags zlib || echo -I$(USR)/$(INCLUDE)` $(PRERELEASE_CFLAGS)
MANDATORY_LIBS = `pkg-config --libs zlib || echo -lz`
ifndef NO_SRIPDLIB
MANDATORY_CFLAGS += -std=c++11 -pedantic
endif
MANDATORY_LIBS += -L$(USR)/$(LIB)
ifeq ($(notdir $(CXX)),g++)
MANDATORY_CFLAGS += -Wall -Wextra -Wfatal-errors
MANDATORY_LIBS += -lm
endif
ifeq ($(OS),Unix)
MANDATORY_CFLAGS += -Dcimg_use_vt100
endif
ifeq ($(OS),Darwin)
ifndef NO_STDLIB
MANDATORY_CFLAGS += -stdlib=libc++
endif
endif
ifeq ($(OS),Windows)
MANDATORY_LIBS += -Wl,--stack,16777216
endif

# Flags to enable software debugging (slow down the code!).
DEBUG_CFLAGS = -pedantic -Dcimg_verbosity=3 -Dcimg_strict_warnings -g -fsanitize=address # -fsanitize=thread # -fsanitize=undefined # Removed '-ansi' as it (silently) disables c++11.

# Flags to enable optimizations.
ifeq ($(notdir $(CXX)),g++)
OPT_CFLAGS = -O3 -mtune=generic # -Ofast -march=native
else
ifeq ($(notdir $(CXX)),icpc)
OPT_CFLAGS = -fast
else
OPT_CFLAGS = -O3
endif
endif

# Flags to enable parallel processing.
PARALLEL_CFLAGS = -Dgmic_is_parallel
ifneq ($(OS),Windows)
PARALLEL_LIBS = -lpthread
endif

# Flags to enable extra checking of image buffer pointers.
ifneq ($(OS),Darwin)
CHECKIMAGE_CFLAGS = # -Dgmic_check_image
endif

# Flags to enable cancellation points in CImg library methods.
# (may slow down the code a little bit).
ABORT_CFLAGS = -Dcimg_use_abort

# Flags to enable parallelization of image processing algorithms, using OpenMP.
OPENMP_CFLAGS = -fopenmp -Dcimg_use_openmp
OPENMP_LIBS = -lgomp

# Flags to enable image display, using X11
# (keep /usr/ dirname here since X11 is located in /usr/ on Mac too).
# This requires the presence of the X11 include and library files.
ifneq ($(OS),Darwin)
X11_CFLAGS = -Dcimg_display=1 -Dcimg_appname=\\\"gmic\\\" `pkg-config --cflags x11 || echo -I/usr/X11R6/include` #-Dcimg_use_xrandr
X11_LIBS = `pkg-config --libs x11 || echo -L/usr/X11R6/lib -lX11` -lpthread #`pkg-config --libs xrandr || echo -lXrandr`
else
ifeq (,$(wildcard /usr/X11R6))
X11_CFLAGS = -Dcimg_display=0 -Dcimg_appname=\\\"gmic\\\" #-Dcimg_use_xrandr
X11_LIBS = -lpthread #`pkg-config --libs xrandr || echo -lXrandr`
else
X11_CFLAGS = -Dcimg_display=1 -Dcimg_appname=\\\"gmic\\\" `pkg-config --cflags x11 || echo -I/usr/X11R6/include` #-Dcimg_use_xrandr
X11_LIBS = -L/usr/X11R6/lib -lX11 -lpthread #`pkg-config --libs xrandr || echo -lXrandr`
endif
endif
# Flags to enable fast display, using XShm.
# This requires the presence of the X11 extension include and library files.
XSHM_CFLAGS = -Dcimg_use_xshm `pkg-config --cflags xcb-shm`
XSHM_LIBS = `pkg-config --libs xcb-shm || echo -L$(USR)/X11R6/lib -lXext`

# Flags to enable image display, using GDI32.
# This requires the presence of the GDI32 include and library files (Windows only).
GDI32_CFLAGS = -Dcimg_display=2 -Dcimg_appname=\\\"gmic\\\"
GDI32_LIBS = -lgdi32

# Flags to enable native support for PNG image files, using the PNG library.
# This requires the presence of the libpng include and library files.
ifneq ($(OS),Darwin)
PNG_CFLAGS = -Dcimg_use_png `pkg-config --cflags libpng`
PNG_LIBS = `pkg-config --libs libpng || echo -lpng -lz`
else
ifeq (,$(wildcard /tmp/skl))
PNG_CFLAGS = -Dcimg_use_png `pkg-config --cflags libpng`
PNG_LIBS = `pkg-config --libs libpng || echo -lpng -lz`
else
PNG_CFLAGS = -Dcimg_use_png
PNG_LIBS = -lpng -lz
endif
endif

# Flags to enable native support for JPEG image files, using the JPEG library.
# This requires the presence of the libjpeg include and library files.
JPEG_CFLAGS = -Dcimg_use_jpeg
JPEG_LIBS = -ljpeg

# Flags to enable native support for TIFF image files, using the TIFF library.
# This requires the presence of the libtiff include and library files.
TIFF_CFLAGS = -Dcimg_use_tiff `pkg-config --cflags libtiff-4`
TIFF_LIBS = `pkg-config --libs libtiff-4 || echo -ltiff`

# Flags to enable native support for MINC2 image files, using the MINC2 library.
# ( http://en.wikibooks.org/wiki/MINC/Reference/MINC2.0_Users_Guide )
MINC2_CFLAGS = -Dcimg_use_minc2 -I${HOME}/local/include
MINC2_LIBS = -lminc_io -lvolume_io2 -lminc2 -lnetcdf -lhdf5 -lz -L${HOME}/local/lib

# Flags to enable native support for downloading files from the network.
# ( http://curl.haxx.se/libcurl/ )
CURL_CFLAGS = -Dcimg_use_curl `pkg-config --cflags libcurl`
CURL_LIBS = `pkg-config --libs libcurl || echo -lcurl`

# Flags to enable native support of webcams and video streaming, using the OpenCV library.
# This requires the presence of the OpenCV include and library files.
OPENCV_CFLAGS = -Dcimg_use_opencv  `pkg-config opencv --cflags || echo -I/usr/include/opencv` -I/usr/include/opencv
OPENCV_LIBS = `pkg-config opencv --libs || echo -lopencv_core -lopencv_highgui`

# Flags to enable native support of most classical image file formats, using the GraphicsMagick++ library.
# This requires the presence of the GraphicsMagick++ include and library files.
MAGICK_CFLAGS = -Dcimg_use_magick `pkg-config --cflags GraphicsMagick++ || echo -I$(USR)/$(INCLUDE)/GraphicsMagick`
MAGICK_LIBS = `pkg-config --libs GraphicsMagick++ || echo -lGraphicsMagick++`

# Flags to enable native support of EXR file format, using the OpenEXR library/
# This requires the presence of the OpenEXR include and library files.
OPENEXR_CFLAGS = -Dcimg_use_openexr -I$(USR)/$(INCLUDE)/OpenEXR
OPENEXR_LIBS = -lIlmImf -lHalf

# Flags to enable the use of the FFTW3 library.
# This requires the presence of the FFTW3 include and library files.
FFTW_CFLAGS = -Dcimg_use_fftw3 `pkg-config --cflags fftw3`
FFTW_LIBS = `pkg-config --libs fftw3 || echo -lfftw3`
ifneq ($(OS),Windows)
FFTW_LIBS += -lfftw3_threads
endif

# Flags to enable the use of the BOARD library.
# This requires the presence of the BOARD include and library files.
BOARD_CFLAGS = -Dcimg_use_board
BOARD_LIBS = -lboard

#---------------------------------------------------------
# Predefine sets of flags to build default configurations.
#---------------------------------------------------------

# CLI interface: Standard build.
#-------------------------------
GMIC_CLI_CFLAGS = $(MANDATORY_CFLAGS) $(ABORT_CFLAGS) $(PARALLEL_CFLAGS) $(FFTW_CFLAGS) $(CURL_CFLAGS) $(PNG_CFLAGS) $(JPEG_CFLAGS) $(TIFF_CFLAGS)
GMIC_CLI_LIBS = $(MANDATORY_LIBS) $(PARALLEL_LIBS) $(FFTW_LIBS) $(CURL_LIBS) $(PNG_LIBS) $(JPEG_LIBS) $(TIFF_LIBS)
ifeq ($(OS),Unix) # Unix.
GMIC_CLI_CFLAGS += $(OPENMP_CFLAGS) $(X11_CFLAGS) $(OPENEXR_CFLAGS) $(OPENCV_CFLAGS) # $(XSHM_CFLAGS) $(MAGICK_CFLAGS)
GMIC_CLI_LIBS += $(OPENMP_LIBS) $(X11_LIBS) $(OPENEXR_LIBS) $(OPENCV_LIBS) # $(XSHM_LIBS) # $(MAGICK_LIBS)
else
ifeq ($(OS),Darwin) # MacOSX.
GMIC_CLI_CFLAGS += $(X11_CFLAGS) $(OPENEXR_CFLAGS)
GMIC_CLI_LIBS += $(X11_LIBS) $(OPENEXR_LIBS) $(OPT_LIBS)
else # Windows.
GMIC_CLI_CFLAGS += $(OPENMP_CFLAGS) $(GDI32_CFLAGS) $(OPENCV_CFLAGS)
GMIC_CLI_LIBS += $(OPENMP_LIBS) $(GDI32_LIBS) $(OPENCV_LIBS)
endif
endif

# CLI interface: Static build.
#-----------------------------
GMIC_STATIC_CLI_PATH = $(USR)/${LIB}/x86_64-linux-gnu
GMIC_STATIC_CLI_EXTRA =
GMIC_STATIC_CLI_CFLAGS = $(MANDATORY_CFLAGS) $(ABORT_CFLAGS) $(PARALLEL_CFLAGS) $(ZLIB_CFLAGS) $(FFTW_CFLAGS) $(PNG_CFLAGS) $(JPEG_CFLAGS) -Dcimg_display=0
GMIC_STATIC_CLI_LIBS = $(PARALLEL_LIBS) \
	               $(GMIC_STATIC_CLI_PATH)/libpng.a \
	               $(GMIC_STATIC_CLI_PATH)/libjpeg.a \
                       $(GMIC_STATIC_CLI_PATH)/libz.a \
                       $(GMIC_STATIC_CLI_PATH)/libfftw3.a $(GMIC_STATIC_CLI_PATH)/libfftw3_threads.a \
	               $(GMIC_STATIC_CLI_EXTRA)

# GIMP plug-in interface: Standard build.
#----------------------------------------
GMIC_GIMP_CFLAGS = $(MANDATORY_CFLAGS) $(ABORT_CFLAGS) $(PARALLEL_CFLAGS) $(FFTW_CFLAGS) $(CURL_CFLAGS) $(PNG_CFLAGS) -Dcimg_use_rng
GMIC_GIMP_LIBS = $(MANDATORY_LIBS) $(PARALLEL_LIBS) $(FFTW_LIBS) $(CURL_LIBS) $(PNG_LIBS)
ifeq ($(OS),Unix) # Unix.
GMIC_GIMP_CFLAGS += $(OPENMP_CFLAGS) $(X11_CFLAGS)
GMIC_GIMP_LIBS += $(OPENMP_LIBS) $(X11_LIBS)
else
ifeq ($(OS),Darwin) # MaxOSX.
GMIC_GIMP_CFLAGS += $(X11_CFLAGS)
GMIC_GIMP_LIBS += $(X11_LIBS)
else # Windows.
GMIC_GIMP_CFLAGS += $(OPENMP_CFLAGS) $(GDI32_CFLAGS)
GMIC_GIMP_LIBS += -mwindows $(OPENMP_LIBS) $(GDI32_LIBS) -lpthread -DPSAPI_VERSION=1 -lpsapi
endif
endif

# Libgmic interface: Standard build.
#-----------------------------------
GMIC_LIB_CFLAGS = $(MANDATORY_CFLAGS) $(ABORT_CFLAGS) $(PARALLEL_CFLAGS) $(FFTW_CFLAGS) $(CURL_CFLAGS) $(PNG_CFLAGS) $(JPEG_CFLAGS) $(TIFF_CFLAGS)
GMIC_LIB_LIBS = $(MANDATORY_LIBS) $(PARALLEL_LIBS) $(FFTW_LIBS) $(CURL_LIBS) $(PNG_LIBS) $(JPEG_LIBS) $(TIFF_LIBS)
ifeq ($(OS),Unix) # Unix.
GMIC_LIB_CFLAGS += $(OPENMP_CFLAGS) $(X11_CFLAGS)
GMIC_LIB_LIBS += $(OPENMP_LIBS) $(X11_LIBS)
else
ifeq ($(OS),Darwin) # MacOSX.
GMIC_LIB_CFLAGS += $(X11_CFLAGS)
GMIC_LIB_LIBS += $(X11_LIBS) $(OPT_LIBS)
else # Windows.
GMIC_LIB_CFLAGS += $(OPENMP_CFLAGS) $(GDI32_CFLAGS)
GMIC_LIB_LIBS += $(OPENMP_LIBS) $(GDI32_LIBS)
endif
endif

# LibCgmic interface: Standard build.
#------------------------------------
GMIC_LIBC_CFLAGS = $(MANDATORY_CFLAGS) $(ABORT_CFLAGS) $(PARALLEL_CFLAGS) $(FFTW_CFLAGS) $(CURL_CFLAGS) $(PNG_CFLAGS) $(JPEG_CFLAGS) $(TIFF_CFLAGS)
GMIC_LIBC_LIBS = $(MANDATORY_LIBS) $(PARALLEL_LIBS) $(FFTW_LIBS) $(CURL_LIBS) $(PNG_LIBS) $(JPEG_LIBS) $(TIFF_LIBS)
ifeq ($(OS),Unix) # Unix.
GMIC_LIBC_CFLAGS += $(OPENMP_CFLAGS) $(X11_CFLAGS)
GMIC_LIBC_LIBS += $(OPENMP_LIBS) $(X11_LIBS)
else
ifeq ($(OS),Darwin) # MacOSX.
GMIC_LIBC_CFLAGS += $(X11_CFLAGS)
GMIC_LIBC_LIBS += $(X11_LIBS) $(OPT_LIBS)
else # Windows.
GMIC_LIBC_CFLAGS += $(OPENMP_CFLAGS) $(GDI32_CFLAGS)
GMIC_LIBC_LIBS += $(OPENMP_LIBS) $(GDI32_LIBS)
endif
endif

# LibCgmic interface: Static build.
#---------------------------------
GMIC_STATIC_LIBC_CFLAGS = $(MANDATORY_CFLAGS) $(ABORT_CFLAGS) $(PARALLEL_CFLAGS) $(FFTW_CFLAGS) $(PNG_CFLAGS) $(JPEG_CFLAGS) # $(CURL_CFLAGS) $(TIFF_CFLAGS)
ifeq ($(OS),Window)
GMIC_STATIC_LIBC_LIBS = -static -lpthread
endif
GMIC_STATIC_LIBC_LIBS += $(MANDATORY_LIBS) $(PARALLEL_LIBS) $(FFTW_LIBS) $(PNG_LIBS) $(JPEG_LIBS) # $(CURL_LIBS) $(TIFF_LIBS)
ifeq ($(OS),Unix) # Unix.
GMIC_STATIC_LIBC_CFLAGS += $(OPENMP_CFLAGS) $(X11_CFLAGS)
GMIC_STATIC_LIBC_LIBS += $(OPENMP_LIBS) $(X11_LIBS)
else
ifeq ($(OS),Darwin) # MacOSX.
GMIC_STATIC_LIBC_CFLAGS += $(X11_CFLAGS)
GMIC_STATIC_LIBC_LIBS += $(X11_LIBS) $(OPT_LIBS)
else # Windows.
GMIC_STATIC_LIBC_CFLAGS += $(OPENMP_CFLAGS) $(GDI32_CFLAGS)
GMIC_STATIC_LIBC_LIBS += $(OPENMP_LIBS) $(GDI32_LIBS)
endif
endif

# G'MIC Online interface: Standard build.
#----------------------------------------
GMICOL_LIB_PATH = /usr/lib/x86_64-linux-gnu/
GMICOL_CFLAGS = $(MANDATORY_CFLAGS) $(PARALLEL_CFLAGS) $(FFTW_CFLAGS) $(PNG_CFLAGS) $(JPEG_CFLAGS) $(ZLIB_CFLAGS) -Dcimg_display=0
GMICOL_LIBS = $(MANDATORY_LIBS) $(PARALLEL_LIBS) $(PNG_LIBS) $(JPEG_LIBS) $(ZLIB_LIBS) $(GMICOL_LIB_PATH)/libfftw3.a $(GMICOL_LIB_PATH)/libfftw3_threads.a

#--------------------------
# Define Makefile entries.
#--------------------------

# Main default entries.
print-%: ; @echo $*=$($*)

all: CImg.h gmic_stdlib.h
ifeq ($(OS),Unix)
	@echo "**"
	@echo "** Start building G'MIC with default Unix configuration."
	@echo "**"
	$(MAKE) cli gimp lib
else
ifeq ($(OS),Darwin)
	@echo "**"
	@echo "** Start building G'MIC with default MacOSX configuration."
	@echo "**"
	$(MAKE) cli gimp lib
else
	@echo "**"
	@echo "** Start building G'MIC with default Windows configuration."
	@echo "**"
	$(MAKE) cli gimp
endif
endif

cli:
	$(MAKE) "CFLAGS+=$(GMIC_CLI_CFLAGS) $(OPT_CFLAGS)" "LIBS+=$(GMIC_CLI_LIBS)" gmic
	$(STRIP) gmic$(EXE)

cli_s:
ifeq ($(OS),Darwin)
	c++ -o gmic gmic.cpp -Dgmic_main -Dgmic_build -Dcimg_use_zlib -I/opt/local/include  -Dgmic_prerelease=\"011316\" -std=c++11 -stdlib=libc++ -Dcimg_use_abort -Dcimg_use_png -Dcimg_use_jpeg -Dcimg_use_tiff -I/opt/local/include  -Dcimg_use_curl -I/opt/local/include  -Dcimg_use_fftw3 -I/opt/local/include  -Dgmic_is_parallel -Dcimg_display=1 -Dcimg_appname=\"gmic\" -I/opt/local/include -O2 -mmacosx-version-min=10.7 -L/tmp/skl/Gimp.app/Contents/Resources/lib -lpng -lz -ljpeg  -ltiff -lcurl -lfftw3  -lfftw3_threads -lpthread -lHalf -L/usr/X11R6/lib -lX11 -lpthread
	$(STRIP) gmic
endif

debug:
	$(MAKE) "CFLAGS+=$(GMIC_CLI_CFLAGS) $(DEBUG_CFLAGS)" "LIBS+=$(GMIC_CLI_LIBS)" gmic

gimp:
	$(MAKE) "CFLAGS+=$(GMIC_GIMP_CFLAGS) $(OPT_CFLAGS)" "LIBS+=$(GMIC_GIMP_LIBS)" gmic_gimp

gmicol:
	$(MAKE) "CFLAGS+=$(GMICOL_CFLAGS) $(OPT_CFLAGS)" "LIBS+=$(GMICOL_LIBS)" gmic
	$(STRIP) gmic$(EXE)

lib:
	$(MAKE) "CFLAGS+=$(GMIC_LIB_CFLAGS) $(OPT_CFLAGS)" "LIBS+=$(GMIC_LIB_LIBS)" libgmic

libc:
	$(MAKE) "CFLAGS+=$(GMIC_LIBC_CFLAGS) $(OPT_CFLAGS)" "LIBS+=$(GMIC_LIBC_LIBS)" libcgmic

native:
	$(MAKE) "CFLAGS+=$(GMIC_CLI_CFLAGS) -Ofast -march=native" "LIBS+=$(GMIC_CLI_LIBS)" cli gimp
	$(STRIP) gmic$(EXE)

static:
	$(MAKE) "CFLAGS+=$(GMIC_STATIC_CLI_CFLAGS) $(OPT_CFLAGS)" "LIBS+=$(GMIC_STATIC_CLI_LIBS)" gmic
	$(STRIP) gmic$(EXE)

staticlibc:
	$(MAKE) "CFLAGS+=$(GMIC_STATIC_LIBC_CFLAGS) $(OPT_CFLAGS)" "LIBS+=$(GMIC_STATIC_LIBC_LIBS)" libcgmicstatic

zart: CImg.h gmic_stdlib.h
	@if [ ! -d ../zart ]; then ln -fs ../gmic-community/zart ..; fi
ifeq ($(OS),Darwin)
	cd ../zart && qmake zart.pro && $(MAKE) && $(STRIP) zart.app/Contents/MacOS/zart
else
	cd ../zart && qmake zart.pro && $(MAKE) && $(STRIP) zart
endif

# Internal rules to build compilation modules.

libgmic.o: gmic.cpp gmic.h gmic_stdlib.h CImg.h
	$(CXX) -o libgmic.o -c gmic.cpp $(PIC) $(CFLAGS)
libgmic: libgmic.o use_libgmic.cpp
	ar rcs libgmic.a libgmic.o
ifeq ($(OS),Darwin)
	$(CXX) -shared -o libgmic.so libgmic.o $(LIBS)
else
	$(CXX) -shared -Wl,-soname,libgmic.so.1 -o libgmic.so libgmic.o $(LIBS)
	$(CXX) -o use_libgmic use_libgmic.cpp -L. -lgmic $(LIBS)
endif

libcgmic.o: gmic.cpp gmic.h gmic_stdlib.h CImg.h
	@if [ ! -f gmic_libc.cpp ]; then ln -fs ../../gmic-community/libcgmic/gmic_libc.cpp .; fi
	@if [ ! -f gmic_libc.h ]; then ln -fs ../../gmic-community/libcgmic/gmic_libc.h .; fi
	$(CXX) -o libcgmic.o -c gmic_libc.cpp $(PIC) $(CFLAGS)

libcgmic: libgmic.o libcgmic.o
ifeq ($(OS),Darwin)
	ar rcs libcgmic.a libcgmic.o
	$(CXX) -shared -o libcgmic.so libcgmic.o libgmic.o $(LIBS)
else
ifeq ($(OS),Windows)
	$(CXX) -shared -Wl,-soname,libcgmic.so.1 -o libcgmic$(SO) libcgmic.o libgmic.o -Wl,--output-def,libcgmic.def,--out-implib,libcgmic.a $(LIBS)
else
	$(CXX) -shared -Wl,-soname,libcgmic.so.1 -o libcgmic$(SO) libcgmic.o libgmic.o $(LIBS)
endif
endif
	@if [ ! -f use_libcgmic.c ]; then ln -fs ../../gmic-community/libcgmic/use_libcgmic.c .; fi
	$(CC) -std=c99 -o use_libcgmic use_libcgmic.c -L. -lcgmic

libcgmicstatic.o: gmic.cpp gmic.h gmic_stdlib.h CImg.h
	$(CXX) -o libcgmicstatic.o -c gmic_libc.cpp $(PIC) $(CFLAGS)

libcgmicstatic: libcgmicstatic.o libgmic.o
ifeq ($(OS),Darwin)
	ar rcs libcgmicstatic.a libcgmicstatic.o
	$(CXX) -shared -static -o libcgmicstatic.so libcgmicstatic.o libgmic.o $(PIC) $(CFLAGS) $(LIBS)
else
ifeq ($(OS),Windows)
	$(CXX) -shared -static -Wl,-soname,libcgmicstatic.so.1 -o libcgmicstatic$(SO) libcgmicstatic.o libgmic.o $(PIC) -Wl,--output-def,libcgmicstatic.def,--out-implib,libcgmicstatic.a $(CFLAGS) $(LIBS)
else
	$(CXX) -shared -Wl,-soname,libcgmicstatic.so.1 -o libcgmicstatic$(SO) libcgmicstatic.o libgmic.o $(PIC) $(CFLAGS) $(LIBS)
endif
endif
	$(CC) -std=c99 -o use_libcgmic_static use_libcgmic.c -L. -lcgmicstatic

gmic_gimp.o: gmic.cpp gmic.h gmic_stdlib.h CImg.h
	$(CXX) -o gmic_gimp.o -c gmic.cpp -Dgmic_gimp $(CFLAGS)

gmic_gimp: gmic_gimp.o gmic_gimp.cpp
ifeq ($(OS),Darwin)
ifeq (,$(wildcard /tmp/skl))
	$(CXX) -o gmic_gimp gmic_gimp.cpp gmic_gimp.o -Dgmic_gimp `gimptool-2.0$(EXE) --cflags` `gimptool-2.0$(EXE) --libs` $(CFLAGS) $(LIBS)
else
	$(CXX) -o gmic_gimp gmic_gimp.cpp gmic_gimp.o -Dgmic_gimp `gimptool-2.0$(EXE) --cflags` $(subst -L/opt/local,-L/tmp/skl/Gimp.app/Contents/Resources,$(shell gimptool-2.0$(EXE) --libs) $(CFLAGS) $(LIBS))
endif
else
	$(CXX) -o gmic_gimp gmic_gimp.cpp gmic_gimp.o -Dgmic_gimp `gimptool-2.0$(EXE) --cflags` `gimptool-2.0$(EXE) --libs` $(CFLAGS) $(LIBS)
endif
	$(STRIP) gmic_gimp$(EXE)

gmic: gmic.cpp gmic.h gmic_stdlib.h CImg.h
	$(CXX) -o gmic gmic.cpp -Dgmic_main $(CFLAGS) $(LIBS)

gmic_stdlib.h:
	@echo -n "> Retrieve G'MIC Standard Library..."
	@$(WGET) gmic_stdlib.h http://gmic.eu/gmic_stdlib.h
	@touch gmic_stdlib.h
	@echo " done!"
CImg.h:
	@echo -n "> Retrieve CImg Library..."
	@if [ -f ../../CImg/CImg.h ]; then \
          if [ ! -f ./CImg.h ]; then ln -s ../../CImg/CImg.h .; fi; \
        else \
          $(WGET) CImg.h https://github.com/dtschump/CImg/blob/master/CImg.h?raw=true; \
          touch CImg.h; \
        fi
	@echo " done!"

# Generate gmic_stdlib.h file (requires 'gmic' to be installed first).
stdlib:
	@echo "/*\n\
#\n\
#  File        : gmic_stdlib.h\n\
#                ( C++ header file )\n\
#\n\
#  Description : This file defines compressed buffers containing the command\n\
#                definitions of the G'MIC Standard Library, as well as the\n\
#                default filters in the G'MIC plug-in for GIMP.\n\
#                This file has been automatically generated by Makefile entry\n\
#                'stdlib:' (mainly from the G'MIC command file 'gmic_stdlib.gmic').\n\
#                ( http://gmic.eu )\n\
#\n\
#  Copyright   : David Tschumperle\n\
#                ( http://tschumperle.users.greyc.fr/ )\n\
#\n\
#  License     : CeCILL v2.0\n\
#                ( http://www.cecill.info/licences/Licence_CeCILL_V2-en.html )\n\
#\n\
#  This software is governed by the CeCILL  license under French law and\n\
#  abiding by the rules of distribution of free software.  You can  use,\n\
#  modify and/ or redistribute the software under the terms of the CeCILL\n\
#  license as circulated by CEA, CNRS and INRIA at the following URL\n\
#  \"http://www.cecill.info\".\n\
#\n\
#  As a counterpart to the access to the source code and  rights to copy,\n\
#  modify and redistribute granted by the license, users are provided only\n\
#  with a limited warranty  and the software's author,  the holder of the\n\
#  economic rights,  and the successive licensors  have only  limited\n\
#  liability.\n\
#\n\
#  In this respect, the user's attention is drawn to the risks associated\n\
#  with loading,  using,  modifying and/or developing or reproducing the\n\
#  software by the user in light of its specific status of free software,\n\
#  that may mean  that it is complicated to manipulate,  and  that  also\n\
#  therefore means  that it is reserved for developers  and  experienced\n\
#  professionals having in-depth computer knowledge. Users are therefore\n\
#  encouraged to load and test the software's suitability as regards their\n\
#  requirements in conditions enabling the security of their systems and/or\n\
#  data to be ensured and,  more generally, to use and operate it in the\n\
#  same conditions as regards security.\n\
#\n\
#  The fact that you are presently reading this means that you have had\n\
#  knowledge of the CeCILL license and that you accept its terms.\n\
#\n\
*/\n" > gmic_stdlib.h
	@echo "#ifndef gmic_gimp" >> gmic_stdlib.h
	@\gmic gmic_stdlib.gmic raw:gmic_stdlib.gmic,char -compress_gmic_cli 1 -a y -serialize char,1,0 -o -.h,uchar | sed 's/unsigned char/const unsigned char/' | sed 's/unnamed/gmic_stdlib/' >> gmic_stdlib.h
	@echo "#else" >> gmic_stdlib.h
	@\gmic gmic_stdlib.gmic raw:gmic_stdlib.gmic,char -compress_gmic_gimp 1 -a y -serialize char,1,0 -o -.h,uchar | sed 's/unsigned char/const unsigned char/' | sed 's/unnamed/gmic_stdlib/' >> gmic_stdlib.h
	@\gmic -v - ../html/img/gimp_logos.png -serialize uchar,1,0 -o -.h,uchar | sed 's/unsigned char/const unsigned char/' | sed 's/unnamed/gmic_logo/' >> gmic_stdlib.h
	@echo "const unsigned long size_data_gmic_logo = (unsigned long)sizeof(data_gmic_logo);" >> gmic_stdlib.h
	@echo "#endif" >> gmic_stdlib.h
	@echo "const unsigned long size_data_gmic_stdlib = (unsigned long)sizeof(data_gmic_stdlib);" >> gmic_stdlib.h
	@echo >>gmic_stdlib.h
	@\gmic -v - -_update_server_upload gmic_stdlib.h

# Generate bash completion script (requires 'gmic' to be installed first).
bashcompletion:
	@mkdir -p ../resources
	@\gmic -v - gmic_stdlib.gmic raw:gmic_stdlib.gmic,uchar -document_gmic bash 2> ../resources/gmic_bashcompletion.sh

# Generate man page (requires 'gmic' to be installed).
man:
	@mkdir -p ../man
	@\gmic -v - gmic_stdlib.gmic raw:gmic_stdlib.gmic,uchar -__help man 2> ../man/gmic.1
	@gzip -f ../man/gmic.1

# Install/uninstall/clean procedures.
install:
	mkdir -p $(DESTDIR)$(PLUGINDIR)/
	cp -f gmic_gimp $(DESTDIR)$(PLUGINDIR)/
	cp -f ../resources/gmic_film_cluts.gmz $(DESTDIR)$(PLUGINDIR)/
	mkdir -p $(DESTDIR)$(USR)/$(BIN)/
	cp -f gmic $(DESTDIR)$(USR)/$(BIN)/
	mkdir -p $(DESTDIR)$(USR)/$(INCLUDE)/
	cp -f gmic.h $(DESTDIR)$(USR)/$(INCLUDE)/
	@if [ -f gmic_libc.h ]; then cp -f gmic_libc.h $(DESTDIR)$(USR)/$(INCLUDE)/; fi

ifneq ($(OS),Darwin)
	mkdir -p $(DESTDIR)$(USR)/share
	mkdir -p $(DESTDIR)$(USR)/$(LIB)
	cp -f libgmic.so $(DESTDIR)$(USR)/$(LIB)/libgmic.so._gmic_versio3
	ln -fs libgmic.so._gmic_versio3 $(DESTDIR)$(USR)/$(LIB)/libgmic.so._gmic_versio1
	ln -fs libgmic.so._gmic_versio1 $(DESTDIR)$(USR)/$(LIB)/libgmic.so
	@if [ -f libcgmic.so ]; then \
	  cp -f gmic_libc.h $(DESTDIR)$(USR)/$(INCLUDE)/; \
	  cp -f libcgmic.so $(DESTDIR)$(USR)/$(LIB)/libcgmic.so._gmic_versio3; \
          ln -fs libcgmic.so._gmic_versio3 $(DESTDIR)$(USR)/$(LIB)/libcgmic.so._gmic_versio1 ; \
          ln -fs libcgmic.so._gmic_versio1 $(DESTDIR)$(USR)/$(LIB)/libcgmic.so; \
        fi
	@if [ -f ../zart/zart ]; then \
	  cp -f ../zart/zart $(DESTDIR)$(USR)/$(BIN)/; \
        fi
endif
	mkdir -p $(DESTDIR)$(USR)/share/man/
	mkdir -p $(DESTDIR)$(USR)/share/man/man1/
	mkdir -p $(DESTDIR)$(USR)/share/man/fr/man1/
	cp -f ../man/gmic.1.gz $(DESTDIR)$(USR)/share/man/man1/gmic.1.gz
	cp -f ../man/gmic.1.gz $(DESTDIR)$(USR)/share/man/fr/man1/gmic.1.gz
	if [ -d /etc/bash_completion.d/ ]; then mkdir -p $(DESTDIR)/etc/bash_completion.d/; cp -f ../resources/gmic_bashcompletion.sh $(DESTDIR)/etc/bash_completion.d/gmic; fi
	if [ -d /opt/local/etc/bash_completion.d/ ]; then mkdir -p $(DESTDIR)/opt/local/etc/bash_completion.d/; cp -f ../resources/gmic_bashcompletion.sh $(DESTDIR)/opt/local/etc/bash_completion.d/gmic; fi

uninstall:
	rm -f $(DESTDIR)$(USR)/$(BIN)/gmic
	rm -f $(DESTDIR)$(USR)/$(BIN)/zart
	rm -f $(DESTDIR)$(USR)/$(INCLUDE)/gmic.h
	rm -f $(DESTDIR)$(USR)/$(LIB)/libgmic.so._gmic_versio3
	rm -f $(DESTDIR)$(USR)/$(LIB)/libgmic.so._gmic_versio1
	rm -f $(DESTDIR)$(USR)/$(LIB)/libgmic.so
	rm -f $(DESTDIR)$(USR)/$(LIB)/libcgmic.so._gmic_versio3
	rm -f $(DESTDIR)$(USR)/$(LIB)/libcgmic.so._gmic_versio1
	rm -f $(DESTDIR)$(USR)/$(LIB)/libcgmic$(SO)
	rm -rf $(DESTDIR)$(USR)/share/doc/gmic/
	rm -f $(DESTDIR)$(USR)/share/man/man1/gmic.1.gz
	rm -f $(DESTDIR)$(USR)/share/man/fr/man1/gmic.1.gz

distclean: clean

clean:
	rm -rf CImg.h gmic_stdlib.h gmic$(EXE) gmic_gimp$(EXE) use_libgmic$(EXE) use_libcgmic$(EXE) use_libcgmic_static$(EXE) gmic*.o libgmic* libcgmic* *~
	@if [ -h ../zart ]; then rm -f ../zart; fi
	@if [ -h gmic_libc.h ]; then rm -f gmic_libc.h; fi
	@if [ -h gmic_libc.cpp ]; then rm -f gmic_libc.cpp; fi
	@if [ -h use_libcgmic.c ]; then rm -f use_libcgmic.c; fi

# End of Makefile.
