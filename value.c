/* value.c:
 *
 *
 * jim frost 10.02.89
 *
 * Copyright 1989 Jim Frost.
 * See included file "copyright.h" for complete copyright information.
 */


#include "xli.h"

/* flips all the bits in a length len byte array p at byte intervals */
void flipBits( byte* p, unsigned int len )
{
	static int init= 0;
	static byte flipped[256];

	if ( !init ) {
		int a, b;
		byte norm;

		for ( a= 0; a < 256; a++ ) {
			flipped[a]= 0;
			norm= a;
			for ( b= 0; b < 8; b++ ) {
				flipped[a]= ( flipped[a] << 1 ) | ( norm & 1 );
				norm = norm >> 1;
			}
		}
	}

	while ( len-- ) {
		p[len]= flipped[p[len]];
	}
}
