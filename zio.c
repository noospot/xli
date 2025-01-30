/* zio.c:
 *
 * this properly opens and reads from an image file, compressed or otherwise.
 *
 * jim frost 10.03.89
 *
 * this was hacked on 09.12.90 to cache reads and to use stdin.
 *
 * Copyright 1989, 1990 Jim Frost.  See included file "copyright.h" for
 * complete copyright information.
 *
 * Re-worked by Graeme W. Gill on 91/12/04 to add zrewind, zunread and
 * better performance of per character reads.
 *
 *
 */


#include "xli.h"
#include <ctype.h>
#include <string.h>

#ifdef VMS
	#define NO_UNCOMPRESS		/* VMS doesn't have uncompress */
#endif

/* ANSI C doesn't declare popen in stdio.h ! */
FILE* popen( const strbyte*, const strbyte* );

#define MAX_ZFILES 32



static ZFILE ZFileTable[MAX_ZFILES];
static bool ZForceCache = FALSE;

int zread( ZFILE* zf, byte* buf, int len )
{
	int lentoread = len;
	struct cache* dataptr = zf->dataptr;

	while ( len > 0 && !zf->eof ) {
		/* Read any data in cache buffers or aux buffer */
		if ( zf->bufptr < zf->endptr ) {
			int readlen = zf->endptr - zf->bufptr;
			if ( readlen > len ) {
				readlen = len;
			}
			memcpy( buf, zf->bufptr,  readlen );
			buf += readlen;
			len -= readlen;
			zf->bufptr += readlen;
			continue;
		}
		/* else we have run out of buffered data */

		/* If we were reading from an aux buffer, */
		/* retore the previous state */
		if ( zf->auxb != NULL ) {		/* restore previous buffer */
			if ( zf->auxb != zf->buf ) {	/* if not getc buffer */
				lfree( zf->auxb );      /* was un-read buffer */
			}
			zf->auxb = NULL;
			zf->bufptr = zf->oldbufptr;
			zf->endptr = zf->oldendptr;
			continue;
		}
		/* If this is a newly opened file or we are */
		/* no longer reading previously cached data */
		if ( dataptr == NULL ) {
			if ( zf->nocache ) {	/* If the cache is turned off, read directly */
				if ( len > 1 ) {	/* more than a byte being read */
					len -= fread( buf, 1, len, zf->stream );
					if ( len > 0 ) {
						zf->eof = TRUE;
					}
					continue;
				}
				/* else we are reading one byte directly - this is a little */
				/* inefficient :-). Buffer it instead. */
				zf->bufptr = zf->buf;
				zf->endptr = zf->bufptr + fread( zf->bufptr, 1, BUFSIZ, zf->stream );
				if ( zf->endptr == zf->bufptr ) {
					zf->eof = TRUE;
				}
				continue;
			}
			/* else fall through to deal with first cached buffered read */
		} else {	/* we must move on to the next cache buffer */
			if ( zf->bufptr == NULL ) {	/* If this is a first re-read of the cache */
				zf->bufptr = dataptr->buf;	/* init the pointers */
				zf->endptr = dataptr->end;
				continue;
			}
			/* If there are more buffers, then advance */
			/* to the next one */
			if ( dataptr->next != NULL ) {
				/* advance to the next buffer */
				dataptr = zf->dataptr = dataptr->next;
				zf->bufptr = dataptr->buf;
				zf->endptr = dataptr->end;
				continue;
			}
			/* There are no more buffers */
			if ( dataptr->eof ) {
				zf->eof = TRUE;		/* we know that was the last one */
				continue;
			}
		}

		/* We have come to the end of the cache buffers or */
		/* we are doing the first cached read. */

		/* If the cache is off, switch it fully */
		/* out of the picture from now on */
		if ( zf->nocache ) {	/* read directly */
			zf->dataptr = dataptr = NULL;
			zf->bufptr = NULL;
			zf->endptr = NULL;
			continue;
		}
		/* We are caching and have to read another buffer or maybe we have */
		/* to read the very first buffer (zf->data and dataptr == NULL) */
		zf->dataptr = ( struct cache* ) lmalloc( sizeof( struct cache ) );
		zf->dataptr->next = NULL;
		zf->dataptr->eof = FALSE;
		if ( zf->data == NULL ) {	/* If very first read */
			zf->data = zf->dataptr;      /* set to first allocated buffer */
		} else {	/* link buffer into chain */
			dataptr->next = zf->dataptr;
		}
		dataptr = zf->dataptr;
		zf->bufptr = dataptr->buf;
		zf->endptr = dataptr->end = zf->bufptr + fread( zf->bufptr, 1, BUFSIZ, zf->stream );
		dataptr->eof = ( ( zf->endptr - zf->bufptr ) < BUFSIZ );
	}
	return ( lentoread - len );
}

