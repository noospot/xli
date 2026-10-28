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

	hp->bfFileSize = GULONG4( &infobuf[2] );
	hp->bfxHotSpot = GUINT2( &infobuf[6] );
	hp->bfyHotSpot = GUINT2( &infobuf[8] );
	hp->bfImageOffset = GULONG4( &infobuf[10] );

	/* Read enough of the file info to figure the type out */
	if ( zread( zf, infobuf, 4 ) != 4 ) {
		return FALSE;
	}

	hp->biInfoHeaderSize = GULONG4( &infobuf[0] );
	if ( hp->biInfoHeaderSize == WIN_INFOHEADER_LEN ) {
		hp->bfType = C_WIN;
	} else if ( hp->biInfoHeaderSize == OS2_INFOHEADER_LEN ) {
		hp->bfType = C_OS2;
	} else if ( hp->biInfoHeaderSize >= OS2_V2_INFOHEADER_MIN_LEN && hp->biInfoHeaderSize <= OS2_V2_INFOHEADER_MAX_LEN ) {
		hp->bfType = C_OS2_V2;
	} else if ( hp->biInfoHeaderSize == BMP_V2INFOHEADER_LEN ) {
		hp->bfType = C_WIN_V2;
	} else if ( hp->biInfoHeaderSize == BMP_V3INFOHEADER_LEN ) {
		hp->bfType = C_WIN_V3;
	} else if ( hp->biInfoHeaderSize == BMP_V4INFOHEADER_LEN ) {
		hp->bfType = C_WIN_V4;
	} else if ( hp->biInfoHeaderSize == BMP_V5INFOHEADER_LEN ) {
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
		hp->biPixelSize = GUINT2( &infobuf[10] );
		hp->biCompression = BI_RGB;
		hp->biSizeImage = 0;
		hp->biXPelsPerMeter = 0;
		hp->biYPelsPerMeter = 0;
		hp->biClrUsed = 0;
		hp->biClrImportant = 0;
	} else if ( hp->bfType == C_OS2_V2 && hp->biInfoHeaderSize < OS2_V2_INFOHEADER_MAX_LEN ) {
		/* read in the rest of the truncated info header for OS/2 2.x (16 to 63 bytes) */
		if ( zread( zf, infobuf + 4, hp->biInfoHeaderSize - 4 )
		        != ( int )( hp->biInfoHeaderSize - 4 ) ) {
			return FALSE;
		}

		hp->biWidth = ( hp->biInfoHeaderSize >= 8 ) ? GULONG4( &infobuf[4] ) : 0;
		hp->biHeight = ( hp->biInfoHeaderSize >= 12 ) ? GSINT4( &infobuf[8] ) : 0;
		hp->biPlanes = ( hp->biInfoHeaderSize >= 14 ) ? GUINT2( &infobuf[12] ) : 1;
		hp->biPixelSize = ( hp->biInfoHeaderSize >= 16 ) ? GUINT2( &infobuf[14] ) : 1;
		hp->biCompression = ( hp->biInfoHeaderSize >= 20 ) ? GULONG4( &infobuf[16] ) : BI_RGB;
		hp->biSizeImage = ( hp->biInfoHeaderSize >= 24 ) ? GULONG4( &infobuf[20] ) : 0;
		hp->biXPelsPerMeter = ( hp->biInfoHeaderSize >= 28 ) ? GULONG4( &infobuf[24] ) : 0;
		hp->biYPelsPerMeter = ( hp->biInfoHeaderSize >= 32 ) ? GULONG4( &infobuf[28] ) : 0;
		hp->biClrUsed = ( hp->biInfoHeaderSize >= 36 ) ? GULONG4( &infobuf[32] ) : 0;
		hp->biClrImportant = ( hp->biInfoHeaderSize >= 40 ) ? GULONG4( &infobuf[36] ) : 0;
		hp->redMask = 0;
		hp->greenMask = 0;
		hp->blueMask = 0;
		hp->alphaMask = 0;
	} else {		/* Windows or OS/2 V2+ full 64 */
		if ( hp->biInfoHeaderSize > BMP_V5INFOHEADER_LEN ) {
			return FALSE;
		}
		/* read in the rest of the info header */
		if ( zread( zf, infobuf + 4, hp->biInfoHeaderSize - 4 )
		        != ( int )( hp->biInfoHeaderSize - 4 ) ) {
			return FALSE;
		}

		hp->biWidth = GULONG4( &infobuf[4] );
		hp->biHeight = GSINT4( &infobuf[8] );
		hp->biPlanes = GUINT2( &infobuf[12] );
		hp->biPixelSize = GUINT2( &infobuf[14] );
		hp->biCompression = GULONG4( &infobuf[16] );
		hp->biSizeImage = GULONG4( &infobuf[20] );
		hp->biXPelsPerMeter = GULONG4( &infobuf[24] );
		hp->biYPelsPerMeter = GULONG4( &infobuf[28] );
		hp->biClrUsed = GULONG4( &infobuf[32] );
		hp->biClrImportant = GULONG4( &infobuf[36] );
	}

	/* Read or extract bitmasks if BI_FIELDS */
	if ( hp->biCompression == BI_FIELDS && hp->bfType != C_OS2 && hp->bfType != C_OS2_V2 ) {
		if ( hp->biInfoHeaderSize == WIN_INFOHEADER_LEN ) {
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
			if ( hp->biInfoHeaderSize >= BMP_V3INFOHEADER_LEN ) {
				hp->alphaMask = GULONG4( &infobuf[52] );
			} else {
				hp->alphaMask = 0;
			}
		}
		
	} else {
		if ( hp->biPixelSize == 16 ) {
			hp->redMask = 0x7c00;
			hp->greenMask = 0x03e0;
			hp->blueMask = 0x001f;
			hp->alphaMask = 0;
		} else if ( hp->biPixelSize == 32 ) {
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

	if ( hp->biPixelSize != 1
	        && hp->biPixelSize != 2
	        && hp->biPixelSize != 4
	        && hp->biPixelSize != 8
	        && hp->biPixelSize != 16
	        && hp->biPixelSize != 24
	        && hp->biPixelSize != 32
	        && hp->biPixelSize != 64 ) {
		fprintf( stderr, "bmpLoad: %s - image PixelSize error\n", hp->name );
		return FALSE;
	}
	if  ( hp->biCompression != BI_RGB
	        && hp->biCompression != BI_RLE8
	        && hp->biCompression != BI_RLE4
	        && hp->biCompression != BI_FIELDS  // !! also BI_HUFF1D
	        && hp->biCompression != BI_JPEG    // !! also BI_RLE24
	        && hp->biCompression != BI_PNG 
		&& hp->biCompression != BI_ALPHAFIELDS	) {
		fprintf( stderr, "bmpLoad: %s - image compression type error\n", hp->name );
		return FALSE;
	}

	if  ( ( hp->bfType == C_OS2_V2 && hp->biCompression == BI_HUFF1D && hp->biPixelSize != 1 ) 
	        || ( hp->biCompression == BI_RLE4 && hp->biPixelSize != 4 ) 	
		|| ( hp->biCompression == BI_RLE8 && hp->biPixelSize != 8 )
		|| ( hp->bfType == C_OS2_V2 && hp->biCompression == BI_RLE24 && hp->biPixelSize != 24 ) 
	) {
		fprintf( stderr, "bmpLoad: %s - image compression/depth mismatch error\n", hp->name );
		return FALSE;
	}
	
	/* top to bottom images cannot be RLE */
	if ( hp->biHeight < 0 && ( hp->biCompression == BI_RLE8 || hp->biCompression == BI_RLE4 ) ) {
		fprintf( stderr, "bmpLoad: %s - top to bottom with RLE not supported\n", hp->name );
		return FALSE;
	}
	if ( hp->biPlanes != 1 ) {
		fprintf( stderr, "bmpLoad: %s - image Planes value error\n", hp->name );
		return FALSE;
	}
	
	/* Calculate headers, masks, and palette lengths */
	{
		unsigned long color_entry_len = 4;
		unsigned long masks_header_len = 0;
		unsigned long all_headers_len = 0;
		unsigned long palette_len = 0;

		if ( hp->biCompression == BI_FIELDS && hp->biInfoHeaderSize == WIN_INFOHEADER_LEN ) {
			masks_header_len = 12;
		}

		all_headers_len = BMP_FILEHEADER_LEN + hp->biInfoHeaderSize + masks_header_len;
		palette_len = ( hp->bfImageOffset > all_headers_len ) ? ( hp->bfImageOffset - all_headers_len ) : 0;

		if ( hp->bfType == C_OS2 ) {
			color_entry_len = 3; /* OS/2 v1 always uses 3-byte RGBTRIPLE entries */
		} else if ( hp->bfType == C_OS2_V2 ) {
			/* OS/2 v2 dynamically permits either 3-byte or 4-byte entries */
			if ( palette_len > 0 && ( palette_len % 3 == 0 ) && ( palette_len / 3 <= ( 1UL << hp->biPixelSize ) ) ) {
				color_entry_len = 3;
				fprintf( stderr, "bmpLoad: %s - OS/2 v2 file handles 3-byte palette entries\n", hp->name );
			} else if ( palette_len > 0 && ( palette_len % 4 == 0 ) && ( palette_len / 4 <= ( 1UL << hp->biPixelSize ) ) ) {
				color_entry_len = 4;
			} else {
				color_entry_len = 3; /* Standard fallback for native OS/2 streams */
			}
		} else {
			color_entry_len = 4; /* Windows formats always use 4-byte RGBQUAD entries */
		}

		/* Determine number of colors used for indexed color modes */
		if ( hp->biPixelSize <= PAL_MAX ) {
			unsigned long max_colors = ( 1UL << hp->biPixelSize );
			
			/* try to find number of colors with distance between header and image data */
			if ( palette_len > 0 && color_entry_len > 0 && ( palette_len % color_entry_len == 0 ) ) {
				unsigned long colors_num = palette_len / color_entry_len;
				if ( colors_num <= max_colors ) {
					hp->biClrUsed = colors_num;
				} else {
					hp->biClrUsed = max_colors;
				}
			} 
			/* Fallback to header field or absolute bit-depth limit */
			else if ( hp->biClrUsed == 0 || hp->biClrUsed > max_colors ) {
				hp->biClrUsed = max_colors;
			}
		} else {
			hp->biClrUsed = 0; /* TrueColor/HighColor streams do not utilize a required palette */
		}

		hp->all_headers_len = all_headers_len;
		hp->masks_header_len = masks_header_len;
		hp->palette_len = palette_len;
		hp->color_entry_len = color_entry_len;
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

	const strbyte* colormode = ( hp->biPixelSize <= PAL_MAX ) ? "palette" : "TrueColor";
	unsigned long abs_height = ( hp->biHeight < 0 ) ? ( unsigned long )( -hp->biHeight ) : ( unsigned long )hp->biHeight;
	const strbyte* topbot = ( hp->biHeight < 0 ) ? "top-bot, " : "";

	const strbyte* complabel = "";
	if ( hp->biCompression == BI_RLE8 || hp->biCompression == BI_RLE4 ) {
		complabel = "RLE, ";
	} else if ( ( hp->bfType != C_OS2 && hp->bfType != C_OS2_V2 ) && hp->biCompression == BI_JPEG ) {
		complabel = "includes JPEG, ";
	} else if ( ( hp->bfType == C_OS2 || hp->bfType == C_OS2_V2 ) && hp->biCompression == BI_RLE24 ) {
		complabel = "RLE24, ";			
	} else if ( hp->biCompression == BI_PNG ) {
		complabel = "includes PNG, ";
	} else if ( ( hp->bfType != C_OS2 && hp->bfType != C_OS2_V2 ) && hp->biCompression == BI_FIELDS ) {
		complabel = "Fields Masks, ";
	} else if ( ( hp->bfType == C_OS2 || hp->bfType == C_OS2_V2 ) && hp->biCompression == BI_HUFF1D ) {
		complabel = "Huff1d, ";	
	} else if (  hp->biCompression == BI_ALPHAFIELDS ) {
		complabel = "Alpha fields Masks, ";		
	} else if ( hp->biCompression == BI_RGB ) {
		complabel = "";
	}

	printf( "%s is a %lux%lu %d bit deep, %s%s%s %s BMP image\n", hp->name,
	        hp->biWidth, abs_height, hp->biPixelSize, topbot, complabel, colormode,
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

	const strbyte* colormode = ( hp->biPixelSize <= PAL_MAX ) ? "palette" : "TrueColor";
	unsigned long abs_height = ( hp->biHeight < 0 ) ? ( unsigned long )( -hp->biHeight ) : ( unsigned long )hp->biHeight;
	const strbyte* topbot = ( hp->biHeight < 0 ) ? "top-bot" : "bot-top";


	const strbyte* complabel = "";
	if ( hp->biCompression == BI_RLE8 || hp->biCompression == BI_RLE4 ) {
		complabel = "RLE4 ";
	} else if (hp->biCompression == BI_RLE8 ) {
		complabel = "RLE8 ";
	} else if ( ( hp->bfType != C_OS2 && hp->bfType != C_OS2_V2 ) && hp->biCompression == BI_JPEG ) {
		complabel = "JPEG ";
	} else if ( ( hp->bfType == C_OS2 || hp->bfType == C_OS2_V2 ) && hp->biCompression == BI_RLE24 ) {
		complabel = "RLE24 ";			
	} else if ( hp->biCompression == BI_PNG ) {
		complabel = "PNG ";
	} else if ( ( hp->bfType != C_OS2 && hp->bfType != C_OS2_V2 ) && hp->biCompression == BI_FIELDS ) {
		complabel = "Fields Masks ";
	} else if ( ( hp->bfType == C_OS2 || hp->bfType == C_OS2_V2 ) && hp->biCompression == BI_HUFF1D ) {
		complabel = "Huff1d ";	
	} else if (  hp->biCompression == BI_ALPHAFIELDS ) {
		complabel = "Alpha fields Masks ";		
	} else if ( hp->biCompression == BI_RGB ) {
		complabel = "None ";
	}

	printf( " File Name name:                      %s\n", hp->name );
	printf( " File Type Identifier bfType:         %i (0x%04X) [%s]\n", hp->bfType, hp->bfType, typelabel );
	printf( " Total File Size bfFileSize:          %lu bytes\n", hp->bfFileSize );
	printf( " X Hot Spot bfxHotSpot:               %u\n", hp->bfxHotSpot );
	printf( " Y Hot Spot bfyHotSpot:               %u\n", hp->bfyHotSpot );
	printf( " Pixel Data Offset bfImageOffset:     %lu bytes\n", hp->bfImageOffset );
	printf( " Info Header Size biInfoHeaderSize:   %lu bytes\n", hp->biInfoHeaderSize );
	printf( " Image Width biWidth:                 %lu pixels\n", hp->biWidth );
	printf( " Image Height biHeight:               %i pixels (Abs: %lu, Orientation: %s)\n", hp->biHeight, abs_height, topbot );
	printf( " Color Planes biPlanes:               %u\n", hp->biPlanes );
	printf( " Bits Per Pixel biPixelSize:          %u bits [%s]\n", hp->biPixelSize, colormode );
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
	bool top_bot;

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
	if ( globals._Xdebug ) {
		print_bmp_hdr( &hdr );	
	}
	znocache( zf );

	/* Report  64-bit files as identifiable but not displayable yet */
	if (  hdr.biPixelSize == 64 ) {
		fprintf( stderr, "bmpLoad: %s - %d-bit BMP loading not supported\n", hdr.name, ( int )hdr.biPixelSize );
		zclose( zf );
		return NULL;
	}

	top_bot = ( hdr.biHeight < 0 );
	abs_height = top_bot ? ( unsigned long )( -hdr.biHeight ) : ( unsigned long )hdr.biHeight;

	/* Create the appropriate image and colormap */
	if ( hdr.biPixelSize <= PAL_MAX ) {
		/* must be 1, 2, 4 or 8 bit mapped type */
		unsigned int i;
		int n = ( int )hdr.color_entry_len;
		byte buf[4];
		/* maximum number of colors */
		unsigned int used = ( 1 << hdr.biPixelSize );

		if ( hdr.biPixelSize == 1 ) {	/* bitmap */
			image = newBitImage( hdr.biWidth, abs_height );
		} else {
			image = newRGBImage( hdr.biWidth, abs_height,
			                     hdr.biPixelSize );
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

	/* find amount of bytes to Skip to image offset */
	{
		unsigned long palette_bytes;

		if ( hdr.biPixelSize <= PAL_MAX ) {
			palette_bytes = hdr.color_entry_len * hdr.biClrUsed;
		} else {
			palette_bytes = 0;
		}

		unsigned long min_pixel_offset = hdr.all_headers_len + palette_bytes;

		if ( hdr.bfImageOffset == 0 ) {
			hdr.bfImageOffset = min_pixel_offset;
		}

		if ( hdr.bfImageOffset < hdr.all_headers_len ) {
			fprintf( stderr, "bmpLoad: %s - bfImageOffset underflow or overlaps header\n", hdr.name );
			freeImage( image );
			zclose( zf );
			return NULL;
		}

		if ( hdr.biPixelSize <= PAL_MAX && hdr.bfImageOffset < min_pixel_offset ) {
			unsigned long available_palette_bytes = hdr.bfImageOffset - hdr.all_headers_len;
			if ( available_palette_bytes > 0 && ( available_palette_bytes % hdr.color_entry_len ) == 0 ) {
				hdr.biClrUsed = available_palette_bytes / hdr.color_entry_len;
				min_pixel_offset = hdr.bfImageOffset;
			} else {
				fprintf( stderr, "bmpLoad: %s - bfImageOffset overlaps header or invalid palette size\n", hdr.name );
				freeImage( image );
				zclose( zf );
				return NULL;
			}
		}

		skip = hdr.bfImageOffset - min_pixel_offset;
	}
	/* Skip to image offset, specified in file header */
	while ( skip > 0 ) {
		if ( zgetc( zf ) == EOF ) {
			goto data_short;
		}
		skip = skip - 1;
	}

	/* Read the pixel data */
	if ( hdr.biPixelSize == 1 ) {
		byte* data, pad[4];
		int illen, padlen, rowcount;
		long row_stride;

		/* round bits up to byte */
		illen = ( image->width + 7 ) / 8;
		/* extra bytes to word boundary */
		padlen = ( ( ( image->width + 31 ) / 32 ) * 4 ) - illen;
		row_stride = top_bot ? ( long )illen : -( long )illen;
		data = top_bot ? image->data : image->data + ( abs_height - 1 ) * illen;
		rowcount = ( int )abs_height;

		while ( rowcount > 0 ) {
			if ( zread( zf, data, illen ) != illen
			        || zread( zf, pad, padlen ) != padlen ) {
				goto data_short;
			}
			rowcount = rowcount - 1;
			data = data + row_stride;
		}
	} else if ( hdr.biPixelSize == 2 ) {
		byte* data;
		unsigned int illen, padlen, rowcount, i, padbytes, packedbyte;
		long row_stride;

		illen = ( image->width * 2 + 7 ) / 8;
		padlen = ( ( ( image->width * 2 + 31 ) / 32 ) * 4 ) - illen;
		row_stride = top_bot ? ( long )image->width : -( long )image->width;
		data = top_bot ? image->data : image->data + ( abs_height - 1 ) * image->width;
		rowcount = ( int )abs_height;

		while ( rowcount > 0 ) {
			i = 0;
			while ( i < image->width ) {
				packedbyte = zgetc( zf );
				if ( packedbyte == ( unsigned int )EOF ) {
					goto data_short;
				}
				if ( i < image->width ) {
					data[i] = ( packedbyte >> 6 ) & 0x3;
					i = i + 1;
				}
				if ( i < image->width ) {
					data[i] = ( packedbyte >> 4 ) & 0x3;
					i = i + 1;
				}
				if ( i < image->width ) {
					data[i] = ( packedbyte >> 2 ) & 0x3;
					i = i + 1;
				}
				if ( i < image->width ) {
					data[i] = packedbyte & 0x3;
					i = i + 1;
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
			data = data + row_stride;
		}
	} else if ( hdr.biPixelSize == 4 ) {
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
					unsigned int start_x = x;
					unsigned int run_len = ( unsigned int )d;
					x = x + run_len;
					if ( y > abs_height ) {
						data_bounds = TRUE;
						goto skip_iter_rle4;
					}
					if ( x > image->width ) {
						if ( start_x < image->width ) {
							run_len = image->width - start_x;
						} else {
							run_len = 0;
						}
						x = image->width;
					}
					if ( ( e = zgetc( zf ) ) == EOF ) {
						goto data_short;
					}
					f = e & 0xf;
					e = e >> 4;
					i = ( int )( run_len / 2 );
					while ( i > 0 ) {
						*( data ) = e;
						data = data + 1;
						*( data ) = f;
						data = data + 1;
						i = i - 1;
					}
					if ( run_len & 1 ) {
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
				/* Absolute / unencoded run (d >= 3) */
				{
					unsigned int start_x = x;
					unsigned int run_len = ( unsigned int )d;
					int total_bytes = d / 2 + ( d & 1 ) + ( ( ( d + 1 ) & 2 ) >> 1 );
					int bytes_read = 0;

					x = x + run_len;
					if ( y > abs_height ) {
						data_bounds = TRUE;
						goto skip_iter_rle4;
					}
					if ( x > image->width ) {
						if ( start_x < image->width ) {
							run_len = image->width - start_x;
						} else {
							run_len = 0;
						}
						x = image->width;
					}

					i = ( int )( run_len / 2 );
					while ( i > 0 ) {
						if ( ( e = zgetc( zf ) ) == EOF ) {
							goto data_short;
						}
						bytes_read = bytes_read + 1;
						*( data ) = e >> 4;
						data = data + 1;
						*( data ) = e & 0xf;
						data = data + 1;
						i = i - 1;
					}
					if ( run_len & 1 ) {
						if ( ( e = zgetc( zf ) ) == EOF ) {
							goto data_short;
						}
						bytes_read = bytes_read + 1;
						*( data ) = e >> 4;
						data = data + 1;
					}

					while ( bytes_read < total_bytes ) {
						if ( zgetc( zf ) == EOF ) {
							goto data_short;
						}
						bytes_read = bytes_read + 1;
					}
				}
skip_iter_rle4:
				;
			}
		} else {	/* No 4 bit rle compression */
			int d, s, p;
			int i, e;
			byte* row_ptr;
			long row_stride;
			d = image->width / 2;
			s = image->width & 1;
			p = ( 4 - ( d + s ) ) & 0x3;

			row_stride = top_bot ? ( long )illen : -( long )illen;
			row_ptr = top_bot ? image->data : image->data + ( abs_height - 1 ) * illen;
			y = ( int )abs_height;

			while ( y > 0 ) {
				byte* row_data = row_ptr;
				i = d;
				while ( i > 0 ) {
					if ( ( e = zgetc( zf ) ) == EOF ) {
						goto data_short;
					}
					*( row_data ) = e >> 4;
					row_data = row_data + 1;
					*( row_data ) = e & 0xf;
					row_data = row_data + 1;
					i = i - 1;
				}
				if ( s ) {
					if ( ( e = zgetc( zf ) ) == EOF ) {
						goto data_short;
					}
					*( row_data ) = e >> 4;
					row_data = row_data + 1;
				}
				i = p;
				while ( i > 0 ) {
					if ( zgetc( zf ) == EOF ) {
						goto data_short;
					}
					i = i - 1;
				}
				y = y - 1;
				row_ptr = row_ptr + row_stride;
			}
		}
	} else if ( hdr.biPixelSize == 8 ) {
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
					unsigned int start_x = x;
					unsigned int run_len = ( unsigned int )d;
					x = x + run_len;
					if ( y > abs_height ) {
						data_bounds = TRUE;
						goto skip_iter_rle8;
					}
					if ( x > image->width ) {
						if ( start_x < image->width ) {
							run_len = image->width - start_x;
						} else {
							run_len = 0;
						}
						x = image->width;
					}
					if ( ( e = zgetc( zf ) ) == EOF ) {
						goto data_short;
					}
					memset( data, e, run_len );
					data = data + run_len;
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
					int delta_x, delta_y;
					if ( ( delta_x = zgetc( zf ) ) == EOF ||
					        ( delta_y = zgetc( zf ) ) == EOF ) {
						goto data_short;
					}
					x = x + delta_x;
					y = y + delta_y;
					if ( x > image->width || y > abs_height ) {
						data_bounds = TRUE;
						x = x - delta_x;
						y = y - delta_y;
						goto skip_iter_rle8;
					}
					data = data + delta_x - ( delta_y * illen );
					goto skip_iter_rle8;
				}
				/* Absolute / unencoded run (d >= 3) */
				{
					unsigned int start_x = x;
					unsigned int run_len = ( unsigned int )d;
					int total_bytes = d + ( d & 1 );
					int bytes_read = 0;

					x = x + run_len;
					if ( y > abs_height ) {
						data_bounds = TRUE;
						goto skip_iter_rle8;
					}
					if ( x > image->width ) {
						if ( start_x < image->width ) {
							run_len = image->width - start_x;
						} else {
							run_len = 0;
						}
						x = image->width;
					}

					while ( bytes_read < ( int )run_len ) {
						int pixel_val = zgetc( zf );
						if ( pixel_val == EOF ) {
							goto data_short;
						}
						*( data ) = ( byte )pixel_val;
						data = data + 1;
						bytes_read = bytes_read + 1;
					}

					while ( bytes_read < total_bytes ) {
						if ( zgetc( zf ) == EOF ) {
							goto data_short;
						}
						bytes_read = bytes_read + 1;
					}
				}
skip_iter_rle8:
				;
			}
		} else {	/* No 8 bit rle compression */
			byte pad[4];
			int padlen;
			long row_stride;

			padlen = ( ( image->width + 3 ) & ~3 ) - illen;
			row_stride = top_bot ? ( long )illen : -( long )illen;
			data = top_bot ? image->data : image->data + ( abs_height - 1 ) * illen;
			y = ( int )abs_height;

			while ( y > 0 ) {
				if ( zread( zf, data, illen ) != illen
				        || zread( zf, pad, padlen ) != padlen ) {
					goto data_short;
				}
				y = y - 1;
				data = data + row_stride;
			}
		}
	} else if ( hdr.biPixelSize == 24 ) {		/* hdr.biPixelSize == 24 */
		byte* data, pad[4];
		int illen; 

		illen = image->width * image->pixlen;
		
		if ( hdr.biCompression == BI_RLE24 ) {
			int d;
			unsigned int x,y;
			byte triplet[3];
			memset( ( byte* ) image->data, 0, ( size_t )image->width * ( size_t )image->height * ( size_t )image->pixlen );
			x = 0;
			y = 0;
			data = image->data + ( abs_height - 1 ) * illen;
			while ( 1 ) {
				int i;
				if ( ( d = zgetc( zf ) ) == EOF ) {
					goto data_short;
				}
				if ( d != 0 ) {	/* run of pixels */
					unsigned int start_x = x;
					unsigned int run_len = ( unsigned int )d;
					x = x + run_len;
					if ( y > abs_height ) {
						data_bounds = TRUE;
						goto skip_iter_rle24;
					}
					if ( x > image->width ) {
						if ( start_x < image->width ) {
							run_len = image->width - start_x;
						} else {
							run_len = 0;
						}
						x = image->width;
					}
					if ( zread( zf, triplet, 3 ) != 3 ) {
						goto data_short;
					}
					i = ( int )run_len;
					while ( i > 0 ) {
						*( data + 0 ) = triplet[2];
						*( data + 1 ) = triplet[1];
						*( data + 2 ) = triplet[0];
						data = data + 3;
						i = i - 1;
					}
					goto skip_iter_rle24;
				}
				if ( ( d = zgetc( zf ) ) == EOF ) {
					goto data_short;
				}
				if ( d == 0 ) {	/* end of line */
					data = data - ( ( x * image->pixlen ) + illen );
					x = 0;
					y = y + 1;
					goto skip_iter_rle24;
				}
				if ( d == 1 ) {	/* end of bitmap */
					break;
				}
				if ( d == 2 ) {	/* delta */
					int delta_x, delta_y;
					if ( ( delta_x = zgetc( zf ) ) == EOF ||
					        ( delta_y = zgetc( zf ) ) == EOF ) {
						goto data_short;
					}
					x = x + delta_x;
					y = y + delta_y;
					if ( x > image->width || y > abs_height ) {
						data_bounds = TRUE;
						x = x - delta_x;
						y = y - delta_y;
						goto skip_iter_rle24;
					}
					data = data + ( delta_x * image->pixlen ) - ( delta_y * illen );
					goto skip_iter_rle24;
				}
				/* Absolute / unencoded run (d >= 3) */
				{
					unsigned int start_x = x;
					unsigned int run_len = ( unsigned int )d;
					int total_bytes = d * 3;
					int padded_bytes = ( total_bytes + 1 ) & ~1;
					int bytes_read = 0;

					x = x + run_len;
					if ( y > abs_height ) {
						data_bounds = TRUE;
						goto skip_iter_rle24;
					}
					if ( x > image->width ) {
						if ( start_x < image->width ) {
							run_len = image->width - start_x;
						} else {
							run_len = 0;
						}
						x = image->width;
					}

					i = ( int )run_len;
					while ( i > 0 ) {
						if ( zread( zf, triplet, 3 ) != 3 ) {
							goto data_short;
						}
						bytes_read = bytes_read + 3;
						*( data + 0 ) = triplet[2];
						*( data + 1 ) = triplet[1];
						*( data + 2 ) = triplet[0];
						data = data + 3;
						i = i - 1;
					}

					while ( bytes_read < padded_bytes ) {
						if ( zgetc( zf ) == EOF ) {
							goto data_short;
						}
						bytes_read = bytes_read + 1;
					}
				}
skip_iter_rle24:
				;
			}
		} else {                  // no rle24
		
			int padlen;
			int y;
			long row_stride;
			padlen = ( ( ( image->width * 3 ) + 3 ) & ~3 ) - illen;
			row_stride = top_bot ? ( long )illen : -( long )illen;
			data = top_bot ? image->data : image->data + ( abs_height - 1 ) * illen;
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
				data = data + row_stride;
			}
		}
	} else if ( hdr.biPixelSize == 16 ) {		/* hdr.biPixelSize == 16 */
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
		row_stride = top_bot ? ( long )illen : -( long )illen;

		pixel_row = ( byte* )calloc( ( size_t )src_illen + 4, 1 );
		if ( pixel_row == NULL ) {
			fprintf( stderr, "bmpLoad: %s - Out of memory\n", hdr.name );
			freeImage( image );
			zclose( zf );
			return NULL;
		}

		dst_ptr = top_bot ? image->data : image->data + ( abs_height - 1 ) * illen;
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
	} else if ( hdr.biPixelSize == 32 ) {		/* hdr.biPixelSize == 32 */
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
		row_stride = top_bot ? ( long )illen : -( long )illen;

		src_row = ( byte* )calloc( ( size_t )src_illen + 4, 1 );
		if ( src_row == NULL ) {
			fprintf( stderr, "bmpLoad: %s - Out of memory\n", hdr.name );
			freeImage( image );
			zclose( zf );
			return NULL;
		}

		data = top_bot ? image->data : image->data + ( abs_height - 1 ) * illen;
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