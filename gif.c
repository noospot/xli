/* gif.c:
 *
 * adapted from code by kirk johnson (tuna@athena.mit.edu).  most of this
 * code is unchanged. -- jim frost 12.31.89
 *
 * gifin.c
 * kirk johnson
 * november 1989
 *
 * routines for reading GIF files
 *
 * Copyright 1989 Kirk L. Johnson (see the included file
 * "kljcpyrght.h" for complete copyright information)
 */

#define GIF_C

#include "xli.h"
#include "gif.h"
#include "imagetypes.h"

#include <string.h>

/****
 **
 ** local variables
 **
 ****/

static int interlace_start[4]= { /* start line for interlacing */
	0, 4, 2, 1
};

static int interlace_rate[4]= { /* rate at which we accelerate vertically */
	8, 8, 4, 2
};

static byte file_open  = 0;     /* status flags */
static byte image_open = 0;

static ZFILE* ins;              /* input stream */

static int  root_size;          /* root code size */
static int  clr_code;           /* clear code */
static int  eoi_code;           /* end of information code */
static int  code_size;          /* current code size */
static int  code_mask;          /* current code mask */
static int  prev_code;          /* previous code */

/*
 * NOTE: a long is assumed to be at least 32 bits wide
 */
static long work_data;          /* working bit buffer */
static int  work_bits;          /* working bit count */

static byte buf[256];           /* byte buffer */
static int  buf_cnt;            /* byte count */
static int  buf_idx;            /* buffer index */

static int table_size;          /* string table size */
static int prefix[STAB_SIZE];   /* string table : prefixes */
static int extnsn[STAB_SIZE];   /* string table : extensions */

static byte pstk[PSTK_SIZE];    /* pixel stack */
static int  pstk_idx;           /* pixel stack pointer */


/****
 **
 ** global variables
 **
 ****/

static int  gifin_rast_width;          /* raster width */
static int  gifin_rast_height;         /* raster height */
static byte gifin_g_cmap_flag;         /* global colormap flag */
static int  gifin_g_pixel_bits;        /* bits per pixel, global colormap */
static int  gifin_g_ncolors;           /* number of colors, global colormap */
static byte gifin_g_cmap[3][256];      /* global colormap */
static byte gifin_g_cmap_sorted;       /* global colormap sorted (GIF89a only) */
static int  gifin_bg_color;            /* background color index */
static int  gifin_color_bits;          /* bits of color resolution */
static double  gifin_aspect;           /* pixel aspect ratio (width/height) */
static int  gifin_version;             /* gif file version */

static int  gifin_img_left;            /* image position on raster */
static int  gifin_img_top;             /* image position on raster */
static int  gifin_img_width;           /* image width */
static int  gifin_img_height;          /* image height */
static byte gifin_l_cmap_flag;         /* local colormap flag */
static int  gifin_l_pixel_bits;        /* bits per pixel, local colormap */
static int  gifin_l_ncolors;           /* number of colors, local colormap */
static byte gifin_l_cmap[3][256];      /* local colormap */
static byte gifin_interlace_flag;      /* interlace image format flag */



/*
 * d: push a single pixel onto the pixel stack or return error if overflow
 */
static inline int push_pixel( int* pstk_idx, byte* pstk, byte p )
{
	if ( *pstk_idx == PSTK_SIZE ) {
		return GIFIN_ERR_PSO;
	}
	pstk[*pstk_idx] = p;
	*pstk_idx = *pstk_idx + 1;
	return GIFIN_OK;
}


/*
 * d: push a string denoted by a code onto the pixel stack
 */