#ifdef ZAPTOCACHE
/* append the given buffer data to the files read cache */
/* (Allows reading blocks directly from input stream while */
/*  maintaining cache data) */
static void _zaptocache( ZFILE* zf, byte* cbuf, int blen )
{
	struct cache* newdat, *dat;
	int len;
	while ( blen > 0 ) {	/* until we've stashed all of input */
		len = blen;
		if ( len > BUFSIZ ) {
			len = BUFSIZ;
		}

		/* create new cache data block */
		newdat = ( struct cache* ) lmalloc( sizeof( struct cache ) );
		memcpy( newdat->buf, cbuf,  len );
		newdat->end = newdat->buf + len;
		newdat->next = NULL;
		newdat->eof = FALSE;

		/* append it to cache buffers */
		if ( zf->data == NULL ) {		/* nothing read yet */
			zf->data = newdat;
			zf->dataptr = zf->data;
		} else {
			for ( dat = zf->data; dat->next != NULL; dat = dat->next );	/* go to end of chain */
			dat->next = newdat;
			dat->eof = FALSE;
		}
		cbuf += len;
		blen -= len;
	}
}
#endif
/* Return the files EOF status */

int zeof( ZFILE* zf )
{
	return zf->eof;
}

void zclearerr( ZFILE* zf )
{
	clearerr( ( zf )->stream );
	zf->eof = 0;
}


/* reset a read cache (reset means really close the file)
 */

void zreset( strbyte* filename )
{
	int a;
	ZFILE* zf;

	if ( TRUE == ZForceCache ) {
		return;
	}

	/* if NULL filename, reset the entire table
	 */
	if ( !filename ) {
		for ( a = 0; a < MAX_ZFILES; a++ )
			if ( ZFileTable[a].filename ) {
				zreset( ZFileTable[a].filename );
			}
		return;
	}
	for ( zf = ZFileTable; zf < ( ZFileTable + MAX_ZFILES ); zf++ )
		if ( zf->filename && !strcmp( filename, zf->filename ) ) {
			break;
		}

	if ( zf == ( ZFileTable + MAX_ZFILES ) ) {	/* no go joe */
		return;
	}

	_zreset( zf );
}

/* reset by file descriptor */
void _zreset( ZFILE* zf )
{
	struct cache* old;
	if ( zf->dataptr != zf->data )
		fprintf( stderr, "zreset: warning: ZFILE for %s was not closed properly\n",
		         zf->filename );
	while ( zf->data ) {
		old = zf->data;
		zf->data = zf->data->next;
		lfree( ( byte* ) old );
	}
	lfree( ( byte* ) zf->filename );
	zf->filename = NULL;
	zf->data = NULL;
	zf->dataptr = NULL;
	zf->nocache = FALSE;
	if ( zf->auxb != NULL && zf->auxb != zf->buf ) {
		lfree( ( byte* ) zf->auxb );
	}
	zf->auxb = NULL;
	zf->bufptr = NULL;
	zf->endptr = NULL;
	zf->eof = FALSE;

	switch ( zf->type ) {
		case ZSTANDARD:
			fclose( zf->stream );
			break;
#ifndef NO_UNCOMPRESS
		case ZPIPE:
			pclose( zf->stream );
			break;
#endif				/* NO_UNCOMPRESS */
		case ZSTDIN:
			break;
		default:
			fprintf( stderr, "zreset: bad ZFILE structure\n" );
			exit( 1 );
	}
}

/* discard all data that has been read. Return to state just after file was opened */
void _zclear( ZFILE* zf )
{
	struct cache* old;

	if ( zf->auxb != NULL && zf->auxb != zf->buf ) {
		lfree( zf->auxb );
	}

	while ( zf->data ) {
		old = zf->data;
		zf->data = zf->data->next;
		free( old );
	}
	zf->auxb = NULL;
	zf->bufptr = NULL;
	zf->endptr = NULL;
	zf->eof = FALSE;
	zf->data = NULL;
	zf->dataptr = NULL;
	zf->nocache = FALSE;
}

