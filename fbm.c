/*
 * fbm.c:
 *
 * adapted from code by Michael Mauldin, (mlm) at Carnegie-Mellon
 * University, (fbm tools) and Kirk L. Johnson, (tuna@athena.mit.edu),
 * (gif.c).
 *
 * fbmin.c
 * Mark Majhor
 * August 1990
 *
 * routines for reading FBM files
 *
 * Copyright 1990 Mark Majhor (see the included file
 * "mrmcpyrght.h" for complete copyright information)
 */
# include "xli.h"
# include <ctype.h>
# include <string.h>
#define FBM_C
# include "fbm.h"
# include "imagetypes.h"

/****
 **
 ** local variables
 **
 ****/

static byte file_open = 0;	/* status flags */
static byte image_open = 0;

static ZFILE* ins;		/* input stream */
static FBMFILEHDR phdr;		/* header structure */

/****
 **
 ** global variables
 **
 ****/

static int  fbmin_img_width;           /* image width */
static int  fbmin_img_height;          /* image height */
static int  fbmin_img_depth;	       /* Depth (1 for B+W, 3 for RGB) */
static int  fbmin_img_bits;	       /* Bits per pixel */
static int  fbmin_img_rowlen;	       /* length of one row of data */
static int  fbmin_img_plnlen;	       /* length of one plane of data */
static int  fbmin_img_clrlen;	       /* length of the colormap */
static double  fbmin_img_aspect;       /* image aspect ratio */
static int  fbmin_img_physbits;	       /* physical bits per pixel */
static strbyte* fbmin_img_title;	       /* name of image */
static strbyte* fbmin_img_credit;	       /* credit for image */

static int fbmin_image_test( void );

static strbyte* get_err_string( int errcode )
{
	int i = 0;
	while ( fbm_err_strings[i].err_no != 0 ) {
		if ( fbm_err_strings[i].err_no == errcode ) {
			return fbm_err_strings[i].name;
		}
		i = i + 1;
	}
	return "";
}

/*
 * open FBM image in the input stream; returns FBMIN_SUCCESS if
 * successful. (might also return various FBMIN_ERR codes.)
 */
static int fbmin_open_image( ZFILE* s )
{
	byte* hp;		/* header pointer */

	/* make sure there isn't already a file open */
	if ( file_open ) {
		return( FBMIN_ERR_FAO );
	}

	/* remember that we've got this file open */
	file_open = 1;
	ins = s;

	/* read in the fbm file header */
	hp = ( byte* ) &phdr;
	if ( zread( ins, hp, sizeof( phdr ) ) != sizeof( phdr ) ) {
		return FBMIN_ERR_BAD_SIG;      /* can't be FBM file */
	}

	if ( strncmp( FBM_MAGIC, phdr.magic, sizeof( FBM_MAGIC ) ) != 0 ) {
		return FBMIN_ERR_BAD_SIG;
	}

	/* Now extract relevant features of FBM file header */
	fbmin_img_width    = atoi( phdr.cols );
	fbmin_img_height   = atoi( phdr.rows );
	fbmin_img_depth    = atoi( phdr.planes );
	fbmin_img_bits     = atoi( phdr.bits );
	fbmin_img_rowlen   = atoi( phdr.rowlen );
	fbmin_img_plnlen   = atoi( phdr.plnlen );
	fbmin_img_clrlen   = atoi( phdr.clrlen );
	fbmin_img_aspect   = atof( phdr.aspect );
	fbmin_img_physbits = atoi( phdr.physbits );
	fbmin_img_title    = phdr.title;
	fbmin_img_credit   = phdr.credits;

	if ( fbmin_image_test() != FBMIN_SUCCESS ) {
		return FBMIN_ERR_BAD_SD;
	}

	return FBMIN_SUCCESS;
}

/*
 * close an open FBM file
 */

static int fbmin_close_file( void )
{
	/* make sure there's a file open */
	if ( !file_open ) {
		return FBMIN_ERR_NFO;
	}

	/* mark file (and image) as closed */
	file_open  = 0;
	image_open = 0;

	/* done! */
	return FBMIN_SUCCESS;
}

