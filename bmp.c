/*
 * Read a Microsoft/IBM BMP file.
 *
 * Created for xli by Graeme Gill,
 *
 * Based on tga.c, and guided by the Microsoft file format
 * description, and bmptoppm.c by David W. Sanderson.
 *
 */

#include "xli.h"
#include "imagetypes.h"
#include "bmp.h"

// d: extract unsigned 4-byte little-endian integer into unsigned long
static inline unsigned long GULONG4( const uint8_t* bp )
{
	unsigned long value = ( unsigned long )bp[0]
	                      | ( ( unsigned long )bp[1] << 8 )
	                      | ( ( unsigned long )bp[2] << 16 )
	                      | ( ( unsigned long )bp[3] << 24 );
	return value;
}

// d: extract unsigned 2-byte little-endian integer into unsigned long
static inline unsigned long GULONG2( const uint8_t* bp )
{
	unsigned long value = ( unsigned long )bp[0]
	                      | ( ( unsigned long )bp[1] << 8 );
	return value;
}

// d: extract unsigned 2-byte little-endian integer into unsigned int
static inline unsigned int GUINT2( const uint8_t* bp )
{
	unsigned int value = ( unsigned int )bp[0]
	                     | ( ( unsigned int )bp[1] << 8 );
	return value;
}

// d: extract signed 4-byte little-endian integer into int32_t
static inline int32_t GSINT4( const uint8_t* bp )
{
	uint32_t value = ( uint32_t )bp[0]
	                 | ( ( uint32_t )bp[1] << 8 )
	                 | ( ( uint32_t )bp[2] << 16 )
	                 | ( ( uint32_t )bp[3] << 24 );
	return ( int32_t )value;
}

// d: extract signed 2-byte little-endian integer into int32_t
static inline int32_t GSINT2( const uint8_t* bp )
{
	uint16_t value = ( uint16_t )bp[0]
	                 | ( ( uint16_t )bp[1] << 8 );
	return ( int32_t )( int16_t )value;
}


