/*
 * mac.c:
 *
 * adapted from code by Patrick Naughton (naughton@sun.soe.clarkson.edu)
 *
 * macin.c
 * Mark Majhor
 * August 1990
 *
 * routines for reading MAC files
 *
 * Copyright 1990 Mark Majhor (see the included file
 * "mrmcpyrght.h" for complete copyright information)
 */

/* Edit History

04/15/91   2 nazgul	Check for end of file instead of using -1 as a runlength!

*/
# include "xli.h"
#include "imagetypes.h"
# include <ctype.h>
# include "mac.h"

/****
 **
 ** local variables
 **
 ****/

static byte file_open = 0;	/* status flags */
static byte image_open = 0;

static ZFILE* ins;		/* input stream */

/****
 **
 ** global variables
 **
 ****/

static int  macin_img_width;           /* image width */
static int  macin_img_height;          /* image height */
static int  macin_img_depth;	       /* image depth */
static int  macin_img_planes;	       /* image planes */
static int  macin_img_BPL;	       /* image bytes per line */

/*
 * open MAC image in the input stream; returns MACIN_SUCCESS if
 * successful. (might also return various MACIN_ERR codes.)
 */
/* ARGSUSED */
static int macin_open_image( ZFILE* s )
{
	byte mhdr[MAC_HDR_LEN+20];
	byte* hp;		/* header pointer */

	/* make sure there isn't already a file open */
	if ( file_open ) {
		return( MACIN_ERR_FAO );
	}

	/* remember that we've got this file open */
	file_open = 1;
	ins = s;

	/*
	 * the mac paint files that came with xmac had an extra
	 * 128 byte header on the front, with a image name in it.
	 * true mac paint images don't seem to have this extra
	 * header.  The following code tries to figure out what
	 * type the image is and read the right amount of file
	 * header (512 or 640 bytes).
	 */
	/* read in the mac file header */
	hp = ( byte* ) mhdr;
	if ( zread( ins, hp, ADD_HDR_LEN ) != ADD_HDR_LEN ) {
		return MACIN_ERR_EOF;
	}

	if ( mhdr[0] != MAC_MAGIC ) {
		return MACIN_ERR_BAD_SD;
	}

	//  PNTG
	if ( mhdr[65] == 0x50
	      && mhdr[66] == 0x4E
	      && mhdr[67] == 0x54
	      && mhdr[68] == 0x47
	   ) {
		// file is macpaint with macbinhex header

		if ( mhdr[74] != 0 ) {
			return MACIN_ERR_BAD_SD;      // 0
		}

		// if (mhdr[82] != 0) return MACIN_ERR_BAD_SD; // 0

		if ( mhdr[87] != 0 ) {
			return MACIN_ERR_BAD_SD;      // 0
		}

		if ( zread( ins, hp, MAC_HDR_LEN ) != MAC_HDR_LEN ) {
			return MACIN_ERR_EOF;
		}
	}  else {

		/* has no macbin2header, so get remainder of regular header */
		if ( zread( ins, hp+ADD_HDR_LEN, MAC_HDR_LEN - ADD_HDR_LEN ) != MAC_HDR_LEN - ADD_HDR_LEN ) {
			return MACIN_ERR_EOF;
		}

		if ( mhdr[0] != MAC_MAGIC )    {
			return MACIN_ERR_BAD_SD;      // 0
		}
		if ( mhdr[1] != MAC_MAGIC )    {
			return MACIN_ERR_BAD_SD;      // 0
		}
		if ( mhdr[2] != MAC_MAGIC )    {
			return MACIN_ERR_BAD_SD;      // 0
		}
		// if (mhdr[3] != MAC_MAGIC)    return MACIN_ERR_BAD_SD;  // 0 or 2

		byte i=0;
		while ( i<204 ) {
			if ( mhdr[308+i] != 0 ) {
				return MACIN_ERR_BAD_SD;
			}
			i=i+1;
		}

		// some mac pict image seems as a macpaint.
		// try finding if it's a  mac pict with 20 additionnal bytes.
		if ( zread( ins, hp+MAC_HDR_LEN, 20 ) != 20 ) {
			return MACIN_ERR_EOF;
		}

		if ( mhdr[522] == 0x11 )
			if ( mhdr[523] == 0x01 )  {
				return MACIN_ERR_BAD_SD;
			} // file Mac Pict v1.0
		if ( mhdr[522] == 0x00 )
			if ( mhdr[523] == 0x11 )
				if ( mhdr[524] == 0x02 )
					if ( mhdr[525] == 0xFF ) {
						return MACIN_ERR_BAD_SD;
					} // file Mac Pict v2.0

		zunread( ins, hp+MAC_HDR_LEN, 20 ) ;
	}

	/* Now set relevant values */
	macin_img_width  = BYTES_LINE * 8;
	macin_img_height = MAX_LINES;
	macin_img_depth  = 1;		/* always monochrome */
	macin_img_planes = 1;		/* always 1 */
	macin_img_BPL    = BYTES_LINE;

	return MACIN_SUCCESS;
}