/* d: validate row length against width and plane length without overflow */
static int fbmin_image_test( void )
{
	long plane_min;

	if ( fbmin_img_width < 1 || fbmin_img_width > 32767 ) {
		fprintf ( stderr, "Invalid width (%d) on input\n", fbmin_img_width );
		return FBMIN_ERR_BAD_SD;
	}

	if ( fbmin_img_height < 1 || fbmin_img_height > 32767 ) {
		fprintf ( stderr, "Invalid height (%d) on input\n", fbmin_img_height );
		return FBMIN_ERR_BAD_SD;
	}

	if ( fbmin_img_depth != 1 && fbmin_img_depth != 3 ) {
		fprintf ( stderr, "Invalid number of planes (%d) on input %s\n",
		          fbmin_img_depth, "(must be 1 or 3)" );
		return FBMIN_ERR_BAD_SD;
	}

	if ( fbmin_img_bits < 1 || fbmin_img_bits > 8 ) {
		fprintf ( stderr, "Invalid number of bits (%d) on input %s\n",
		          fbmin_img_bits, "(must be [1..8])" );
		return FBMIN_ERR_BAD_SD;
	}

	if ( fbmin_img_physbits != 1 && fbmin_img_physbits != 8 ) {
		fprintf ( stderr, "Invalid number of physbits (%d) on input %s\n",
		          fbmin_img_physbits, "(must be 1 or 8)" );
		return FBMIN_ERR_BAD_SD;
	}

	if ( fbmin_img_rowlen < 1 || fbmin_img_rowlen > 32767 ) {
		fprintf ( stderr, "Invalid row length (%d) on input\n",
		          fbmin_img_rowlen );
		return FBMIN_ERR_BAD_SD;
	}

	if ( fbmin_img_rowlen < fbmin_img_width ) {
		fprintf ( stderr, "Invalid row length (%d) < width (%d) on input\n",
		          fbmin_img_rowlen, fbmin_img_width );
		return FBMIN_ERR_BAD_SD;
	}

	plane_min = ( long ) fbmin_img_height * ( long ) fbmin_img_rowlen;

	if ( fbmin_img_depth > 1 && fbmin_img_plnlen < 1 ) {
		fprintf ( stderr, "Invalid plane length (%d) on input\n",
		          fbmin_img_plnlen );
		return FBMIN_ERR_BAD_SD;
	}

	if ( ( long ) fbmin_img_plnlen < plane_min ) {
		fprintf ( stderr, "Invalid plane length (%d) < (%d) * (%d) on input\n",
		          fbmin_img_plnlen, fbmin_img_height, fbmin_img_rowlen );
		return FBMIN_ERR_BAD_SD;
	}

	if ( fbmin_img_aspect < 0.01 || fbmin_img_aspect > 100.0 ) {
		fprintf ( stderr, "Invalid aspect ratio %g on input\n",
		          fbmin_img_aspect );
		return FBMIN_ERR_BAD_SD;
	}
	return FBMIN_SUCCESS;
}

/*
 * these are the routines added for interfacing to xli
 */

/*
 * tell someone what the image we're loading is.  this could be a little more
 * descriptive but I don't care
 */

static void tellAboutImage( strbyte* name )
{
	printf( "%s is a %dx%d FBM image, depth %d with %d colors\n", name,
	        fbmin_img_width, fbmin_img_height, fbmin_img_bits * fbmin_img_depth,
	        fbmin_img_clrlen / 3 );
}

