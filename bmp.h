//
// bmp.h - bmp images files
//
// image bfType
#define C_WIN		1
#define C_OS2		2
#define C_OS2_V2	3
#define C_WIN_V2	4
#define C_WIN_V3	5
#define C_WIN_V4	6
#define C_WIN_V5	7

#define BMP_FILEHEADER_LEN 14

// compression type, Windows
#define BI_RGB		0
#define BI_RLE8		1
#define BI_RLE4		2
#define BI_FIELDS	3
#define BI_JPEG		4
#define BI_PNG		5
#define BI_ALPHAFIELDS	6
// compression type, OS/2 v2
#define BI_HUFF1D	3
#define BI_RLE24	4

// info header size
#define WIN_INFOHEADER_LEN		40  // Standard Windows 3.x / NT
#define OS2_INFOHEADER_LEN		12  // OS/2 1.x / Windows 2.x core
#define OS2_V2_INFOHEADER_MIN_LEN	16  // OS/2 2.x minimum size
#define OS2_V2_INFOHEADER_MAX_LEN	64  // OS/2 2.x full/maximum
#define BMP_V2INFOHEADER_LEN		52  // Adobe Photoshop / Windows CE extension with RGB masks
#define BMP_V3INFOHEADER_LEN		56  // Adobe Photoshop / Windows CE extension with Alpha mask
#define BMP_V4INFOHEADER_LEN		108 // Windows 95 / NT 4.0 with color spaces
#define BMP_V5INFOHEADER_LEN		124 // Windows 98 / 2000 with ICC

// maximum depth supporting a color palette
#define PAL_MAX 8

typedef struct {
	     strbyte* name;		// image name
// file header
	          int bfType;		// Windows or OS/2
	unsigned long bfFileSize;	// size of file in bytes
	 unsigned int bfxHotSpot;	// not used
	 unsigned int bfyHotSpot;	// not used
	unsigned long bfImageOffset;	// offset of image from start of header
// info header
	unsigned long biInfoHeaderSize;	// size of info header in bytes
	unsigned long biWidth;		// image width in pixels
	      int32_t biHeight;		// image height in pixels
	 unsigned int biPlanes;		// planes. must == 1
	 unsigned int biPixelSize;	// bits per pixels. must be one of 1 2 4 8 16 24 32 64
	unsigned long biCompression;	// compression type
	unsigned long biSizeImage;	// size of image in bytes
	unsigned long biXPelsPerMeter;	// x pixels per meter
	unsigned long biYPelsPerMeter;	// y pixels per meter
	unsigned long biClrUsed;	// colormap entries number (0 == max)
	unsigned long biClrImportant;	// important colors number
// masks
	unsigned long redMask;		// red channel mask   
	unsigned long greenMask;	// green channel mask 
	unsigned long blueMask;		// blue channel mask  
	unsigned long alphaMask;	// alpha channel mask 
// resoved
	unsigned long all_headers_len;	// cumulative size of all headers and masks
	unsigned long masks_header_len;	// size of bi_fields mask header extension
	unsigned long palette_len;	// calculated byte length of color palette
	unsigned long color_entry_len;	// length of individual palette entry (3 or 4 bytes)
	unsigned long resolved_image_offset;	// resolved absolute pixel data offset
	unsigned long resolved_clr_used;	// resolved effective number of colormap entries
} bmpHeader;
