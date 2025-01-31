/* xli.h:
 * jim frost 06.21.89
 *
 * Copyright 1989 Jim Frost.  See included file "copyright.h" for complete
 * copyright information.
 */

#if defined(SVR4) && !defined(SYSV)
	#define SYSV			/* SYSV is out System V flag */
#endif

#include "patchlevel.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#ifndef VMS
	#include <unistd.h>
#endif
#include <string.h>


typedef uint8_t byte;	/* unsigned byte type */
typedef int8_t sbyte;
typedef char strbyte;

#ifndef FALSE
	#define FALSE 0
	#define TRUE (!FALSE)
#endif


#include "ddxli.h"
#include "image.h"
#include "options.h"

#ifndef INIT_MAXIMAGES
	#define INIT_MAXIMAGES 1024
#endif

/* image name and option structure used when processing arguments */
typedef struct {
	strbyte* name;		/* name of image */
	strbyte* fullname;		/* full pathname for -delete */
	int loader_idx;		/* loader last used successfully with image */
	int atx, aty;		/* location to load image at */
	bool ats;		/* TRUE if atx and aty have been set */
	bool center;		/* true if image is to be centered */
	bool colordither;	/* true if color reduction is to dither image */
	bool expand;		/* true if image should be forced to TrueColor depth */	
	unsigned int bright;	/* brightness multiplier */
	int clipx, clipy;	/* Offset and area of image to be used */
	unsigned int clipw, cliph;
	strbyte* border;	/* Border colour used in clipping */
	XColor bordercol;	/* X RGB of above */
	unsigned int colors;	/* max # of colors to use for this image */
	int delay;		/* # of seconds delay before auto pic advance */
	unsigned int dither;	/* true if image is to be dithered */
	float gamma;		/* display gamma */
	bool gray;		/* true if image is to be grayed */
	bool merge;		/* true if we should merge onto previous */
	bool normalize;	        /* true if image is to be normalized */
	bool smooth;		/* true if image is to be smoothed */
	int rotate;		/* # degrees to rotate image */
	strbyte* title;		/* Override title on image */
	unsigned int xzoom, yzoom;	/* zoom percentages */
	strbyte* fg, *bg;	/* foreground/background colors if mono image */
	int xpmkeyc;		/* Overriding color context key value for xpm pixmaps */
	int iscale;		/* image-dependent scaling factor */
	bool iscale_auto;	/* automatically iscale to fit on screen */
	bool zoom_auto;	        /* automatically zoom to fit on screen */
	short reseerved0;       // padding
	int reserved1;          // padding
} ImageOptions;

/* globals and global options
 */

typedef struct {
	DisplayInfo dinfo;	/* device dependent display information */
	strbyte* argv0;		/* name of this programs */
	strbyte* lastfunc;	/* name of last function called (used in error handlers) */
	int _Xdebug;		/* dump on X error flag */
	int _DumpCore;		/* dump on signal flag */
	float display_gamma;
	strbyte* dname;
	bool forall;
	bool fillscreen;
	bool fit;
	bool fullscreen;
	strbyte* go_to;		/* label to goto */
	bool identify;
	bool install;
	bool onroot;
	bool private_cmap;
	int visual_class;	/* user-defined visual class */
	bool set_default;
	bool use_pixmap;
	short reserved;         /* padding */
	strbyte* user_geometry;	/* -geometry passed by user */

	unsigned int dest_window;	/* window id to put image onto */
	bool do_fork;
	bool verbose;	
	bool delete;		/* enable deleting current image with 'x' */
	bool focus;		/* take keyboard focus when viewing in window */
} GlobalsRec;

/* Global declarations */

extern GlobalsRec globals;

#define CURRFUNC(aa) (globals.lastfunc = (aa))

/* Gamma correction stuff */

// use files gamma values if there are some
#define  FILE_GAMMA 0

/* the default target display gamma. This can be overridden on the
 * command line or by settin an environment variable.
 */
#define DEFAULT_DISPLAY_GAMMA 2.2

/* the default IRGB image gamma. This can be overridden on the
 * command line.
 */
#define DEFAULT_IRGB_GAMMA 2.2

/* Compare gammas for equality */
#define GAMMA_NOT_EQUAL(g1,g2)   ((g1) > ((g2) + 0.00001) || (g1) < ((g2) - 0.00001))

/* Cached/uncompressed file I/O structures. */
struct cache {
	byte* end;
	byte buf[BUFSIZ];
	struct cache* next;
	bool eof;
	byte  reserved0;   // padding
	short reserved1;   // padding
};






