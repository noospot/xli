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
#include <math.h>

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

// d: get bit shift from bitmask
static inline unsigned int get_mask_shift( unsigned long mask )
{
	unsigned int shift = 0;
	if ( mask == 0 ) {
		return 0;
	}
	while ( ( mask & 1 ) == 0 ) {
		mask = mask >> 1;
		shift = shift + 1;
	}
	return shift;
}

// d: get bit width from bitmask
static inline unsigned int get_mask_bits( unsigned long mask )
{
	unsigned int bits = 0;
	if ( mask == 0 ) {
		return 0;
	}
	while ( ( mask & 1 ) == 0 ) {
		mask = mask >> 1;
	}
	while ( ( mask & 1 ) != 0 ) {
		mask = mask >> 1;
		bits = bits + 1;
	}
	return bits;
}

// d: apply alpha to a single color register
static inline byte colorreg_applyalpha( byte colorreg, byte alpha )
{
	if ( alpha == 0x00 ) {
		return colorreg;
	}
	if ( alpha == 0xff ) {
		return colorreg;
	}
	double tmp = ( double )( ( double )colorreg * ( double )alpha ) / 255.0;
	if ( tmp < 0.0 ) {
		tmp = 0.0;
	}
	if ( tmp > 255.0 ) {
		tmp = 255.0;
	}
	return ( byte )round( tmp );
}