ZFILE* zopen( strbyte* name )
{
	ZFILE* zf;

	/* look for filename in open file table
	 */

	for ( zf = ZFileTable; zf < ( ZFileTable + MAX_ZFILES ); zf++ )
		if ( zf->filename && !strcmp( name, zf->filename ) ) {

			/* if we try to reopen a file whose caching was
			 * disabled, warn the user and try to recover.
			 * we cannot recover if it was stdin.
			 */

			if ( zf->nocache ) {
				if ( zf->type == ZSTDIN ) {
					fprintf( stderr, "zopen: caching was disabled by previous caller; can't reopen stdin\n" );
					return ( NULL );
				}
				fprintf( stderr, "zopen: warning: caching was disabled by previous caller\n" );
				zreset( zf->filename );	/* remove entry and treat like new open */
				break;
			}
			if ( zf->dataptr != zf->data ) {
				fprintf( stderr, "zopen: warning: file doubly opened\n" );
			}
			zf->dataptr = zf->data;		/* re-start with cache if it exists */
			if ( zf->auxb != NULL && zf->auxb != zf->buf ) {
				lfree( ( byte* ) zf->auxb );
			}
			zf->auxb = NULL;
			zf->bufptr = NULL;
			zf->endptr = NULL;
			zf->eof = FALSE;
			return ( zf );
		}
	/* find unused ZFileTable entry
	 */

	for ( zf = ZFileTable; zf < ( ZFileTable + MAX_ZFILES ) && zf->filename; zf++ )
		/* EMPTY */
		;

	if ( zf >= ( ZFileTable + MAX_ZFILES ) ) {
		fprintf( stderr, "zopen: no more files available\n" );
		exit( 1 );
	}
	zf->filename = dupString( name );
	if ( !_zopen( zf ) ) {	/* failed */
		lfree( ( byte* )zf->filename );
		zf->filename = NULL;
		return ( NULL );
	}
	return ( zf );
}

/* Do an open given a file pointer */
/* Return TRUE if open suceeded */
bool
_zopen( ZFILE* zf )
{
	strbyte* name = zf->filename;

	zf->data = NULL;
	zf->dataptr = NULL;
	zf->auxb = NULL;
	zf->bufptr = NULL;
	zf->endptr = NULL;
	zf->nocache = FALSE;
	zf->eof = FALSE;


	/* file filename is `stdin' then use stdin
	 */

	if ( !strcmp( name, "stdin" ) ) {
		zf->type = ZSTDIN;
		zf->stream = stdin;
	} else {
		strbyte const* cmd = 0;

#ifndef NO_UNCOMPRESS
		/* if filename ends in `.Z' '.gz' or '.bz2', assign command to cmd
		 * if the system doesn't have uncompress, 
		 * NO_UNCOMPRESS can be defined and it just won't try.
		 */

#ifdef HAVE_GUNZIP
		/* use gunzip with both .gz and .Z */
		if (
		      ( strlen( name ) > 3 &&
		        !strcasecmp( ".gz", name + strlen( name ) - 3 ) ) ||
		      ( strlen( name ) > 2 &&
		        !strcasecmp( ".Z", name + strlen( name ) - 2 ) )
		) {
			cmd = "gunzip -c ";
		}
#else
		/* it's a unix .Z compressed file, so use uncompress */
		if ( ( strlen( name ) > ( unsigned ) 2 ) &&
		      !strcmp( ".Z", name + ( strlen( name ) - 2 ) ) ) {
			cmd = "uncompress -c ";
		}
#endif /* HAVE_GUNZIP */
#ifdef HAVE_BUNZIP2
		if ( strlen( name ) > 4 &&
		      !strcasecmp( ".bz2", name + strlen( name ) - 4 ) ) {
			cmd = "bunzip2 -c ";
		}
#endif /* HAVE_BUNZIP2 */
#endif /* NO_UNCOMPRESS */

		if ( cmd ) {
			strbyte* buf, *s, *t;

			/* protect in single quotes, replacing single quotes
			 * with '"'"', so worst-case expansion is 5x
			 */
			buf = ( strbyte* ) lmalloc( strlen( cmd ) + 1 + 5 * strlen( name ) + 1 + 1 );
			strcpy( buf, cmd );
			s = buf + strlen( buf );
			*s++ = '\'';
			for ( t = name; *t; ++t ) {
				if ( '\'' == *t ) {
					strcpy( s, "'\"'\"'" );
					s += strlen( s );
				} else {
					*s++ = *t;
				}
			}
			*s++ = '\'';
			*s = '\0';

			zf->type = ZPIPE;
			zf->stream = popen( buf, "r" );
			lfree( ( byte* )buf );
		} else {
			/* default to normal stream
			 */
			zf->type = ZSTANDARD;
#ifdef VMS
			zf->stream = fopen( name, "r", "ctx=bin", "ctx=stm", "rfm=stmlf" );
#else
			zf->stream = fopen( name, "r" );
#endif
		}
	}

	if ( !zf->stream ) {
		return ( FALSE );
	}

	return ( TRUE );
}

/* The function zgetc, rather than the macro */
int _zgetc( ZFILE* zf )
{
	byte c;

	if ( zf->bufptr < zf->endptr ) {
		return *zf->bufptr++;
	}
	if ( zread( zf, &c, 1 ) > 0 ) {
		return ( c );
	} else {
		return ( EOF );
	}
}