static inline int gifin_push_string( int code, int* pstk_idx, byte* pstk, int* prefix, int* extnsn, int* ps_rslt )
{
	int ps_code;
	int push_status;

	ps_code = code;

	while ( ((unsigned int)ps_code) < STAB_SIZE && prefix[ps_code] != NULL_CODE ) {
		push_status = push_pixel( pstk_idx, pstk, (byte)extnsn[ps_code] );
		if ( push_status != GIFIN_OK ) {
			return push_status;
		}
		ps_code = prefix[ps_code];
	}

	if ( ((unsigned int)ps_code) >= STAB_SIZE ) {
		return GIFIN_ERR_TAO;
	}

	push_status = push_pixel( pstk_idx, pstk, (byte)extnsn[ps_code] );
	if ( push_status != GIFIN_OK ) {
		return push_status;
	}

	*ps_rslt = extnsn[ps_code];
	if ( ((unsigned int)*ps_rslt) >= STAB_SIZE ) {
		return GIFIN_ERR_TAO;
	}

	return GIFIN_OK;
}


/*
 * Look up the ascii message coresponding to
 * the error number.
 */
static inline strbyte* get_err_string( int errno )
{
	int i;
	i = 0;
	while ( gif_err_strings[i].err_no != 0 ) {
		if ( gif_err_strings[i].err_no == errno ) {
			return gif_err_strings[i].name;
		}
		i = i + 1;
	}
	return "";
}




/*
 * load a colormap from the input stream
 */

static inline int gifin_load_cmap( byte ( *cmap )[256], int ncolors )
{
	int i;

	i = 0;
	while ( i < ncolors ) {
		if ( zread( ins, buf, 3 ) != 3 ) {
			return GIFIN_ERR_EOF;
		}

		cmap[GIF_RED][i] = buf[GIF_RED];
		cmap[GIF_GRN][i] = buf[GIF_GRN];
		cmap[GIF_BLU][i] = buf[GIF_BLU];
		i = i + 1;
	}

	/* done! */
	return GIFIN_OK;
}


/*
 * verify raster and image dimensions
 */
static inline int verify_boundaries( int rwidth, int rheight )
{
	if ( rwidth < MIN_GIF_WIDTH || rwidth > MAX_GIF_WIDTH ) {
		return GIFIN_ERR_BAD_DES;
	}
	if ( rheight < MIN_GIF_HEIGHT || rheight > MAX_GIF_HEIGHT ) {
		return GIFIN_ERR_BAD_DES;
	}
	return GIFIN_OK;
}


/*
 * verify code size and color table bounds
 */
static inline int verify_codesize_and_colors( int csize, int ccount )
{
	if ( csize < MIN_GIF_CODESIZE || csize > MAX_GIF_CODESIZE ) {
		return GIFIN_ERR_BAD_DES;
	}
	if ( ccount < 0 || ( ccount > 0 && ( ccount < MIN_GIF_COLORS || ccount > MAX_GIF_COLORS ) ) ) {
		return GIFIN_ERR_BAD_DES;
	}
	return GIFIN_OK;
}


/*
 * open a GIF file, using s as the input stream
 */

