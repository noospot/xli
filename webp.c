//
// webp.c
//
// compiling require adding    -lwebp
//
// rno 20250101
//
//
#include "xli.h"
#include "imagetypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <webp/decode.h>

#define NOTALLOC NULL

// apply alpha to a single register R, G or B
static inline byte colorreg_applyalpha( byte colorreg, byte alpha )
{
	if( alpha==0x00 ) {
		return 0x00;
	}
	if( alpha==0xFF ) {
		return colorreg;
	}
	double tmp= ( double )( ( double )colorreg*( double )alpha )/255.0;
	if ( tmp<0.0 ) {
		tmp=0.0;
	}
	if ( tmp>255.0 ) {
		tmp=255.0;
	}
	return ( byte )round( tmp );
}


int filesize( const strbyte* file )
{
	FILE* fh = fopen( file, "rb" );
	if ( !fh ) {
		fprintf( stderr, "xli/filesize() Error opening file: %s\n", file );
		return 0;
	}
	fseek( fh, 0, SEEK_END );
	int file_size = ftell( fh );
	fseek( fh, 0, SEEK_SET );
	fclose( fh );
	return( file_size );
}

byte* file_fetch( const strbyte* file )
{
	FILE* fh = fopen( file, "rb" );
	if ( !fh ) {
		fprintf( stderr, "Error opening file: %s\n", file );
		return NOTALLOC;
	}

	fseek( fh, 0, SEEK_END );
	int  file_size = ftell( fh );
	fseek( fh, 0, SEEK_SET );

	if ( file_size <= 0 ) {
		fprintf( stderr, "Error: Empty or invalid file: %s\n", file );
		fclose( fh );
		return NOTALLOC;
	}

	byte* file_data = ( byte* )calloc( file_size, sizeof( byte ) );
	if ( !file_data ) {
		fprintf( stderr, "Memory allocation error\n" );
		fclose( fh );
		return NOTALLOC;
	}

	int bytes_read = fread( file_data, 1, file_size, fh );
	fclose( fh );
	if ( bytes_read != file_size ) {
		fprintf( stderr, "Error reading file: %s\n", file );
		free( file_data );
		return NOTALLOC;
	}

	return file_data;
}



