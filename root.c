/* root.c:
 *
 * loads an image onto the root window.  changes to allow proper
 * freeing of previously allocated resources made by Deron Dann Johnson
 * (dj@eng.sun.com).
 *
 * jim frost 10.03.89
 *
 * Copyright 1989, 1990, 1991 Jim Frost.
 * See included file "copyright.h" for complete copyright information.
 */


#include "xli.h"
#include <string.h>

#define RETAIN_PROP_NAME	"_XSETROOT_ID"

/* Sets the close-down mode of the client to 'RetainPermanent'
 * so all client resources will be preserved after the client
 * exits.  Puts a property on the default root window containing
 * an XID of the client so that the resources can later be killed.
 */

static void preserveResource( Display* dpy, Window w )
{
	/* create dummy resource */
	Pixmap pm = XCreatePixmap( dpy, w, 1, 1, 1 );
	byte* data = ( byte* ) &pm;

	/* intern the property name */
	Atom atom = XInternAtom( dpy, RETAIN_PROP_NAME, 0 );

	/* create or replace the property */
	XChangeProperty( dpy, w, atom, XA_PIXMAP, 32, PropModeReplace,
	                 data, sizeof( Pixmap )/4 );

	/* retain all client resources until explicitly killed */
	XSetCloseDownMode( dpy, RetainPermanent );
}


/* Flushes any resources previously retained by the client,
 * if any exist.
 */

static void freePrevious( Display* dpy, Window w )
{
	Pixmap* pm;
	byte* prop_return;
	Atom actual_type;
	int format;
	unsigned long nitems;
	unsigned long bytes_after;
	int returncode;

	/* intern the property name */
	Atom atom = XInternAtom( dpy, RETAIN_PROP_NAME, 0 );

	/* look for existing resource allocation */
	nitems = sizeof( Pixmap )/4;
	returncode = XGetWindowProperty( dpy, w, atom,
	                                 0, nitems, 1/*delete*/,
	                                 XA_PIXMAP, &actual_type,
	                                 &format, &nitems,
	                                 &bytes_after, &prop_return );
	if ( returncode != Success ) {
		if ( globals.verbose )
			fprintf( stderr, "failed to look for %s with return code %i.\n",
			         RETAIN_PROP_NAME, returncode );
		return;
	}

	/* Check if the property was found */
	if ( actual_type == None ) {
		if ( globals.verbose ) {
			fprintf( stderr, "didn't find evidence of prior run.\n" );
		}
		return;
	}

	/* Make sure the dummy value is still present */
	if ( actual_type != XA_PIXMAP ) {
		if ( globals.verbose ) {
			fprintf( stderr, "found wrong data type - skipped.\n" );
		}
		return;
	}

	/* Check size, in case we're a different architecture */
	if ( ( nitems != sizeof( Pixmap )/4 ) ||
	      ( format != 32 ) ||
	      ( bytes_after != 0 ) ) {
		if ( globals.verbose )
			fprintf( stderr, "saw wrong %li / word size %i / architecture %li.\n",
			         bytes_after, format, nitems );
		return;
	}

	/* blast it away */
	pm = ( Pixmap* ) prop_return;
	XKillClient( dpy, ( XID ) *pm );
	XFree( prop_return );
}

/* this returns the root window for DECWindows servers.  this is right
 * out of xsetroot with minor formatting changes.
 * (Later DEC OSF/1 releases don't need this stuff)
 */

