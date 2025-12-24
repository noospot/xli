# Makefile for xli
# 

# miscellaneous defines.
# -DHAVE_GUNZIP if having gunzip and wanting to use .gz rather than  .Z files
# -DHAVE_BUNZIP2 if having bzip2 and wanting to handle .bz2 files
# -DNO_UNCOMPRESS if system doesn't have uncompress

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
LIBS= -lX11 -lm -lXext -lexif -lwebp -ltiff `$(PKG_CONFIG) --libs libpng` `$(PKG_CONFIG) --libs libjpeg`
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
       reduce.c jpeg.c rle.c rlelib.c root.c rotate.c send.c smooth.c \
       sunraster.c $(OPTIONALSFILES) value.c window.c xbitmap.c xli.c \
       xpixmap.c xwd.c zio.c zoom.c ddxli.c tga.c bmp.c png.c \
       exif.c webp.c tiff.c

OBJS1= bright.o clip.o cmuwmrast.o compact.o dither.o faces.o fbm.o \
       fill.o  g3.o gif.o halftone.o imagetypes.o img.o mac.o  \
       merge.o misc.o new.o options.o path.o pbm.o pcx.o \
       reduce.o jpeg.o rle.o rlelib.o root.o rotate.o send.o smooth.o \
       sunraster.o $(OPTIONALOFILES) value.o window.o xbitmap.o xli.o \
       xpixmap.o xwd.o zio.o zoom.o ddxli.o tga.o bmp.o png.o \
       exif.o webp.o tiff.o


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
	$(GCC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o xli $(OBJS1) $(LIBS)

all:: xli

#.c.o: xli.h
#	$(GCC) -c $(CFLAGS) $(CPPFLAGS) $*.c

clean::
	rm -f *.o *~ xli *.tar *.tar.gz

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