/*
 * close an open MAC file
 */

static int macin_close_file( void )
{
	/* make sure there's a file open */
	if ( !file_open ) {
		return MACIN_ERR_NFO;
	}

	/* mark file (and image) as closed */
	file_open  = 0;
	image_open = 0;

	/* done! */
	return MACIN_SUCCESS;
}

/*
 * these are the routines added for interfacing to xli
 */

/*
 * tell someone what the image we're loading is.  this could be a little more
 * descriptive
 */

static void tellAboutImage( strbyte* name )
{
	printf( "%s is a %dx%d MacPaint image\n",
	        name, macin_img_width, macin_img_height );
}

Image* macLoad( strbyte* fullname, ImageOptions* image_ops, bool verbose )
{
	ZFILE*        zf;
	strbyte*         name = image_ops->name;
	Image* image;
	byte* pixptr, ch;
	int	eof;
	register int scanLine;
	register unsigned int i, j, k;

	if ( ! ( zf = zopen( fullname ) ) ) {
		return( NULL );
	}
	if ( macin_open_image( zf ) != MACIN_SUCCESS ) { /* read image header */
		macin_close_file();
		zclose( zf );
		return( NULL );
	}

	image = newBitImage( macin_img_width, macin_img_height );
	image->title = dupString( name );

	if ( verbose ) {
		tellAboutImage( name );
	}

	pixptr = &( image->data[0] );
	scanLine = 0;
	k = 0;

	while ( scanLine < macin_img_height ) {
		if ( ( eof = zgetc( zf ) ) == -1 ) {
			break;
		}
		ch = ( byte ) eof;	/* Count byte */
		i = ( unsigned int ) ch;
		if ( ch < 0x80 ) {	/* Unpack next (I+1) bytes as is */
			for ( j = 0; j <= i; j++ ) {
				if ( scanLine < macin_img_height ) {
					if ( ( eof = zgetc( zf ) ) == -1 ) {
						break;
					}
					*pixptr++ = ( byte ) eof;
					k++;
					if ( !( k %= BYTES_LINE ) ) {
						scanLine++;
					}
				}
			}
		} else {	/* Repeat next byte (2's comp I) times */
			if ( ( eof = zgetc( zf ) ) == -1 ) {
				break;
			}
			ch = ( byte ) eof;
			for ( j = 0; j <= 256 - i; j++ ) {
				if ( scanLine < macin_img_height ) {
					*pixptr++ = ( byte ) ch;
					k++;
					if ( !( k %= BYTES_LINE ) ) {
						scanLine++;
					}
				}
			}
		}
	}

	if ( scanLine < macin_img_height ) {
		printf( "macLoad: Short read within image data, '%s'\n", fullname );
		macin_close_file();
		zclose( zf );
		return ( image );
	}

	macin_close_file();


	zclose( zf );

	return image;
}

int macIdent( strbyte* fullname, strbyte* name )
{
	ZFILE*        zf;
	unsigned int  ret;

	if ( ! ( zf = zopen( fullname ) ) ) {
		return( 0 );
	}
	if ( macin_open_image( zf ) == MACIN_SUCCESS ) {
		tellAboutImage( name );
		ret = 1;
	} else {
		ret = 0;
	}
	macin_close_file();
	zclose( zf );
	return( ret );
}


