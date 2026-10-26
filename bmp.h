/*
 * bmp.h - header file for Bmp files
 */

#define C_WIN	1		/* Image bfType */
#define C_OS2	2
#define C_OS2_V2	3
#define C_WIN_V2	4
#define C_WIN_V3	5
#define C_WIN_V4	6	
#define C_WIN_V5	7


#define BMP_FILEHEADER_LEN 14

#define BI_RGB  0	       /* Compression type */
#define BI_RLE8 1
#define BI_RLE4 2
#define BI_FIELDS 3 
#define BI_JPEG 4
#define BI_PNG  5

#define BI_HUFF1D 3         /* Compression type, OS/2 v2*/
#define BI_RLE24 4

// info header

#define WIN_INFOHEADER_LEN	40
#define OS2_INFOHEADER_LEN	12
#define OS2_V2_INFOHEADER_MIN_LEN	16
#define OS2_V2_INFOHEADER_MAX_LEN	64
#define BMP_V2INFOHEADER_LEN	52
#define BMP_V3INFOHEADER_LEN	56
#define BMP_V4INFOHEADER_LEN	108
#define BMP_V5INFOHEADER_LEN	124

// d: definition for maximum bit depth supporting a color palette
#define PAL_MAX 8

/* Header structure definition. */
typedef struct {
	     strbyte* name;		/* stash pointer to name here too */
	          int bfType;		/* Windows or OS/2 */

	unsigned long bfFileSize;	/* Size of file in bytes */
	unsigned int  bfxHotSpot;	/* Not used */
	unsigned int  bfyHotSpot;	/* Not used */
	unsigned long bfImageOffset;	/* Offset of image from start of header */

	unsigned long biInfoHeaderSize;	/* Size of info header in bytes */
	unsigned long biWidth;		/* Image width in pixels */
	      int32_t biHeight;		/* Image height in pixels */
	unsigned int  biPlanes;		/* Planes. Must == 1 */
	
	unsigned int  biPixelSize;	/* Bits per pixels. Must be one of 1 2 4 8 16 24 32 64 */
	unsigned long biCompression;	/* Compression type */
	unsigned long biSizeImage;	/* Size of image in bytes */
	unsigned long biXPelsPerMeter;	/* X pixels per meter */
	unsigned long biYPelsPerMeter;	/* Y pixels per meter */
	unsigned long biClrUsed;	/* Number of colormap entries (0 == max) */
	unsigned long biClrImportant;	/* Number of important colors */
	unsigned long redMask;		/* BI_FIELDS Red channel mask    */
	unsigned long greenMask;	/* BI_FIELDS Green channel mask  */
	unsigned long blueMask;		/* BI_FIELDS Blue channel mask   */
	unsigned long alphaMask;	/* BI_FIELDS Alpha channel mask  */
	
	unsigned long all_headers_len;	/* Cumulative size of all headers and masks */
	unsigned long masks_header_len;	/* Size of BI_FIELDS mask header extension */
	unsigned long palette_len;	/* Calculated byte length of color palette */
	unsigned long color_entry_len;	/* Length of individual palette entry (3 or 4 bytes) */
} bmpHeader;
