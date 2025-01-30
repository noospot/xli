//
// tiff.c
//
// compiling require adding    -ltiff
//
// rno 20250112
//
//
#include "xli.h"
#include "imagetypes.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <tiff.h>
#include <tiffio.h>


int tiff_info( strbyte* filename_with_path, strbyte* name, bool verbose )
{
	CURRFUNC( "tiffIdent" );

	if ( filename_with_path == NULL ) {
		return 0;
	}
	FILE* fp = fopen( filename_with_path, "rb" );
	if ( fp == NULL ) {
		return 0;
	}

	uint8_t magic[4];
	magic[0]=fgetc( fp );
	magic[1]=fgetc( fp );
	magic[2]=fgetc( fp );
	magic[3]=fgetc( fp );

	fclose( fp );

	//printf("magic:%0x08\n",*magic);
	//if( !( magic==0x49492A00 || magic==0x4D4D002A ) ) { return 0; }

	bool II = ( magic[0] == 'I' &&
	               magic[1] == 'I' &&
	               magic[2] == 42 &&
	               magic[3] == 0 );

	bool MM = ( magic[0] == 'M' &&
	               magic[1] == 'M' &&
	               magic[2] == 0 &&
	               magic[3] == 42 );

	if ( !( II || MM ) ) {
		return 0;
	}

	TIFF* tif = TIFFOpen( filename_with_path, "r" );
	if ( tif == NULL ) {
		return 0;
	}

	unsigned int width = 0;
	unsigned int height = 0;
	unsigned int bpp = 0;

	TIFFGetField( tif, TIFFTAG_IMAGEWIDTH, &width );
	TIFFGetField( tif, TIFFTAG_IMAGELENGTH, &height );
	TIFFGetField( tif, TIFFTAG_SAMPLESPERPIXEL, &bpp );

	if ( verbose ) {
		printf( "%s is a %dx%d TIFF image, %d bytes/pixel\n", name, width, height, bpp );
	}

	TIFFClose( tif );

	return 1;
}

int tiffIdent( strbyte* filename_with_path,  strbyte* name )
{
	bool res=tiff_info( filename_with_path, name, 1 );
	return res;
}

// apply alpha to a single register R, G or B
static inline byte byte_applyalpha3( byte colorreg, byte alpha )
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

Image* tiffLoad( strbyte* filename_with_path, ImageOptions* image_ops, bool verbose )
{
	CURRFUNC( "tiffLoad" );

	Image* rec;
	memset( &rec, 0, sizeof( rec ) );

	if( !tiff_info( filename_with_path, image_ops->name, verbose ) ) {
		return rec;
	}

	TIFF* tif = TIFFOpen( filename_with_path, "r" );
	if ( tif == NULL ) {
		return rec;
	}

	unsigned int width = 0;
	unsigned int height = 0;
	unsigned int bpp = 0;

	TIFFGetField( tif, TIFFTAG_IMAGEWIDTH, &width );
	TIFFGetField( tif, TIFFTAG_IMAGELENGTH, &height );
	TIFFGetField( tif, TIFFTAG_SAMPLESPERPIXEL, &bpp );


	uint32_t* tiff_data = ( uint32_t* )_TIFFmalloc( width * height * sizeof( uint32_t ) );
	if ( !tiff_data ) {
		fprintf( stderr, "Couldn't get memory to load Tiff file\n" );
		return rec;
	}

	byte res=TIFFReadRGBAImageOriented( tif, width, height,  tiff_data, ORIENTATION_TOPLEFT, 0 );
	if ( res!=1 ) {
		_TIFFfree( tiff_data );
		fprintf( stderr, "Error decoding Tiff file\n" );
		return rec;
	}

	rec = newTrueImage( width, height );
	rec->title = dupString( image_ops->name );

	byte a=0;
	size_t i = 0;
	while ( i < width * height ) {
		if ( bpp % 4==0 ) {
			a = TIFFGetA( tiff_data[i] );
			rec->data[i*3 + 0] = byte_applyalpha3( TIFFGetR( tiff_data[i] ), a );
			rec->data[i*3 + 1] = byte_applyalpha3( TIFFGetG( tiff_data[i] ), a );
			rec->data[i*3 + 2] = byte_applyalpha3( TIFFGetB( tiff_data[i] ), a );
		} else {
			rec->data[i*3 + 0] = TIFFGetR( tiff_data[i] );
			rec->data[i*3 + 1] = TIFFGetG( tiff_data[i] );
			rec->data[i*3 + 2] = TIFFGetB( tiff_data[i] );
		}
		i = i + 1;
	}

	_TIFFfree( tiff_data );

	TIFFClose( tif );

	if ( rec->data == NULL ) {
		fprintf( stderr, "Error decoding Tiff file\n" );
		return rec;
	}

	return rec;
}
