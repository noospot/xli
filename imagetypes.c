/* imagetypes.c:
 *
 * this contains the ImageTypes array
 *
 * jim frost 09.27.89
 *
 * Copyright 1989, 1991 Jim Frost.
 * See included file "copyright.h" for complete copyright information.
 */


#include "xli.h"
#include "imagetypes.h"
#include <errno.h>
#include <string.h>



/* some of these are order-dependent */

static struct {
	int ( *identifier ) ( strbyte*, strbyte* );
	Image* ( *loader ) ( strbyte*, ImageOptions*, bool );
	strbyte* name;
} ImageTypes[] = {
#ifdef HAS_PNG
	{pngIdent,	pngLoad,	"Portable Network Graphics (PNG)"},
#endif
	{gifIdent,	gifLoad,	"GIF Image"},
#ifdef HAS_JPEG
	{jpegIdent,	jpegLoad,	"JFIF style jpeg Image"},
#endif
#ifdef HAS_WEBP
	{webpIdent,	webpLoad,	"WEBP Image"},
#endif
	{tgaIdent,	tgaLoad,	"Targa Image (TGA)"},
	{bmpIdent,	bmpLoad,	"Windows, OS/2 RLE Image (early BMP)"},
	{pbmIdent,	pbmLoad,	"Portable Bit Map (PNM, PBM, PGM, PPM)"},
	{pcxIdent,	pcxLoad,	"PC Paintbrush Image (PCX)"},
	{xpixmapIdent,	xpixmapLoad,	"X Pixmap XPM"},
	{xbitmapIdent,	xbitmapLoad,	"X Bitmap XBM"},
	{xwdIdent,	xwdLoad,	"X Window Dump XWD"},
#ifdef HAS_TIFF
	{tiffIdent,	tiffLoad,	"Tag Image File Format (TIFF)"},
#endif
	{sunRasterIdent, sunRasterLoad,	"Sun Rasterfile RAS"},
	{cmuwmIdent,	cmuwmLoad,	"CMU WM Raster"},
	{facesIdent,	facesLoad,	"Facesaver Project (FS)"},
	{g3Ident,	g3Load,		"G3 FAX Image"},
	{fbmIdent,	fbmLoad, 	"FBM Image"},
	{rleIdent,	rleLoad,	"Utah RLE Image (RLE)"},
	{imgIdent,	imgLoad,	"GEM Image (IMG)"},
	{macIdent,	macLoad,	"MacPaint Image"},
	{NULL,		NULL,		NULL}
};


/* load a named image */
Image* loadImage( ImageOptions* image_ops, bool verbose )
{
	strbyte fullname[BUFSIZ];
	Image* image;
	int a;

	if ( findImage( image_ops->name, fullname ) < 0 ) {
		if ( errno == ENOENT ) {
			printf( "%s: image not found\n", image_ops->name );
		} else if ( errno == EISDIR ) {
			printf( "%s: directory\n", image_ops->name );
		} else {
			perror( fullname );
		}
		return ( NULL );
	}

	image_ops->fullname = ( strbyte* )lmalloc( strlen( fullname ) + 1 );
	strcpy( image_ops->fullname, fullname );

	/* We've done this before !! */
	if ( image_ops->loader_idx != -1 ) {
		image = ImageTypes[image_ops->loader_idx].loader( fullname,
		            image_ops, verbose );
		if ( image ) {
			zreset( NULL );
			return ( image );
		}
	} else {
		for ( a = 0; ImageTypes[a].loader; a++ ) {
			image = ImageTypes[a].loader( fullname, image_ops,
			                              verbose );
			if ( image ) {
				zreset( NULL );
				return ( image );
			}
		}
	}
	printf( "%s: unknown or unsupported image type\n", fullname );
	zreset( NULL );
	lfree( ( byte* )image_ops->fullname );
	return ( NULL );
}

/* identify what kind of image a named image is */
void identifyImage( strbyte* name )
{
	strbyte fullname[BUFSIZ];
	int a;

	if ( findImage( name, fullname ) < 0 ) {
		if ( errno == ENOENT ) {
			printf( "%s: image not found\n", name );
		} else if ( errno == EISDIR ) {
			printf( "%s: directory\n", name );
		} else {
			perror( fullname );
		}
		return;
	}
	for ( a = 0; ImageTypes[a].identifier; a++ ) {
		if ( ImageTypes[a].identifier( fullname, name ) ) {
			zreset( NULL );
			return;
		}
	}
	zreset( NULL );
	printf( "%s: unknown or unsupported image type\n", fullname );
}

/* tell user what image types we support */
void supportedImageTypes( void )
{
	int a;

	printf( "Image types supported:\n" );
	for ( a = 0; ImageTypes[a].name; a++ ) {
		printf( "  %s\n", ImageTypes[a].name );
	}
}
