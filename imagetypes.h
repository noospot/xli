/* imagetypes.h:
 *
 * supported image types and the imagetypes array declaration.
 * to add a new image type, only the makefile and this header need to be
 * changed.
 *
 * jim frost 10.15.89
 */

Image* facesLoad( strbyte* fullname, ImageOptions* image_ops, bool verbose );
Image* pbmLoad( strbyte* fullname, ImageOptions* image_ops, bool verbose );
Image* sunRasterLoad( strbyte* fullname, ImageOptions* image_ops, bool verbose );
Image* gifLoad( strbyte* fullname, ImageOptions* image_ops, bool verbose );
Image* jpegLoad( strbyte* fullname, ImageOptions* image_ops, bool verbose );
Image* rleLoad( strbyte* fullname, ImageOptions* image_ops, bool verbose );
Image* bmpLoad( strbyte* fullname, ImageOptions* image_ops, bool verbose );

Image* xwdLoad( strbyte* fullname, ImageOptions* image_ops, bool verbose );
Image* xbitmapLoad( strbyte* fullname, ImageOptions* image_ops, bool verbose );
Image* xpixmapLoad( strbyte* fullname, ImageOptions* image_ops, bool verbose );
Image* g3Load( strbyte* fullname, ImageOptions* image_ops, bool verbose );
Image* fbmLoad( strbyte* fullname, ImageOptions* image_ops, bool verbose );
Image* pcxLoad( strbyte* fullname, ImageOptions* image_ops, bool verbose );
Image* imgLoad( strbyte* fullname, ImageOptions* image_ops, bool verbose );
Image* macLoad( strbyte* fullname, ImageOptions* image_ops, bool verbose );
Image* cmuwmLoad( strbyte* fullname, ImageOptions* image_ops, bool verbose );

Image* tgaLoad( strbyte* fullname, ImageOptions* image_ops, bool verbose );
Image* pngLoad( strbyte* fullname, ImageOptions* opt, bool verbose );
Image* webpLoad( strbyte* filename_with_path, ImageOptions* image_ops, bool verbose );
Image* tiffLoad( strbyte* filename_with_path, ImageOptions* image_ops, bool verbose );

int facesIdent( strbyte* fullname, strbyte* name );
int pbmIdent( strbyte* fullname, strbyte* name );
int sunRasterIdent( strbyte* fullname, strbyte* name );
int gifIdent( strbyte* fullname, strbyte* name );
int jpegIdent( strbyte* fullname, strbyte* name );
int rleIdent( strbyte* fullname, strbyte* name );
int bmpIdent( strbyte* fullname, strbyte* name );

int xwdIdent( strbyte* fullname, strbyte* name );
int xbitmapIdent( strbyte* fullname, strbyte* name );
int xpixmapIdent( strbyte* fullname, strbyte* name );
int g3Ident( strbyte* fullname, strbyte* name );
int fbmIdent( strbyte* fullname, strbyte* name );
int pcxIdent( strbyte* fullname, strbyte* name );
int imgIdent( strbyte* fullname, strbyte* name );
int macIdent( strbyte* fullname, strbyte* name );
int cmuwmIdent( strbyte* fullname, strbyte* name );

int tgaIdent( strbyte* fullname, strbyte* name );
int pngIdent( strbyte* fullname, strbyte* name );
int webpIdent( strbyte* filename_with_path, strbyte* name );
int tiffIdent( strbyte* filename_with_path, strbyte* name );
