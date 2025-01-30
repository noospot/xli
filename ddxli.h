/*
 * General Device dependent code for xli.
 *
 * Author; Graeme W. Gill
 */

/* OS dependent stuff */
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

#if defined(SYSV) || defined(VMS)
	#include <string.h>
	#ifndef index			/* some SysV's do this */
		#define index strchr
	#endif
	#ifndef rindex
		#define rindex strrchr
	#endif
#else				/* !SYSV && !VMS */
	#include <strings.h>
#endif				/* !SYSV && !VMS */

#ifdef VMS
	#define R_OK 4
	#define NO_UNCOMPRESS
#endif

/* xli specific data types */

typedef unsigned long Pixel;	        /* what X thinks a pixel is */
typedef unsigned short Intensity;	/* what X thinks an RGB intensity is */

/* Display device dependent Information structure */
typedef struct {
	int width;
	int height;
	Display* disp;
	int scrn;
	Colormap colormap;
} DisplayInfo;

/* This struct holds the X-client side bits for a rendered image. */

typedef struct {
	Display* disp;		/* destination display */
	int scrn;		/* destination screen */
	int depth;		/* depth of drawable we want/have */
	Drawable drawable;	/* drawable to send image to */
	Pixel* index;		/* array of pixel values allocated */
	int no;			/* number of pixels in the array */
	Pixel foreground;	/* foreground and background pixels for mono images */
	Pixel background;
	Colormap cmap;		/* colormap used for image */
	GC gc;			/* cached gc for sending image */
	XImage* ximage;		/* ximage structure */
	XShmSegmentInfo shm;	/* valid if shm.shmid >= 0 */
	bool rootimage;	        /* True if is a root image - eg, retain colors */
	byte reserved0;         // Padding
	byte reserved1;         // Padding
	byte reserved2;         // Padding
} XImageInfo;

/* ddxli.c */
strbyte* xliDisplayName( strbyte* name );
int xliParseXColor( DisplayInfo* dinfo, strbyte* spec, XColor* xlicolor );
int xliDefaultVisual( void );
void xliGammaCorrectXColor( XColor* xlicolor, double gamma );