/* Read the header of the file, and */
/* Return TRUE if this looks like a bmp file */
static bool read_bmpHeader( ZFILE* zf, bmpHeader* hp, strbyte* name )
{
	hp->name = name;
	byte buf[WIN_INFOHEADER_LEN];	/* largest we'll need */

	if ( zread( zf, buf, BMP_FILEHEADER_LEN ) != BMP_FILEHEADER_LEN ) {
		return FALSE;
	}

	if ( buf[0] != 'B' || buf[1] != 'M' ) {
		return FALSE;      /* bad magic number */
	}

	hp->bfSize = GULONG4( &buf[2] );
	hp->bfxHotSpot = GUINT2( &buf[6] );
	hp->bfyHotSpot = GUINT2( &buf[8] );
	hp->bfOffBits = GULONG4( &buf[10] );

	/* Read enough of the file info to figure the type out */
	if ( zread( zf, buf, 4 ) != 4 ) {
		return FALSE;
	}

	hp->biSize = GULONG4( &buf[0] );
	if ( hp->biSize == WIN_INFOHEADER_LEN ) {
		hp->bfType = C_WIN;
	} else if ( hp->biSize == OS2_INFOHEADER_LEN ) {
		hp->bfType = C_OS2;
	} else {
		return FALSE;
	}

	if ( hp->bfType == C_WIN ) {
		/* read in the rest of the info header */
		if ( zread( zf, buf + 4, WIN_INFOHEADER_LEN - 4 )
		        != WIN_INFOHEADER_LEN - 4 ) {
			return FALSE;
		}

		hp->biWidth = GULONG4( &buf[4] );
		hp->biHeight = GSINT4( &buf[8] );
		hp->biPlanes = GUINT2( &buf[12] );
		hp->biBitCount = GUINT2( &buf[14] );
		hp->biCompression = GULONG4( &buf[16] );
		hp->biSizeImage = GULONG4( &buf[20] );
		hp->biXPelsPerMeter = GULONG4( &buf[24] );
		hp->biYPelsPerMeter = GULONG4( &buf[28] );
		hp->biClrUsed = GULONG4( &buf[32] );
		hp->biClrImportant = GULONG4( &buf[36] );
	} else {		/* C_OS2 */
		/* read in the rest of the info header */
		if ( zread( zf, buf + 4, OS2_INFOHEADER_LEN - 4 )
		        != OS2_INFOHEADER_LEN - 4 ) {
			return FALSE;
		}

		hp->biWidth = GULONG2( &buf[4] );
		hp->biHeight = GSINT2( &buf[6] );
		hp->biPlanes = GUINT2( &buf[8] );
		hp->biBitCount = GUINT2( &buf[10] );
		hp->biCompression = BI_RGB;
		hp->biSizeImage = 0;
		hp->biXPelsPerMeter = 0;
		hp->biYPelsPerMeter = 0;
		hp->biClrUsed = 0;
		hp->biClrImportant = 0;
	}

	/* Check file corruption and dimensions bounds */
	if ( hp->biWidth == 0 || hp->biHeight == 0 ) {
		fprintf( stderr, "bmpLoad: %s - Image dimension is 0\n", hp->name );
		return FALSE;
	}	
	if ( hp->biHeight < 0 ) {
		fprintf( stderr, "bmpLoad: %s - Illegal image Height value (top-down or negative height not supported)\n", hp->name );
		return FALSE;
	}
	if ( hp->biWidth > 65535 || hp->biHeight > 65535 ) {
		fprintf( stderr, "bmpLoad: %s - Image dimensions exceed bounds\n", hp->name );
		return FALSE;
	}
	if ( hp->biBitCount != 1
	        && hp->biBitCount != 4
	        && hp->biBitCount != 8
	        && hp->biBitCount != 24 ) {
		fprintf( stderr, "bmpLoad: %s - Illegal image BitCount\n", hp->name );
		return FALSE;
	}
	if ( ( hp->biCompression != BI_RGB
	        && hp->biCompression != BI_RLE8
	        && hp->biCompression != BI_RLE4 )
	        || ( hp->biCompression == BI_RLE8 && hp->biBitCount != 8 )
	        || ( hp->biCompression == BI_RLE4 && hp->biBitCount != 4 ) ) {
		fprintf( stderr,	"bmpLoad: %s - Illegal image compression type\n", hp->name );
		return FALSE;
	}
	if ( hp->biPlanes != 1 ) {
		fprintf( stderr, "bmpLoad: %s - Illegal image Planes value\n", hp->name );
		return FALSE;
	}
	/* Fix up a few things */
	if ( hp->biBitCount < 24 ) {
		if ( hp->biClrUsed == 0 || hp->biClrUsed > ( long unsigned int )( 1 << hp->biBitCount ) ) {
			hp->biClrUsed = ( 1 << hp->biBitCount );
		}
	} else {
		hp->biClrUsed = 0;
	}
	
	return TRUE;
}

/* Print a brief description of the image */
static void tell_about_bmp( bmpHeader* hp )
{
	printf( "%s is a %lux%i %d bit deep, %s%s BMP image\n", hp->name,
	        hp->biWidth, hp->biHeight, hp->biBitCount,
	        hp->biCompression == BI_RGB ? "" : "Run length compressed, ",
	        hp->bfType == C_WIN ? "Windows" : "OS/2" );
}

int bmpIdent( strbyte* fullname, strbyte* name )
{
	ZFILE* zf;
	bmpHeader hdr = {0};

	if ( !( zf = zopen( fullname ) ) ) {
		perror( "bmpIdent" );
		return ( 0 );
	}
	if ( !read_bmpHeader( zf, &hdr, name ) ) {
		zclose( zf );
		return 0;	/* Nope, not a BMP file */
	}
	tell_about_bmp( &hdr );
	zclose( zf );
	return 1;
}

