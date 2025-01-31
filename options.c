/* options.c:
 *
 * finds which option in an array an argument matches
 *
 * jim frost 10.03.89
 *
 * Copyright 1989 Jim Frost.
 * See included file "copyright.h" for complete copyright information.
 */


#include "xli.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* options array and definitions. 
 * If adding something here, also add its OptionId in options.h.
 */
static OptionArray Options[] = {

	/* general options */

	{
		"debug", DBUG, NULL, "Turn on synchronous mode for debugging. Dump core on error",
	},
	{
		"dumpcore", DUMPCORE, NULL, "Dump core immediately on error signal",
	},
	{
		"default", DEFAULT, NULL, "Set the root background to the default pattern and colors.",
	},
	{
		"delay", DELAY, "seconds", "Set the automatic advance delay for all images.",
	},
	{
		"display", DISPLAY, NULL, "Indicate the X display to use.",
	},
	{
		"dispgamma", DISPLAYGAMMA, "value", "Specify the gamma of the display. Default value is 2.2."
		"If images need brightening or darkening, use the -gamma option. "
	},
	{
		"fillscreen", FILLSCREEN, NULL, "Use the whole screen for displaying an image. "
		"The image will be zoomed so that it just fits the size of the screen. "
		"If -onroot is also specified, it will be zoomed to completely fill the screen.",
	},
	{
		"fit", FIT, NULL, "Force the image(s) to use the default colormap.",
	},
	{
		"forall", FORALL, NULL, "Let option fillscreen and onroot to affect all images.",
	},
	{
		"fork", FORK, NULL, "Background automatically.  Turns on -quiet.",
	},
	{
		"fullscreen", FULLSCREEN, NULL, 
		"Use the whole screen for displaying an image. The image will be surrounded by "
		"a border if it is smaller than the screen. If -onroot is also specified, "
		"the image will be zoomed so that it just fits the size of the screen.",
	},
	{
		"geometry", GEOMETRY, "window_geometry", 
		"Specify the size of the display window.  Ignored if -fullscreen or -fillscreen is given. "
		"If used in conjunction with -onroot, defines the size of the base image.",
	},
	{
		"goto", GOTO, "image_name", "When the end of the list of images is reached, go to image_name.",
	},
	{
		"help", HELP, NULL, "Print help ",
	},
	{
		"identify", IDENTIFY, NULL, "Identify images rather than displaying them.",
	},
	{
		"install", INSTALL, NULL, "Force colormap installation.  Useful with window managers"
		"unable to handle colormap installation, but should be avoided"
		"unless necessary.",
	},
	{
		"list", LIST, NULL, "List the images along the image path.  also see '-path' ",
	},
	{
		"onroot", ONROOT, NULL, 
		"Place the image on the root window.  If used in conjunction with -fullscreen, "
		"the image will be zoomed to just fit. If used with -fillscreen, the image will "
		"be zoomed to completely fill the screen. -border, -at, and -center also affect the"
		"results.",
	},
	{
		"path", PATH, NULL, "Display the image path and default extensions loaded from the .xlirc file.",
	},
	{
		"pixmap", PIXMAP, NULL, "Use a pixmap as backing store. "
		"May improve performance but may not work on memory-limited servers.",
	},
	{
		"private", PRIVATE, NULL, "Use  a private colormap. "
		"Happens automatically if a visual other than the default is used. "
		"is the opposite of -fit.",
	},
	{
		"quiet", QUIET, NULL, "Turn off verbose mode. "
		"Default if using -onroot or -windowid.",
	},
	{
		"supported", SUPPORTED, NULL, "Print a list of supported image types.",
	},
	{
		"verbose", VERBOSE, NULL, "Turn on verbose mode. Default if using -view.",
	},
	{
		"version", VER_NUM, NULL, "Show xli version number.",
	},
	{
		"view", VIEW, NULL, "View an image in a window. Default behavior.",
	},
	{
		"visual", VISUAL, NULL, "Force the use of a particular visual to display an image. "
		"Normally xli will attempt to pick a visual which is reasonable for the supplied image.",
	},
	{
		"windowid", WINDOWID, "window_id", "Display the image in an existing particular window. "
		"Similar to -onroot and is useful for servers which use an untagged virtual root. "
		"The window ID should be supplied as a hexadecimal number, eg 0x40003.",
	},
	{
		"cache", CACHE, NULL, "Cache entire input, useful for interactively twiddling images"
		"loaded from the standard input.",
	},
	{
		"delete", DELETE, NULL, "Enable deleting images with the 'x' key.",
	},
	{
		"focus", FOCUS, NULL, "Take keyboard focus when viewing in window.",
	},

	/* image options */

	{
		"at", AT, NULL, "Load the image onto the base image (if using -merge) or the root window (if"
"using -onroot) at a specific location.",
	},
	{
		"background", BACKGROUND, "color", "Set the background pixel color for a monochrome image. " 
		"See also -foreground and -invert.",
	},
	{
		"border", BORDER, "color", "Set the color used for the border around centered, placed or clipped images.",
	},
	{
		"brighten", BRIGHT, "percentage", "Brighten or darken the image by a percentage. "
		"Values greater than 100 will brighten the image, values smaller than 100 will darken it. "
		"See also the -gamma option.",
	},
	{
		"center", CENTER, NULL, "Center the image on the base image (if using -merge) "
		"or the root window (if using -onroot).",
	},
	{
		"clip", CLIP, "X,Y,W,H", "Clip out the rectangle specified by X,Y,W,H and use that as the image.",
	},
	{
		"colordither", COLORDITHER, NULL, "Dither the image if the number of colors is reduced. "
		"It will be slower, but will give a better looking result when 256 colors or less are used.",
	},
	{
		"cdither", COLORDITHER, NULL, "alias, See -colordither.",
	},
	{
		"colors", COLORS, "number_of_colors", "Specify the maximum number of colors to be used in displaying the image. "
		"Values of 1-32768 are acceptable although low values will not look good. "
		"Made automatically if the server cannot support the depth of the image.",
	},
	{
		"dither", DITHER, NULL, "Dither the image into monochrome. "
		"Happens automatically if sent to a monochrome display.",
	},
	{
		"expand", EXPAND, NULL, "Expand the image to TrueColor depth.",
	},
	{
		"foreground", FOREGROUND, "color", "Set the foreground pixel color for a monochrome image. "
		"See -background and -invert.",
	},
	{
		"gamma", GAMMA, "value", "\
Specify the gamma of the display the image was intended to be displayed"
"on.  By default, xli assumes that"
"images have been gamma corrected and need no other processing.\n\
the -gamma option allows overriding of the default value.\n\
Some filetypes could store a gamma value, it will be ignored by xli as a default.\n\
In general, values smaller than 2.2 will lighten the image, and values"
"greater than 2.2 will darken the image.\n\
This often works better than the -brighten option.",
	},
	{
		"gray", GRAY, NULL, "Convert a color image to grayscale.  Also called -grey.",
	},
	{
		"grey", GRAY, NULL, "Alias, see -gray.",
	},
	{
		"halftone", HALFTONE, NULL, "Dither the image into monochrome using a halftone dither. "
		"Keeps image detail yet grows the image up by sixteen times.",
	},
	{
		"idelay", IDELAY, NULL, "Set the automatic advance delay for this image. "
		"This overrides -delay temporarily.",
	},
	{
		"invert", INVERT, NULL, "Invert colors of a monochrome image.",
	},
	{
		"iscale", ISCALE, "scale factor", 
		"Scale the image using a fast, image-dependent method, if available. "
		"Positive values make the image smaller, negative values larger. "
		"Specifying `auto' will fast-scale the image to fit on the screen.",
	},
	{
		"merge", MERGE, NULL, "Merge this image onto the previous image. "
		" When used in conjunction with -at, -center, and -clip collages can be generated.",
	},
	{
		"name", NAME, NULL, "Specify that the next argument is to be the name of an image."
		"Useful for loading images whose names look to be options.",
	},
	{
		"newoptions", NEWOPTIONS, NULL, "Clear the options which propagate to all following images. "
		"Turn off image processing options which were specified for previous images.",
	},
	{
		"normalize", NORMALIZE, NULL, "Normalize the image. "
		"Expands color coverage to fit the colormap as closely as possible. "
		"It may have good effects on an image which is too bright or too dark.",
	},
	{
		"rotate", ROTATE, "degrees", "Rotate the image by 90, 180, or 270 degrees.",
	},
	{
		"smooth", SMOOTH, NULL, "Perform a smoothing convolution on the image. "
		"Useful for making a zoomed image look less blocky. "
		"Multiple -smooth arguments will run the smoother multiple times. "
		"Can be quite slow on large images.",
	},
	{
		"title", TITLE, "window_title", "Set the title of the window used to display the image.",
	},
	{
		"xpm", XPM, "{ m | g4 | g | c }", "Select the preferred xpm colour mapping: "
		"(m = mono, g4 = 4 level gray, g = gray, c = color ).",
	},
	{
		"xzoom", XZOOM, "percentage", "Zoom the image along the X axis by a percentage. See -zoom.",
	},
	{
		"yzoom", YZOOM, "percentage", "Zoom the image along the X axis by a percentage.  See -zoom.",
	},
	{
		"zoom", ZOOM, NULL, "Zoom the image along both axes. "
		"Values smaller than 100 will reduce the size of the image, "
		"values greater than 100 will enlarge it. "
		"See also -xzoom and -yzoom.",
	},
	
	
	{NULL, OPT_NOTOPT, NULL, NULL}
};