Image* fbmLoad( strbyte* fullname, ImageOptions* image_ops, bool verbose )
{
	ZFILE*        zf;
	strbyte*         name = image_ops->name;
	Image* image;
	register int    x, y, j, k, width, rowpad, plnpad;
	byte* pixptr, *cm;
	byte* r, *g, *b;
	int retv;

	CURRFUNC( "fbmLoad" );
	if ( ! ( zf= zopen( fullname ) ) ) {
		return( NULL );
	}
	if ( ( retv = fbmin_open_image( zf ) ) != FBMIN_SUCCESS ) { /* read image header */
		fbmin_close_file();
		zclose( zf );
		if( retv != FBMIN_ERR_BAD_SIG ) {
			fprintf ( stderr, "fbmLoad: %s - aborting, '%s'\n",name, get_err_string( retv ) );
		}
		return( NULL );
	}
	if ( verbose ) {
		tellAboutImage( name );
	}

/* d: diagnostic fprintf block for tracing cmap8 short read at row 127 */
	fprintf( stderr, "dbg fbmLoad: file=%s w=%d h=%d depth=%d bits=%d rowlen=%d plnlen=%d clrlen=%d\n",
	         fullname, fbmin_img_width, fbmin_img_height, fbmin_img_depth,
	         fbmin_img_bits, fbmin_img_rowlen, fbmin_img_plnlen, fbmin_img_clrlen );
	fprintf( stderr, "dbg fbmLoad: pre-znocache zf=%p type=%d eof=%d bufptr=%p endptr=%p\n",
	         ( void* ) zf, ( int ) zf->type, ( int ) zf->eof,
	         ( void* ) zf->bufptr, ( void* ) zf->endptr );
	znocache( zf );
	fprintf( stderr, "dbg fbmLoad: post-znocache zf=%p type=%d eof=%d bufptr=%p endptr=%p\n",
	         ( void* ) zf, ( int ) zf->type, ( int ) zf->eof,
	         ( void* ) zf->bufptr, ( void* ) zf->endptr );






	if( fbmin_img_depth==1 ) {
		if( fbmin_img_bits==1 ) {
			image= newBitImage( fbmin_img_width, fbmin_img_height );
		} else {
			image = newRGBImage( fbmin_img_width, fbmin_img_height, fbmin_img_bits );
		}
	} else {	/* must be 3 */
		image = newTrueImage( fbmin_img_width, fbmin_img_height );
	}

	image->title= dupString( name );


	/* default linear grayscale ramp when 8-bit FBM imager is without a colormap */
	if ( fbmin_img_depth == 1 && fbmin_img_clrlen > 0 ) {
		cm = ( byte* ) lmalloc( fbmin_img_clrlen );

		if ( zread( ins, cm, fbmin_img_clrlen ) != fbmin_img_clrlen ) {
			fprintf ( stderr, "fbmLoad: %s - can't read colormap (%d bytes)\n", name, fbmin_img_clrlen );
			fbmin_close_file();
			lfree( cm );
			zclose( zf );
			freeImage( image );
			return( NULL );
		}
		/*
		 * fbm color map is organized as
		 * buf[3][16]
		 */
		y = fbmin_img_clrlen / 3;
		r = &cm[0], g = &cm[y], b = &cm[2 * y];
		for ( x = 0; x < y; x++, r++, g++, b++ ) {
			image->rgb.red[x]   = *r << 8;
			image->rgb.green[x] = *g << 8;
			image->rgb.blue[x]  = *b << 8;
		}
		image->rgb.used = y;

		cm = NULL;
	// else if has a colormap
	} else if ( fbmin_img_depth == 1 && fbmin_img_bits > 1 ) {
		unsigned int cmap_entries;
		unsigned int ramp_step;
		unsigned int channel_val;

		cmap_entries = 1U << ( unsigned int ) fbmin_img_bits;
		if ( cmap_entries > image->rgb.size ) {
			cmap_entries = image->rgb.size;
		}
		ramp_step = 0U;
		while ( ramp_step < cmap_entries ) {
			if ( cmap_entries > 1U ) {
				channel_val = ( ramp_step * 65535U ) / ( cmap_entries - 1U );
			} else {
				channel_val = 0U;
			}
			image->rgb.red[ramp_step] = ( Intensity ) channel_val;
			image->rgb.green[ramp_step] = ( Intensity ) channel_val;
			image->rgb.blue[ramp_step] = ( Intensity ) channel_val;
			ramp_step = ramp_step + 1U;
		}
		image->rgb.used = cmap_entries;
		cm = NULL;
	} else {
		cm = NULL;
	}


	width = fbmin_img_width;
	rowpad = fbmin_img_rowlen - width;
	plnpad = fbmin_img_plnlen - ( fbmin_img_height * fbmin_img_rowlen );

	if( fbmin_img_depth == 1 ) {
		if( fbmin_img_bits==1 ) {
			int xx,bb;
			byte* rb;
			rb = ( byte* ) lmalloc( width > rowpad ? width : rowpad );
			pixptr = image->data;
			for ( j = 0; j < fbmin_img_height; j++, pixptr += ( ( width+7 )/8 ) ) {
				if ( zread( ins, rb, width ) != width ) {
					fprintf( stderr, "fbmLoad: %s -mono- Short read within image data\n", name );
					if ( cm != NULL ) {
						lfree( cm );
					}
					lfree( rb );
					fbmin_close_file();
					zclose( zf );
					return( image );
				}
				for ( xx = 0,bb=0; xx < width; xx++ ) {
					bb = ( bb << 1 ) | ( rb[xx] ? 0 : 1 );
					if ( ( xx & 7 ) == 7 ) {
						pixptr[xx/8] = bb;
						bb = 0;
					}
				}
				if ( zread( ins, rb, rowpad ) != rowpad ) {
					fprintf( stderr, "fbmLoad: %s - Short read within row padding\n", name );
					if ( cm != NULL ) {
						lfree( cm );
					}
					lfree( rb );
					fbmin_close_file();
					zclose( zf );
					return( image );
				}
			}
			lfree( rb );
		} else {
			byte* rb;
			rb = ( byte* ) lmalloc( rowpad );
			pixptr = image->data;
			for ( j = 0; j < fbmin_img_height; j++, pixptr += width ) {
				if ( zread( ins, pixptr, width ) != width ) {
					fprintf( stderr, "fbmLoad: %s - Short read within image data\n", name );
					if ( cm != NULL ) {
						lfree( cm );
					}
					lfree( rb );
					fbmin_close_file();
					zclose( zf );
					return( image );
				}
				if ( zread( ins, rb, rowpad ) != rowpad ) {
					fprintf( stderr,"fbmLoad: %s - Short read within row padding\n", name );
					if ( cm != NULL ) {
						lfree( cm );
					}
					lfree( rb );
					fbmin_close_file();
					zclose( zf );
					return( image );
				}
			}
			lfree( rb );
		}
	} else {  /* depth == 3 case */
		int xx;
		byte* rb;
		rb = ( byte* ) lmalloc( width > rowpad ? ( width > plnpad ? width : plnpad )
		                        :( rowpad > plnpad ? rowpad : plnpad ) );
		for ( k = 0; k < fbmin_img_depth; k++ ) {
			pixptr = image->data + k;
			for ( j = 0; j < fbmin_img_height; j++ ) {
				if ( zread( ins, rb, width ) != width ) {
					fprintf( stderr, "fbmLoad: %s -depth 3- Short read within image data at row %d\n", name, j );
					if ( cm != NULL ) {
						lfree( cm );
					}
					lfree( rb );
					fbmin_close_file();
					zclose( zf );
					return( image );
				}
				for ( xx = 0; xx < width; xx++,pixptr+=3 ) {
					*pixptr = rb[xx];
				}
				if ( zread( ins, rb, rowpad ) != rowpad ) {
					fprintf( stderr,"fbmLoad: %s - Short read within row padding\n", name );
					if ( cm != NULL ) {
						lfree( cm );
					}
					lfree( rb );
					fbmin_close_file();
					zclose( zf );
					return( image );
				}
			}
			if ( zread( ins, rb, plnpad ) != plnpad ) {
				fprintf( stderr,"fbmLoad: %s - Short read within plane padding\n", name );
				if ( cm != NULL ) {
					lfree( cm );
				}
				lfree( rb );
				fbmin_close_file();
				zclose( zf );
				return( image );
			}
		}
		lfree( rb );
	}

	if ( cm != NULL ) {
		lfree( cm );
	}
	fbmin_close_file();

	zclose( zf );
	return( image );
}

int fbmIdent( strbyte* fullname, strbyte* name )
{
	ZFILE*        zf;
	unsigned int  ret;
	int retv;

	if ( ! ( zf= zopen( fullname ) ) ) {
		return( 0 );
	}
	if ( ( retv = fbmin_open_image( zf ) ) == FBMIN_SUCCESS ) {
		tellAboutImage( name );
		ret = 1;
	} else {
		if ( retv != FBMIN_ERR_BAD_SIG ) {
			fprintf ( stderr, "FBM file error\n" );
			ret = 1;		/* file was identified as FBM */
		} else {
			ret = 0;
		}
	}
	fbmin_close_file();
	zclose( zf );
	return( ret );
}