Image* bmpLoad( strbyte* fullname, ImageOptions* image_ops, bool verbose )
{
	ZFILE* zf;
	bmpHeader hdr = {0};
	Image* image;
	bool data_bounds = FALSE;
	unsigned long skip;

	if ( !( zf = zopen( fullname ) ) ) {
		perror( "bmpIdent" );
		return ( 0 );
	}
	if ( !read_bmpHeader( zf, &hdr, image_ops->name ) ) {
		zclose( zf );
		return NULL;	/* Nope, not a BMP file */
	}
	if ( verbose ) {
		tell_about_bmp( &hdr );
	}
	znocache( zf );

	/* Create the appropriate image and colormap */
	if ( hdr.biBitCount < 24 ) {
		/* must be 1, 4 or 8 bit mapped type */
		unsigned int i;
		int n = 3;
		byte buf[4];
		/* maximum number of colors */
		unsigned int used = ( 1 << hdr.biBitCount );

		if ( hdr.biBitCount == 1 ) {	/* bitmap */
			image = newBitImage( hdr.biWidth, hdr.biHeight );
		} else {
			image = newRGBImage( hdr.biWidth, hdr.biHeight,
			                     hdr.biBitCount );
		}
		image->title = dupString( hdr.name );

		if ( hdr.bfType == C_WIN ) {
			n = n + 1;
		}
		i = 0;
		while ( i < hdr.biClrUsed ) {
			if ( zread( zf, buf, n ) != n ) {
				fprintf( stderr, "bmpLoad: %s - Short read within Colormap\n", hdr.name );
				freeImage( image );
				zclose( zf );
				return NULL;
			}
			image->rgb.red[i] = buf[2] << 8;
			image->rgb.green[i] = buf[1] << 8;
			image->rgb.blue[i] = buf[0] << 8;
			i = i + 1;
		}

		/* init rest of colormap (if any) */
		while ( i < used ) {
			image->rgb.red[i] = 0;
			image->rgb.green[i] = 0;
			image->rgb.blue[i] = 0;
			i = i + 1;
		}
		/* Don't know how many colors are actually used. */
		/* (believing the header could cause a fault) */
		/* compress() will figure it out. */
		image->rgb.used = used;		/* so use max possible */
	} else {		/* else must be a true color image */
		image = newTrueImage( hdr.biWidth, hdr.biHeight );
		image->title = dupString( hdr.name );
	}

	/* Skip to image data offset specified in file header */
	{
		unsigned long min_offbits = BMP_FILEHEADER_LEN
		                            + ( hdr.bfType == C_WIN ? WIN_INFOHEADER_LEN : OS2_INFOHEADER_LEN )
		                            + ( hdr.biBitCount < 24 ? ( hdr.bfType == C_WIN ? 4 : 3 ) * hdr.biClrUsed : 0 );
		if ( hdr.bfOffBits < min_offbits ) {
			fprintf( stderr, "bmpLoad: %s - Illegal bfOffBits (underflow or overlaps header)\n", hdr.name );
			freeImage( image );
			zclose( zf );
			return NULL;
		}
		skip = hdr.bfOffBits - min_offbits;
	}

	while ( skip > 0 ) {
		if ( zgetc( zf ) == EOF ) {
			goto data_short;
		}
		skip = skip - 1;
	}

	/* Read the pixel data */
	if ( hdr.biBitCount == 1 ) {
		byte* data, pad[4];
		int illen, padlen, y;

		/* round bits up to byte */
		illen = ( image->width + 7 ) / 8;
		/* extra bytes to word boundary */
		padlen = ( ( ( image->width + 31 ) / 32 ) * 4 ) - illen;
		/* start at bottom */
		data = image->data + ( image->height - 1 ) * illen;
		y = image->height;
		while ( y > 0 ) {
			/* BMP files are left bit == ms bit,
			 * so read straight in.
			 */
			if ( zread( zf, data, illen ) != illen
			        || zread( zf, pad, padlen ) != padlen ) {
				goto data_short;
			}
			y = y - 1;
			data = data - illen;
		}
	} else if ( hdr.biBitCount == 4 ) {
		byte* data;
		unsigned int illen, x, y;

		illen = image->width;
		/* start at bottom */
		data = image->data + ( image->height - 1 ) * illen;

		if ( hdr.biCompression == BI_RLE4 ) {
			int d, e;
			memset( ( byte* ) image->data, 0, ( size_t )image->width * ( size_t )image->height );
			x = 0;
			y = 0;
			while ( 1 ) {
				int i, f;
				if ( ( d = zgetc( zf ) ) == EOF ) {
					goto data_short;
				}
				if ( d != 0 ) {	/* run of pixels */
					x = x + d;
					if ( x > image->width ||
					        y > image->height ) {
						/* don't run off buffer */
						data_bounds = TRUE;
						/* ignore this run */
						x = x - d;
						if ( ( e = zgetc( zf ) ) == EOF ) {
							goto data_short;
						}
						goto skip_iter_rle4;
					}
					if ( ( e = zgetc( zf ) ) == EOF ) {
						goto data_short;
					}
					f = e & 0xf;
					e = e >> 4;
					i = d / 2;
					while ( i > 0 ) {
						*( data ) = e;
						data = data + 1;
						*( data ) = f;
						data = data + 1;
						i = i - 1;
					}
					if ( d & 1 ) {
						*( data ) = e;
						data = data + 1;
					}
					goto skip_iter_rle4;
				}
				/* else code */
				if ( ( d = zgetc( zf ) ) == EOF ) {
					goto data_short;
				}
				if ( d == 0 ) {	/* end of line */
					data = data - ( x + illen );
					x = 0;
					y = y + 1;
					goto skip_iter_rle4;
				}
				/* else */
				if ( d == 1 ) {	/* end of bitmap */
					break;
				}
				/* else */
				if ( d == 2 ) {	/* delta */
					if ( ( d = zgetc( zf ) ) == EOF ||
					        ( e = zgetc( zf ) ) == EOF ) {
						goto data_short;
					}
					x = x + d;
					y = y + e;
					if ( x > image->width || y > image->height ) {
						data_bounds = TRUE;
						x = x - d;
						y = y - e;
						goto skip_iter_rle4;
					}
					data = data + d - ( e * illen );
					goto skip_iter_rle4;
				}
				/* else run of literals */
				x = x + d;
				if ( x > image->width || y > image->height ) {
					int btr;
					/* don't run off buffer */
					data_bounds = TRUE;
					x = x - d;		/* ignore this run */
					btr = d / 2 + ( d & 1 )
					      + ( ( ( d + 1 ) & 2 ) >> 1 );
					while ( btr > 0 ) {
						if ( ( e = zgetc( zf ) ) == EOF ) {
							goto data_short;
						}
						btr = btr - 1;
					}
					goto skip_iter_rle4;
				}
				i = d / 2;
				while ( i > 0 ) {
					if ( ( e = zgetc( zf ) ) == EOF ) {
						goto data_short;
					}
					*( data ) = e >> 4;
					data = data + 1;
					*( data ) = e & 0xf;
					data = data + 1;
					i = i - 1;
				}
				if ( d & 1 ) {
					if ( ( e = zgetc( zf ) ) == EOF ) {
						goto data_short;
					}
					*( data ) = e >> 4;
					data = data + 1;
				}
				if ( ( d + 1 ) & 2 ) {	/* read pad byte */
					if ( zgetc( zf ) == EOF ) {
						goto data_short;
					}
				}
skip_iter_rle4:
				;
			}
		} else {	/* No 4 bit rle compression */
			int d, s, p;
			int i, e;
			d = image->width / 2;	/* double pixel count */
			s = image->width & 1;	/* single pixel */
			p = ( 4 - ( d + s ) ) & 0x3;	/* byte pad */
			y = image->height;
			while ( y > 0 ) {
				i = d;
				while ( i > 0 ) {
					if ( ( e = zgetc( zf ) ) == EOF ) {
						goto data_short;
					}
					*( data ) = e >> 4;
					data = data + 1;
					*( data ) = e & 0xf;
					data = data + 1;
					i = i - 1;
				}
				if ( s ) {
					if ( ( e = zgetc( zf ) ) == EOF ) {
						goto data_short;
					}
					*( data ) = e >> 4;
					data = data + 1;
				}
				i = p;
				while ( i > 0 ) {
					if ( zgetc( zf ) == EOF ) {
						goto data_short;
					}
					i = i - 1;
				}
				y = y - 1;
				data = data - ( 2 * illen );
			}
		}
	} else if ( hdr.biBitCount == 8 ) {
		byte* data;
		int illen;
		unsigned int x, y;

		illen = image->width;
		/* start at bottom */
		data = image->data + ( image->height - 1 ) * illen;

		if ( hdr.biCompression == BI_RLE8 ) {
			int d, e;
			memset( ( byte* ) image->data, 0, ( size_t )image->width * ( size_t )image->height );
			x = 0;
			y = 0;
			while ( 1 ) {
				if ( ( d = zgetc( zf ) ) == EOF ) {
					goto data_short;
				}
				if ( d != 0 ) {	/* run of pixels */
					x = x + d;
					if ( x > image->width ||
					        y > image->height ) {
						/* don't run off buffer */
						data_bounds = TRUE;
						/* ignore this run */
						x = x - d;
						if ( ( e = zgetc( zf ) ) == EOF ) {
							goto data_short;
						}
						goto skip_iter_rle8;
					}
					if ( ( e = zgetc( zf ) ) == EOF ) {
						goto data_short;
					}
					memset( data, e, d );
					data = data + d;
					goto skip_iter_rle8;
				}
				/* else code */
				if ( ( d = zgetc( zf ) ) == EOF ) {
					goto data_short;
				}
				if ( d == 0 ) {	/* end of line */
					data = data - ( x + illen );
					x = 0;
					y = y + 1;
					goto skip_iter_rle8;
				}
				/* else */
				if ( d == 1 ) {	/* end of bitmap */
					break;
				}
				/* else */
				if ( d == 2 ) {	/* delta */
					if ( ( d = zgetc( zf ) ) == EOF ||
					        ( e = zgetc( zf ) ) == EOF ) {
						goto data_short;
					}
					x = x + d;
					y = y + e;
					if ( x > image->width || y > image->height ) {
						data_bounds = TRUE;
						x = x - d;
						y = y - e;
						goto skip_iter_rle8;
					}
					data = data + d - ( e * illen );
					goto skip_iter_rle8;
				}
				/* else run of literals */
				x = x + d;
				if ( x > image->width || y > image->height ) {
					int btr;
					/* don't run off buffer */
					data_bounds = TRUE;
					/* ignore this run */
					x = x - d;
					btr = d + ( d & 1 );
					while ( btr > 0 ) {
						if ( ( e = zgetc( zf ) ) == EOF ) {
							goto data_short;
						}
						btr = btr - 1;
					}
					goto skip_iter_rle8;
				}
				if ( zread( zf, data, d ) != d ) {
					goto data_short;
				}
				data = data + d;
				if ( d & 1 ) {	/* read pad byte */
					if ( zgetc( zf ) == EOF ) {
						goto data_short;
					}
				}
skip_iter_rle8:
				;
			}
		} else {	/* No 8 bit rle compression */
			byte pad[4];
			int padlen;

			/* extra bytes to word boundary */
			padlen = ( ( image->width + 3 ) & ~3 ) - illen;
			y = image->height;
			while ( y > 0 ) {
				if ( zread( zf, data, illen ) != illen
				        || zread( zf, pad, padlen ) != padlen ) {
					goto data_short;
				}
				y = y - 1;
				data = data - illen;
			}
		}
	} else {		/* hdr.biBitCount == 24 */
		byte* data, pad[4];
		int illen, padlen, y;
		illen = image->width * image->pixlen;
		/* extra bytes to word boundary */
		padlen = ( ( ( image->width * 3 ) + 3 ) & ~3 ) - illen;
		/* start at bottom */
		data = image->data + ( image->height - 1 ) * illen;
		y = image->height;
		while ( y > 0 ) {
			int i;

			/* BMP files are RGB, so read straight in. */
			if ( zread( zf, data, illen ) != illen
			        || zread( zf, pad, padlen ) != padlen ) {
				goto data_short;
			}
			/* Oh, no they're not */
			i = 3 * image->width - 1;
			while ( i > 0 ) {
				int t = data[i];
				data[i] = data[i - 2];
				data[i - 2] = t;
				i = i - 3;
			}
			y = y - 1;
			data = data - illen;
		}

	}
	if ( data_bounds ) {
		fprintf( stderr, "bmpLoad: %s - Data outside image area\n",
		         hdr.name );
	}
	zclose( zf );
	return image;

data_short:
	fprintf( stderr, "bmpLoad: %s - Short read within Data\n", hdr.name );
	zclose( zf );
	return image;
}