typedef struct {
	unsigned int type;	/* ZIO file type */
	FILE* stream;		/* file input stream */
	strbyte* filename;	/* filename */
	struct cache* data;	/* data cache */
	struct cache* dataptr;	/* ptr to current cache block */
	byte* bufptr;		/* ptr within current cache block */
	byte* endptr;		/* ptr to end of current cache block */
	byte* auxb;		/* non NULL if auxiliary buffer in use */
	byte* oldbufptr;	/* save bufptr here when aux buffer is in use */
	byte* oldendptr;	/* save endptr here when aux buffer is in use */
	byte buf[BUFSIZ];	/* getc buffer when un-cached */
	bool nocache;	        /* TRUE if caching has been disabled */
	bool eof;		/* TRUE if we are at encountered EOF */
	short reserved0;        // padding
} ZFILE;

#define ZSTANDARD 0		/* standard file */
#define ZPIPE     1		/* file is a pipe (ie uncompress) */
#define ZSTDIN    2		/* file is stdin */

/*
   C library functions that may not be decalared elsewehere
 */

int atoi( const strbyte* );
long atol( const strbyte* );
double atof( const strbyte* );
strbyte* getenv( const strbyte* );

/* function declarations */

/*
 * Note on image processing functions :-
 *
 * The assumption is always that an image processing function that returns
 * an image may return a new image or one of the input images, and that
 * input images are preserved.
 *
 * This means that within an image processing function the following
 * technique of managing intermediate images is recomended:
 *
 * Image *func(isrc)
 * Image *isrc;
 *   {
 *   Image *src = isrc, *dst, *tmp;
 *   .
 *   .
 *   tmp = other_func(src);
 *   if (src != tmp && src != isrc)
 *     freeImage(src);
 *   src = tmp;
 *   .
 *   .
 *   dst = process(src);
 *   .
 *   .
 *   if (src != isrc)
 *     freeImage(src);
 *   return (dst);
 *
 * This may seem redundant in places, but allows changes to be made
 * without looking at the overal image structure usage.
 *
 */

/* imagetypes.c */
void supportedImageTypes( void );

/* misc.c */
strbyte* tail( strbyte* path );
void memoryExhausted( void );
void internalError( int sig );
void version( void );
void usage( strbyte* name );
Image* processImage( DisplayInfo* dinfo, Image* iimage, ImageOptions* options );
int errorHandler( Display* disp, XErrorEvent* error );
extern short LEHexTable[];	/* Little Endian conversion value */
extern short BEHexTable[];	/* Big Endian conversion value */
#define HEXSTART_BAD -1		/* bitmap_faces useage */
#define HEXDELIM_BAD -2
#define HEXDELIM_IGNORE -3
#define HEXBAD   -4
void initLEHexTable( void );
void initBEHexTable( void );
/* ascii hex number to integer (string, length) */
int hstoi( strbyte* s, int n );
strbyte* xlistrstr( strbyte* s1, strbyte* s2 );

/* path.c */
strbyte* expandPath( strbyte* p );
int findImage( strbyte* name, strbyte* fullname );
void listImages( void );
void loadPathsAndExts( void );
void showPath( void );

/* root.c */
void imageOnRoot( DisplayInfo* dinfo, Image* image, ImageOptions* options );

/* window.c */
void cleanUpWindow( DisplayInfo* dinfo );
strbyte imageInWindow( DisplayInfo* dinfo, Image* image, ImageOptions* options,
                    int argc, strbyte** argv );

/* options.c */
int visualClassFromName( strbyte* name );
strbyte* nameOfVisualClass( int class );

/* clip.c */
Image* clip( Image* iimage, int clipx, int clipy, unsigned int clipw,
             unsigned int cliph, ImageOptions* imgopp );

/* bright.c */
void brighten( Image* image, unsigned int percent, unsigned int verbose );
void gray( Image* image, int verbose );
Image* normalize( Image* image, unsigned int verbose );
void gammacorrect( Image* image, float target_gam, unsigned int verbose );
extern int gammamap[256];
#define GAMMA16(color16) (gammamap[(color16)>>8]<<8)
#define GAMMA8(color8) (gammamap[(color8)])
#define GAMMA16to8(color16) (gammamap[(color16)>>8])
void defaultgamma( Image* image );

/* compact.c */
void compact_cmap( Image* image, unsigned int verbose );

