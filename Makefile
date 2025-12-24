# Makefile for xli
# 

# miscellaneous defines.
# -DHAVE_GUNZIP if having gunzip and wanting to use .gz rather than  .Z files
# -DHAVE_BUNZIP2 if having bzip2 and wanting to handle .bz2 files
# -DNO_UNCOMPRESS if system doesn't have uncompress

HAS_JPEG ?= 1
HAS_PNG  ?= 1
HAS_TIFF ?= 1
HAS_WEBP ?= 1
HAS_EXIF ?= 1


MISC_DEFINES=
PKG_CONFIG=pkg-config
SYSPATHFILE=/usr/lib/X11/xlirc
INSTALLDIR=/usr/bin/X11
SHELL= /bin/sh
MAKE= make
STD_CC= cc
GCC= gcc
CP= cp
LN= ln -s
RM= rm -f
MV= mv -f
LIBS= -lX11 -lm -lXext

ifeq ($(HAS_JPEG),1)
	CPPFLAGS += -DHAS_JPEG `$(PKG_CONFIG) --cflags libjpeg`
	LIBS     += `$(PKG_CONFIG) --libs libjpeg`
	OPT_SRCS += jpeg.c
	OPT_OBJS += jpeg.o
endif

ifeq ($(HAS_PNG),1)
	CPPFLAGS += -DHAS_PNG `$(PKG_CONFIG) --cflags libpng`
	LIBS     += `$(PKG_CONFIG) --libs libpng`
	OPT_SRCS += png.c
	OPT_OBJS += png.o
endif

ifeq ($(HAS_TIFF),1)
	CPPFLAGS += -DHAS_TIFF
	LIBS     += -ltiff
	OPT_SRCS += tiff.c
	OPT_OBJS += tiff.o
endif

ifeq ($(HAS_WEBP),1)
	CPPFLAGS += -DHAS_WEBP
	LIBS     += -lwebp
	OPT_SRCS += webp.c
	OPT_OBJS += webp.o
endif

ifeq ($(HAS_EXIF),1)
	CPPFLAGS += -DHAS_EXIF
	LIBS     += -lexif
	OPT_SRCS += exif.c
	OPT_OBJS += exif.o
endif

CFLAGS+= -Wall -Wextra -fstack-protector-strong -O1 -DSYSPATHFILE=\"$(SYSPATHFILE)\" $(OPTIONALFLAGS) $(EXTRAFLAGS)
GCCFLAGS= -fstrength-reduce -finline-functions

MISC= Makefile.std README ABOUTGAMMA CHANGES TODO INSTALL xli.man xliguide.txt 

BINMISC= chkgamma.jpg

INCS= cmuwmrast.h copyright.h fbm.h g3.h gif.h image.h imagetypes.h \
      img.h kljcpyrght.h mac.h mcidas.h mrmcpyrght.h options.h \
      pbm.h rle.h sunraster.h tgncpyrght.h xli.h xwd.h mit.cpyrght rgbtab.h \
      tga.h bmp.h ddxli.h

SRCS1= bright.c clip.c cmuwmrast.c compact.c dither.c faces.c fbm.c \
       fill.c  g3.c gif.c halftone.c imagetypes.c img.c mac.c  \
       merge.c misc.c new.c options.c path.c pbm.c pcx.c \
       reduce.c rle.c rlelib.c root.c rotate.c send.c smooth.c \
       sunraster.c value.c window.c xbitmap.c xli.c \
       xpixmap.c xwd.c zio.c zoom.c ddxli.c tga.c bmp.c $(OPT_SRCS)

OBJS1= bright.o clip.o cmuwmrast.o compact.o dither.o faces.o fbm.o \
       fill.o  g3.o gif.o halftone.o imagetypes.o img.o mac.o  \
       merge.o misc.o new.o options.o path.o pbm.o pcx.o \
       reduce.o rle.o rlelib.o root.o rotate.o send.o smooth.o \
       sunraster.o value.o window.o xbitmap.o xli.o \
       xpixmap.o xwd.o zio.o zoom.o ddxli.o tga.o bmp.o $(OPT_OBJS)


ALLTXT= $(MISC) $(INCS) $(SRCS1)

ALL= $(ALLTXT) $(BINMISC)

# standard build with gcc
std:
	@echo "Building standard distribution."
	@$(MAKE) all CC=$(GCC) EXTRAFLAGS="$(GCCFLAGS)"

# system-v build with cc
sysv:
	@echo "Building standard distribution for System-V."
	$(MAKE) all CC=$(STD_CC) EXTRAFLAGS=-DSYSV

# sysv build with gcc
sysv-gcc:
	@echo "Building System-V distribution with GNU cc."
	$(MAKE) all CC=$(GCC) EXTRAFLAGS="-DSYSV $(GCCFLAGS)"

#install:: $(SYSPATHFILE)
#	$(RM) $(INSTALLDIR)/xli
#	$(CP) xli $(INSTALLDIR)/xli


xli: $(OBJS1)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o xli $(OBJS1) $(LIBS)

all:: xli

#.c.o: xli.h
#	$(GCC) -c $(CFLAGS) $(CPPFLAGS) $*.c

clean::
	$(RM) *.o *~ xli *.tar *.tar.gz

.PHONY: debian
debian:
	dpkg-buildpackage -rfakeroot -D -b -uc

$(SYSPATHFILE):
	@echo "*** Creating default $(SYSPATHFILE) since none found."
	@echo "*** See the xli manual page for details on the contents"
	@echo "*** of this file."
	cp /dev/null $(SYSPATHFILE)
	echo "path= /usr/local/images" \
		>> $(SYSPATHFILE)
	echo "extension=.gif .jpg .rle .csun .msun .sun .face .xbm .bm" \
		>>$(SYSPATHFILE)

# target for making man and text manual entries

xli.1: xli.man
	$(RM) xli.1
	nroff -T37 -man xli.man > xli.1

xli.txt: xli.man
	$(RM) xli.txt
	nroff -man -Tlp xli.man | col -b > xli.txt

# target for building debuggable versions

debug:
	@echo Building a debugging version of xli.
	make xli CFLAGS="-Wall -Wextra -Wstrict-prototypes -Winline -Wpadded   -Woverlength-strings -Wstack-protector -Wlong-long  -Wold-style-definition -fsanitize=address  -fno-omit-frame-pointer -g -DDEBUG -DSYSPATHFILE=\\\"$(SYSPATHFILE)\\\""

debug-sysv:
	@echo Building a debugging version of xli for System-V.
	make xli CC=$(STD_CC) CFLAGS="-g -DDEBUG -DSYSPATHFILE=\\\"$(SYSPATHFILE)\\\" -DSYSV"