OptionId optionNumber( strbyte* arg )
{
	int a, b;

	if ( ( *arg ) != '-' ) {
		return ( OPT_NOTOPT );
	}
	for ( a = 0; Options[a].name; a++ ) {
		if ( !strncmp( arg + 1, Options[a].name, strlen( arg ) - 1 ) ) {
			for ( b = a + 1; Options[b].name; b++ )
				if ( !strncmp( arg + 1, Options[b].name, strlen( arg ) - 1 ) ) {
					return ( OPT_SHORTOPT );
				}
			return ( Options[a].option_id );
		}
	}
	return ( OPT_BADOPT );
}

//static void listOptions( void )
//{
//	int a, width;
//
//	printf( "\nThe options are:\n\n" );
//
//	width = 0;
//	for ( a = 0; Options[a].name; a++ ) {
//		width += strlen( Options[a].name ) + 2;
//		if ( width > 78 ) {
//			printf( "\n" );
//			width = strlen( Options[a].name ) + 2;
//		}
//		printf( "%s%s", Options[a].name, ( Options[a + 1].name ? ", " : "\n\n" ) );
//	}
//}

void help( void )
{
	strbyte optionwithhypen[32];
	memset(&optionwithhypen,0,32);
	OptionId i;
	
	printf( "xli version %s.%s.%s \n", VERSION, PATCHLEVEL, REVISION);
	printf( "xli [GENERAL_OPTIONS]  [[LOCAL_OPTIONS] FILE ...]\n" );
	printf( "\n");
	printf( "general options:\n");
	printf( "=========================\n");
	i = GENERAL_OPTIONS_START;
	while( Options[i].name && i<GENERAL_OPTIONS_END ) {
		if ( Options[i].name ) {
			memset(&optionwithhypen,0,32);
			snprintf(optionwithhypen,32,"-%s",Options[i].name);
			printf( "%12s %-21s  %-40s\n", optionwithhypen,
				( Options[i].args ? Options[i].args : "" ),
				Options[i].description );
		}
		i=i+1;
	}
	printf( "\n");
	printf( "local options:\n");
	printf( "=========================\n");
	i = LOCAL_OPTIONS_START;
	while( Options[i].name && i<LOCAL_OPTIONS_END ) {
		if ( Options[i].name ) {
			memset(&optionwithhypen,0,32);
			snprintf(optionwithhypen,32,"-%s",Options[i].name);
			printf( "%12s %-21s  %-40s\n", optionwithhypen,
			( Options[i].args ? Options[i].args : "" ),
			Options[i].description );
		}
		i=i+1;
	}	
	printf( "\n");

	return;
}