/* dither.c */
Image* dither( Image* cimage, unsigned int verbose );

/* exif.c */
Image* exif_rotation( Image* image, const strbyte* file );

/* fill.c */
void fill( Image* image, unsigned int fx, unsigned int fy, unsigned int fw, unsigned int fh, Pixel pixval );

/* halftone.c */
Image* halftone( Image* cimage, unsigned int verbose );

/* imagetypes.c */
Image* loadImage( ImageOptions* image_ops, bool verbose );
void identifyImage( strbyte* name );

/* merge.c */
Image* merge( Image* idst, Image* isrc, int atx, int aty, ImageOptions* imgopp );

/* new.c */
extern unsigned long DepthToColorsTable[];
unsigned long colorsToDepth( long unsigned int ncolors );
strbyte* dupString( strbyte* s );
Image* newBitImage( unsigned int width, unsigned int height );
Image* newRGBImage( unsigned int width, unsigned int height, unsigned int depth );
Image* newTrueImage( unsigned int width, unsigned int height );
void freeImage( Image* image );
void freeImageData( Image* image );
void newRGBMapData( RGBMap* rgb, unsigned int size );
void resizeRGBMapData( RGBMap* rgb, unsigned int size );
void freeRGBMapData( RGBMap* rgb );
byte* lcalloc( unsigned int size );
byte* lmalloc( unsigned int size );
byte* lrealloc( byte* old, unsigned int size );
void lfree( byte* area );

/* options.c */
void help( void );
int doGeneralOption( OptionId opid, strbyte** argv, ImageOptions* persist_ops,
                     ImageOptions* image_ops );
int doLocalOption( OptionId opid, strbyte** argv, bool setpersist,
                   ImageOptions* persist_ops, ImageOptions* image_ops );


/* rlelib.c */
void make_gamma( double gamma, int* gammamap );

/* reduce.c */
Image* reduce( Image* image, unsigned colors, int ditherf, float gamma,
               int verbose );
Image* expandtotrue( Image* image );
Image* expandbittoirgb( Image* image, unsigned int depth );
Image* expandirgbdepth( Image* image, unsigned int depth );

/* rotate.c */
Image* rotate( Image* iimage, int rotate, int verbose );
void mirror_horizontal( Image* image, int width, int height );
void mirror_vertical( Image* image, int width, int height );

/* send.c */
void sendXImage( XImageInfo* xii, int src_x, int src_y, int dst_x, int dst_y,
                 unsigned int w, unsigned int h );
XImageInfo* imageToXImage( Display* disp, int scrn, Visual* visual,
                           unsigned int ddepth, Image* image, unsigned int private_cmap,
                           unsigned int fit, ImageOptions* options );
Pixmap ximageToPixmap( Display* disp, Window parent, XImageInfo* xii );
void freeXImage( Image* image, XImageInfo* xii );

/* smooth.c */
Image* smooth( Image* isrc, int iterations, int verbose );

/* value.c */
void flipBits( byte* p, unsigned int len );

/* xpixmap.c */
int xpmoption( strbyte* s );

/* zio.c */
ZFILE* zopen( strbyte* name );
int zread( ZFILE* zf, byte* buf, int len );
int zeof( ZFILE* zf );
void zunread( ZFILE* zf, byte const* buf, int len );
strbyte* zgets( strbyte* buf, unsigned int size, ZFILE* zf );
bool zrewind( ZFILE* zf );
void zclose( ZFILE* zf );
void znocache( ZFILE* zf );
void zforcecache( bool );
void zreset( strbyte* filename );
void zclearerr( ZFILE* zf );
int _zgetc( ZFILE* zf );
bool _zopen( ZFILE* zf );
void _zreset( ZFILE* zf );
void _zclear( ZFILE* zf );
bool _zreopen( ZFILE* zf );

#define zgetc(zf) (((zf)->bufptr < (zf)->endptr) ? *(zf)->bufptr++ : _zgetc(zf))

/* zoom.c */
Image* zoom( Image* oimage, unsigned int xzoom, unsigned int yzoom,
             bool verbose, bool changetitle );

/* ddxli.c */
bool xliOpenDisplay( DisplayInfo* dinfo, strbyte* name );
void xliCloseDisplay( DisplayInfo* dinfo );
void xliDefaultDispinfo( DisplayInfo* dinfo );
int xliDefaultDepth( void );
void tellAboutDisplay( DisplayInfo* dinfo );