static inline uint32_t fetch_int32( FILE* fh )
{
	uint32_t fi=0;
	fi=fi+( fgetc( fh )<<24 );
	fi=fi+( fgetc( fh )<<16 );
	fi=fi+( fgetc( fh )<<8  );
	fi=fi+( fgetc( fh )     );

	return fi;
}
static inline uint32_t fetch_leint32( FILE* fh )
{
	uint32_t fi=0;
	fi=fi+( fgetc( fh )     );
	fi=fi+( fgetc( fh )<<8  );
	fi=fi+( fgetc( fh )<<16 );
	fi=fi+( fgetc( fh )<<24 );

	return fi;
}
// d: check if a file is likely a WebP image based on its header.
int info_about_webp( strbyte* filename_with_path, bool verbose )
{
	CURRFUNC( "info_about_webp" );

	FILE* file = fopen( filename_with_path, "rb" );
	if ( !file ) {
		return 0; // Failed to open the file
	}

	unsigned int f =  fetch_int32( file );
	// 'RIFF'
	if ( f != 0x52494646 ) {
		fclose( file );return 0;
	}
	size_t filesize =  fetch_leint32( file );
	if( filesize<25 ) {
		fclose( file );return 0;
	}
	f =  fetch_int32( file );
	// 'WEBP'
	if ( f != 0x57454250 ) {
		fclose( file );return 0;
	}
	f =  fetch_int32( file );
	// 'VP8 '
	if ( f == 0x56503820 ) {
		if( filesize<26 ) {
			fclose( file );return 0;
		}
		size_t bs_size =  fetch_leint32( file );
		( void )bs_size;
		f = ( fgetc( file )  ); // todo see rfc6386  9.1
		f = ( fgetc( file )  ); //
		f = ( fgetc( file )  ); //

		f = ( fgetc( file )  );
		if ( f!=0x9d ) {
			printf( "webp vp8 error byte 0\n" );
			fclose( file );return 0;
		}
		f = ( fgetc( file )  );
		if ( f!=0x01 ) {
			printf( "webp vp8 error byte 1\n" );
			fclose( file );return 0;
		}
		f = ( fgetc( file )  );
		if ( f!=0x2a ) {
			printf( "webp vp8 error byte 2\n" );
			fclose( file );return 0;
		}

		// 14 bits width
		uint16_t webp_width = 0;
		f = ( fgetc( file )  );
		webp_width = webp_width + ( f );
		f = ( fgetc( file )  );
		webp_width = webp_width + ( f<<8 );
		webp_width = ( webp_width& 0x3FFF );
		// 2 bits horizontal scaling
		byte webp_hscale = ( f>>14 )&0x3 ;
		// 14 bits height
		uint16_t webp_height =0;
		f = ( fgetc( file )  );
		webp_height = webp_height + ( f );
		f = ( fgetc( file )  );
		webp_height = webp_height + ( f<<8 );
		webp_height = ( webp_height& 0x3FFF );
		// 2 bits vertical scaling
		byte webp_vscale = ( f>>14 )&0x3 ;
		( void )webp_hscale;
		( void )webp_vscale;
		if ( verbose ) {
			printf( "%s is a %ix%i ",filename_with_path,webp_width,webp_height );
		}
		if ( verbose ) {
			printf( "WebP image, VP8 , lossy, " );
		}
		//if (has_alpha) printf( "with alpha, ");
		if ( verbose ) {
			printf( "\n" );
		}
		fclose( file );
		return 1;
	}
	// 'VP8L'
	if ( f == 0x5650384C ) {
		size_t bs_size =  fetch_leint32( file );
		( void )bs_size;
		// VP8L_MAGIC_BYTE
		f = ( fgetc( file )  );
		if ( f != 0x2F )  {
			printf( "webp vp8l error 0x2F\n" );
			fclose( file );return 0;
		}

		f =  fetch_leint32( file );

		byte has_alpha= ( f>>28 )&0x1;
		byte version_number = ( f>>28 )&0xE;

		uint16_t webp_height = ( ( f>>14 )& 0x3FFF ) + 1;
		uint16_t webp_width = ( ( f )& 0x3FFF ) + 1;

		if ( verbose ) {
			printf( "%s is a %ix%i ",filename_with_path,webp_width,webp_height );
		}
		if ( verbose ) {
			printf( "WebP image, VP8L, lossless, " );
		}
		if ( has_alpha ) if ( verbose ) {
				printf( "with alpha, " );
			}

		if ( version_number != 0x0 )  {
			printf( "webp vp8l version!=0 error %0x3 \n",version_number );
			fclose( file );return 0;
		}

		if ( verbose ) {
			printf( "\n" );
		}
		fclose( file );
		return 2;
	}
	// 'VP8X'
	if ( f == 0x56503858 ) {
		if( filesize<26 ) {
			fclose( file );return 0;
		}
		size_t bs_size =  fetch_leint32( file );
		( void )bs_size;
		byte webp_animated=0;
		byte has_alpha=0;
		f = ( fgetc( file )  );
		if ( ( f>>6 & 0x3 )!=0 ) {;} // MUST be 0. Readers MUST ignore field.
		if ( ( f>>5 & 0x1 ) ) {;} // has icc
		if ( ( f>>4 & 0x1 ) ) {
			has_alpha=1;
		} // has alpha
		if ( ( f>>3 & 0x1 ) ) {;} // has exif
		if ( ( f>>2 & 0x1 ) ) {;} // has xmp
		if ( ( f>>1 & 0x1 ) ) {
			webp_animated=1;
		}
		if ( ( f & 0x1 )!=0 ) {;} // MUST be 0. Readers MUST ignore field.
		f = ( fgetc( file )  ); // MUST be 0. Readers MUST ignore field.
		f = ( fgetc( file )  ); // MUST be 0. Readers MUST ignore field.
		f = ( fgetc( file )  ); // MUST be 0. Readers MUST ignore field.

		// 24bits canvas width
		unsigned int webp_width = 1;
		f = ( fgetc( file )  );
		webp_width = webp_width + ( f );
		f = ( fgetc( file )  );
		webp_width = webp_width + ( f<<8 );
		f = ( fgetc( file )  );
		webp_width = webp_width + ( f<<16 );
		f = f + 1;
		// 24bits canvas height
		unsigned int webp_height = 1;
		f = ( fgetc( file )  );
		webp_height = webp_height + ( f );
		f = ( fgetc( file )  );
		webp_height = webp_height + ( f<<8 );
		f = ( fgetc( file )  );
		webp_height = webp_height + ( f<<16 );
		f = f + 1;

		// byte has_alpha= 0;

		if ( verbose ) {
			printf( "%s is a %ix%i ",filename_with_path,webp_width,webp_height );
		}
		if ( verbose ) {
			printf( "WebP image, VP8X, extended, " );
		}
		if ( has_alpha ) {
			if ( verbose ) {
				printf( "with alpha, " );
			}
		}
		if ( webp_animated ) {
			if ( verbose ) {
				printf( "animated, " );
			}
			if ( verbose ) {
				printf( "unsupported\n" );
			}
			fclose( file );return 0;
		}
		if ( verbose ) {
			printf( "\n" );
		}
		fclose( file );
		return 3;
	}

	fclose( file );
	return 1;
}

