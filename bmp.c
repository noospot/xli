//
// Read a Microsoft/IBM BMP file.
//
// Created for xli by Graeme Gill,
//
// Based on tga.c, and guided by the Microsoft file format
// description, and bmptoppm.c by David W. Sanderson.
//
//
// 20261019-20261031 rno/noospot
//
//  TODO
//   - find and report if data between header,palette and image
//   - rgb32 with alpha,  images with invisible pixels, colors with 0x00 alpha
//   - keep file found masks and resolved mask in distinct variables
//   - use fixed width types
//   - cleanup and optimise 
//   - support OS/2 files with magics BA CI CP IC PT
//   - support bmp v4 v5 and OS/2 extra fields, load gamma value

#include <sys/stat.h>
#include <math.h>

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
static inline unsigned int get_mask_factors( unsigned long mask )
{
	unsigned int factor = 0;
	if ( mask == 0 ) {
		return 0;
	}
	while ( ( mask & 1 ) == 0 ) {
		mask = mask >> 1;
	}
	while ( ( mask & 1 ) != 0 ) {
		mask = mask >> 1;
		factor = factor + 1;
	}
	return factor;
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


// Read the header of the file, and
// Return TRUE if this looks like a bmp file
static bool read_bmpHeader( ZFILE* zf, bmpHeader* hp, strbyte* name )
{
	hp->name = name;
	byte infobuf[BMP_V5INFOHEADER_LEN];	// largest we'll need

	// get file header
	if ( zread( zf, infobuf, BMP_FILEHEADER_LEN ) != BMP_FILEHEADER_LEN ) {
		return FALSE;
	}

	if ( infobuf[0] != 'B' || infobuf[1] != 'M' ) {
		return FALSE;      // bad magic number
	}

	hp->bfFileSize = GULONG4( &infobuf[2] );
	hp->bfxHotSpot = GUINT2( &infobuf[6] );
	hp->bfyHotSpot = GUINT2( &infobuf[8] );
	hp->bfImageOffset = GULONG4( &infobuf[10] );

	// Read enough of info header to figure the type out 
	if ( zread( zf, infobuf, 4 ) != 4 ) {
		return FALSE;
	}
	
	// type
	hp->biInfoHeaderSize = GULONG4( &infobuf[0] );
	if ( hp->biInfoHeaderSize == WIN_INFOHEADER_LEN ) {
		hp->bfType = C_WIN;
	} else if ( hp->biInfoHeaderSize == OS2_INFOHEADER_LEN ) {
		hp->bfType = C_OS2;
	} else if ( hp->biInfoHeaderSize == BMP_V2INFOHEADER_LEN ) {
		hp->bfType = C_WIN_V2; // **FIXME** also could be OS/2 v2
	} else if ( hp->biInfoHeaderSize == BMP_V3INFOHEADER_LEN ) {
		hp->bfType = C_WIN_V3; // **FIXME** also could be OS/2 v2		
	} else if ( hp->biInfoHeaderSize >= OS2_V2_INFOHEADER_MIN_LEN 
	       &&   hp->biInfoHeaderSize <= OS2_V2_INFOHEADER_MAX_LEN ) {
		hp->bfType = C_OS2_V2;
	} else if ( hp->biInfoHeaderSize == BMP_V4INFOHEADER_LEN ) {
		hp->bfType = C_WIN_V4;
	} else if ( hp->biInfoHeaderSize == BMP_V5INFOHEADER_LEN ) {
		hp->bfType = C_WIN_V5;
	} else {
		return FALSE;
	}
	
	// read in the rest of info header depending on type
	if ( hp->bfType == C_OS2 ) {  
		// OS/2 v1.x
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
		// OS/2 v2.x, 16 to 63 bytes
		if ( zread( zf, infobuf + 4, hp->biInfoHeaderSize - 4 )
		        != ( int )( hp->biInfoHeaderSize - 4 ) ) {
			return FALSE;
		}

		if ( hp->biInfoHeaderSize >= 8 ) {
			hp->biWidth = GULONG4( &infobuf[4] );
		} else {
			hp->biWidth = 0;
		}
		if ( hp->biInfoHeaderSize >= 12 ) {
			hp->biHeight = GSINT4( &infobuf[8] );
		} else {
			hp->biHeight = 0;
		}
		if ( hp->biInfoHeaderSize >= 14 ) {
			hp->biPlanes = GUINT2( &infobuf[12] );
		} else {
			hp->biPlanes = 1;
		}
		if ( hp->biInfoHeaderSize >= 16 ) {
			hp->biPixelSize = GUINT2( &infobuf[14] );
		} else {
			hp->biPixelSize = 1;
		}
		if ( hp->biInfoHeaderSize >= 20 ) {
			hp->biCompression = GULONG4( &infobuf[16] );
		} else {
			hp->biCompression = BI_RGB;
		}
		if ( hp->biInfoHeaderSize >= 24 ) {
			hp->biSizeImage = GULONG4( &infobuf[20] );
		} else {
			hp->biSizeImage = 0;
		}
		if ( hp->biInfoHeaderSize >= 28 ) {
			hp->biXPelsPerMeter = GULONG4( &infobuf[24] );
		} else {
			hp->biXPelsPerMeter = 0;
		}
		if ( hp->biInfoHeaderSize >= 32 ) {
			hp->biYPelsPerMeter = GULONG4( &infobuf[28] );
		} else {
			hp->biYPelsPerMeter = 0;
		}
		if ( hp->biInfoHeaderSize >= 36 ) {
			hp->biClrUsed = GULONG4( &infobuf[32] );
		} else {
			hp->biClrUsed = 0;
		}
		if ( hp->biInfoHeaderSize >= 40 ) {
			hp->biClrImportant = GULONG4( &infobuf[36] );
		} else {
			hp->biClrImportant = 0;
		}
		hp->redMask = 0;
		hp->greenMask = 0;
		hp->blueMask = 0;
		hp->alphaMask = 0; 
	} else {		
		// Windows or OS/2 V2+ full 64
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
	
	// PNG compression could not have image dimensions in bmp header
	if ( hp->biCompression == BI_PNG  ) {
		fprintf( stderr, "bmpLoad: %s - PNG compression not supported\n", hp->name );
		return FALSE;
	}
	// JPEG compression could not have image dimensions in bmp header
	if (  hp->bfType != C_OS2_V2 && hp->biCompression == BI_JPEG  ) {
		fprintf( stderr, "bmpLoad: %s - JPEG compression not supported\n", hp->name );
		return FALSE;
	}	
	
	//
	if (       hp->biPixelSize != 1
	        && hp->biPixelSize != 2
	        && hp->biPixelSize != 4
	        && hp->biPixelSize != 8
	        && hp->biPixelSize != 16
	        && hp->biPixelSize != 24
	        && hp->biPixelSize != 32
	        && hp->biPixelSize != 64   ) {
		fprintf( stderr, "bmpLoad: %s - image PixelSize error\n", hp->name );
		return FALSE;
	}
	//
	if  (      hp->biCompression != BI_RGB
	        && hp->biCompression != BI_RLE8
	        && hp->biCompression != BI_RLE4
	        && hp->biCompression != BI_FIELDS  // !! also BI_HUFF1D
	        && hp->biCompression != BI_JPEG    // !! also BI_RLE24
	        && hp->biCompression != BI_PNG
	        && hp->biCompression != BI_ALPHAFIELDS	) {
		fprintf( stderr, "bmpLoad: %s - image compression type error\n", hp->name );
		return FALSE;
	}
	
	// Read or extract bitmasks if BI_FIELDS or BI_ALPHAFIELDS
	if ( ( hp->biCompression == BI_FIELDS || hp->biCompression == BI_ALPHAFIELDS ) 
	&& hp->bfType != C_OS2 
	&& hp->bfType != C_OS2_V2 ) {
		if ( hp->biInfoHeaderSize == WIN_INFOHEADER_LEN ) {
			unsigned int mask_bytes;
			if ( hp->biCompression == BI_ALPHAFIELDS ) {
				mask_bytes = 16;
			} else {
				mask_bytes = 12;
			}
			byte maskbuf[16];
			if ( zread( zf, maskbuf, mask_bytes ) != ( int )mask_bytes ) {
				return FALSE;
			}
			hp->redMask = GULONG4( &maskbuf[0] );
			hp->greenMask = GULONG4( &maskbuf[4] );
			hp->blueMask = GULONG4( &maskbuf[8] );
			if ( hp->biCompression == BI_ALPHAFIELDS ) {
				hp->alphaMask = GULONG4( &maskbuf[12] );
			} else {
				hp->alphaMask = 0;
			}
		} else {
			hp->redMask = GULONG4( &infobuf[40] );
			hp->greenMask = GULONG4( &infobuf[44] );
			hp->blueMask = GULONG4( &infobuf[48] );
			if ( hp->biInfoHeaderSize >= BMP_V3INFOHEADER_LEN || hp->biCompression == BI_ALPHAFIELDS ) {
				hp->alphaMask = GULONG4( &infobuf[52] );
			} else {
				hp->alphaMask = 0;
			}
		}
	// OS/2 and/or no FIELDS default masks
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
	
	if ( hp->bfType == C_OS2_V2 && hp->biCompression == BI_HUFF1D && hp->biPixelSize != 1 ) {
		fprintf( stderr, "bmpLoad: %s - OS/2 V2 HUFF1D compression requires depth 1\n", hp->name );
		return FALSE;
	}
	
	if (  hp->bfType == C_OS2_V2 && hp->biCompression == BI_HUFF1D  ) {
		fprintf( stderr, "bmpLoad: %s - HUFF1D compression not supported\n", hp->name );
		return FALSE;
	}
	
	if ( hp->biCompression == BI_RLE4 && hp->biPixelSize != 4 ) {
		fprintf( stderr, "bmpLoad: %s - RLE4 compression requires depth 4\n", hp->name );
		return FALSE;
	}
	
	if ( hp->biCompression == BI_RLE8 && hp->biPixelSize != 8 ) {
		fprintf( stderr, "bmpLoad: %s - RLE8 compression requires depth 8\n", hp->name );
		return FALSE;
	}
	
	if ( hp->bfType == C_OS2_V2 && hp->biCompression == BI_RLE24 && hp->biPixelSize != 24 ) {
		fprintf( stderr, "bmpLoad: %s - OS/2 V2 RLE24 compression requires depth 24\n", hp->name );
		return FALSE;
	}
	
	if ( hp->biPlanes != 1 ) {
		fprintf( stderr, "bmpLoad: %s - image Planes value error\n", hp->name );
		return FALSE;
	}	
	
	// 0 width and/or 0 height image no supported
	if ( hp->biWidth == 0 || hp->biHeight == 0  ) {
		fprintf( stderr, "bmpLoad: %s - Image dimension is 0 \n", hp->name );
		return FALSE;
	}
	// 0x80000000 cannot be positive within an int32_t
	if ( hp->biHeight == ( int32_t )0x80000000 ) {
		fprintf( stderr, "bmpLoad: %s - Image dimension is invalid\n", hp->name );
		return FALSE;
	}	
	
	// top to bottom images cannot be RLE
	if ( hp->biHeight < 0 && ( hp->biCompression == BI_RLE8 || hp->biCompression == BI_RLE4 ) ) {
		fprintf( stderr, "bmpLoad: %s - top to bottom with RLE not supported\n", hp->name );
		return FALSE;
	}

	// Calculate headers, masks, and palette lengths and validate resolved offset / overlaps
	{
		unsigned long color_entry_len = 4;
		unsigned long masks_header_len = 0;
		unsigned long all_headers_len = 0;
		unsigned long palette_len = 0;
		
		if ( hp->biHeight < 0 ) {
			hp->abs_height = ( unsigned long )( -hp->biHeight );
		} else {
			hp->abs_height = ( unsigned long )hp->biHeight;
		}
		
		if ( hp->biWidth > MAX_WIDTH || hp->abs_height > MAX_HEIGHT ) {
			fprintf( stderr, "bmpLoad: %s - Image dimensions exceed bounds\n", hp->name );
			return FALSE;
		}
		
		if ( hp->biCompression == BI_FIELDS && hp->biInfoHeaderSize == WIN_INFOHEADER_LEN ) {
			masks_header_len = 12;
		} else if ( hp->biCompression == BI_ALPHAFIELDS && hp->biInfoHeaderSize == WIN_INFOHEADER_LEN ) {
			masks_header_len = 16;
		}

		all_headers_len = BMP_FILEHEADER_LEN + hp->biInfoHeaderSize + masks_header_len;

		// **FIXME** there could be some other data between headers, palette and image
		if ( hp->bfImageOffset > all_headers_len ) {
			palette_len = hp->bfImageOffset - all_headers_len;
		} else {
			palette_len = 0;
		}

		if ( hp->bfType == C_OS2 ) {
			color_entry_len = 3; // OS/2 v1 always uses 3-byte RGBTRIPLE entries
		} else if ( hp->bfType == C_OS2_V2 ) {
			// OS/2 v2 dynamically permits either 3-byte or 4-byte entries
			if ( palette_len > 0 && ( palette_len % 3 == 0 ) && ( palette_len / 3 <= ( 1UL << hp->biPixelSize ) ) ) {
				color_entry_len = 3;
				fprintf( stderr, "bmpLoad: %s - OS/2 v2 file handles 3-byte palette entries\n", hp->name );
			} else if ( palette_len > 0 && ( palette_len % 4 == 0 ) && ( palette_len / 4 <= ( 1UL << hp->biPixelSize ) ) ) {
				color_entry_len = 4;
			} else {
				color_entry_len = 3; // Standard fallback for native OS/2 streams
			}
		} else {
			color_entry_len = 4; // Windows formats always use 4-byte RGBQUAD entries
		}

		unsigned long effective_clr_used = 0;

		// Determine number of colors used for indexed color modes
		if ( hp->biPixelSize <= PAL_MAX_DEPTH ) {
			unsigned long max_colors = ( 1UL << hp->biPixelSize );
			effective_clr_used = hp->biClrUsed;

			if ( effective_clr_used > max_colors ) {
				fprintf( stderr, "bmpLoad: %s - biClrUsed (%lu) exceeds depth maximum colors (%lu)\n", hp->name, effective_clr_used, max_colors );
				effective_clr_used = max_colors;
			}
			
			// Try to find number of colors with distance between header and image data
			if ( palette_len > 0 && color_entry_len > 0 && ( palette_len % color_entry_len == 0 ) ) {
				unsigned long colors_num = palette_len / color_entry_len;
				if ( colors_num <= max_colors ) {
					effective_clr_used = colors_num;
				} else {
					effective_clr_used = max_colors;
				}
			} 
			// Fallback to header field or absolute bit-depth limit
			else if ( effective_clr_used == 0 ) {
				effective_clr_used = max_colors;
			}
		} else {
			effective_clr_used = 0; // TrueColor/HighColor streams do not utilize a required palette
		}

		// Determine real file size via fstat for standard file streams
		unsigned long real_file_size = 0;
		if ( zf->type == ZSTANDARD && zf->stream != NULL ) {
			struct stat st;
			if ( fstat( fileno( zf->stream ), &st ) == 0 ) {
				real_file_size = ( unsigned long )st.st_size;
			}
		}
		
		bool BMP_HDRFILESIZE_OK = TRUE;
		if ( hp->bfFileSize > 0 && real_file_size > 0 && hp->bfFileSize != real_file_size ) {
			fprintf( stderr, "bmpLoad: %s - header file size value (%lu) does not match real file size (%lu)\n", hp->name, hp->bfFileSize, real_file_size );
			BMP_HDRFILESIZE_OK = FALSE;
		}


		unsigned long resolved_offset = hp->bfImageOffset;
		unsigned long pixel_depth_colors_len;
		if ( hp->biPixelSize <= PAL_MAX_DEPTH ) {
			pixel_depth_colors_len = effective_clr_used * color_entry_len;
		} else {
			pixel_depth_colors_len = 0;
		}
		unsigned long min_pixel_offset = all_headers_len + pixel_depth_colors_len;

		if ( resolved_offset < all_headers_len ) {
			fprintf( stderr, "bmpLoad: %s - bfImageOffset (%lu) overlaps header (minimum expected: %lu)\n", hp->name, hp->bfImageOffset, all_headers_len );
		} else if ( resolved_offset < min_pixel_offset ) {
			if ( hp->bfImageOffset > 0 && hp->bfImageOffset < min_pixel_offset ) {
				fprintf( stderr, "bmpLoad: %s - bfImageOffset (%lu) overlaps colormap (minimum expected: %lu)\n", hp->name, hp->bfImageOffset, min_pixel_offset );
			}
			resolved_offset = min_pixel_offset;
		} else if ( hp->bfFileSize > 0 && BMP_HDRFILESIZE_OK && resolved_offset > hp->bfFileSize ) {
			fprintf( stderr, "bmpLoad: %s - resolved image offset (%lu) exceeds file size (%lu)\n", hp->name, resolved_offset, hp->bfFileSize );
			return FALSE;
		}

		hp->all_headers_len = all_headers_len;
		hp->masks_header_len = masks_header_len;
		hp->palette_len = palette_len;
		hp->color_entry_len = color_entry_len;
		hp->resolved_image_offset = resolved_offset;
		hp->resolved_clr_used = effective_clr_used;
	}

	return TRUE;
}

// Print a brief description of the image
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

	const strbyte* colormode;
	if ( hp->biPixelSize <= PAL_MAX_DEPTH ) {
		colormode = "palette";
	} else {
		colormode = "TrueColor";
	}

	const strbyte* topbot;
	if ( hp->biHeight < 0 ) {
		topbot = "top-bot, ";
	} else {
		topbot = "";
	}

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
		complabel = "fields masks, ";
	} else if ( ( hp->bfType == C_OS2 || hp->bfType == C_OS2_V2 ) && hp->biCompression == BI_HUFF1D ) {
		complabel = "Huff1d, ";
	} else if (  hp->biCompression == BI_ALPHAFIELDS ) {
		complabel = "fields masks with alpha, ";
	} else if ( hp->biCompression == BI_RGB ) {
		complabel = "";
	}

	printf( "%s is a %lux%lu BMP image, depth %d, %s%s%s %s\n", hp->name,
	        hp->biWidth, hp->abs_height, hp->biPixelSize, topbot, complabel, colormode,
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

	const strbyte* colormode;
	if ( hp->biPixelSize <= PAL_MAX_DEPTH ) {
		colormode = "palette";
	} else {
		colormode = "TrueColor";
	}

	const strbyte* topbot;
	if ( hp->biHeight < 0 ) {
		topbot = "top-bot";
	} else {
		topbot = "bot-top";
	}

	const strbyte* complabel = "";
	if ( hp->biCompression == BI_RLE8 || hp->biCompression == BI_RLE4 ) {
		complabel = "RLE4 ";
	} else if ( hp->biCompression == BI_RLE8 ) {
		complabel = "RLE8 ";
	} else if ( ( hp->bfType != C_OS2 && hp->bfType != C_OS2_V2 ) && hp->biCompression == BI_JPEG ) {
		complabel = "JPEG ";
	} else if ( ( hp->bfType == C_OS2 || hp->bfType == C_OS2_V2 ) && hp->biCompression == BI_RLE24 ) {
		complabel = "RLE24 ";
	} else if ( hp->biCompression == BI_PNG ) {
		complabel = "PNG ";
	} else if ( ( hp->bfType != C_OS2 && hp->bfType != C_OS2_V2 ) && hp->biCompression == BI_FIELDS ) {
		complabel = "fields masks ";
	} else if ( ( hp->bfType == C_OS2 || hp->bfType == C_OS2_V2 ) && hp->biCompression == BI_HUFF1D ) {
		complabel = "Huff1d ";
	} else if (  hp->biCompression == BI_ALPHAFIELDS ) {
		complabel = "fields masks with alpha";
	} else if ( hp->biCompression == BI_RGB ) {
		complabel = "None ";
	}

	printf( " file name:                           %s\n", hp->name );
	printf( " ==============================================\n" );
	printf( " file header:\n" );
	printf( " file type identifier bfType:         %i (0x%04X) [%s]\n", hp->bfType, hp->bfType, typelabel );
	printf( " total file size bfFileSize:          %lu bytes\n", hp->bfFileSize );
	printf( " x hot spot bfxHotSpot:               %u\n", hp->bfxHotSpot );
	printf( " y hot spot bfyHotSpot:               %u\n", hp->bfyHotSpot );
	printf( " pixel data offset bfImageOffset:     %lu bytes\n", hp->bfImageOffset );
	printf( " ==============================================\n" );
	printf( " info header:\n" );
	printf( " info header size biInfoHeaderSize:   %lu bytes\n", hp->biInfoHeaderSize );
	printf( " image width biWidth:                 %lu pixels\n", hp->biWidth );
	printf( " image height biHeight:               %i pixels (abs: %lu, orientation: %s)\n", hp->biHeight, hp->abs_height, topbot );
	printf( " color planes biPlanes:               %u\n", hp->biPlanes );
	printf( " depth biPixelSize:                   %u  [%s]\n", hp->biPixelSize, colormode );
	printf( " compression type biCompression:      %lu [%s]\n", hp->biCompression, complabel );
	printf( " raw image data size biSizeImage:     %lu bytes\n", hp->biSizeImage );
	printf( " horizontal resolution biXPels/Meter: %lu\n", hp->biXPelsPerMeter );
	printf( " vertical resolution biYPels/Meter:   %lu\n", hp->biYPelsPerMeter );
	printf( " colormap entries used biClrUsed:     %lu\n", hp->biClrUsed );
	printf( " important colors biClrImportant:     %lu\n", hp->biClrImportant );
	printf( " ==============================================\n" );
	printf( " masks:\n" );
	printf( " red channel mask redMask:            0x%08lx\n", hp->redMask );
	printf( " green Channel mask greenMask:        0x%08lx\n", hp->greenMask );
	printf( " blue channel mask blueMask:          0x%08lx\n", hp->blueMask );
	printf( " alpha channel mask alphaMask:        0x%08lx\n", hp->alphaMask );
	printf( " ==============================================\n" );
	printf( " resolved:\n" );	
	printf( " resolved image offset:               %lu bytes\n", hp->resolved_image_offset );	
	printf( " resolved colormap entries:           %lu\n", hp->resolved_clr_used );	
	printf( " resolved palette length:             %lu bytes\n", hp->palette_len );
	printf( " resolved color entry length:         %lu bytes\n", hp->color_entry_len );
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
		return 0;	// Nope, not a BMP file
	}
	tell_about_bmp( &hdr );
	zclose( zf );
	return 1;
}

// d: get image data, depth 1  
static bool get_bmp_pixels_depth1( ZFILE* zf, Image* image, bmpHeader* hp )
{
	byte data_bytes[4];
	byte* memblock;
	int src_row_bytelen = ( ( int )image->width + 7 ) / 8;
	int src_padlen = ( ( ( ( int )image->width + 31 ) / 32 ) * 4 ) - src_row_bytelen;
	bool top_bot = ( hp->biHeight < 0 );
	int rowcount = ( int )hp->abs_height;
	long dest_row_stride;
	if ( top_bot ) {
		dest_row_stride = ( long )src_row_bytelen;
		memblock = image->data;
	} else {
		dest_row_stride = -( long )src_row_bytelen;
		memblock = image->data + ( ( unsigned long )hp->abs_height - 1 ) * ( unsigned long )src_row_bytelen;
	}

	while ( rowcount > 0 ) {
		if ( zread( zf, memblock, src_row_bytelen ) != src_row_bytelen
		        || zread( zf, data_bytes, src_padlen ) != src_padlen ) {
			return FALSE;
		}
		rowcount = rowcount - 1;
		memblock = memblock + dest_row_stride;
	}
	return TRUE;
}

// d: get image data, depth 2 
static bool get_bmp_pixels_depth2( ZFILE* zf, Image* image, bmpHeader* hp )
{
	byte* memblock;
	unsigned int src_row_bytelen;
	unsigned int src_padlen;
	unsigned int rowcount;
	unsigned int i;
	unsigned int padbytes;
	unsigned int packedbyte;
	long dest_row_stride;
	bool top_bot = ( hp->biHeight < 0 );

	src_row_bytelen = ( image->width * 2 + 7 ) / 8;
	src_padlen = ( ( ( image->width * 2 + 31 ) / 32 ) * 4 ) - src_row_bytelen;

	if ( top_bot ) {
		dest_row_stride = ( long )image->width;
		memblock = image->data;
	} else {
		dest_row_stride = -( long )image->width;
		memblock = image->data + ( ( unsigned long )hp->abs_height - 1 ) * image->width;
	}

	rowcount = ( unsigned int )hp->abs_height;

	while ( rowcount > 0 ) {
		i = 0;
		while ( i < image->width ) {
			packedbyte = ( unsigned int )zgetc( zf );
			if ( packedbyte == ( unsigned int )EOF ) {
				return FALSE;
			}
			if ( i < image->width ) {
				memblock[i] = ( byte )( ( packedbyte >> 6 ) & 0x3 );
				i = i + 1;
			}
			if ( i < image->width ) {
				memblock[i] = ( byte )( ( packedbyte >> 4 ) & 0x3 );
				i = i + 1;
			}
			if ( i < image->width ) {
				memblock[i] = ( byte )( ( packedbyte >> 2 ) & 0x3 );
				i = i + 1;
			}
			if ( i < image->width ) {
				memblock[i] = ( byte )( packedbyte & 0x3 );
				i = i + 1;
			}
		}
		padbytes = 0;
		while ( padbytes < src_padlen ) {
			if ( zgetc( zf ) == EOF ) {
				return FALSE;
			}
			padbytes = padbytes + 1;
		}
		rowcount = rowcount - 1;
		memblock = memblock + dest_row_stride;
	}
	return TRUE;
}

// d: get image data, depth 4  with or without RLE4
static bool get_bmp_pixels_depth4( ZFILE* zf, Image* image, bmpHeader* hp, bool* data_bounds )
{
	byte* memblock;
	unsigned int src_row_bytelen;
	unsigned int x;
	unsigned int y;
	bool top_bot = ( hp->biHeight < 0 );

	src_row_bytelen = image->width;
	*data_bounds = FALSE;

	if ( hp->biCompression == BI_RLE4 ) {
		int d;
		int e;
		memset( ( byte* )image->data, 0, ( size_t )image->width * ( size_t )image->height );
		x = 0;
		y = 0;
		memblock = image->data + ( hp->abs_height - 1 ) * src_row_bytelen;
		while ( 1 ) {
			int i;
			int f;
			d = zgetc( zf );
			if ( d == EOF ) {
				return FALSE;
			}
			if ( d != 0 ) {
				unsigned int start_x = x;
				unsigned int run_len = ( unsigned int )d;
				x = x + run_len;
				if ( y > hp->abs_height ) {
					*data_bounds = TRUE;
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
				e = zgetc( zf );
				if ( e == EOF ) {
					return FALSE;
				}
				f = e & 0xf;
				e = e >> 4;
				i = ( int )( run_len / 2 );
				while ( i > 0 ) {
					*memblock = ( byte )e;
					memblock = memblock + 1;
					*memblock = ( byte )f;
					memblock = memblock + 1;
					i = i - 1;
				}
				if ( ( run_len & 1 ) != 0 ) {
					*memblock = ( byte )e;
					memblock = memblock + 1;
				}
				goto skip_iter_rle4;
			}
			d = zgetc( zf );
			if ( d == EOF ) {
				return FALSE;
			}
			if ( d == 0 ) {
				memblock = memblock - ( x + src_row_bytelen );
				x = 0;
				y = y + 1;
				goto skip_iter_rle4;
			}
			if ( d == 1 ) {
				break;
			}
			if ( d == 2 ) {
				d = zgetc( zf );
				e = zgetc( zf );
				if ( d == EOF || e == EOF ) {
					return FALSE;
				}
				x = x + ( unsigned int )d;
				y = y + ( unsigned int )e;
				if ( x > image->width || y > hp->abs_height ) {
					*data_bounds = TRUE;
					x = x - ( unsigned int )d;
					y = y - ( unsigned int )e;
					goto skip_iter_rle4;
				}
				memblock = memblock + d - ( e * src_row_bytelen );
				goto skip_iter_rle4;
			}
			{
				unsigned int start_x = x;
				unsigned int run_len = ( unsigned int )d;
				int total_bytes = d / 2 + ( d & 1 ) + ( ( ( d + 1 ) & 2 ) >> 1 );
				int bytes_read = 0;

				x = x + run_len;
				if ( y > hp->abs_height ) {
					*data_bounds = TRUE;
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
					e = zgetc( zf );
					if ( e == EOF ) {
						return FALSE;
					}
					bytes_read = bytes_read + 1;
					*memblock = ( byte )( e >> 4 );
					memblock = memblock + 1;
					*memblock = ( byte )( e & 0xf );
					memblock = memblock + 1;
					i = i - 1;
				}
				if ( ( run_len & 1 ) != 0 ) {
					e = zgetc( zf );
					if ( e == EOF ) {
						return FALSE;
					}
					bytes_read = bytes_read + 1;
					*memblock = ( byte )( e >> 4 );
					memblock = memblock + 1;
				}

				while ( bytes_read < total_bytes ) {
					if ( zgetc( zf ) == EOF ) {
						return FALSE;
					}
					bytes_read = bytes_read + 1;
				}
			}
			skip_iter_rle4:
			;
		}
	} else {
		int d;
		int s;
		int p;
		int i;
		int e;
		byte* dest_row_madress;
		long dest_row_stride;

		d = ( int )image->width / 2;
		s = ( int )image->width & 1;
		p = ( 4 - ( d + s ) ) & 0x3;

		if ( top_bot ) {
			dest_row_stride = ( long )src_row_bytelen;
			dest_row_madress = image->data;
		} else {
			dest_row_stride = -( long )src_row_bytelen;
			dest_row_madress = image->data + ( hp->abs_height - 1 ) * src_row_bytelen;
		}

		y = ( int )hp->abs_height;

		while ( y > 0 ) {
			byte* row_data = dest_row_madress;
			i = d;
			while ( i > 0 ) {
				e = zgetc( zf );
				if ( e == EOF ) {
					return FALSE;
				}
				*row_data = ( byte )( e >> 4 );
				row_data = row_data + 1;
				*row_data = ( byte )( e & 0xf );
				row_data = row_data + 1;
				i = i - 1;
			}
			if ( s != 0 ) {
				e = zgetc( zf );
				if ( e == EOF ) {
					return FALSE;
				}
				*row_data = ( byte )( e >> 4 );
				row_data = row_data + 1;
			}
			i = p;
			while ( i > 0 ) {
				if ( zgetc( zf ) == EOF ) {
					return FALSE;
				}
				i = i - 1;
			}
			y = y - 1;
			dest_row_madress = dest_row_madress + dest_row_stride;
		}
	}
	return TRUE;
}

// d: get image data, depth 8  with or without RLE8 
static bool get_bmp_pixels_depth8( ZFILE* zf, Image* image, bmpHeader* hp, bool* data_bounds )
{
	byte* memblock;
	unsigned int src_row_bytelen;
	unsigned int x;
	unsigned int y;
	bool top_bot = ( hp->biHeight < 0 );

	src_row_bytelen = image->width;
	*data_bounds = FALSE;

	if ( hp->biCompression == BI_RLE8 ) {
		int d;
		int e;
		memset( ( byte* )image->data, 0, ( size_t )image->width * ( size_t )image->height );
		x = 0;
		y = 0;
		memblock = image->data + ( hp->abs_height - 1 ) * src_row_bytelen;
		while ( 1 ) {
			d = zgetc( zf );
			if ( d == EOF ) {
				return FALSE;
			}
			if ( d != 0 ) {
				unsigned int start_x = x;
				unsigned int run_len = ( unsigned int )d;
				x = x + run_len;
				if ( y > hp->abs_height ) {
					*data_bounds = TRUE;
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
				e = zgetc( zf );
				if ( e == EOF ) {
					return FALSE;
				}
				memset( memblock, ( byte )e, run_len );
				memblock = memblock + run_len;
				goto skip_iter_rle8;
			}
			d = zgetc( zf );
			if ( d == EOF ) {
				return FALSE;
			}
			if ( d == 0 ) {
				memblock = memblock - ( x + src_row_bytelen );
				x = 0;
				y = y + 1;
				goto skip_iter_rle8;
			}
			if ( d == 1 ) {
				break;
			}
			if ( d == 2 ) {
				int delta_x;
				int delta_y;
				delta_x = zgetc( zf );
				delta_y = zgetc( zf );
				if ( delta_x == EOF || delta_y == EOF ) {
					return FALSE;
				}
				x = x + ( unsigned int )delta_x;
				y = y + ( unsigned int )delta_y;
				if ( x > image->width || y > hp->abs_height ) {
					*data_bounds = TRUE;
					x = x - ( unsigned int )delta_x;
					y = y - ( unsigned int )delta_y;
					goto skip_iter_rle8;
				}
				memblock = memblock + delta_x - ( delta_y * ( int )src_row_bytelen );
				goto skip_iter_rle8;
			}
			{
				unsigned int start_x = x;
				unsigned int run_len = ( unsigned int )d;
				int total_bytes = d + ( d & 1 );
				int bytes_read = 0;

				x = x + run_len;
				if ( y > hp->abs_height ) {
					*data_bounds = TRUE;
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
					int pixel_v = zgetc( zf );
					if ( pixel_v == EOF ) {
						return FALSE;
					}
					*memblock = ( byte )pixel_v;
					memblock = memblock + 1;
					bytes_read = bytes_read + 1;
				}

				while ( bytes_read < total_bytes ) {
					if ( zgetc( zf ) == EOF ) {
						return FALSE;
					}
					bytes_read = bytes_read + 1;
				}
			}
			skip_iter_rle8:
			;
		}
	} else {
		byte pad[4];
		int src_padlen;
		long dest_row_stride;

		src_padlen = ( ( ( int )image->width + 3 ) & ~3 ) - ( int )src_row_bytelen;
		
		if ( top_bot ) {
			dest_row_stride = ( long )src_row_bytelen;
			memblock = image->data;
		} else {
			dest_row_stride = -( long )src_row_bytelen;
			memblock = image->data + ( hp->abs_height - 1 ) * src_row_bytelen;
		}

		y = ( unsigned int )hp->abs_height;

		while ( y > 0 ) {
			if ( zread( zf, memblock, src_row_bytelen ) != ( int )src_row_bytelen
			        || zread( zf, pad, src_padlen ) != src_padlen ) {
				return FALSE;
			}
			y = y - 1;
			memblock = memblock + dest_row_stride;
		}
	}
	return TRUE;
}

// d: read 16-bit truecolor
static bool get_bmp_pixels_depth16( ZFILE* zf, Image* image, bmpHeader* hp )
{
	byte* pixel_row;
	byte* src_madress;
	byte* dst_madress;
	byte pad_bytes[4];
	int dest_row_bytelen = ( int )image->width * ( int )image->pixlen;
	int src_row_bytelen = ( int )image->width * 2;
	int src_padlen = ( ( src_row_bytelen + 3 ) & ~3 ) - src_row_bytelen;
	bool top_bot = ( hp->biHeight < 0 );
	long dest_row_stride;
	if ( top_bot ) {
		dest_row_stride = ( long )dest_row_bytelen;
		dst_madress = image->data;
	} else {
		dest_row_stride = -( long )dest_row_bytelen;
		dst_madress = image->data + ( ( unsigned long )hp->abs_height - 1 ) * ( unsigned long )dest_row_bytelen;
	}

	unsigned int red_shift = get_mask_shift( hp->redMask );
	unsigned int green_shift = get_mask_shift( hp->greenMask );
	unsigned int blue_shift = get_mask_shift( hp->blueMask );
	unsigned int alpha_shift = get_mask_shift( hp->alphaMask );
	unsigned int red_flags = get_mask_factors( hp->redMask );
	unsigned int green_flags = get_mask_factors( hp->greenMask );
	unsigned int blue_flags = get_mask_factors( hp->blueMask );
	unsigned int alpha_flags = get_mask_factors( hp->alphaMask );
	int y = ( int )hp->abs_height;

	pixel_row = ( byte* )calloc( ( size_t )src_row_bytelen + 4, 1 );
	if ( pixel_row == NULL ) {
		return FALSE;
	}

	while ( y > 0 ) {
		if ( zread( zf, pixel_row, src_row_bytelen ) != src_row_bytelen
		|| ( src_padlen > 0 && zread( zf, pad_bytes, src_padlen ) != src_padlen ) ) {
			free( pixel_row );
			return FALSE;
		}
		
		{
			unsigned int x = 0;
			src_madress = pixel_row;
			byte* dest_row_madress = dst_madress;
			while ( x < image->width ) {
				unsigned long pixelval = ( unsigned long )GUINT2( src_madress );
				unsigned long r_v = 0;
				unsigned long g_v = 0;
				unsigned long b_v = 0;
				unsigned long a_v = 0;
				byte r = 0;
				byte g = 0;
				byte b = 0;
				byte a = 0xff;

				if ( red_flags > 0 ) {
					r_v = ( pixelval & hp->redMask ) >> red_shift;
					r = ( byte )( ( r_v * 255 ) / ( ( 1 << red_flags ) - 1 ) );
				}
				if ( green_flags > 0 ) {
					g_v = ( pixelval & hp->greenMask ) >> green_shift;
					g = ( byte )( ( g_v * 255 ) / ( ( 1 << green_flags ) - 1 ) );
				}
				if ( blue_flags > 0 ) {
					b_v = ( pixelval & hp->blueMask ) >> blue_shift;
					b = ( byte )( ( b_v * 255 ) / ( ( 1 << blue_flags ) - 1 ) );
				}
				if ( alpha_flags > 0 ) {
					a_v = ( pixelval & hp->alphaMask ) >> alpha_shift;
					a = ( byte )( ( a_v * 255 ) / ( ( 1 << alpha_flags ) - 1 ) );
				}

				if ( alpha_flags == 0 || a == 0x00 ) {
					a = 0xff;
				}

				dest_row_madress[0] = colorreg_applyalpha( r, a );
				dest_row_madress[1] = colorreg_applyalpha( g, a );
				dest_row_madress[2] = colorreg_applyalpha( b, a );

				src_madress = src_madress + 2;
				dest_row_madress = dest_row_madress + 3;
				x = x + 1;
			}
		}
		y = y - 1;
		dst_madress = dst_madress + dest_row_stride;
	}
	free( pixel_row );
	return TRUE;
}

// d: get image data, depth 24  with or without RLE24
static bool get_bmp_pixels_depth24( ZFILE* zf, Image* image, bmpHeader* hp, bool* data_bounds )
{
	byte* memblock;
	byte pad[4];
	int src_row_bytelen;
	bool top_bot = ( hp->biHeight < 0 );

	src_row_bytelen = ( int )image->width * ( int )image->pixlen;
	*data_bounds = FALSE;

	if ( hp->biCompression == BI_RLE24 ) {
		int d;
		unsigned int x;
		unsigned int y;
		byte triplet[3];
		memset( ( byte* )image->data, 0, ( size_t )image->width * ( size_t )image->height * ( size_t )image->pixlen );
		x = 0;
		y = 0;
		memblock = image->data + ( hp->abs_height - 1 ) * ( unsigned long )src_row_bytelen;
		while ( 1 ) {
			int i;
			d = zgetc( zf );
			if ( d == EOF ) {
				return FALSE;
			}
			if ( d != 0 ) {
				unsigned int start_x = x;
				unsigned int run_len = ( unsigned int )d;
				x = x + run_len;
				if ( y > hp->abs_height ) {
					*data_bounds = TRUE;
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
					return FALSE;
				}
				i = ( int )run_len;
				while ( i > 0 ) {
					*( memblock + 0 ) = triplet[2];
					*( memblock + 1 ) = triplet[1];
					*( memblock + 2 ) = triplet[0];
					memblock = memblock + 3;
					i = i - 1;
				}
				goto skip_iter_rle24;
			}
			d = zgetc( zf );
			if ( d == EOF ) {
				return FALSE;
			}
			if ( d == 0 ) {
				memblock = memblock - ( ( x * image->pixlen ) + ( unsigned int )src_row_bytelen );
				x = 0;
				y = y + 1;
				goto skip_iter_rle24;
			}
			if ( d == 1 ) {
				break;
			}
			if ( d == 2 ) {
				int delta_x;
				int delta_y;
				delta_x = zgetc( zf );
				delta_y = zgetc( zf );
				if ( delta_x == EOF || delta_y == EOF ) {
					return FALSE;
				}
				x = x + ( unsigned int )delta_x;
				y = y + ( unsigned int )delta_y;
				if ( x > image->width || y > hp->abs_height ) {
					*data_bounds = TRUE;
					x = x - ( unsigned int )delta_x;
					y = y - ( unsigned int )delta_y;
					goto skip_iter_rle24;
				}
				memblock = memblock + ( delta_x * ( int )image->pixlen ) - ( delta_y * src_row_bytelen );
				goto skip_iter_rle24;
			}
			{
				unsigned int start_x = x;
				unsigned int run_len = ( unsigned int )d;
				int total_bytes = d * 3;
				int padded_bytes = ( total_bytes + 1 ) & ~1;
				int bytes_read = 0;

				x = x + run_len;
				if ( y > hp->abs_height ) {
					*data_bounds = TRUE;
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
						return FALSE;
					}
					bytes_read = bytes_read + 3;
					*( memblock + 0 ) = triplet[2];
					*( memblock + 1 ) = triplet[1];
					*( memblock + 2 ) = triplet[0];
					memblock = memblock + 3;
					i = i - 1;
				}

				while ( bytes_read < padded_bytes ) {
					if ( zgetc( zf ) == EOF ) {
						return FALSE;
					}
					bytes_read = bytes_read + 1;
				}
			}
			skip_iter_rle24:
			;
		}
	} else {
		int src_padlen;
		int y;
		long dest_row_stride;
		src_padlen = ( ( ( ( int )image->width * 3 ) + 3 ) & ~3 ) - src_row_bytelen;

		if ( top_bot ) {
			dest_row_stride = ( long )src_row_bytelen;
			memblock = image->data;
		} else {
			dest_row_stride = -( long )src_row_bytelen;
			memblock = image->data + ( hp->abs_height - 1 ) * ( unsigned long )src_row_bytelen;
		}

		y = ( int )hp->abs_height;

		while ( y > 0 ) {
			int i;
			if ( zread( zf, memblock, src_row_bytelen ) != src_row_bytelen
			|| zread( zf, pad, src_padlen ) != src_padlen ) {
				return FALSE;
			}
			i = 3 * ( int )image->width - 1;
			while ( i > 0 ) {
				int t = memblock[i];
				memblock[i] = memblock[i - 2];
				memblock[i - 2] = ( byte )t;
				i = i - 3;
			}
			y = y - 1;
			memblock = memblock + dest_row_stride;
		}
	}
	return TRUE;
}

// d: get image data, depth 32 
static bool get_bmp_pixels_depth32( ZFILE* zf, Image* image, bmpHeader* hp )
{
	byte* memblock;
	byte* src_row;
	byte pad[4];
	int dest_row_bytelen;
	int src_row_bytelen;
	int src_padlen;
	int y;
	long dest_row_stride;
	bool top_bot = ( hp->biHeight < 0 );
	unsigned int red_shift = get_mask_shift( hp->redMask );
	unsigned int green_shift = get_mask_shift( hp->greenMask );
	unsigned int blue_shift = get_mask_shift( hp->blueMask );
	unsigned int alpha_shift = get_mask_shift( hp->alphaMask );
	unsigned int red_flags = get_mask_factors( hp->redMask );
	unsigned int green_flags = get_mask_factors( hp->greenMask );
	unsigned int blue_flags = get_mask_factors( hp->blueMask );
	unsigned int alpha_flags = get_mask_factors( hp->alphaMask );

	dest_row_bytelen = ( int )image->width * ( int )image->pixlen;
	src_row_bytelen = ( int )image->width * 4;
	src_padlen = ( ( src_row_bytelen + 3 ) & ~3 ) - src_row_bytelen;

	if ( top_bot ) {
		dest_row_stride = ( long )dest_row_bytelen;
		memblock = image->data;
	} else {
		dest_row_stride = -( long )dest_row_bytelen;
		memblock = image->data + ( hp->abs_height - 1 ) * ( unsigned long )dest_row_bytelen;
	}

	src_row = ( byte* )calloc( ( size_t )src_row_bytelen + 4, 1 );
	if ( src_row == NULL ) {
		return FALSE;
	}

	y = ( int )hp->abs_height;
	while ( y > 0 ) {
		if ( zread( zf, src_row, src_row_bytelen ) != src_row_bytelen
		        || ( src_padlen > 0 && zread( zf, pad, src_padlen ) != src_padlen ) ) {
			free( src_row );
			return FALSE;
		}
		{
			unsigned int x = 0;
			byte* src_madress = src_row;
			byte* dst_madress = memblock;
			while ( x < image->width ) {
				unsigned long pixelval = GULONG4( src_madress );
				unsigned long r_v;
				unsigned long g_v;
				unsigned long b_v;
				unsigned long a_v;
				byte r;
				byte g;
				byte b;
				byte a;
				
				if ( red_flags > 0 ) {
					r_v = ( pixelval & hp->redMask ) >> red_shift;
					r = ( byte )( ( r_v * 255 ) / ( ( 1 << red_flags ) - 1 ) );
				} else {
					r_v = 0;
					r = src_madress[2];
				}

				if ( green_flags > 0 ) {
					g_v = ( pixelval & hp->greenMask ) >> green_shift;
					g = ( byte )( ( g_v * 255 ) / ( ( 1 << green_flags ) - 1 ) );
				} else {
					g_v = 0;
					g = src_madress[1];
				}

				if ( blue_flags > 0 ) {
					b_v = ( pixelval & hp->blueMask ) >> blue_shift;
					b = ( byte )( ( b_v * 255 ) / ( ( 1 << blue_flags ) - 1 ) );
				} else {
					b_v = 0;
					b = src_madress[0];
				}

				if ( alpha_flags > 0 ) {
					a_v = ( pixelval & hp->alphaMask ) >> alpha_shift;
					a = ( byte )( ( a_v * 255 ) / ( ( 1 << alpha_flags ) - 1 ) );
				} else {
					a_v = 0;
					a = src_madress[3];
				}


				if ( alpha_flags == 0 || a == 0x00 ) {
					a = 0xff;
				}

				dst_madress[0] = colorreg_applyalpha( r, a );
				dst_madress[1] = colorreg_applyalpha( g, a );
				dst_madress[2] = colorreg_applyalpha( b, a );

				src_madress = src_madress + 4;
				dst_madress = dst_madress + 3;
				x = x + 1;
			}
		}
		y = y - 1;
		memblock = memblock + dest_row_stride;
	}
	free( src_row );
	return TRUE;
}


// d: load BMP image, xli interface
Image* bmpLoad( strbyte* fullname, ImageOptions* image_ops, bool verbose )
{
	ZFILE* zf;
	bmpHeader hdr = {0};
	Image* image;
	bool data_bounds = FALSE;

	if ( !( zf = zopen( fullname ) ) ) {
		perror( "bmpIdent" );
		return ( 0 );
	}
	if ( !read_bmpHeader( zf, &hdr, image_ops->name ) ) {
		zclose( zf );
		return NULL;	// Nope, not a BMP file
	}
	if ( verbose ) {
		tell_about_bmp( &hdr );
	}
	if ( globals._Xdebug ) {
		print_bmp_hdr( &hdr );
	}

	// Report  64-bit files as identifiable but not displayable yet
	if (  hdr.biPixelSize == 64 ) {
		fprintf( stderr, "bmpLoad: %s - %d-bit BMP loading not supported\n", hdr.name, ( int )hdr.biPixelSize );
		zclose( zf );
		return NULL;
	}

	// Create the appropriate image and colormap
	if ( hdr.biPixelSize <= PAL_MAX_DEPTH ) {

		unsigned int i;
		int n = ( int )hdr.color_entry_len;
		byte buf[4];
		// maximum number of colors
		unsigned int used = ( 1 << hdr.biPixelSize );

		if ( hdr.biPixelSize == 1 ) {	// 2 colors  type
			image = newBitImage( hdr.biWidth, hdr.abs_height );
		} else {   // palette type, must be 1, 2, 4 or 8 bit mapped type
			image = newRGBImage( hdr.biWidth, hdr.abs_height, hdr.biPixelSize );
		}
		image->title = dupString( hdr.name );

		i = 0;
		while ( i < hdr.resolved_clr_used ) {
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

		// init rest of colormap (if any)
		while ( i < used ) {
			image->rgb.red[i] = 0;
			image->rgb.green[i] = 0;
			image->rgb.blue[i] = 0;
			i = i + 1;
		}
		// Don't know how many colors are actually used.
		image->rgb.used = used;
	} else {		// else must be a true color image
		image = newTrueImage( hdr.biWidth, hdr.abs_height );
		image->title = dupString( hdr.name );
	}

	// Rewind file stream to offset 0 and strictly skip to bfImageOffset (or resolved fallback if 0)
	{
		unsigned long skip_offset;
		if ( hdr.bfImageOffset > 0 ) {
			skip_offset = hdr.bfImageOffset;
		} else {
			skip_offset = hdr.resolved_image_offset;
		}
		unsigned long skip_count = skip_offset;

		if ( !zrewind( zf ) ) {
			fprintf( stderr, "bmpLoad: %s - Failed to rewind file stream\n", hdr.name );
			freeImage( image );
			zclose( zf );
			return NULL;
		}

		while ( skip_count > 0 ) {
			if ( zgetc( zf ) == EOF ) {
				fprintf( stderr, "bmpLoad: %s - Short read while skipping to image data offset\n", hdr.name );
				freeImage( image );
				zclose( zf );
				return NULL;
			}
			skip_count = skip_count - 1;
		}
	}

	// disable caching next header
	znocache( zf );

	// get image pixels data
	if ( hdr.biPixelSize == 1 ) {
		if ( !get_bmp_pixels_depth1( zf, image, &hdr ) ) {
			goto data_short;
		}
	} else if ( hdr.biPixelSize == 2 ) {
		if ( !get_bmp_pixels_depth2( zf, image, &hdr ) ) {
			goto data_short;
		}
	} else if ( hdr.biPixelSize == 4 ) {
		if ( !get_bmp_pixels_depth4( zf, image, &hdr, &data_bounds ) ) {
			goto data_short;
		}
	} else if ( hdr.biPixelSize == 8 ) {
		if ( !get_bmp_pixels_depth8( zf, image, &hdr, &data_bounds ) ) {
			goto data_short;
		}
	} else if ( hdr.biPixelSize == 16 ) {
		if ( !get_bmp_pixels_depth16( zf, image, &hdr ) ) {
			goto data_short;
		}
	} else if ( hdr.biPixelSize == 24 ) {
		if ( !get_bmp_pixels_depth24( zf, image, &hdr, &data_bounds ) ) {
			goto data_short;
		}
	} else if ( hdr.biPixelSize == 32 ) {
		if ( !get_bmp_pixels_depth32( zf, image, &hdr ) ) {
			goto data_short;
		}
	} else {
		// Unsupported bit depth
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