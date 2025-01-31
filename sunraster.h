/* sunraster.h
 *
 * header for Sun rasterfiles,
 * used to improve portability and to avoid distribution problems.
 * if having SunOS, a better description is /usr/include/rasterfile.h
 *
 * Copyright 1989 Jim Frost.  See included file "copyright.h" for complete
 * copyright information.
 */



struct rheader {
	byte magic[4];   /* magic number */
	byte width[4];   /* width of image in pixels */
	byte height[4];  /* height of image in pixels */
	byte depth[4];   /* depth of each pixel */
	byte length[4];  /* length of the image in bytes */
	byte type[4];    /* format of file */
	byte maptype[4]; /* type of colormap */
	byte maplen[4];  /* length of colormap in bytes */
};

/* following the header is the colormap (unless maplen is zero) then
 * the image.  each row of the image is rounded to 2 bytes.
 */

#define RMAGICNUMBER 0x59a66a95 /* sunraster files magic number */

/* these are the possible file formats
 */

#define ROLD       0 /* old format, see /usr/include/rasterfile.h */
#define RSTANDARD  1 /* standard format */
#define RRLENCODED 2 /* run length encoding */
#define RRGB       3 /* RGB-format instead of BGR in 24 or 32-bit mode */
#define RTIFF      4 /* TIFF <-> rasterfile */
#define RIFF       5 /* IFF (TAAC) <-> rasterfile */

/* these are the possible colormap types.  if it's in RGB format,
 * the map is made up of three byte arrays (red, green, then blue)
 * that are each 1/3 of the colormap length.
 */

#define RNOMAP  0 /* no colormap follows the header */
#define RRGBMAP 1 /* rgb colormap */
#define RRAWMAP 2 /* raw colormap; good luck */

#define RESC 128 /* run-length encoding escape */