// d: check if a file is likely a WebP image based on its header.
int webpIdent( strbyte* filename_with_path,  strbyte* name )
{
	( void )name;
	CURRFUNC( "webpIdent" );
	return info_about_webp( filename_with_path,1 );
	// file is likely a webp file
	return 1;
}


Image* webpLoad( strbyte* filename_with_path, ImageOptions* image_ops, bool verbose )
{
	CURRFUNC( "webpLoad" );
	( void )image_ops;
	( void )verbose;

	byte res=info_about_webp( filename_with_path,verbose );
	if ( res==0 ) {
		return 0;
	}


	byte* file_data= file_fetch( filename_with_path );
	int file_size= filesize( filename_with_path );
	int32_t webp_width;
	int32_t webp_height;
	Image* rec;
	memset( &rec, 0, sizeof( rec ) );
	if ( file_data==NOTALLOC ) {
		return 0;
	}
	// Get WebP image dimensions
	if ( !WebPGetInfo( file_data, file_size, &webp_width, &webp_height ) ) {
		if ( file_data!=NOTALLOC ) {
			free( file_data );
			file_data=NOTALLOC;
		}
		//fprintf( stderr, "Error: Could not get WebP info or invalid WebP file.\n" );
		return rec;
	}

	byte* rgba_tmp=calloc( ( webp_width*webp_height*4 ), sizeof( byte ) );
	rgba_tmp = WebPDecodeRGBAInto( file_data, file_size, rgba_tmp, ( webp_width*webp_height*4 ),( webp_width*4 ) );
	if ( file_data!=NOTALLOC ) {
		free( file_data );
		file_data=NOTALLOC;
	}

	rec = newTrueImage( webp_width,webp_height );
	rec->title = dupString( image_ops->name );

	byte a=0;
	int32_t i=0;
	while ( i<( webp_width*webp_height ) ) {
		a=rgba_tmp[i*4+3];
		rec->data[i*3]=colorreg_applyalpha( rgba_tmp[i*4], a );
		rec->data[i*3+1]=colorreg_applyalpha( rgba_tmp[i*4+1], a );
		rec->data[i*3+2]=colorreg_applyalpha( rgba_tmp[i*4+2], a );
		i=i+1;
	}
	free( rgba_tmp );

	if ( rec->data == NULL ) {
		fprintf( stderr, "Error decoding WebP file\n" );
		return rec;
	}

	return rec;
}