/*
 * Code to process options and set up options structures.
 */

/* Do general options and return no of argv's advanced */
int doGeneralOption( OptionId opid, strbyte** argv, ImageOptions* persist_ops,
                     ImageOptions* image_ops )
{
	int a = 0;
	switch ( opid ) {
		case ONROOT:
			globals.onroot = 1;
			globals.fit = TRUE;	/* assume -fit */
			break;

		case DBUG:
			globals._Xdebug = TRUE;
			break;

		case DUMPCORE:
			globals._DumpCore = TRUE;
			break;

		case DEFAULT:
			globals.set_default = TRUE;
			break;

		case DELAY:
			if ( !argv[++a] ) {
				break;
			}
			persist_ops->delay =
			      image_ops->delay = atoi( argv[a] );
			if ( image_ops->delay < 0 ) {
				printf( "Bad argument to -delay\n" );
				usage( globals.argv0 );
				/* NOTREACHED */
				break;
			}
			break;

		case DISPLAY:
			if ( argv[++a] ) {
				globals.dname = argv[a];
			}
			break;

		case DISPLAYGAMMA:
			if ( argv[++a] ) {
				globals.display_gamma = atof( argv[a] );
			}
			break;

		case FILLSCREEN:
			globals.fillscreen = TRUE;
			break;

		case FIT:
			globals.fit = TRUE;
			break;

		case FORALL:
			globals.forall = TRUE;
			break;

		case FORK:
			globals.do_fork = TRUE;
			/* background processes should be seen but not heard */
			globals.verbose = FALSE;
			break;

		case FULLSCREEN:
			globals.fullscreen = TRUE;
			break;

		case GEOMETRY:
			if ( argv[++a] ) {
				globals.user_geometry = argv[a];
			}
			break;

		case GOTO:
			if ( argv[++a] ) {
				globals.go_to = argv[a];
			}
			break;

		case HELP:
			help();
			exit( 0 );

		case IDENTIFY:
			globals.identify = TRUE;
			break;

		case LIST:
			listImages();
			exit( 0 );

		case INSTALL:
			globals.install = TRUE;
			break;

		case PATH:
			showPath();
			break;

		case PIXMAP:
			globals.use_pixmap = TRUE;
			break;

		case PRIVATE:
			globals.private_cmap = TRUE;
			break;

		case QUIET:
			globals.verbose = FALSE;
			break;

		case SUPPORTED:
			supportedImageTypes();
			break;

		case VERBOSE:
			globals.verbose = TRUE;
			break;

		case VER_NUM:
			version();
			break;

		case VIEW:
			globals.onroot = FALSE;
			break;

		case VISUAL:
			if ( argv[++a] ) {
				globals.visual_class = visualClassFromName( argv[a] );
			}
			break;

		case WINDOWID:
			if ( !argv[++a] ) {
				break;
			}
			if ( sscanf( argv[a], "0x%x", &globals.dest_window ) != 1 ) {
				printf( "Bad argument to -windowid\n" );
				usage( globals.argv0 );
				/* NOTREACHED */
			}
			globals.onroot = TRUE;	/* this means "on special root" */
			globals.fit = TRUE;	/* assume -fit */
			break;

		case CACHE:
			zforcecache( TRUE );
			break;

		case DELETE:
			globals.delete = TRUE;
			break;

		case FOCUS:
			globals.focus = TRUE;
			break;

		default:
			fprintf( stderr, "strange global option #%d\n", opid );
			exit( -1 );
			break;
	}

	return a;
}

