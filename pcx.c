/*
** pcx.c - load a ZSoft PC Paintbrush (PCX) file for use inside xli
**
**	Tim Northrup <tim@BRS.Com>
**	Adapted from code by Jef Poskanzer (see Copyright below).
**
**	Version 0.1 --  4/25/91 -- Initial cut
**	Version 0.2 -- 2001-12-15 -- Support 8bit color images with palette.
**					(Alexandre Duret-Lutz <duret_g@epita.fr>)
**
**  Copyright (c) 1991 Tim Northrup
**	(see file "tgncpyrght.h" for complete copyright information)
*/

/*
** Copyright (C) 1988 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
**
** This program (pcxtopbm) is based on the pcx2rf program by:
**   Mike Macgirvin
**   Stanford Relativity Gyro Program GP-B
**   Stanford, Calif. 94503
**   ARPA: mike@relgyro.stanford.edu
*/


#include "xli.h"
#include "imagetypes.h"

#define PCX_MAGIC 0x0a			/* first byte in a PCX image file */

// d: identify file 
// Returns 1 if file is a PCX file, 0 otherwise
int pcxIdent( strbyte* fullname, strbyte* name )
{
	ZFILE* zf;
	int  ret;
	int xmin;
	int xmax;
	int ymin;
	int ymax;
	byte pcxhd[128];

	ret = 0;

	if ( ! ( zf = zopen( fullname ) ) ) {
		perror( "pcxIdent" );
		return( 0 );
	}

	/* Read .pcx header. */
	if ( zread( zf,pcxhd,128 ) == 128 ) {

		// pcx magic MUST be 10  0x0a
		if ( pcxhd[0] != PCX_MAGIC ) {
			goto ident_ret;
		}

		// version
		if ( pcxhd[1]>5 || pcxhd[1]==1  ) {
			goto ident_ret;
		}

		// encoding
		const strbyte* encoding;
		if ( pcxhd[2] == 1 ) {
			encoding = "RLE, ";
		} else {
			encoding = "";
		}
		if ( pcxhd[2] > 1 ) {
			goto ident_ret;
		}

		// bpp of each plane 1, 2, 4, or 8
		if ( pcxhd[3] > 8 ) {
			goto ident_ret;
		}

		// reserved MUST be 0
		if ( pcxhd[64]!=0  ) {
			//goto ident_ret; // many images with noise
		}

		// NPlane  1, 3 or 4
		if ( pcxhd[65] == 0 || pcxhd[65]>4 ) {
			goto ident_ret;
		}
		
		// result bpp, max 255
		int tmp = pcxhd[3]*pcxhd[65] ;
		if ( tmp>0xFF ) {
			goto ident_ret;
		}
		byte result_bpp = tmp;
		
	
		// colormode
		const strbyte* colormode;
		if ( pcxhd[68]<1 || pcxhd[68]>2 ) {
			colormode="";
			// goto ident_ret; // many images  0
		}
		if ( pcxhd[68] == 1 ) {
			colormode="Color/BW ";
		}
		if ( pcxhd[68] == 2 ) {
			colormode="Grayscale ";
		}

		// blank
		byte i=0;
		while ( i<54 ) {
			if ( pcxhd[74+i]!=0 ) {
				// goto ident_ret; // many images with noise
			}
			i=i+1;
		}

		const strbyte* version = "";
		if ( pcxhd[1] == 0 ) {
			version = "v2.5";
		} else if ( pcxhd[1] == 2 ) {
			version = "v2.8, with palette";
		} else if ( pcxhd[1] == 3 ) {
			version = "v2.8";
		} else if ( pcxhd[1] == 4 ) {
			version = "Windows version";
		} else if ( pcxhd[1] == 5 ) {
			version = "v3.x";
		}

		// Calculate raster header and swap bytes.
		xmin = pcxhd[4] + ( 256 * pcxhd[5] );
		ymin = pcxhd[6] + ( 256 * pcxhd[7] );
		xmax = pcxhd[8] + ( 256 * pcxhd[9] );
		ymax = pcxhd[10] + ( 256 * pcxhd[11] );
		xmax = xmax - xmin + 1;
		ymax = ymax - ymin + 1;

		printf( "%s is a %dx%d PCX Paintbrush %simage, %s, %s%i planes, depth %i\n", 
		         name,xmax,ymax, colormode, version, encoding, pcxhd[65], result_bpp );
		ret = 1;
	}

ident_ret:
	;
	zclose( zf );
	return( ret );
}


// d: load file trailer 256-color VGA palette
static bool pcx_load_trailer_palette( ZFILE* zf, Image* image )
{
	int b = zgetc( zf );
	// byte 12 0x0C 
	if ( b == 12 ) {
		int cnt = 0;
		while ( cnt < 256 ) {
			int r, g, bval;
			r = zgetc( zf );
			g = zgetc( zf );
			bval = zgetc( zf );
			if ( r == EOF || g == EOF || bval == EOF ) {
				image->rgb.used = cnt;
				return ( cnt > 0 );
			}
			image->rgb.red[cnt] = r << 8;
			image->rgb.green[cnt] = g << 8;
			image->rgb.blue[cnt] = bval << 8;
			cnt = cnt + 1;
		}
		image->rgb.used = 256;
	} else if ( b == EOF ) {
		return FALSE;
	}
	return TRUE;
}