/* Read the header of the file, and */
/* Return TRUE if this looks like a bmp file */
static bool read_bmpHeader( ZFILE* zf, bmpHeader* hp, strbyte* name )
{
	hp->name = name;
	byte infobuf[BMP_V5INFOHEADER_LEN];	/* largest we'll need */

	if ( zread( zf, infobuf, BMP_FILEHEADER_LEN ) != BMP_FILEHEADER_LEN ) {
		return FALSE;
	}

	if ( infobuf[0] != 'B' || infobuf[1] != 'M' ) {
		return FALSE;      /* bad magic number */
	}

	hp->bfSize = GULONG4( &infobuf[2] );
	hp->bfxHotSpot = GUINT2( &infobuf[6] );
	hp->bfyHotSpot = GUINT2( &infobuf[8] );
	hp->bfOffBits = GULONG4( &infobuf[10] );

	/* Read enough of the file info to figure the type out */
	if ( zread( zf, infobuf, 4 ) != 4 ) {
		return FALSE;
	}

	hp->biSize = GULONG4( &infobuf[0] );
	if ( hp->biSize == WIN_INFOHEADER_LEN ) {
		hp->bfType = C_WIN;
	} else if ( hp->biSize == OS2_INFOHEADER_LEN ) {
		hp->bfType = C_OS2;
	} else if ( hp->biSize == OS2_V2_INFOHEADER_LEN ) {
		hp->bfType = C_OS2_V2;
	} else if ( hp->biSize == BMP_V2INFOHEADER_LEN ) {
		hp->bfType = C_WIN_V2;
	} else if ( hp->biSize == BMP_V3INFOHEADER_LEN ) {
		hp->bfType = C_WIN_V3;
	} else if ( hp->biSize == BMP_V4INFOHEADER_LEN ) {
		hp->bfType = C_WIN_V4;
	} else if ( hp->biSize == BMP_V5INFOHEADER_LEN ) {
		hp->bfType = C_WIN_V5;
	} else {
		return FALSE;
	}

	if ( hp->bfType == C_OS2 ) {
		/* read in the rest of the info header for OS/2 1.x */
		if ( zread( zf, infobuf + 4, OS2_INFOHEADER_LEN - 4 )
		        != OS2_INFOHEADER_LEN - 4 ) {
			return FALSE;
		}

		hp->biWidth = GULONG2( &infobuf[4] );
		hp->biHeight = GSINT2( &infobuf[6] );
		hp->biPlanes = GUINT2( &infobuf[8] );
		hp->biBitCount = GUINT2( &infobuf[10] );
		hp->biCompression = BI_RGB;
		hp->biSizeImage = 0;
		hp->biXPelsPerMeter = 0;
		hp->biYPelsPerMeter = 0;
		hp->biClrUsed = 0;
		hp->biClrImportant = 0;
	} else {		/* Windows or OS/2 V2+ */
		if ( hp->biSize > BMP_V5INFOHEADER_LEN ) {
			return FALSE;
		}
		/* read in the rest of the info header */
		if ( zread( zf, infobuf + 4, hp->biSize - 4 )
		        != ( int )( hp->biSize - 4 ) ) {
			return FALSE;
		}

		hp->biWidth = GULONG4( &infobuf[4] );
		hp->biHeight = GSINT4( &infobuf[8] );
		hp->biPlanes = GUINT2( &infobuf[12] );
		hp->biBitCount = GUINT2( &infobuf[14] );
		hp->biCompression = GULONG4( &infobuf[16] );
		hp->biSizeImage = GULONG4( &infobuf[20] );
		hp->biXPelsPerMeter = GULONG4( &infobuf[24] );
		hp->biYPelsPerMeter = GULONG4( &infobuf[28] );
		hp->biClrUsed = GULONG4( &infobuf[32] );
		hp->biClrImportant = GULONG4( &infobuf[36] );
	}

	/* Read or extract bitmasks if BI_FIELDS */
	if ( hp->biCompression == BI_FIELDS ) {
		if ( hp->biSize == WIN_INFOHEADER_LEN ) {
			byte maskbuf[12];
			if ( zread( zf, maskbuf, 12 ) != 12 ) {
				return FALSE;
			}
			hp->redMask = GULONG4( &maskbuf[0] );
			hp->greenMask = GULONG4( &maskbuf[4] );
			hp->blueMask = GULONG4( &maskbuf[8] );
			hp->alphaMask = 0;
		} else {
			hp->redMask = GULONG4( &infobuf[40] );
			hp->greenMask = GULONG4( &infobuf[44] );
			hp->blueMask = GULONG4( &infobuf[48] );
			if ( hp->biSize >= BMP_V4INFOHEADER_LEN ) {
				hp->alphaMask = GULONG4( &infobuf[52] );
			} else {
				hp->alphaMask = 0;
			}
		}
		
	} else {
		if ( hp->biBitCount == 16 ) {
			hp->redMask = 0x7c00;
			hp->greenMask = 0x03e0;
			hp->blueMask = 0x001f;
			hp->alphaMask = 0;
		} else if ( hp->biBitCount == 32 ) {
			hp->redMask = 0x00ff0000;
			hp->greenMask = 0x0000ff00;
			hp->blueMask = 0x000000ff;
			hp->alphaMask = 0xff000000;
		} else {
			hp->redMask = 0;
			hp->greenMask = 0;
			hp->blueMask = 0;
			hp->alphaMask = 0;
		}
	}

	/* Check for file corruption and dimensions bounds */
	if ( hp->biWidth == 0 || hp->biHeight == 0 || hp->biHeight == ( int32_t )0x80000000 ) {
		fprintf( stderr, "bmpLoad: %s - Image dimension is 0 or invalid\n", hp->name );
		return FALSE;
	}	

	{
		unsigned long abs_width = hp->biWidth;
		unsigned long abs_height = ( hp->biHeight < 0 ) ? ( unsigned long )( -hp->biHeight ) : ( unsigned long )hp->biHeight;
		if ( abs_width > 65535 || abs_height > 65535 ) {
			fprintf( stderr, "bmpLoad: %s - Image dimensions exceed bounds\n", hp->name );
			return FALSE;
		}
	}

	if ( hp->biBitCount != 1
	        && hp->biBitCount != 2
	        && hp->biBitCount != 4
	        && hp->biBitCount != 8
	        && hp->biBitCount != 16
	        && hp->biBitCount != 24
	        && hp->biBitCount != 32
	        && hp->biBitCount != 64 ) {
		fprintf( stderr, "bmpLoad: %s - Illegal image BitCount\n", hp->name );
		return FALSE;
	}
	if ( ( hp->biCompression != BI_RGB
	        && hp->biCompression != BI_RLE8
	        && hp->biCompression != BI_RLE4
	        && hp->biCompression != BI_FIELDS
	        && hp->biCompression != BI_JPEG
	        && hp->biCompression != BI_PNG )
	        || ( hp->biCompression == BI_RLE8 && hp->biBitCount != 8 )
	        || ( hp->biCompression == BI_RLE4 && hp->biBitCount != 4 ) ) {
		fprintf( stderr, "bmpLoad: %s - Illegal image compression type\n", hp->name );
		return FALSE;
	}
	/* Top-down bitmaps cannot be RLE compressed */
	if ( hp->biHeight < 0 && ( hp->biCompression == BI_RLE8 || hp->biCompression == BI_RLE4 ) ) {
		fprintf( stderr, "bmpLoad: %s - Top-down RLE compression not supported\n", hp->name );
		return FALSE;
	}
	if ( hp->biPlanes != 1 ) {
		fprintf( stderr, "bmpLoad: %s - Illegal image Planes value\n", hp->name );
		return FALSE;
	}
	/* Fix up a few things */
	if ( hp->biBitCount <=8 ) {
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
	const strbyte* typelabel = "Windows";
	if ( hp->bfType == C_OS2 ) {
		typelabel = "OS/2";
	} else if ( hp->bfType == C_OS2_V2 ) {
		typelabel = "OS/2 V2";
	} else if ( hp->bfType == C_WIN_V2 ) {
		typelabel = "Windows V2";
	} else if ( hp->bfType == C_WIN_V3 ) {
		typelabel = "Windows V3";
	} else if ( hp->bfType == C_WIN_V4 ) {
		typelabel = "Windows V4";
	} else if ( hp->bfType == C_WIN_V5 ) {
		typelabel = "Windows V5";
	}

	const strbyte* colormode = ( hp->biBitCount <= 8 ) ? "palette" : "TrueColor";
	unsigned long abs_height = ( hp->biHeight < 0 ) ? ( unsigned long )( -hp->biHeight ) : ( unsigned long )hp->biHeight;
	const strbyte* topdown = ( hp->biHeight < 0 ) ? "topdown, " : "";

	const strbyte* complabel = "";
	if ( hp->biCompression == BI_RLE8 || hp->biCompression == BI_RLE4 ) {
		complabel = "Run length compressed, ";
	} else if ( hp->biCompression == BI_JPEG ) {
		complabel = "JPEG compressed, ";
	} else if ( hp->biCompression == BI_PNG ) {
		complabel = "PNG compressed, ";
	} else if ( hp->biCompression == BI_FIELDS ) {
		complabel = "Bitfields, ";
	} else if ( hp->biCompression == BI_RGB ) {
		complabel = "Uncompressed, ";
	}

	printf( "%s is a %lux%lu %d bit deep, %s%s%s %s BMP image\n", hp->name,
	        hp->biWidth, abs_height, hp->biBitCount, topdown, complabel, colormode,
	        typelabel );
}

// d: print detailed developer header information including decoded labels and flags
static void print_bmp_hdr( bmpHeader* hp )
{
	const strbyte* typelabel = "Windows";
	if ( hp->bfType == C_OS2 ) {
		typelabel = "OS/2";
	} else if ( hp->bfType == C_OS2_V2 ) {
		typelabel = "OS/2 V2";
	} else if ( hp->bfType == C_WIN_V2 ) {
		typelabel = "Windows V2";
	} else if ( hp->bfType == C_WIN_V3 ) {
		typelabel = "Windows V3";
	} else if ( hp->bfType == C_WIN_V4 ) {
		typelabel = "Windows V4";
	} else if ( hp->bfType == C_WIN_V5 ) {
		typelabel = "Windows V5";
	}

	const strbyte* colormode = ( hp->biBitCount <= 8 ) ? "palette" : "TrueColor";
	unsigned long abs_height = ( hp->biHeight < 0 ) ? ( unsigned long )( -hp->biHeight ) : ( unsigned long )hp->biHeight;
	const strbyte* topdown = ( hp->biHeight < 0 ) ? "top-down" : "bottom-up";

	const strbyte* complabel = "Unknown";
	if ( hp->biCompression == BI_RGB ) {
		complabel = "Uncompressed (BI_RGB)";
	} else if ( hp->biCompression == BI_RLE8 ) {
		complabel = "RLE 8-bit (BI_RLE8)";
	} else if ( hp->biCompression == BI_RLE4 ) {
		complabel = "RLE 4-bit (BI_RLE4)";
	} else if ( hp->biCompression == BI_FIELDS ) {
		complabel = "Bitfields (BI_FIELDS)";
	} else if ( hp->biCompression == BI_JPEG ) {
		complabel = "JPEG (BI_JPEG)";
	} else if ( hp->biCompression == BI_PNG ) {
		complabel = "PNG (BI_PNG)";
	}

	printf( " File Name name:                      %s\n", hp->name );
	printf( " File Type Identifier bfType:         %i (0x%04X) [%s]\n", hp->bfType, hp->bfType, typelabel );
	printf( " Total File Size bfSize:              %lu bytes\n", hp->bfSize );
	printf( " X Hot Spot bfxHotSpot:               %u\n", hp->bfxHotSpot );
	printf( " Y Hot Spot bfyHotSpot:               %u\n", hp->bfyHotSpot );
	printf( " Pixel Data Offset bfOffBits:         %lu bytes\n", hp->bfOffBits );
	printf( " Info Header Size biSize:             %lu bytes\n", hp->biSize );
	printf( " Image Width biWidth:                 %lu pixels\n", hp->biWidth );
	printf( " Image Height biHeight:               %i pixels (Abs: %lu, Orientation: %s)\n", hp->biHeight, abs_height, topdown );
	printf( " Color Planes biPlanes:               %u\n", hp->biPlanes );
	printf( " Bits Per Pixel biBitCount:           %u bits [%s]\n", hp->biBitCount, colormode );
	printf( " Compression Type biCompression:      %lu [%s]\n", hp->biCompression, complabel );
	printf( " Raw Image Data Size biSizeImage:     %lu bytes\n", hp->biSizeImage );
	printf( " Horizontal Resolution biXPels/Meter: %lu\n", hp->biXPelsPerMeter );
	printf( " Vertical Resolution biYPels/Meter:   %lu\n", hp->biYPelsPerMeter );
	printf( " Colormap Entries Used biClrUsed:     %lu\n", hp->biClrUsed );
	printf( " Important Colors biClrImportant:     %lu\n", hp->biClrImportant );
	printf( " Red Channel Mask redMask:            0x%08lx\n", hp->redMask );
	printf( " Green Channel Mask greenMask:        0x%08lx\n", hp->greenMask );
	printf( " Blue Channel Mask blueMask:          0x%08lx\n", hp->blueMask );
	printf( " Alpha Channel Mask alphaMask:        0x%08lx\n", hp->alphaMask );
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
	unsigned long abs_height;
	bool top_down;

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
		//print_bmp_hdr( &hdr );
	}
	znocache( zf );

	/* Report 16-bit and 64-bit files as identifiable but not displayable yet */
	if (  hdr.biBitCount == 64 ) {
		fprintf( stderr, "bmpLoad: %s - %d-bit BMP loading not supported yet\n", hdr.name, ( int )hdr.biBitCount );
		zclose( zf );
		return NULL;
	}

	top_down = ( hdr.biHeight < 0 );
	abs_height = top_down ? ( unsigned long )( -hdr.biHeight ) : ( unsigned long )hdr.biHeight;

	/* Create the appropriate image and colormap */
	if ( hdr.biBitCount <=8 ) {
		/* must be 1, 2, 4 or 8 bit mapped type */
		unsigned int i;
		int n = 4;
		byte buf[4];
		/* maximum number of colors */
		unsigned int used = ( 1 << hdr.biBitCount );

		if ( hdr.bfType == C_OS2 ) {
			n = 3;
		}

		if ( hdr.biBitCount == 1 ) {	/* bitmap */
			image = newBitImage( hdr.biWidth, abs_height );
		} else {
			image = newRGBImage( hdr.biWidth, abs_height,
			                     hdr.biBitCount );
		}
		image->title = dupString( hdr.name );

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
		image->rgb.used = used;
	} else {		/* else must be a true color image */
		image = newTrueImage( hdr.biWidth, abs_height );
		image->title = dupString( hdr.name );
	}

	/* Skip to offset specified in file header */
	{
		unsigned long color_entry_len = ( hdr.bfType == C_OS2 ) ? 3 : 4;
		unsigned long extra_header_len = 0;
		if ( hdr.biCompression == BI_FIELDS && hdr.biSize == WIN_INFOHEADER_LEN ) {
			extra_header_len = 12;
		}
		unsigned long min_offbits = BMP_FILEHEADER_LEN
		                            + hdr.biSize
		                            + extra_header_len
		                            + ( hdr.biBitCount < 24 ? color_entry_len * hdr.biClrUsed : 0 );
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
		int illen, padlen, rowcount;

		/* round bits up to byte */
		illen = ( image->width + 7 ) / 8;
		/* extra bytes to word boundary */
		padlen = ( ( ( image->width + 31 ) / 32 ) * 4 ) - illen;

		if ( top_down ) {
			data = image->data;
			rowcount = ( int )abs_height;
			while ( rowcount > 0 ) {
				if ( zread( zf, data, illen ) != illen
				        || zread( zf, pad, padlen ) != padlen ) {
					goto data_short;
				}
				rowcount = rowcount - 1;
				data = data + illen;
			}
		} else {
			data = image->data + ( abs_height - 1 ) * illen;
			rowcount = ( int )abs_height;
			while ( rowcount > 0 ) {
				if ( zread( zf, data, illen ) != illen
				        || zread( zf, pad, padlen ) != padlen ) {
					goto data_short;
				}
				rowcount = rowcount - 1;
				data = data - illen;
			}
		}
	} else if ( hdr.biBitCount == 2 ) {
		byte* data;
		unsigned int illen, padlen, rowcount, colidx, padbytes, packedbyte;

		illen = ( image->width * 2 + 7 ) / 8;
		padlen = ( ( ( image->width * 2 + 31 ) / 32 ) * 4 ) - illen;

		if ( top_down ) {
			data = image->data;
			rowcount = ( int )abs_height;
			while ( rowcount > 0 ) {
				colidx = 0;
				while ( colidx < image->width ) {
					packedbyte = zgetc( zf );
					if ( packedbyte == ( unsigned int )EOF ) {
						goto data_short;
					}
					if ( colidx < image->width ) {
						data[colidx] = ( packedbyte >> 6 ) & 0x3;
						colidx = colidx + 1;
					}
					if ( colidx < image->width ) {
						data[colidx] = ( packedbyte >> 4 ) & 0x3;
						colidx = colidx + 1;
					}
					if ( colidx < image->width ) {
						data[colidx] = ( packedbyte >> 2 ) & 0x3;
						colidx = colidx + 1;
					}
					if ( colidx < image->width ) {
						data[colidx] = packedbyte & 0x3;
						colidx = colidx + 1;
					}
				}
				padbytes = 0;
				while ( padbytes < padlen ) {
					if ( zgetc( zf ) == EOF ) {
						goto data_short;
					}
					padbytes = padbytes + 1;
				}
				rowcount = rowcount - 1;
				data = data + image->width;
			}
		} else {
			data = image->data + ( abs_height - 1 ) * image->width;
			rowcount = ( int )abs_height;
			while ( rowcount > 0 ) {
				colidx = 0;
				while ( colidx < image->width ) {
					packedbyte = zgetc( zf );
					if ( packedbyte == ( unsigned int )EOF ) {
						goto data_short;
					}
					if ( colidx < image->width ) {
						data[colidx] = ( packedbyte >> 6 ) & 0x3;
						colidx = colidx + 1;
					}
					if ( colidx < image->width ) {
						data[colidx] = ( packedbyte >> 4 ) & 0x3;
						colidx = colidx + 1;
					}
					if ( colidx < image->width ) {
						data[colidx] = ( packedbyte >> 2 ) & 0x3;
						colidx = colidx + 1;
					}
					if ( colidx < image->width ) {
						data[colidx] = packedbyte & 0x3;
						colidx = colidx + 1;
					}
				}
				padbytes = 0;
				while ( padbytes < padlen ) {
					if ( zgetc( zf ) == EOF ) {
						goto data_short;
					}
					padbytes = padbytes + 1;
				}
				rowcount = rowcount - 1;
				data = data - image->width;
			}
		}
	} else if ( hdr.biBitCount == 4 ) {
		byte* data;
		unsigned int illen, x, y;

		illen = image->width;

		if ( hdr.biCompression == BI_RLE4 ) {
			int d, e;
			memset( ( byte* ) image->data, 0, ( size_t )image->width * ( size_t )image->height );
			x = 0;
			y = 0;
			data = image->data + ( abs_height - 1 ) * illen;
			while ( 1 ) {
				int i, f;
				if ( ( d = zgetc( zf ) ) == EOF ) {
					goto data_short;
				}
				if ( d != 0 ) {	/* run of pixels */
					x = x + d;
					if ( x > image->width ||
					        y > abs_height ) {
						data_bounds = TRUE;
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
				if ( ( d = zgetc( zf ) ) == EOF ) {
					goto data_short;
				}
				if ( d == 0 ) {	/* end of line */
					data = data - ( x + illen );
					x = 0;
					y = y + 1;
					goto skip_iter_rle4;
				}
				if ( d == 1 ) {	/* end of bitmap */
					break;
				}
				if ( d == 2 ) {	/* delta */
					if ( ( d = zgetc( zf ) ) == EOF ||
					        ( e = zgetc( zf ) ) == EOF ) {
						goto data_short;
					}
					x = x + d;
					y = y + e;
					if ( x > image->width || y > abs_height ) {
						data_bounds = TRUE;
						x = x - d;
						y = y - e;
						goto skip_iter_rle4;
					}
					data = data + d - ( e * illen );
					goto skip_iter_rle4;
				}
				x = x + d;
				if ( x > image->width || y > abs_height ) {
					int btr;
					data_bounds = TRUE;
					x = x - d;
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
				if ( ( d + 1 ) & 2 ) {
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
			d = image->width / 2;
			s = image->width & 1;
			p = ( 4 - ( d + s ) ) & 0x3;

			if ( top_down ) {
				data = image->data;
				y = ( int )abs_height;
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
				}
			} else {
				data = image->data + ( abs_height - 1 ) * illen;
				y = ( int )abs_height;
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
		}
	} else if ( hdr.biBitCount == 8 ) {
		byte* data;
		int illen;
		unsigned int x, y;

		illen = image->width;

		if ( hdr.biCompression == BI_RLE8 ) {
			int d, e;
			memset( ( byte* ) image->data, 0, ( size_t )image->width * ( size_t )image->height );
			x = 0;
			y = 0;
			data = image->data + ( abs_height - 1 ) * illen;
			while ( 1 ) {
				if ( ( d = zgetc( zf ) ) == EOF ) {
					goto data_short;
				}
				if ( d != 0 ) {	/* run of pixels */
					x = x + d;
					if ( x > image->width ||
					        y > abs_height ) {
						data_bounds = TRUE;
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
				if ( ( d = zgetc( zf ) ) == EOF ) {
					goto data_short;
				}
				if ( d == 0 ) {	/* end of line */
					data = data - ( x + illen );
					x = 0;
					y = y + 1;
					goto skip_iter_rle8;
				}
				if ( d == 1 ) {	/* end of bitmap */
					break;
				}
				if ( d == 2 ) {	/* delta */
					if ( ( d = zgetc( zf ) ) == EOF ||
					        ( e = zgetc( zf ) ) == EOF ) {
						goto data_short;
					}
					x = x + d;
					y = y + e;
					if ( x > image->width || y > abs_height ) {
						data_bounds = TRUE;
						x = x - d;
						y = y - e;
						goto skip_iter_rle8;
					}
					data = data + d - ( e * illen );
					goto skip_iter_rle8;
				}
				x = x + d;
				if ( x > image->width || y > abs_height ) {
					int btr;
					data_bounds = TRUE;
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
				if ( d & 1 ) {
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

			padlen = ( ( image->width + 3 ) & ~3 ) - illen;
			if ( top_down ) {
				data = image->data;
				y = ( int )abs_height;
				while ( y > 0 ) {
					if ( zread( zf, data, illen ) != illen
					        || zread( zf, pad, padlen ) != padlen ) {
						goto data_short;
					}
					y = y - 1;
					data = data + illen;
				}
			} else {
				data = image->data + ( abs_height - 1 ) * illen;
				y = ( int )abs_height;
				while ( y > 0 ) {
					if ( zread( zf, data, illen ) != illen
					        || zread( zf, pad, padlen ) != padlen ) {
						goto data_short;
					}
					y = y - 1;
					data = data - illen;
				}
			}
		}
	} else if ( hdr.biBitCount == 24 ) {		/* hdr.biBitCount == 24 */
		byte* data, pad[4];
		int illen, padlen, y;
		illen = image->width * image->pixlen;
		padlen = ( ( ( image->width * 3 ) + 3 ) & ~3 ) - illen;

		if ( top_down ) {
			data = image->data;
			y = ( int )abs_height;
			while ( y > 0 ) {
				int i;
				if ( zread( zf, data, illen ) != illen
				        || zread( zf, pad, padlen ) != padlen ) {
					goto data_short;
				}
				i = 3 * image->width - 1;
				while ( i > 0 ) {
					int t = data[i];
					data[i] = data[i - 2];
					data[i - 2] = t;
					i = i - 3;
				}
				y = y - 1;
				data = data + illen;
			}
		} else {
			data = image->data + ( abs_height - 1 ) * illen;
			y = ( int )abs_height;
			while ( y > 0 ) {
				int i;
				if ( zread( zf, data, illen ) != illen
				        || zread( zf, pad, padlen ) != padlen ) {
					goto data_short;
				}
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
	} else if ( hdr.biBitCount == 16 ) {		/* hdr.biBitCount == 16 */
		byte* pixel_row, *src_ptr, *dst_ptr, pad[4];
		int illen, src_illen, src_padlen, y;
		long row_stride;
		unsigned int red_shift = get_mask_shift( hdr.redMask );
		unsigned int green_shift = get_mask_shift( hdr.greenMask );
		unsigned int blue_shift = get_mask_shift( hdr.blueMask );
		unsigned int alpha_shift = get_mask_shift( hdr.alphaMask );
		unsigned int red_bits = get_mask_bits( hdr.redMask );
		unsigned int green_bits = get_mask_bits( hdr.greenMask );
		unsigned int blue_bits = get_mask_bits( hdr.blueMask );
		unsigned int alpha_bits = get_mask_bits( hdr.alphaMask );

		illen = image->width * image->pixlen;
		src_illen = image->width * 2;
		src_padlen = ( ( src_illen + 3 ) & ~3 ) - src_illen;
		row_stride = top_down ? ( long )illen : -( long )illen;

		pixel_row = ( byte* )calloc( ( size_t )src_illen + 4, 1 );
		if ( pixel_row == NULL ) {
			fprintf( stderr, "bmpLoad: %s - Out of memory\n", hdr.name );
			freeImage( image );
			zclose( zf );
			return NULL;
		}

		dst_ptr = top_down ? image->data : image->data + ( abs_height - 1 ) * illen;
		y = ( int )abs_height;
		while ( y > 0 ) {
			if ( zread( zf, pixel_row, src_illen ) != src_illen
			        || ( src_padlen > 0 && zread( zf, pad, src_padlen ) != src_padlen ) ) {
				free( pixel_row );
				goto data_short;
			}
			{
				unsigned int x = 0;
				src_ptr = pixel_row;
				byte* row_dst_ptr = dst_ptr;
				while ( x < image->width ) {
					unsigned long pixelval = ( unsigned long )GUINT2( src_ptr );
					unsigned long r_val = red_bits > 0 ? ( ( pixelval & hdr.redMask ) >> red_shift ) : 0;
					unsigned long g_val = green_bits > 0 ? ( ( pixelval & hdr.greenMask ) >> green_shift ) : 0;
					unsigned long b_val = blue_bits > 0 ? ( ( pixelval & hdr.blueMask ) >> blue_shift ) : 0;
					unsigned long a_val = alpha_bits > 0 ? ( ( pixelval & hdr.alphaMask ) >> alpha_shift ) : 0;

					byte r = red_bits > 0 ? ( byte )( ( r_val * 255 ) / ( ( 1 << red_bits ) - 1 ) ) : 0;
					byte g = green_bits > 0 ? ( byte )( ( g_val * 255 ) / ( ( 1 << green_bits ) - 1 ) ) : 0;
					byte b = blue_bits > 0 ? ( byte )( ( b_val * 255 ) / ( ( 1 << blue_bits ) - 1 ) ) : 0;
					byte a = alpha_bits > 0 ? ( byte )( ( a_val * 255 ) / ( ( 1 << alpha_bits ) - 1 ) ) : 0xff;

					if ( alpha_bits == 0 || a == 0x00 ) {
						a = 0xff;
					}

					row_dst_ptr[0] = colorreg_applyalpha( r, a );
					row_dst_ptr[1] = colorreg_applyalpha( g, a );
					row_dst_ptr[2] = colorreg_applyalpha( b, a );

					src_ptr = src_ptr + 2;
					row_dst_ptr = row_dst_ptr + 3;
					x = x + 1;
				}
			}
			y = y - 1;
			dst_ptr = dst_ptr + row_stride;
		}
		free( pixel_row );
	} else if ( hdr.biBitCount == 32 ) {		/* hdr.biBitCount == 32 */
		byte* data, *src_row, pad[4];
		int illen, src_illen, src_padlen, y;
		long row_stride;
		unsigned int red_shift = get_mask_shift( hdr.redMask );
		unsigned int green_shift = get_mask_shift( hdr.greenMask );
		unsigned int blue_shift = get_mask_shift( hdr.blueMask );
		unsigned int alpha_shift = get_mask_shift( hdr.alphaMask );
		unsigned int red_bits = get_mask_bits( hdr.redMask );
		unsigned int green_bits = get_mask_bits( hdr.greenMask );
		unsigned int blue_bits = get_mask_bits( hdr.blueMask );
		unsigned int alpha_bits = get_mask_bits( hdr.alphaMask );

		illen = image->width * image->pixlen;
		src_illen = image->width * 4;
		src_padlen = ( ( src_illen + 3 ) & ~3 ) - src_illen;
		row_stride = top_down ? ( long )illen : -( long )illen;

		src_row = ( byte* )calloc( ( size_t )src_illen + 4, 1 );
		if ( src_row == NULL ) {
			fprintf( stderr, "bmpLoad: %s - Out of memory\n", hdr.name );
			freeImage( image );
			zclose( zf );
			return NULL;
		}

		data = top_down ? image->data : image->data + ( abs_height - 1 ) * illen;
		y = ( int )abs_height;
		while ( y > 0 ) {
			if ( zread( zf, src_row, src_illen ) != src_illen
			        || ( src_padlen > 0 && zread( zf, pad, src_padlen ) != src_padlen ) ) {
				free( src_row );
				goto data_short;
			}
			{
				unsigned int x = 0;
				byte* src_ptr = src_row;
				byte* dst_ptr = data;
				while ( x < image->width ) {
					unsigned long pixelval = GULONG4( src_ptr );
					unsigned long r_val = red_bits > 0 ? ( ( pixelval & hdr.redMask ) >> red_shift ) : 0;
					unsigned long g_val = green_bits > 0 ? ( ( pixelval & hdr.greenMask ) >> green_shift ) : 0;
					unsigned long b_val = blue_bits > 0 ? ( ( pixelval & hdr.blueMask ) >> blue_shift ) : 0;
					unsigned long a_val = alpha_bits > 0 ? ( ( pixelval & hdr.alphaMask ) >> alpha_shift ) : 0;

					byte r = red_bits > 0 ? ( byte )( ( r_val * 255 ) / ( ( 1 << red_bits ) - 1 ) ) : src_ptr[2];
					byte g = green_bits > 0 ? ( byte )( ( g_val * 255 ) / ( ( 1 << green_bits ) - 1 ) ) : src_ptr[1];
					byte b = blue_bits > 0 ? ( byte )( ( b_val * 255 ) / ( ( 1 << blue_bits ) - 1 ) ) : src_ptr[0];
					byte a = alpha_bits > 0 ? ( byte )( ( a_val * 255 ) / ( ( 1 << alpha_bits ) - 1 ) ) : src_ptr[3];

					if ( alpha_bits == 0 || a == 0x00 ) {
						a = 0xff;
					}

					dst_ptr[0] = colorreg_applyalpha( r, a );
					dst_ptr[1] = colorreg_applyalpha( g, a );
					dst_ptr[2] = colorreg_applyalpha( b, a );

					src_ptr = src_ptr + 4;
					dst_ptr = dst_ptr + 3;
					x = x + 1;
				}
			}
			y = y - 1;
			data = data + row_stride;
		}
		free( src_row );
	} else {
		
	}
	if ( data_bounds ) {
		fprintf( stderr, "bmpLoad: %s - Data outside image area\n", hdr.name );
	}
	zclose( zf );
	return image;

data_short:
	fprintf( stderr, "bmpLoad: %s - Short read within Data\n", hdr.name );
	zclose( zf );
	return image;
}