static int gifin_open_file( ZFILE* s, bool verbose )
{
	int errno;
	/* make sure there isn't already a file open */
	if ( file_open ) {
		fprintf( stderr, "gifin_open_file: %s\n", GIFIN_ERR_FAO_STR );
		return GIFIN_ERR_FAO;
	}

	/* remember that we've got this file open */
	file_open = 1;
	ins       = s;

	/* check GIF signature */
	if ( zread( ins, buf, GIF_SIG_LEN ) != GIF_SIG_LEN ) {
		fprintf( stderr, "gifin_open_file: %s\n", GIFIN_ERR_EOF_STR );
		return GIFIN_ERR_EOF;
	}

	buf[GIF_SIG_LEN] = '\0';
	if ( strcmp( ( strbyte* ) buf, GIF_SIG ) == 0 ) {
		gifin_version = GIF87a;
	} else if( strcmp( ( strbyte* ) buf, GIF_SIG_89 ) == 0 ) {
		gifin_version = GIF89a;
	} else {
		fprintf( stderr, "gifin_open_file: %s\n", GIFIN_ERR_BAD_SIG_STR );
		return GIFIN_ERR_BAD_SIG;
	}

	/* read screen descriptor */
	if ( zread( ins, buf, GIF_SD_SIZE ) != GIF_SD_SIZE ) {
		fprintf( stderr, "gifin_open_file: %s\n", GIFIN_ERR_EOF_STR );
		return GIFIN_ERR_EOF;
	}

	/* decode screen descriptor */
	gifin_rast_width   = ( buf[1] << 8 ) + buf[0];
	gifin_rast_height  = ( buf[3] << 8 ) + buf[2];
	
	if ( ( errno = verify_boundaries( gifin_rast_width, gifin_rast_height ) ) != GIFIN_OK ) {
		fprintf( stderr, "gifin_open_file: %s\n", GIFIN_ERR_BAD_SD_STR );
		return errno;
	}

	gifin_g_cmap_flag  = ( buf[4] & 0x80 ) ? 1 : 0;
	gifin_color_bits   = ( ( ( int )( buf[4] & 0x70 ) ) >> 4 ) + 1;
	gifin_g_pixel_bits = ( buf[4] & 0x07 ) + 1;
	gifin_bg_color     = buf[5];
	gifin_aspect = 1.0;

	if ( gifin_version == GIF87a ) {
		if ( buf[4] & 0x08 || buf[6] != 0 ) {
			if ( verbose ) {
				fprintf( stderr, "gifLoad: ignoring non-null screen descriptor in Gif87a image\n" );
			}
		}
	} else {
		gifin_g_cmap_sorted = ( ( buf[4] & 0x08 ) != 0 );
		if ( buf[6] != 0 ) {
			gifin_aspect = ( ( double )buf[6] + 15.0 ) / 64.0;
		}
	}

	/* load global colormap */
	if ( gifin_g_cmap_flag ) {
		gifin_g_ncolors = ( 1 << gifin_g_pixel_bits );

		if ( gifin_g_ncolors < MIN_GIF_COLORS || gifin_g_ncolors > MAX_GIF_COLORS ) {
			fprintf( stderr, "gifin_open_file: %s\n", GIFIN_ERR_BAD_SD_STR );
			return GIFIN_ERR_BAD_DES;
		}

		if ( ( errno = gifin_load_cmap( gifin_g_cmap, gifin_g_ncolors ) ) != GIFIN_OK ) {
			fprintf( stderr, "gifin_open_file: %s\n", GIFIN_ERR_EOF_STR );
			return errno;
		}
	} else {
		gifin_g_ncolors = 0;
	}

	/* done! */
	return GIFIN_OK;
}


/*
 * read a new data block from the input stream
 */

static int gifin_read_data_block( void )
{
	/* read the data block header */
	buf_cnt = zgetc( ins );

	if ( buf_cnt < 0 || buf_cnt > MAX_GIF_BLOCKSIZE ) {
		fprintf( stderr, "gifin_read_data_block: %s\n", GIFIN_ERR_BAD_DES_STR );
		return GIFIN_ERR_BAD_DES;
	}

	/* read the data block body */
	if ( zread( ins, buf, buf_cnt ) != buf_cnt ) {
		fprintf( stderr, "gifin_read_data_block: %s\n", GIFIN_ERR_EOF_STR );
		return GIFIN_ERR_EOF;
	}

	buf_idx = 0;

	/* done! */
	return GIFIN_OK;
}


/*
 * skip an extension block in the input stream
 */

static inline int gifin_skip_extension( void )
{
	int errno;

	/* get the extension function byte */
	zgetc( ins );

	/* skip any remaining raster data */
	do {
		if ( ( errno = gifin_read_data_block() ) != GIFIN_OK ) {
			return errno;
		}
	} while ( buf_cnt > 0 );

	/* done! */
	return GIFIN_OK;
}


/*
 * open next GIF image in the input stream; returns GIFIN_OK if
 * successful. if there are no more images, returns GIFIN_DONE. (might
 * also return various GIFIN_ERR codes.)
 */