// d: load file header 16-color EGA/CGA palette
static void pcx_load_header_palette( byte* pcxhd , Image* image )
{
	/* Load 16-color palette from header bytes 16 to 47 */
	int palette_idx = 0;
	while ( palette_idx < 16 ) {
		int header_offset = 16 + ( palette_idx * 3 );
		image->rgb.red[palette_idx] = pcxhd[header_offset] << 8;
		image->rgb.green[palette_idx] = pcxhd[header_offset + 1] << 8;
		image->rgb.blue[palette_idx] = pcxhd[header_offset + 2] << 8;
		palette_idx = palette_idx + 1;
	}
	image->rgb.used = 16;
}


// d: load image data
// Returns FALSE if there was a short read.
static bool PCX_LoadImage ( ZFILE* zf, int in_bpr, int img_bpr, Image* image, int rows, bool readpal )
{
	/* Goes like this: Read a byte.  If the two high bits are set,
	** then the low 6 bits contain a repeat count, and the byte to
	** repeat is the next byte in the file.  If the two high bits are
	** not set, then it is a byte to write.
	*/

	register byte* ptr;
	int row = 0;
	int row_bytes = 0;
	int b, i, cnt;
	/* if a binary image set mask  */
	int xor_mask = ( image->type == IBITMAP ) ? 0xff : 0;
	
	// set a default grayscale palette so if...
	if ( readpal ) {
		cnt = 0;
		while ( cnt < 256 ) {
			image->rgb.red[cnt] = cnt << 8;
			image->rgb.green[cnt] = cnt << 8;
			image->rgb.blue[cnt] = cnt << 8;
			cnt = cnt + 1;
		}
		image->rgb.used = 256;
	}
	
	
	ptr = &( image->data[0] );

	while ( row < rows && ( b = zgetc( zf ) ) != EOF ) {

		if ( row >= rows ) {
			break;
		}

		if ( ( b & 0xC0 ) == 0xC0 ) {
			/* have a repetition count -- mask flag bits */
			cnt = b & 0x3F;
			b = zgetc( zf );
			if ( b == EOF ) {
				return FALSE;
			}
		} else {
			cnt = 1;		/* no repeating this one */
		}

		// operator  '^' == XOR
		b = b ^ xor_mask;

		i = 0;
		while ( i < cnt ) {
			if ( row_bytes < img_bpr ) {
				*ptr = ( byte ) b;
				ptr = ptr + 1;
			}
			row_bytes = row_bytes + 1;
			if ( row_bytes == in_bpr ) {
				/* start of a new line */
				row = row + 1;
				row_bytes = 0;
			}
			i = i + 1;
		}
	}

	if ( readpal ) {
		if ( !pcx_load_trailer_palette( zf, image ) ) {
			return FALSE;
		}
	}

	return TRUE;
}


// d: Load PCX Paintbrush file into an Image structure.
// Returns pointer to allocated struct if successful, NULL otherwise
Image* pcxLoad( strbyte* fullname, ImageOptions* image_ops, bool verbose )
{
	ZFILE* zf;
	strbyte* name = image_ops->name;
	byte pcxhd[128];
	int xmin;
	int xmax;
	int ymin;
	int ymax;
	int in_bpr;	/* in input format bytes per row */
	int img_bpr;	/* xli image bytes per row */
	register Image* image;

	/* Open file. */
	if ( ! ( zf = zopen( fullname ) ) ) {
		perror( "pcxLoad" );
		return( ( Image* )NULL );
	}

	/* Read .pcx header. */
	if ( zread( zf,pcxhd,128 ) != 128 ) {
		zclose( zf );
		return( ( Image* )NULL );
	}

	if ( ( pcxhd[0] != PCX_MAGIC ) || ( pcxhd[1] > 5 ) ) {
		zclose( zf );
		return( ( Image* )NULL );
	}

	/* Calculate raster header and swap bytes. */
	xmin = pcxhd[4] + ( 256 * pcxhd[5] );
	ymin = pcxhd[6] + ( 256 * pcxhd[7] );
	xmax = pcxhd[8] + ( 256 * pcxhd[9] );
	ymax = pcxhd[10] + ( 256 * pcxhd[11] );
	xmax = xmax - xmin + 1;
	ymax = ymax - ymin + 1;
	in_bpr = pcxhd[66] + ( 256 * pcxhd[67] );

	/*  number of bytes per lines.  */
	img_bpr = ( xmax * pcxhd[3] + 7 )/8;

	/* double check image */
	if ( xmax < 0 || ymax < 0 || in_bpr < img_bpr ) {
		zclose( zf );
		return( ( Image* )NULL );
	}

	if ( verbose ) {
		printf( "%s is a %dx%d PC Paintbrush image\n",name,xmax,ymax );
	}

	/* Only monochrome or 8bits paletized images are supported.  */
	if ( pcxhd[65] > 1 || !( pcxhd[3] == 1 || pcxhd[3] == 8 ) ) {
		fprintf( stderr, "pcxLoad: %s - Unsupported PCX type\n", name );
		zclose( zf );
		return( ( Image* )NULL );
	}

	znocache( zf );

	/* Allocate image. */
	if ( pcxhd[3] == 1 ) {
		image = newBitImage( xmax, ymax );
	} else {
		image = newRGBImage( xmax, ymax, pcxhd[3] );
		pcx_load_header_palette(  pcxhd, image );
	}
	image->title = dupString( name );
	
	
	/* Read compressed bitmap. */
	if ( !PCX_LoadImage( zf, in_bpr, img_bpr, image, ymax, (pcxhd[3] == 8) ) ) {
		fprintf( stderr, "pcxLoad: %s - Short read of PCX file\n", name );
		zclose( zf );
		return( image );
	}


	zclose( zf );
	return( image );
}

