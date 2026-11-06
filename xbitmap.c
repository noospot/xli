/* xbitmap.c:
 *
 * at one time this was XRdBitF.c.  it bears very little resemblence to it
 * now.  that was ugly code.  this is cleaner, faster, and more reliable
 * in most cases.
 *
 * jim frost 10.06.89
 *
 * Copyright, 1987, Massachusetts Institute of Technology
 *
 * Copyright 1989 Jim Frost.  See included file "copyright.h" for complete
 * copyright information.
 *
 *
 */



#include "xli.h"
#include "imagetypes.h"
#include <ctype.h>
#include <string.h>

#define MAX_SIZE 255

/* read a hex value and return its value
 */

static int nextInt( ZFILE* zf )
{
	int c;
	int value= 0;
	int shift= 0;

	for ( ;; ) {
		c= zgetc( zf );
		if ( c == EOF ) {
			return( -1 );
		} else {
			c= BEHexTable[c & 0xff];
			switch( c ) {
				case HEXSTART_BAD:	/* start */
					shift= 0; /* reset shift counter */
					break;
				case HEXDELIM_BAD:	/* delim */
				case HEXDELIM_IGNORE:
					if ( shift ) {
						return( value );
					}
					break;
				case HEXBAD:		/* bad */
					return( -1 );
				default:
					value += ( c << shift );
					shift += 4;
			}
		}
	}
}

/*
 * Return TRUE if the given file is an xbitmap file
 */

static bool isXbitmap( ZFILE* zf, unsigned int* w, unsigned int* h, int* v10p, strbyte** title )
{
	strbyte          line[MAX_SIZE];
	strbyte          name_and_type[MAX_SIZE];
	strbyte*         type;
	int           value;
	int           maxlines=8;	/* 8 lines to find #define etc. */

	*title = NULL;
	*w = 0;
	*h = 0;
	*v10p = 0;
	name_and_type[0] = '\0';

	/* get width/height values */

	while ( zgets( ( strbyte* )line, MAX_SIZE, zf ) && maxlines-- > 0 ) {

		if ( strlen( line ) >= MAX_SIZE - 1 ) {
			return FALSE;
		}

		/* find #define    width/height/hot_x/hot_y  */
		if ( strncmp( line, "#define", 7 ) == 0 ) {
			if ( sscanf( line, " #define %254s %d", name_and_type, &value ) == 2 ) {
				// search last '_'
				type = strrchr( name_and_type, '_' );
				if ( !type ) {
					type = name_and_type;
				} else {
					type = type+1;
				}

				if ( strcmp( "width", type ) == 0 ) {
					*w = ( unsigned int )value;
				} else if ( strcmp( "height", type ) == 0 ) {
					*h = ( unsigned int )value;
				}
			}
			goto next_line;
		}

		// find if  X10 or X11 data
		if ( ( sscanf( line, " static short %254s = {", name_and_type ) == 1 )
		||   ( sscanf( line, " static unsigned short %254s = {", name_and_type ) == 1 )
		) {
			*v10p = 1;
			break;
		}
		if ( ( sscanf( line, " static byte %254s = {", name_and_type ) == 1 )
		||   ( sscanf( line, " static BYTE %254s = {", name_and_type ) == 1 ) 
		||   ( sscanf( line, " static uint8_t %254s = {", name_and_type ) == 1 ) 
		||   ( sscanf( line, " static char %254s = {", name_and_type ) == 1 ) 
		||   ( sscanf( line, " static unsigned char %254s = {", name_and_type ) == 1 ) 
		) {
			*v10p = 0;
			break;
		}

		next_line:;
	}

	if ( !*w || !*h || *w>XLI_MAX_WIDTH || *h>XLI_MAX_HEIGHT ) {
		return FALSE;
	}

	// get title if any
	if ( name_and_type[0] != '\0' ) {
		type = strstr( name_and_type, "_bits" );
		if ( type != NULL ) {
			*type = '\0';
		}
		*title = dupString( name_and_type );
	}
	return TRUE;
}

Image* xbitmapLoad( strbyte* fullname, ImageOptions* image_ops, bool verbose )
{
	ZFILE*        zf;
	strbyte*         name = image_ops->name;
	Image*        image;
	strbyte*         title;
	int           value;
	int           v10p;
	unsigned int  linelen, dlinelen;
	unsigned int  x, y;
	unsigned int  w, h;
	byte*         dataptr;

	if ( ! ( zf= zopen( fullname ) ) ) {
		perror( "xbitmapLoad" );
		return( NULL );
	}

	/* See if it's an x bitmap */
	{
		unsigned int  tw, th;
		int           tv10p;
		if ( !isXbitmap( zf, &tw, &th, &tv10p, &title ) ) {
			zclose( zf );
			return NULL;
		}
		w = tw;
		h = th;
		v10p = tv10p;
	}

	image= newBitImage( w, h );

	if ( title != NULL ) {
		image->title= title;
	} else {
		image->title= dupString( name );
	}

	if ( verbose ) {
		printf( "%s is a %dx%d X%s bitmap file titled '%s'\n",
		        name, image->width, image->height, v10p ? "10" : "11", image->title );
	}

	/* read bitmap data
	 */

	initBEHexTable();

	znocache( zf );
	linelen= ( w / 8 ) + ( w % 8 ? 1 : 0 ); /* internal line length */
	if ( v10p ) {
		dlinelen= ( w / 8 ) + ( w % 16 ? 2 : 0 );
		dataptr= image->data;
		for ( y= 0; y < h; y++ ) {
			for ( x= 0; x < dlinelen; x++ ) {
				if ( ( value= nextInt( zf ) ) < 0 ) {
					fprintf( stderr,"xbitmapLoad: %s - short read on X bitmap file\n", name );
					zclose( zf );
					return( image );
				}
				*( dataptr++ )= value >> 8;
				if ( ++x < linelen ) {
					*( dataptr++ )= value & 0xff;
				}
			}
		}
	} else {
		dataptr= image->data;
		for ( y= 0; y < h; y++ )
			for ( x= 0; x < linelen; x++ ) {
				if ( ( value= nextInt( zf ) ) < 0 ) {
					fprintf( stderr,"xbitmapLoad: %s - short read on X bitmap file\n", name );
					zclose( zf );
					return( image );
				}
				*( dataptr++ )= value;
			}
	}


	zclose( zf );
	return( image );
}

int xbitmapIdent( strbyte* fullname, strbyte* name )
{
	ZFILE*        zf;
	strbyte*         title;
	unsigned int  w, h;
	int           v10p;

	if ( ! ( zf= zopen( fullname ) ) ) {
		perror( "xbitmapIdent" );
		return( 0 );
	}

	/* See if it's an x bitmap */
	if ( !isXbitmap( zf, &w, &h, &v10p, &title ) ) {
		zclose( zf );
		return 0;
	}

	if ( title == NULL ) {
		title= dupString( name );
	}

	printf( "%s is a %dx%d X%s bitmap file titled '%s'\n",
	        name, w, h, v10p ? "10" : "11",title );

	lfree( ( byte* )title );
	zclose( zf );
	return 1;
}