static int gifin_open_image( void )
{
	int i;
	int separator;
	int errno;

	/* make sure there's a file open */
	if ( !file_open ) {
		fprintf( stderr, "gifin_open_image: %s\n", GIFIN_ERR_NFO_STR );
		return GIFIN_ERR_NFO;
	}

	/* make sure there isn't already an image open */
	if ( image_open ) {
		fprintf( stderr, "gifin_open_image: %s\n", GIFIN_ERR_IAO_STR );
		return GIFIN_ERR_IAO;
	}

	/* remember that we've got this image open */
	image_open = 1;

	/* skip over any extension blocks */
	do {
		separator = zgetc( ins );
		if ( separator == GIF_EXTENSION ) {
			if ( ( errno = gifin_skip_extension() ) != GIFIN_OK ) {
				return errno;
			}
		}
	} while ( separator == GIF_EXTENSION );

	/* check for end of file marker */
	if ( separator == GIF_TERMINATOR ) {
		return GIFIN_DONE;
	}

	/* make sure we've got an image separator */
	if ( separator != GIF_SEPARATOR ) {
		fprintf( stderr, "gifin_open_image: %s\n", GIFIN_ERR_BAD_SEP_STR );
		return GIFIN_ERR_BAD_SEP;
	}

	/* read image descriptor */
	if ( zread( ins, buf, GIF_ID_SIZE ) != GIF_ID_SIZE ) {
		fprintf( stderr, "gifin_open_image: %s\n", GIFIN_ERR_EOF_STR );
		return GIFIN_ERR_EOF;
	}

	/* decode image descriptor */
	gifin_img_left       = ( buf[1] << 8 ) + buf[0];
	gifin_img_top        = ( buf[3] << 8 ) + buf[2];
	gifin_img_width      = ( buf[5] << 8 ) + buf[4];
	gifin_img_height     = ( buf[7] << 8 ) + buf[6];

	if ( ( errno = verify_boundaries( gifin_img_width, gifin_img_height ) ) != GIFIN_OK ) {
		fprintf( stderr, "gifin_open_image: %s\n", GIFIN_ERR_BAD_DES_STR );
		return errno;
	}

	gifin_l_cmap_flag    = ( buf[8] & 0x80 ) ? 1 : 0;
	gifin_interlace_flag = ( buf[8] & 0x40 ) ? 1 : 0;
	gifin_l_pixel_bits   = ( buf[8] & 0x07 ) + 1;

	/* load local colormap */
	if ( gifin_l_cmap_flag ) {
		gifin_l_ncolors = ( 1 << gifin_l_pixel_bits );

		if ( gifin_l_ncolors < MIN_GIF_COLORS || gifin_l_ncolors > MAX_GIF_COLORS ) {
			fprintf( stderr, "gifin_open_image: %s\n", GIFIN_ERR_BAD_DES_STR );
			return GIFIN_ERR_BAD_DES;
		}

		if ( ( errno = gifin_load_cmap( gifin_l_cmap, gifin_l_ncolors ) ) != GIFIN_OK ) {
			fprintf( stderr, "gifin_open_image: %s\n", GIFIN_ERR_EOF_STR );
			return errno;
		}
	} else {
		gifin_l_ncolors = 0;
	}

	/* initialize raster data stream decoder */
	root_size = zgetc( ins );

	if ( root_size < MIN_GIF_CODESIZE || root_size > MAX_GIF_CODESIZE ) {
		fprintf( stderr, "gifin_open_image: %s\n", GIFIN_ERR_BAD_DES_STR );
		return GIFIN_ERR_BAD_DES;
	}

	clr_code  = 1 << root_size;
	eoi_code  = clr_code + 1;
	code_size = root_size + 1;
	code_mask = ( 1 << code_size ) - 1;
	work_bits = 0;
	work_data = 0;
	buf_cnt   = 0;
	buf_idx   = 0;

	/* initialize string table */
	i = 0;
	while ( i < STAB_SIZE ) {
		prefix[i] = NULL_CODE;
		extnsn[i] = i;
		i = i + 1;
	}

	/* initialize pixel stack */
	pstk_idx = 0;

	/* done! */
	return GIFIN_OK;
}


