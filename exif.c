//
// libexif rotation function
//
//
//
//
#include "xli.h"

#include <libexif/exif-data.h>


Image* exif_rotation( Image* image, const strbyte* file )
{
	Image* tmpimage;
	ExifData* ed;
	ExifEntry* entry;
	int byte_order, rotation = 0;

	if ( ( ed = exif_data_new_from_file( file ) ) == NULL ) {
		return image;
	}
	byte_order = exif_data_get_byte_order( ed );
	entry = exif_content_get_entry( ed->ifd[EXIF_IFD_0], EXIF_TAG_ORIENTATION );
	if ( entry != NULL ) {
		rotation = exif_get_short( entry->data, byte_order );
	}
	exif_data_unref( ed );

	switch ( rotation ) {
		case 1:    // 1: Top-left,
			if ( globals.verbose ) {
				printf( "xli/exif: %i Top-left\n",rotation );
			}
			// rotate 0 degrees.
			// No rotation
			break;

		case 2: //  2: Top-right
			if ( globals.verbose ) {
				printf( "xli/exif: %i Top-right\n",rotation );
			}
			// flip vertical or mirror horizontally
			mirror_horizontal( image, image->width, image->height );
			break;
		case 3: //  3: Bottom-right
			if ( globals.verbose ) {
				printf( "xli/exif: %i Bottom-right\n",rotation );
			}
			// rotate 180 degrees.
			tmpimage = rotate( image, 180, globals.verbose );
			if ( tmpimage != image ) {
				freeImage( image );
			}
			image = tmpimage;
			break;
		case 4: //  4: Bottom-left
			if ( globals.verbose ) {
				printf( "xli/exif: %i Bottom-left\n",rotation );
			}
			mirror_vertical( image, image->width, image->height );
			// flip horizontal or mirror vertically
			break;
		case 5: //  5: Left-top
			if ( globals.verbose ) {
				printf( "xli/exif: %i Left-top\n",rotation );
			}
			// +mirror horizontally
			mirror_horizontal( image, image->width, image->height );
			// rotate -90 degrees.
			tmpimage = rotate( image, 270, globals.verbose );
			if ( tmpimage != image ) {
				freeImage( image );
			}
			image = tmpimage;
			break;
		case 6: //  6: Right-top
			if ( globals.verbose ) {
				printf( "xli/exif: %i Right-top\n",rotation );
			}
			// rotate 90 degrees.
			tmpimage = rotate( image, 90, globals.verbose );
			if ( tmpimage != image ) {
				freeImage( image );
			}
			image = tmpimage;
			break;
		case 7: //  7: Right-bottom
			if ( globals.verbose ) {
				printf( "xli/exif: %i Right-bottom\n",rotation );
			}
			// +mirror horizontally
			mirror_horizontal( image, image->width, image->height );
			// rotate 180 degrees.
			tmpimage = rotate( image, 90, globals.verbose );
			if ( tmpimage != image ) {
				freeImage( image );
			}
			image = tmpimage;
			break;
		case 8: // 8: Left-bottom
			if ( globals.verbose ) {
				printf( "xli/exif: %i Left-bottom\n",rotation );
			}
			// rotate angle -90
			tmpimage = rotate( image, 270, globals.verbose );
			if ( tmpimage != image ) {
				freeImage( image );
			}
			image = tmpimage;
			break;
		default:
			if ( globals.verbose ) {
				printf( "xli/exif: unknown rotation value %i\n", rotation );
			}
			break;
	}
	return( image );
}
