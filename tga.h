/*
 *tga.h - header file for Targa files
 */

#define TGA_HEADER_LEN 18

/* Header structure definition. */
typedef struct {
	strbyte* name;			/* stash pointer to name here too */
	unsigned int IDLength;		/* length of Identifier String                         1 byte */
	unsigned int CoMapType;		/* 0 = no map 1=map 0..127 targa reserved 128-255 dev  1 byte */
	unsigned int ImgType;		/* image type (see below for values)                   1 byte */
	unsigned int Index;		/* index of first color map entry                      2 byte */
	unsigned int Length;		/* number of entries in color map                      2 byte */
	unsigned int CoSize;		/* size of color map entry (15,16,24,32)               1 byte */
	int          X_org;		/* x origin of image                                   2 byte */
	int          Y_org;		/* y origin of image                                   2 byte */
	unsigned int Width;		/* width of image                                      2 byte */
	unsigned int Height;		/* height of image                                     2 byte */
	unsigned int PixelSize;		/* pixel size (usually 8,16,24,32 yet could be other)  1 byte */
	unsigned int AttBits;		/* 4 bits, number of attribute bits per pixel  (can be alpha) */
	unsigned int Rsrvd;		/* 1 bit, horizontal origin: 0=left , 1=right                 */
	unsigned int OrgBit;		/* 1 bit, vertical origin: 0=lower , 1=upper                  */
	unsigned int IntrLve;		/* 2 bits, interleaving flag                                  */
	bool      RLE;		        /* Run length encoded                                         */
	byte      reserved0;		/* padding                                                    */	
	byte      reserved1;		/* padding                                                    */
	byte      reserved2;		/* padding                                                    */	
} tgaHeader;

/* Definitions for image types.  0..127 targa reserved 128-255 dev */
#define TGA_Null 0      /* No image data included. */ /* Not used */
#define TGA_Map 1       /* color-mapped images. */
#define TGA_RGB 2       /* RGBA images. */
#define TGA_Mono 3      /* greyscale images. */
#define TGA_RLEMap 9    /* Runlength encoded color-mapped images. */
#define TGA_RLERGB 10   /* Runlength encoded RGBA images. */
#define TGA_RLEMono 11  /* Runlength encoded greyscale images. */

#define TGA_CompMap 32	/* color-mapped data, using Huffman, Delta, and runlength encoding. */ /* Not used */
#define TGA_CompMap4 33	/* color-mapped data, using Huffman, Delta, and runlength encoding. 4-pass quadtree-type process. */ /* Not used */

#define TGA_IMAGE_TYPE(tt) \
	tt ==  1 ? "Pseudo color " : \
	tt ==  2 ? "True color " : \
	tt ==  3 ? "Gray scale " : \
	tt ==  9 ? "Run length encoded Pseudo color " : \
	tt == 10 ? "Run length encoded True color " : \
	tt == 11 ? "Run length encoded Grayscale " : \
	           "Unknown "

/* Definitions for interleave flag. */
#define TGA_IL_None 0
#define TGA_IL_Two 1
#define TGA_IL_Four 2

#define TGA_IL_TYPE(tt) \
	tt == 0 ? "" : \
	tt == 1 ? "Two way interleaved " : \
	tt == 2 ? "Four way interleaved " : \
	          "Unknown "