/*
 * add a new string to the string table
 */

static inline int gifin_add_string( int p, int e )
{
	prefix[table_size] = p;
	extnsn[table_size] = e;

	if ( ( table_size == code_mask ) && ( code_size < 12 ) ) {
		code_size = code_size + 1;
		code_mask  = ( 1 << code_size ) - 1;
	}

	table_size = table_size + 1;
	if ( table_size > STAB_SIZE ) {
		fprintf( stderr, "gifin_add_string: %s\n", GIFIN_ERR_TAO_STR );
		return GIFIN_ERR_TAO;
	}
	return GIFIN_OK;
}


/*
 * try to read next pixel from the raster, return result in *pel
 */

static int gifin_get_pixel( int* pel )
{
	int  code;
	int  first;
	int  place;
	int  errno;
	int  push_res;

	/* decode until there are some pixels on the pixel stack */
	while ( pstk_idx == 0 ) {
		/* load bytes until we have enough bits for another code */
		while ( work_bits < code_size ) {
			if ( buf_idx == buf_cnt ) {
				/* read a new data block */
				if ( ( errno = gifin_read_data_block() ) != GIFIN_OK ) {
					return errno;
				}

				if ( buf_cnt == 0 ) {
					fprintf( stderr, "gifin_get_pixel: %s\n", GIFIN_ERR_EOD_STR );
					return GIFIN_ERR_EOD;
				}
			}

			work_data |= ( ( long ) buf[buf_idx] ) << work_bits;
			buf_idx = buf_idx + 1;
			work_bits = work_bits + 8;
		}

		/* get the next code */
		code        = work_data & code_mask;
		work_data >>= code_size;
		work_bits  = work_bits - code_size;

		/* interpret the code */
		if ( code == clr_code ) {
			/* reset decoder stream */
			code_size  = root_size + 1;
			code_mask  = ( 1 << code_size ) - 1;
			prev_code  = NULL_CODE;
			table_size = eoi_code + 1;
		} else if ( code == eoi_code ) {
			/* Ooops! no more pixels */
			fprintf( stderr, "gifin_get_pixel: %s\n", GIFIN_ERR_EOF_STR );
			return GIFIN_ERR_EOF;
		} else if ( prev_code == NULL_CODE ) {
			push_res = gifin_push_string( code, &pstk_idx, pstk, prefix, extnsn, &first );
			if ( push_res != GIFIN_OK ) {
				if ( push_res == GIFIN_ERR_PSO ) {
					fprintf( stderr, "gifin_get_pixel: %s\n", GIFIN_ERR_PSO_STR );
				} else if ( push_res == GIFIN_ERR_TAO ) {
					fprintf( stderr, "gifin_get_pixel: %s\n", GIFIN_ERR_TAO_STR );
				}
				return push_res;
			}
			prev_code = code;
		} else {
			if ( code < table_size ) {
				push_res = gifin_push_string( code, &pstk_idx, pstk, prefix, extnsn, &first );
				if ( push_res != GIFIN_OK ) {
					if ( push_res == GIFIN_ERR_PSO ) {
						fprintf( stderr, "gifin_get_pixel: %s\n", GIFIN_ERR_PSO_STR );
					} else if ( push_res == GIFIN_ERR_TAO ) {
						fprintf( stderr, "gifin_get_pixel: %s\n", GIFIN_ERR_TAO_STR );
					}
					return push_res;
				}
			} else {
				place = pstk_idx;
				push_res = push_pixel( &pstk_idx, pstk, (byte)NULL_CODE );
				if ( push_res != GIFIN_OK ) {
					if ( push_res == GIFIN_ERR_PSO ) {
						fprintf( stderr, "gifin_get_pixel: %s\n", GIFIN_ERR_PSO_STR );
					}
					return push_res;
				}
				push_res = gifin_push_string( prev_code, &pstk_idx, pstk, prefix, extnsn, &first );
				if ( push_res != GIFIN_OK ) {
					if ( push_res == GIFIN_ERR_PSO ) {
						fprintf( stderr, "gifin_get_pixel: %s\n", GIFIN_ERR_PSO_STR );
					} else if ( push_res == GIFIN_ERR_TAO ) {
						fprintf( stderr, "gifin_get_pixel: %s\n", GIFIN_ERR_TAO_STR );
					}
					return push_res;
				}
				pstk[place] = (byte)first;
			}

			if( ( errno = gifin_add_string( prev_code, first ) ) != GIFIN_OK ) {
				return errno;
			}
			prev_code = code;
		}
	}

	/* pop a pixel off the pixel stack */
	pstk_idx = pstk_idx - 1;
	*pel = ( int ) pstk[pstk_idx];

	/* done! */
	return GIFIN_OK;
}