// get string line ending with \n
strbyte* zgets( strbyte* buf, unsigned int size, ZFILE* zf )
{
	if ( ( --size ) <= 0 ) {
		return NULL;
	}
	{
		int ssize = zf->endptr - zf->bufptr;
		if ( ssize > 0 ) {	/* can do this fast */
			register byte* bp = zf->bufptr, *be;
			be = zf->bufptr + ( size < ( unsigned int )ssize ? size : ( unsigned int )ssize );
			while ( bp < be ) {
				if ( *bp++ == '\n' ) {
					break;
				}
			}
			if ( bp < be || size < ( unsigned int )ssize ) {
				memcpy( buf, zf->bufptr,  bp - zf->bufptr );
				buf[bp - zf->bufptr] = '\000';
				zf->bufptr = bp;
				return (  buf );
			}
		}
	}
	/* do this slower */
	{
		register strbyte* cp = buf;
		register int c;
		while ( cp < ( buf + size ) && ( c = zgetc( zf ) ) != EOF ) {
			*cp++ = c;
			if ( c == '\n' ) {
				break;
			}
		}
		if ( cp > buf ) {	/* we read something */
			*cp = '\000';
			return ( buf );
		}
	}
	return ( NULL );
}

/* Return a block of data back to the input stream.
 * Usefull if a load routine does its own buffering
 * and wants to return what is left after it has read an image.
 */
void zunread( ZFILE* zf, byte const* buf, int len )
{
	byte* new;
	/* int sizeaxb;	*/	/*  size of aux buffer */
	int noinaxb;		/* number unread in aux buffer */
	int nofraxb;		/* number free in aux buffer */

	if ( len == 0 ) {
		return;
	}
	if ( zf->auxb != NULL ) {
		/* sizeaxb = zf->endptr - zf->auxb; */	/* size of aux buffer */
		noinaxb = zf->endptr - zf->bufptr;	/* number unread in aux buffer */
		nofraxb = zf->bufptr - zf->auxb;	/* number free in aux buffer */
	} else {
		zf->oldbufptr = zf->bufptr;
		zf->oldendptr = zf->endptr;
		/* sizeaxb = */ noinaxb = nofraxb = 0;
	}
	if ( len <= nofraxb ) {	/* no need to alloc more */
		zf->bufptr -= len;
		memcpy( zf->bufptr, buf, len );
	} else {		/* need some more space */
		int extra = 0;
		if ( len < 100 ) {
			extra = 100;
		}
		new = ( byte* ) lmalloc( extra + noinaxb + len );
		memcpy( new + extra, buf, len );	/* copy new aux data */
		if ( noinaxb != 0 ) {	/* copy old data */
			memcpy( new + extra + len, zf->bufptr, noinaxb );
			if ( zf->auxb != NULL && zf->auxb != zf->buf ) {
				lfree( zf->auxb );
			}
		}
		zf->auxb = new;
		zf->bufptr = new + extra;
		zf->endptr = new + extra + noinaxb + len;
	}
	zf->eof = FALSE;
}


/* this turns off caching when an image has been identified and we will not
 * need to re-open it
 */
void znocache( ZFILE* zf )
{
	if ( FALSE == ZForceCache ) {
		zf->nocache = TRUE;
	}
}


void zforcecache( bool v )
{
	ZForceCache = v;
}


/* reset cache pointers in a ZFILE.  nothing is actually reset until a
 * zreset() is called with the filename.
 */
void zclose( ZFILE* zf )
{
	zf->dataptr = zf->data;
	if ( zf->auxb != NULL && zf->auxb != zf->buf ) {
		lfree( zf->auxb );
	}
	zf->auxb = NULL;
	zf->bufptr = NULL;
	zf->endptr = NULL;
	zf->eof = FALSE;
}

/* close the file and then re-open it. */
/* Return TRUE on sucess. */
bool
_zreopen( ZFILE* zf )
{
	strbyte* tname;
	tname = dupString( zf->filename );
	_zreset( zf );
	zf->filename = tname;
	if ( !_zopen( zf ) ) {	/* failed */
		lfree( ( byte* )zf->filename );
		zf->filename = NULL;
		return ( FALSE );
	}
	return ( TRUE );
}

/* Rewind the cached file. Warn the user if
 * this is not likely to work.
 * Return TRUE on sucess.
 */

bool zrewind( ZFILE* zf )
{
	if ( zf->nocache ) {
		if ( zf->type == ZSTDIN ) {
			fprintf( stderr, "zrewind: caching was disabled by previous caller; can't rewind\n" );
			return ( FALSE );
		}
		fprintf( stderr, "zrewind: warning: caching was disabled by previous caller\n" );
		return !_zreopen( zf );
	}
	zf->dataptr = zf->data;
	if ( zf->auxb != NULL && zf->auxb != zf->buf ) {
		lfree( zf->auxb );
	}
	zf->auxb = NULL;
	zf->bufptr = NULL;
	zf->endptr = NULL;
	zf->eof = FALSE;
	return TRUE;
}