/* Do locals and return no of argv's advanced */
int doLocalOption( OptionId opid, strbyte** argv, bool setpersist,
                   ImageOptions* persist_ops, ImageOptions* image_ops )
{
	int a = 0;

	switch ( opid ) {

		case AT:
			if ( !argv[++a] ) {
				break;
			}
			if ( sscanf( argv[a], "%d,%d",
			             &image_ops->atx, &image_ops->aty ) != 2 ) {
				printf( "Bad argument to -at\n" );
				usage( globals.argv0 );
				/* NOTREACHED */
			} else {
				image_ops->ats = TRUE;
			}
			break;

		case BACKGROUND:
			if ( argv[++a] ) {
				image_ops->bg = argv[a];
			}
			break;

		case BORDER:
			if ( argv[++a] ) {
				image_ops->border = argv[a];
			}
			if ( setpersist ) {
				persist_ops->border = image_ops->border;
			}
			break;

		case BRIGHT:
			if ( argv[++a] ) {
				image_ops->bright = atoi( argv[a] );
				if ( setpersist ) {
					persist_ops->bright = image_ops->bright;
				}
			}
			break;

		case GAMMA:
			if ( argv[++a] ) {
				image_ops->gamma = atof( argv[a] );
				if ( setpersist ) {
					persist_ops->gamma = image_ops->gamma;
				}
			}
			break;

		case GRAY:
			image_ops->gray = 1;
			if ( setpersist ) {
				persist_ops->gray = 1;
			}
			break;

		case CENTER:
			image_ops->center = 1;
			break;

		case CLIP:
			if ( !argv[++a] ) {
				break;
			}
			if ( sscanf( argv[a], "%d,%d,%d,%d",
			             &image_ops->clipx, &image_ops->clipy,
			             &image_ops->clipw, &image_ops->cliph ) != 4 ) {
				printf( "Bad argument to -clip\n" );
				usage( globals.argv0 );
				/* NOTREACHED */
			}
			break;

		case COLORDITHER:
			image_ops->colordither = 1;
			if ( setpersist ) {
				persist_ops->colordither = 1;
			}
			break;

		case COLORS:
			if ( !argv[++a] ) {
				break;
			}
			image_ops->colors = atoi( argv[a] );
			if ( image_ops->colors < 2 ) {
				printf( "Argument to -colors is too low (ignored)\n" );
				image_ops->colors = 0;
			} else if ( image_ops->colors > 65536 ) {
				printf( "Argument to -colors is too high (ignored)\n" );
				image_ops->colors = 0;
			}
			if ( setpersist ) {
				persist_ops->colors = image_ops->colors;
			}
			break;

		case DITHER:
			image_ops->dither = 1;
			if ( setpersist ) {
				persist_ops->dither = 1;
			}
			break;

		case EXPAND:
			image_ops->expand = 1;
			if ( setpersist ) {
				persist_ops->expand = 1;
			}
			break;

		case FOREGROUND:
			if ( argv[++a] ) {
				image_ops->fg = argv[a];
			}
			break;

		case HALFTONE:
			image_ops->dither = 2;
			if ( setpersist ) {
				persist_ops->dither = 2;
			}
			break;

		case IDELAY:
			if ( !argv[++a] ) {
				break;
			}
			image_ops->delay = atoi( argv[a] );
			if ( image_ops->delay < 0 ) {
				printf( "Bad argument to -idelay\n" );
				usage( globals.argv0 );
				/* NOTREACHED */
			}
			break;

		case INVERT:
			image_ops->fg = "white";
			image_ops->bg = "black";
			break;

		case ISCALE:
			if ( argv[++a] ) {
				if ( !strcmp( argv[a], "auto" ) ) {
					image_ops->iscale_auto = TRUE;
				} else {
					image_ops->iscale = atoi( argv[a] );
					image_ops->iscale_auto = FALSE;
				}
				if ( setpersist ) {
					persist_ops->iscale_auto =
					      image_ops->iscale_auto;
				}
			}
			break;

		case MERGE:
			image_ops->merge = 1;
			break;

		case NEWOPTIONS:
			if ( setpersist ) {
				persist_ops->bright = 0;
				persist_ops->colordither = 0;
				persist_ops->colors = 0;
				persist_ops->delay = -1;
				persist_ops->dither = 0;
				persist_ops->gray = 0;
				persist_ops->gamma = UNSET_GAMMA;
				persist_ops->normalize = 0;
				persist_ops->smooth = 0;
				persist_ops->xzoom = 0;
				persist_ops->yzoom = 0;
			}
			break;

		case NORMALIZE:
			image_ops->normalize = 1;
			if ( setpersist ) {
				persist_ops->normalize = image_ops->normalize;
			}
			break;

		case ROTATE:
			if ( !argv[++a] ) {
				break;
			}
			image_ops->rotate = atoi( argv[a] );
			if ( ( image_ops->rotate % 90 ) != 0 ) {
				printf( "Argument to -rotate must be a multiple of 90 (ignored)\n" );
				image_ops->rotate = 0;
			} else
				while ( image_ops->rotate < 0 ) {
					image_ops->rotate += 360;
				}
			break;

		case SMOOTH:
			image_ops->smooth = persist_ops->smooth + 1;
			if ( setpersist ) {
				persist_ops->smooth = image_ops->smooth;
			}
			break;

		case TITLE:
			if ( argv[++a] ) {
				image_ops->title = argv[a];
			}
			break;

		case XPM:
			if ( argv[++a] ) {
				image_ops->xpmkeyc = xpmoption( argv[a] );
				if ( image_ops->xpmkeyc != 0 && setpersist ) {
					persist_ops->xpmkeyc = image_ops->xpmkeyc;
				}
			}
			break;

		case XZOOM:
			if ( argv[++a] ) {
				if ( atoi( argv[a] ) < 0 ) {
					printf( "Zoom argument must be positive (ignored).\n" );
					break;
				}
				image_ops->xzoom = atoi( argv[a] );
				if ( setpersist ) {
					persist_ops->xzoom = image_ops->xzoom;
				}
			}
			break;

		case YZOOM:
			if ( argv[++a] ) {
				if ( atoi( argv[a] ) < 0 ) {
					printf( "Zoom argument must be positive (ignored).\n" );
					break;
				}
				image_ops->yzoom = atoi( argv[a] );
				if ( setpersist ) {
					persist_ops->yzoom = image_ops->yzoom;
				}
			}
			break;

		case ZOOM:
			if ( argv[++a] ) {
				if ( !strcmp( argv[a], "auto" ) ) {
					image_ops->zoom_auto = TRUE;
				} else {
					if ( atoi( argv[a] ) < 0 ) {
						printf( "Zoom argument must be positive (ignored).\n" );
						break;
					}
					image_ops->xzoom = image_ops->yzoom = atoi( argv[a] );
					image_ops->zoom_auto = FALSE;
				}
				if ( setpersist ) {
					persist_ops->xzoom = persist_ops->yzoom = image_ops->xzoom;
					persist_ops->zoom_auto = image_ops->zoom_auto;
				}
			}
			break;

		default:
			fprintf( stderr, "strange local option #%d\n", opid );
			exit( -1 );
			break;
	}

	return a;
}