/*
 * close an open GIF file
 */

static int gifin_close_file( void )
{
	/* make sure there's a file open */
	if ( !file_open ) {
		fprintf( stderr, "gifin_close_file: %s\n", GIFIN_ERR_NFO_STR );
		return GIFIN_ERR_NFO;
	}

	/* mark file (and image) as closed */
	file_open  = 0;
	image_open = 0;

	/* done! */
	return GIFIN_OK;
}


/* these are the routines added for interfacing to xli
 */

/* tell someone what the image we're loading is.  this could be a little more
 * descriptive but I don't care
 */

static void tellAboutImage( strbyte* name )
{
	printf( "%s is a %dx%d %s%s image with %d colors\n",
	        name, gifin_img_width, gifin_img_height,
	        ( gifin_interlace_flag ? "interlaced " : "" ),
	        gif_version_name[gifin_version],
	        ( gifin_l_cmap_flag ? gifin_l_ncolors : gifin_g_ncolors ) );
}


Image* gifLoad( strbyte* fullname, ImageOptions* image_ops, bool verbose )
{
	ZFILE*        zf;
	strbyte*         name = image_ops->name;
	Image* image;
	int    x, y, pixel, pass, scanlen;
	byte*  pixptr, *pixline;
	int errno;

	if ( ! ( zf= zopen( fullname ) ) ) {
		perror( "gifLoad" );
		return( NULL );
	}
	if ( ( gifin_open_file( zf, verbose ) != GIFIN_OK ) || /* read GIF header */
	      ( gifin_open_image() != GIFIN_OK ) ) { /* read image header */
		gifin_close_file();
		zclose( zf );
		return( NULL );
	}
	if ( verbose ) {
		tellAboutImage( name );
	}
	znocache( zf );
	image= newRGBImage( gifin_img_width, gifin_img_height, ( gifin_l_cmap_flag ?
	                    gifin_l_pixel_bits :
	                    gifin_g_pixel_bits ) );
	image->title= dupString( name );
	/* if image has a local colormap, override global colormap
	 */

	if ( gifin_l_cmap_flag ) {
		x = 0;
		while ( x < gifin_l_ncolors ) {
			image->rgb.red[x]= gifin_l_cmap[GIF_RED][x] << 8;
			image->rgb.green[x]= gifin_l_cmap[GIF_GRN][x] << 8;
			image->rgb.blue[x]= gifin_l_cmap[GIF_BLU][x] << 8;
			x = x + 1;
		}
		image->rgb.used= gifin_l_ncolors;
	} else {
		x = 0;
		while ( x < gifin_g_ncolors ) {
			image->rgb.red[x]= gifin_g_cmap[GIF_RED][x] << 8;
			image->rgb.green[x]= gifin_g_cmap[GIF_GRN][x] << 8;
			image->rgb.blue[x]= gifin_g_cmap[GIF_BLU][x] << 8;
			x = x + 1;
		}
		image->rgb.used= gifin_g_ncolors;
	}


	/* interlaced image -- futz with the vertical trace.  i wish i knew what
	 * kind of drugs the GIF people were on when they decided that they
	 * needed to support interlacing.
	 */

	if ( gifin_interlace_flag ) {
		scanlen= image->height * image->pixlen;

		/* interlacing takes four passes to read, each starting at a different
		 * vertical point.
		 */

		pass = 0;
		while ( pass < 4 ) {
			y= interlace_start[pass];
			scanlen= image->width * image->pixlen * interlace_rate[pass];
			pixline= image->data + ( y * image->width * image->pixlen );
			while ( y < gifin_img_height ) {
				pixptr= pixline;
				x= 0;
				while ( x < gifin_img_width ) {
					if ( ( errno = gifin_get_pixel( &pixel ) ) != GIFIN_OK ) {
						fprintf( stderr, "gifLoad: %s - Short read within image data, '%s'\n", name, get_err_string( errno ) );
						y = gifin_img_height;
						x = gifin_img_width;
						goto skip_interlace_row;
					}
					valToMem( pixel, pixptr, image->pixlen );
					pixptr += image->pixlen;
					x = x + 1;
				}
				skip_interlace_row:
				y += interlace_rate[pass];
				pixline += scanlen;
			}
			pass = pass + 1;
		}
	}

	/* not an interlaced image, just read in sequentially
	 */

	else {
		if( image->pixlen == 1 ) {	/* the usual case */
			pixptr= image->data;
			y = 0;
			while ( y < gifin_img_height ) {
				x = 0;
				while ( x < gifin_img_width ) {
					if ( ( errno = gifin_get_pixel( &pixel ) ) != GIFIN_OK ) {
						fprintf( stderr, "gifLoad: %s - Short read within image data, '%s'\n", name, get_err_string( errno ) );
						y = gifin_img_height;
						x = gifin_img_width;
						goto skip_seq_row1;
					}
					valToMem( pixel, pixptr, 1 );
					pixptr += 1;
					x = x + 1;
				}
				skip_seq_row1:
				y = y + 1;
			}
		} else {	/* less ususal case */
			pixptr= image->data;
			y = 0;
			while ( y < gifin_img_height ) {
				x = 0;
				while ( x < gifin_img_width ) {
					if ( ( errno = gifin_get_pixel( &pixel ) ) != GIFIN_OK ) {
						fprintf( stderr, "gifLoad: %s - Short read within image data, '%s'\n", name, get_err_string( errno ) );
						y = gifin_img_height;
						x = gifin_img_width;
						goto skip_seq_row2;
					}
					valToMem( pixel, pixptr, image->pixlen );
					pixptr += image->pixlen;
					x = x + 1;
				}
				skip_seq_row2:
				y = y + 1;
			}
		}
	}
	gifin_close_file();

	zclose( zf );
	if ( gifin_aspect != 1.0 ) {	/* correct for GIF89a aspect ratio */
		Image* timage;
		printf( "  Correcting for GIF89a pixel aspect ratio of %2.4f...",gifin_aspect );
		if ( gifin_aspect < 1.0 ) {	/* tall pixels - image will look too short */
			timage = zoom( image, 0, ( int )( 100.0/gifin_aspect + 0.5 ), FALSE, FALSE );
		} else {	/* wide pixels - image will look too thin */
			timage = zoom( image, ( int )( 100.0 * gifin_aspect + 0.5 ), 0, FALSE, FALSE );
		}
		if ( timage != image ) {
			freeImage( image );
		}
		image = timage;
		printf( "done\n" );
	}
	return( image );
}


int gifIdent( strbyte* fullname, strbyte* name )
{
	ZFILE*        zf;
	unsigned int  ret;

	if ( ! ( zf= zopen( fullname ) ) ) {
		return( 0 );
	}
	if ( ( gifin_open_file( zf, FALSE ) == GIFIN_OK ) &&
	      ( gifin_open_image() == GIFIN_OK ) ) {
		tellAboutImage( name );
		ret= 1;
	} else {
		ret= 0;
	}
	gifin_close_file();
	zclose( zf );
	return( ret );
}