static Window getWmRootWindow( Display* dpy, Window root )
{
	Window parent, retv;
	Window* nthwindows;
	unsigned int nwindows;
	XWindowAttributes rootatt, nthwindowsatt;

	retv = None;
	if ( !XGetWindowAttributes( dpy, root, &rootatt ) ) {
		fprintf( stderr, "XGetWindowAttributes on root failed.\n" );
		exit( 1 );
	}
	if ( XQueryTree( dpy, root, &root, &parent, &nthwindows, &nwindows ) ) {
		unsigned int i;
		for ( i = 0; i < nwindows; i++ ) {
			if ( !XGetWindowAttributes( dpy, nthwindows[i], &nthwindowsatt ) ) {
				XFree(  nthwindows );
				fprintf( stderr, "XGetWindowAttributes on nthwindows failed.\n" );
				exit( 1 );
			}
			if ( ( rootatt.width == nthwindowsatt.width ) &&
			      ( rootatt.height == nthwindowsatt.height ) ) {
				retv = nthwindows[i];
			}
		}
		XFree( nthwindows );
		return retv;
	} else {
		fprintf( stderr, "XQueryTree failed (window doesn't exist).\n" );
		exit( 1 );
	}
}

static Window getDECRootWindow( Display* dpy, Window root )
{
	Window temporary_rootW;

	temporary_rootW = getWmRootWindow( dpy, root );
	return ( getWmRootWindow( dpy, temporary_rootW ) );
}

void imageOnRoot( DisplayInfo* dinfo, Image* image, ImageOptions* options )
{
	Display* disp = dinfo->disp;
	int scrn = dinfo->scrn;
	Pixmap pixmap;
	XImageInfo* ximageinfo;
	Atom __SWM_VROOT = None;
	Window root, rootReturn, parentReturn, *nthwindowsren;
	unsigned int numnthwindowsren;
	unsigned int i;
	strbyte* s;

	if ( globals.dest_window ) {
		root = globals.dest_window;
	} else {
		root = RootWindow( disp, scrn );

		/* look for DECWindows servers because they do strange stuff with the
		 * root window.
		 * (Later DEC OSF/1 releases don't need this stuff)
		 */

		for ( s = ServerVendor( disp ); *s; s++ )
			if ( !strncmp( s, "DECWINDOWS", 10 ) &&
			      !strstr( s, "OSF/1" ) ) {
				root = getDECRootWindow( disp, root );
			}

		/* Added for window managers like swm and tvtwm that follow
		 * solbourne's virtual root window concept.
		 */

		__SWM_VROOT = XInternAtom( disp, "__SWM_VROOT", FALSE );
		XQueryTree( disp, root, &rootReturn, &parentReturn, &nthwindowsren,
		            &numnthwindowsren );
		for ( i = 0; i < numnthwindowsren; i++ ) {
			Atom actual_type;
			int actual_format;
			unsigned long nitems, bytesafter;
			byte* newRoot = 0;

			if ( ( XGetWindowProperty ( disp, nthwindowsren[i], __SWM_VROOT,0,1,
			                            False, XA_WINDOW,
			                            &actual_type, &actual_format,
			                            &nitems, &bytesafter, &newRoot )
			       == Success ) &&
			      newRoot ) {
				root = *( ( Window* ) newRoot );
				break;
			}
		}
		XFree( nthwindowsren );
	}
	freePrevious( disp, root );

	if ( !( ximageinfo = imageToXImage( disp, scrn,
	                                    DefaultVisual( disp, scrn ),
	                                    DefaultDepth( disp, scrn ),
	                                    image, FALSE, TRUE, options ) ) ) {
		fprintf( stderr, "Cannot convert Image to XImage\n" );
		exit( 1 );
	}
	if ( ( pixmap = ximageToPixmap( disp, root, ximageinfo ) ) == None ) {
		printf( "Cannot create background (not enough resources, sorry)\n" );
		exit( 1 );
	}

	/* changing the root colormap is A Bad Thing, so deny it. */
	if ( ximageinfo->cmap != DefaultColormap( disp, scrn ) ) {
		printf( "Loading image onto root would change default colormap (sorry)\n" );
		XFreePixmap( disp, pixmap );
		exit( 1 );
	}
	XSetWindowBackgroundPixmap( disp, root, pixmap );
	XClearWindow( disp, root );
	XFreePixmap( disp, pixmap );
	ximageinfo->rootimage = TRUE;	/* make sure colors arn't freed */
	freeXImage( image, ximageinfo );
	preserveResource( disp, root );
}