/*
 * visual class to name table and back support
 */

static struct visual_class_name {
	int class;		/* numerical value of class */
	strbyte* name;		/* actual name of class */
} VisualClassName[] = {
	{TrueColor,	"TrueColor"},
	{DirectColor,	"DirectColor"},
	{PseudoColor,	"PseudoColor"},
	{StaticColor,	"StaticColor"},
	{GrayScale,	"GrayScale"},
	{StaticGray,	"StaticGray"},
	{StaticGray,	"StaticGrey"},
	{-1,		( strbyte* ) 0}
};

int visualClassFromName( strbyte* name )
{
	int a;
	strbyte* s1, *s2;
	int class = -1;

	for ( a = 0; VisualClassName[a].name; a++ ) {
		for ( s1 = VisualClassName[a].name, s2 = name; *s1 && *s2; s1++, s2++ )
			if ( ( isupper( *s1 ) ? tolower( *s1 ) : *s1 ) !=
			      ( isupper( *s2 ) ? tolower( *s2 ) : *s2 ) ) {
				break;
			}

		if ( ( *s1 == '\0' ) || ( *s2 == '\0' ) ) {

			/* check for uniqueness.  we special-case StaticGray because we have two
			 * spellings but they are unique if either is found
			 */

			if ( ( class != -1 ) && ( class != StaticGray ) ) {
				fprintf( stderr, "%s does not uniquely describe a visual class (ignored)\n", name );
				return ( -1 );
			}
			class = VisualClassName[a].class;
		}
	}
	if ( class == -1 ) {
		fprintf( stderr, "%s is not a visual class (ignored)\n", name );
	}
	return ( class );
}

strbyte* nameOfVisualClass( int class )
{
	int a;

	for ( a = 0; VisualClassName[a].name; a++ )
		if ( VisualClassName[a].class == class ) {
			return ( VisualClassName[a].name );
		}
	return ( "[Unknown Visual Class]" );
}
