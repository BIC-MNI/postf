/* ----------------------------- MNI Header -----------------------------------
   @NAME       : postf4.c
   @INPUT      : argc, argv
   @OUTPUT     : (none)
   @RETURNS    : error status.
   @DESCRIPTION: Program to post image data to gl screen
   @METHOD     : Can read floats from stdin, images from mni or minc files
   @GLOBALS    : none
   @CALLS      : none
   @CREATED    : January 25, 1993 (Gabriel Leger)
   @MODIFIED   : $Log: postf.c,v $
   @MODIFIED   : Revision 1.7  2005-03-17 17:27:27  bert
   @MODIFIED   : Get rid of compilation warnings whereever possible.
   @MODIFIED   :
   @MODIFIED   : Revision 1.6  2005/03/17 16:58:12  bert
   @MODIFIED   : Fixes for problems reported by Christopher Baily (cjb@pet.auh.dk) - fix color scaling, fix ROI drawing, and avoid compilation errors on early MINC's that don't define ARGV_VERINFO
   @MODIFIED   :
   @MODIFIED   : Revision 1.5  2005/03/16 17:51:48  bert
   @MODIFIED   : Latest changes to autoconf stuff and X port
   @MODIFIED   :
   @MODIFIED   : Revision 1.4  2005/03/10 23:54:43  bert
   @MODIFIED   : Major cleanup and speedup
   @MODIFIED   :
   @MODIFIED   : Revision 1.3  2005/03/10 20:11:24  bert
   @MODIFIED   : Massively rewritten to eliminate Ygl, now uses Xlib directly
   @MODIFIED   :
   @MODIFIED   : Revision 1.2  2005/02/14 15:44:37  bert
   @MODIFIED   : Update Gabriel's name and copyright information
   @MODIFIED   :
   @MODIFIED   : Revision 1.1  2005/02/11 22:06:51  bert
   @MODIFIED   : Initial checkin, postf ported to Linux and autoconfiscated
   @MODIFIED   :
 * Revision 1.9  1995/05/19  15:57:00  gaby
 * Now compiles under Irix 5.3
 * No major modifications necessary, but
 * changed ncopen and ncclose to miopen and miclose
 * and added code to account for slice and frame offsets
 * as chosen by user (rslices and rframes) as their number
 * is displayed on the main window.
 *
 * Revision 1.8  93/12/31  16:48:42  gaby
 * Oops, had to fix a little bug in the evaluate_pixel function.
 * When roi_radius = 0, get_roi_tac should not be called!
 * 
 * Revision 1.7  93/12/31  16:23:58  gaby
 * Made the ROI averaging present even when data is not dynamic
 * that is, even if there is only one frame.  In addition, the 
 * program now evaluates the current pixel or ROI on image changes
 * and ROI size changes.
 * 
 * Revision 1.6  93/12/30  23:20:27  gaby
 * A number of new revisions.  The rslices and rframes arguments
 * can now be used with mni files.  To accomplish this a major
 * revision of the read_mni_data and related functions was completed.
 * Now postf uses a number of mni file access routines contained in
 * mnifile.c.  In addition, bugs in the ROI averaging and standard
 * deviation calculation code were fixed (at least results are more
 * reasonable now).  This section of code still requires more complete
 * testing.
 * 
 * Revision 1.5  93/12/29  21:43:05  gaby
 * Added new feature.  It is now possible, when evaluating pixel
 * dynamically, to average out a number of pixels described by a
 * circular ROI.  In addition to the ROI average, pixel dumping
 * also outputs ROI standard deviation.
 * 
 * Revision 1.4  93/12/15  14:14:05  gaby
 * Program now forces slices = 1 when no slice information is available
 * This allows the porgram to function properly when loading minc images
 * that are single slice but multiframe.
 * 
 * Revision 1.3  93/08/19  13:04:15  gaby
 * Program now outputs all progress messages to stderr,
 * but continues to dump pixel information (during dynamic
 * profiling) to stdout.
 * 
 * Revision 1.2  93/08/10  09:41:37  gaby
 * Enhanced the default globals file reading
 * to read, if they are present, globals files
 * named /usr/local/mni/lib/postf.defaults,
 * $HOME/.postf.defaults, and then $cwd/postf.defaults.
 * The program is also a bit cleaner with icons and things.
 * 
 *  Older revions:
 *
 *  93/08/9  gaby
 *  Added support for David's setting of globals at runtime.
 *  Also extended the command line options to include slice and 
 *  frame range specifications.
 *
 *  93/08/2  gaby
 *  fixed small bug that flared up on the new extremes:
 *  When lower left hand corner coordinates of image to be 
 *  draw are -ve wrt view port, then they must be set to zero
 *  and the image "cropped" appropriately.
 *
 * @COPYRIGHT  :
 * Copyright 1993-2005 Gabriel C. Léger, McConnell Brain Imaging Centre,
 * Montreal Neurological Institute, McGill University.  Permission to use,
 * copy, modify, and distribute this software and its documentation for any
 * purpose and without fee is hereby granted, provided that the above
 * copyright notice appear in all copies.  The author and McGill University
 * make no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * ------------------------------------------------------------------------- */

/*
 * Some definitions for the use of Dave globals routines
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define MINC_PLAY_NICE 1        /* Don't pollute my namespace!! */
#include <volume_io.h>


/* Set to 1 if you want to enable the somewhat experimental resampling code.
 */
#ifndef DO_RESAMPLE
#define DO_RESAMPLE 0
#endif

static const char rcsid[] = "$Header: /private-cvsroot/visualization/postf/postf.c,v 1.7 2005-03-17 17:27:27 bert Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/Xdbe.h>

#define DoRGB (DoRed | DoBlue | DoGreen)

#define POSTF_X_EVENT_MASK (ExposureMask | \
                            KeyPressMask | \
                            KeyReleaseMask | \
                            ButtonPressMask | \
                            ButtonReleaseMask | \
                            PointerMotionMask | \
                            StructureNotifyMask)

#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define MAX(a,b) (((a) < (b)) ? (b) : (a))

#define POSTF_X_VISUAL_DEPTH 24
#define POSTF_X_VISUAL_CLASS TrueColor

struct postf_wind {
    Display *display;
    Visual *visual;
    Window wind;
    Window draw;
    Window wbuf[2];
    GC gc;
    double ol;
    double or;
    double ob;
    double ot;
    double xf;
    double yf;
    double xo;
    double yo;
    int vl;
    int vb;
    int vw;
    int vh;
};

#if DO_RESAMPLE    
typedef unsigned char *(*resample_func_t) (unsigned short *, int, int, int, int, int);

resample_func_t resample_function;
int debug_level = 2;
#endif /* DO_RESAMPLE */

/*
 * And some standard defs
 */

#include <minc.h>

#include "image.h"
#include "ParseArgv.h"
#include "conversions.h"
#include "mnifile.h"

#define MAX_STRING_LENGTH 1024

#define BLOCK_SIZE 512
#define FS_WIDTH 12
#define FS_HEIGHT 7
#define FRAMES 1
#define START_FRAME (FRAMES-1)
#define SLICES 1
#define START_SLICE (SLICES-1)
#define NO_OF_IMAGES (FRAMES * SLICES)

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#define NIL 0

#define X 0
#define Y 1
#define XY 2

#define XAXIS 1
#define YAXIS 2
#define ZAXIS 4

#define BOTTOM 0
#define TOP 1

#define NORMAL_STATUS 0
#define ERROR_STATUS 1

#define INITIALIZE TRUE
#define NOT_INITIALIZE FALSE
#define REDRAW_ONLY TRUE
#define NOT_REDRAW_ONLY FALSE

#define WINDOW_HEIGHT 512
#define WINDOW_WIDTH 512

#define AUXILLARY_WINDOW_WIDTH 400
#define AUXILLARY_WINDOW_HEIGHT 400

#define PROFILE 1
#define DYNAMIC 2

#define COLOR_BAR_TOP 84
#define COLOR_BAR_WIDTH 16
#define COLOR_BAR_XOFFSET 16
#define COLOR_BAR_YOFFSET 64
#define COLOR_BAR_HEIGHT (WINDOW_HEIGHT-(COLOR_BAR_YOFFSET*2)-20)
#define COLOR_BAR_WINDOW (COLOR_BAR_WIDTH+(COLOR_BAR_XOFFSET*5))

#define IMAGE_WIDTH 128
#define IMAGE_HEIGHT 128
#define IMAGE_SIZE (IMAGE_WIDTH * IMAGE_HEIGHT)
#define ROI_RADIUS 0
#define MAX_ROI_RADIUS (WINDOW_WIDTH / 2)
#define MAX_ROI_AREA ((MAX_ROI_RADIUS + 1) * (MAX_ROI_RADIUS + 1) * 4)

#define AXES_TICS 5
#define SCALE_TICS 10
#define EXPAND 1
#define TIC_REDUCTION 10
#define SCALE_STEP (COLOR_BAR_HEIGHT/SCALE_TICS)
#define EVAL_XCOORD (WINDOW_WIDTH+COLOR_BAR_XOFFSET)
#define EVAL_YCOORD (COLOR_BAR_YOFFSET/4-1)
#define SCALE_XCOORD (WINDOW_WIDTH+COLOR_BAR_XOFFSET)
#define SCALE_YCOORD (COLOR_BAR_TOP+COLOR_BAR_HEIGHT+(COLOR_BAR_YOFFSET/3))
#define TEXT_SEP (COLOR_BAR_YOFFSET/4)
#define LOOSE 10

#define INDEX_MIN 64
#define INDEX_MAX 255
#define INDEX_RANGE (INDEX_MAX-INDEX_MIN)
#define RGB_MAX 65535

/*
 * The two possible icon modes:
 */
#define MAP_MODE 0
#define TRUE_COLOR_MODE 1

#define R 0
#define G 1
#define B 2
#define RGB 3

/* 
 * SOME GLOBALS!!!
 *
 * we define index_min, index_max and index_range
 * as globals that can be set by the user.
 * 
 */

int
  index_min = INDEX_MIN,
  index_max = INDEX_MAX,
  index_range = INDEX_RANGE;

Display *_xDisplay;
int _xScreen;
Visual *_xVisual;
Colormap _xColormap;
int _xDepth;
unsigned long *_xmap = NULL;
unsigned long TextColor, 
    ProfileBackgroundColor,
    ProfileXColor, 
    ProfileYColor, 
    ProfileZColor,
    DynamicColor,
    ImageBackgroundColor,
    AxisColor;

void 
swapbuffers(struct postf_wind *wnd_ptr)
{
    XdbeSwapInfo xdbesi;
    xdbesi.swap_window = wnd_ptr->wind;
    xdbesi.swap_action = XdbeUndefined;
    XdbeSwapBuffers(_xDisplay, &xdbesi, 1);
    XFlush(_xDisplay);
}

void
frontbuffer(struct postf_wind *wnd_ptr, int which)
{
    wnd_ptr->draw = wnd_ptr->wbuf[1-which];
}

void
force_redraw(struct postf_wind *wnd_ptr)
{
    XEvent e;

    e.type = Expose;
    e.xexpose.window = wnd_ptr->wind;
    e.xexpose.display = _xDisplay;
    e.xexpose.send_event = False;
    e.xexpose.x = 0;
    e.xexpose.y = 0;
    e.xexpose.width = wnd_ptr->vw;
    e.xexpose.height = wnd_ptr->vh;
    e.xexpose.count = 0;

    XPutBackEvent(_xDisplay, &e);
}

/* Fairly trivial X initialization to get things started...
 */
int
xinit(void)
{
    XVisualInfo visualInfo;
    XColor xcolor;

    _xDisplay = XOpenDisplay(NULL);
    _xScreen = XDefaultScreen(_xDisplay);
    _xVisual = XDefaultVisual(_xDisplay, _xScreen);

    if (!XMatchVisualInfo(_xDisplay, _xScreen,
                          POSTF_X_VISUAL_DEPTH,
                          POSTF_X_VISUAL_CLASS,
                          &visualInfo)) {
        fprintf(stderr,
                "Unable to obtain desired depth (%d) and class %d\n",
                POSTF_X_VISUAL_DEPTH,
                POSTF_X_VISUAL_CLASS);
        return (-1);
    }

    _xDepth = visualInfo.depth;
    _xVisual = visualInfo.visual;
    _xColormap = DefaultColormap(_xDisplay, _xScreen);

    xcolor.flags = DoRGB;
    xcolor.red = 0;
    xcolor.green = 0;
    xcolor.blue = 0;
    XAllocColor(_xDisplay, _xColormap, &xcolor);
    ImageBackgroundColor = xcolor.pixel;
    ProfileBackgroundColor = xcolor.pixel;

    xcolor.flags = DoRGB;
    xcolor.red = 65535;
    xcolor.green = 65535;
    xcolor.blue = 65535;
    XAllocColor(_xDisplay, _xColormap, &xcolor);
    TextColor = xcolor.pixel;
    DynamicColor = xcolor.pixel;
    ProfileXColor = xcolor.pixel;

    xcolor.flags = DoRGB;
    xcolor.red = 65535;
    xcolor.green = 0;
    xcolor.blue = 0;
    XAllocColor(_xDisplay, _xColormap, &xcolor);
    AxisColor = xcolor.pixel;

    xcolor.flags = DoRGB;
    xcolor.red = 0;
    xcolor.green = 65535;
    xcolor.blue = 0;
    XAllocColor(_xDisplay, _xColormap, &xcolor);
    ProfileYColor = xcolor.pixel;

    xcolor.flags = DoRGB;
    xcolor.red = 0;
    xcolor.green = 0;
    xcolor.blue = 65535;
    XAllocColor(_xDisplay, _xColormap, &xcolor);
    ProfileZColor = xcolor.pixel;

    return (0);
}

void
xexit(void)
{
    if (_xmap != NULL) {
        free(_xmap);
    }
    XCloseDisplay(_xDisplay);
}

/* ----------------------------- MNI Header -----------------------------------
   @NAME       : describe_postf
   @INPUT      : input from ParseArgv
   @OUTPUT     : (none)
   @RETURNS    : 0
   @DESCRIPTION: Function to be called by ParseArgv to print out a description
   of the user interface for the program postf.
   @METHOD     : 
   @GLOBALS    : 
   @CALLS      : 
   @CREATED    : March 5, 1993 (Peter Neelin)
   @MODIFIED   : 
   ---------------------------------------------------------------------------- */
int describe_postf(char *dst, char *key, char *nextArg)
{     /* ARGSUSED */
  
  static char output_text[] = {
    "\n"
    "The program postf displays slices through a static or dynamic volume\n"
    "of images. It will read data from both minc and mni format files,\n"
    "automatically detecting the file type. If no file name is given,\n"
    "then raw single-precision floating point values are read from standard\n"
    "input. To obtain information on command-line arguments, invoke postf\n"
    "with the -help option.\n"
    "\n"
    "Postf does not have a conventional user-interface.\n"
    "Instead, operations are invoked by simple key presses and\n"
    "mouse motion. These commands are described below. Note that there are\n"
    "two areas of the window that can behave slightly differently: the\n"
    "image area and the colour-bar area - these differences are noted in\n"
    "the command descriptions.\n"
    "\n"
    "Mouse motion :\n"
    "   Left button :\n"
    "      in image : evaluate (and move) current pixel.\n"
    "      in colour bar : move top of colour bar.\n"
    "   Middle button :\n"
    "      in image : move whole image in viewport.\n"
    "      in colour bar : move top and bottom of colour bar together.\n"
    "   Right button :\n"
    "      in colour bar : move bottom of colour bar.\n"
    "   Left and right buttons:\n"
    "      in image : print out time activity curve for current pixel on stdout\n"
    "                 (only works when time activity pop-up is displayed).\n"
    "\n"
    "Control key: Evaluate current pixel (see arrow keys).\n"
    "Alternate key: Change size of pixel evaluation area or ROI (see arrow keys).\n"
    "\n"
    "Arrow keys :\n"
    "   right : Move to next frame, same slice.\n"
    "   left  : Move to previous frame, same slice.\n"
    "   up    : Move to next slice, same frame.\n"
    "   down  : Move to previous slice, same frame.\n"
    "   with shift key down : Move through images without redrawing.\n"
    "   with control key down : Move current pixel by one.\n"
    "   with control and shift keys down : Move current pixel by four.\n"
    "   with alternate key down: increase or decrease ROI evaluation area.\n"
    "\n"
    "Keyboard (unless otherwise noted, case is irrelevant):\n"
    "   <ESC> : Exit program.\n"
    "   q     : Exit program.\n"
    "   g     : Set colour bar to gray-scale.\n"
    "   h     : Set colour bar to hot-metal.\n"
    "   s     : Set colour bar to spectral.\n"
    "   r     : Reset colour bar values.\n"
    "   R     : Reset image position and size.\n"
    "   p     : Pop up a window displaying x and y (and z) activity profiles\n"
    "           through the current pixel.\n"
    "   d     : Pop up a window displaying a graph of activity versus time for\n"
    "           the current pixel.\n"
    "   x     : Toggle display of x activity profile.\n"
    "   y     : Toggle display of y activity profile.\n"
    "   z     : Toggle display of z activity profile.\n"
    "   m     : Locate image containing maximum value\n"
    "           (mni and float data only).\n"
    "   M     : Locate image containing minimum value\n"
    "           (mni and float data only).\n"
    "   -     : Zoom out from image.\n"
    "   +     : Zoom in to image.\n"
    "   space : Print out time activity curve for current pixel on stdout\n"
    "           (only works when time activity pop-up is displayed and control\n"
    "           key or left mouse button are down).\n"
    "\n"
  };
  
  /* Print message */
  fputs(output_text, stdout);
  
  exit(EXIT_FAILURE);
}

unsigned int
get_mouse_ptr(struct postf_wind *wnd_ptr, int mouse_position[XY])
{
    Window root;
    Window child;
    int x_root, y_root;
    unsigned int mask;

    XQueryPointer(_xDisplay, wnd_ptr->wind,
                  &root, &child, 
                  &x_root, &y_root,
                  &mouse_position[X], &mouse_position[Y],
                  &mask);
    mouse_position[Y] = mouse_position[Y];
    return (mask);
}

void 
my_ortho2(struct postf_wind *wnd_ptr, 
          double l, double r, double b, double t)
{
    wnd_ptr->ol = l;
    wnd_ptr->or = r;
    wnd_ptr->ob = b;
    wnd_ptr->ot = t;
    wnd_ptr->xf = (wnd_ptr->vw - 1) / (r - l);
    wnd_ptr->xo = l - wnd_ptr->vl / wnd_ptr->xf;
    wnd_ptr->yf = (wnd_ptr->vh - 1) / (t - b);
    wnd_ptr->yo = b - wnd_ptr->vb / wnd_ptr->yf;
}

int scalex(struct postf_wind *wnd_ptr, double x)
{
    return (int) rint((x - wnd_ptr->xo) * wnd_ptr->xf);
}

int scaley(struct postf_wind *wnd_ptr, double y)
{
    return ((int) rint((wnd_ptr->yo - y) * wnd_ptr->yf) + wnd_ptr->vh - 1);
}

/*
 * define MINC specific stuff
 */

typedef struct {
  char *Name;
  int CdfId;
  int ImgId;
  int TimeId;
  int TimeWidthId;
  int Icv;
  int Slices;
  int Frames;
  int ImageHeight;
  int ImageWidth;
  int ImageSize;
  int nDim;
  int WidthImageDim;
  int HeightImageDim;
  int SliceDim;
  int TimeDim;
} MincInfo;

MincInfo *open_minc_file(char *minc_filename, VIO_BOOL progress, VIO_BOOL debug)
{
  
  int
    dimension,
    dim_ids[MAX_VAR_DIMS],
    attribute,
    natts;
  
  long dim_length[MAX_VAR_DIMS];
  
  char
    image_name[256],
    dim_names[MAX_VAR_DIMS][256],
    attname[256];
  
  nc_type datatype;
  MincInfo *MincFile;
  VIO_BOOL
    frames_present = FALSE,
    slices_present = FALSE;
  
  if (progress && !debug){
    fprintf(stderr, "Getting image information ... ");
    fflush(stderr);
  }
  
  /* 
   * Open the file. On error, return without printing diagnostic messages 
   */
  
  MincFile = (MincInfo *)malloc(sizeof(MincInfo));
  MincFile->Name = minc_filename;
  
  if (debug) {
    fprintf(stderr, "MincFile info:\n");
    fprintf(stderr, " Minc filename: %s\n", MincFile->Name);
  }
  
  ncopts = 0;
  MincFile->CdfId = miopen(MincFile->Name, NC_NOWRITE);
  if (MincFile->CdfId == MI_ERROR){
    return(NULL);
  }
  
  /* 
   * Get id for image variable 
   */
  
  MincFile->ImgId = ncvarid(MincFile->CdfId, MIimage);
  
  /* 
   * Get info about variable 
   */
  
  ncvarinq(MincFile->CdfId, 
           MincFile->ImgId, 
           image_name, 
           &datatype, 
           &MincFile->nDim, 
           dim_ids, 
           &natts);
  
  
  if (debug){
    fprintf(stderr, " Image variable name: %s, attributes: %d, dimensions: %d\n", 
                   image_name, natts, MincFile->nDim);
    fprintf(stderr, " Attributes:\n");
    for (attribute = 0; attribute < natts; attribute++){
      ncattname(MincFile->CdfId, MincFile->ImgId, attribute, attname);
      fprintf(stderr, "  %s\n", attname);
    }
    fprintf(stderr, " Dimension variables:\n");
  }
  
  /*
   * For each dimension, inquire about name and get dimension length
   */
  
  for (dimension = 0; dimension < MincFile->nDim; dimension++){
    
    ncdiminq(MincFile->CdfId,
             dim_ids[dimension],
             dim_names[dimension],
             &dim_length[dimension]);
    
    if (debug){
        fprintf(stderr, "  %d\t%d\t%s\t%ld\n", 
                     dimension,
                     dim_ids[dimension],
                     dim_names[dimension],
                     dim_length[dimension]); 
    }
    
    if (strcmp(MItime,dim_names[dimension]) == 0){
      MincFile->TimeDim = dimension;
      MincFile->Frames = dim_length[dimension];
      frames_present = TRUE;
      if (dimension > MincFile->nDim-3){
        fprintf(stderr, "Found time as an image dimension ... cannot continue\n");
        return(NULL);
      }
      
    }
    else if (dimension == MincFile->nDim-3 || dimension == MincFile->nDim-4){
      MincFile->SliceDim = dimension;
      MincFile->Slices = dim_length[dimension];
      slices_present = TRUE;
      
    }
    else if (dimension == MincFile->nDim-2){
      MincFile->HeightImageDim = dimension;
      MincFile->ImageHeight = dim_length[dimension];
      
    }
    else if (dimension == MincFile->nDim-1){
      MincFile->WidthImageDim = dimension;
      MincFile->ImageWidth = dim_length[dimension];
      
    }
    else{
      fprintf(stderr, "  Too many dimensions, skipping dimension <%s>\n",
                     dim_names[dimension]);
      
    }
    
  }
  
  MincFile->ImageSize = MincFile->ImageWidth * MincFile->ImageHeight;
  
  /*
   * Create the image icv 
   */
  
  MincFile->Icv = miicv_create();
  miicv_setint(MincFile->Icv, MI_ICV_DO_NORM, TRUE);
  miicv_setint(MincFile->Icv, MI_ICV_TYPE, NC_SHORT);
  miicv_setstr(MincFile->Icv, MI_ICV_SIGN, MI_UNSIGNED);
  miicv_setdbl(MincFile->Icv, MI_ICV_VALID_MAX, index_max);
  miicv_setdbl(MincFile->Icv, MI_ICV_VALID_MIN, index_min);
  miicv_setint(MincFile->Icv, MI_ICV_DO_FILLVALUE, TRUE);
  miicv_setint(MincFile->Icv, MI_ICV_DO_DIM_CONV, TRUE);
  miicv_setint(MincFile->Icv, MI_ICV_XDIM_DIR, MI_ICV_POSITIVE);
  miicv_setint(MincFile->Icv, MI_ICV_YDIM_DIR, MI_ICV_POSITIVE);
  miicv_setint(MincFile->Icv, MI_ICV_ZDIM_DIR, MI_ICV_POSITIVE);
  
  /* 
   * Attach icv to image variable 
   */
  
  miicv_attach(MincFile->Icv, MincFile->CdfId, MincFile->ImgId);
  
  /*
   * If frames are present, then get variable id for time
   */
  
  if (frames_present) {
    MincFile->TimeId = ncvarid(MincFile->CdfId, dim_names[MincFile->TimeDim]);  
    MincFile->TimeWidthId = ncvarid(MincFile->CdfId, MItime_width);  
  }
  else{
    MincFile->Frames = 0;
  }
  
  if (!slices_present) MincFile->Slices = 0;
  
  if (progress && !debug) fprintf(stderr, "done\n");
  
  return(MincFile);
  
}

int read_minc_data(MincInfo *MincFile,
                   int      *user_slice_list,
                   int      user_slice_count,
                   int      *user_frame_list,
                   int      user_frame_count,
                   int      images_to_get,
                   VIO_BOOL  frames_present,
                   float    *frame_time,
                   float    *frame_length,
                   float    *min,
                   float    *max,
                   unsigned short *dynamic_volume,
                   VIO_BOOL  progress,
                   VIO_BOOL  debug)
{
  int
    frame,
    slice;
  
  long 
    coord[MAX_VAR_DIMS],
    count[MAX_VAR_DIMS];
  
  int notice_every = 0;
  int image_no = 0;
  
  double
    dbl_min,
    dbl_max;
  
  progress = progress && !debug;
  
  /* 
   * Modify count and coord 
   */
  
  miset_coords(MincFile->nDim, (long) 0, coord);
  miset_coords(MincFile->nDim, (long) 1, count);
  count[MincFile->nDim-1] = MincFile->ImageWidth;
  count[MincFile->nDim-2] = MincFile->ImageHeight;
  coord[MincFile->nDim-1] = 0;
  coord[MincFile->nDim-2] = 0;
  
  if (progress){
    fprintf(stderr, "Reading minc data ");
    notice_every = images_to_get/MAX(user_slice_count,user_frame_count);
    if (notice_every == 0) notice_every = 1;
  }
  
  /* 
   * Get the data 
   */
  
  for (slice = 0; slice < user_slice_count; slice++){
    
    if (MincFile->Slices > 0)
      coord[MincFile->SliceDim] = user_slice_list[slice]-1;
    
    for (frame = 0; frame < user_frame_count; frame++){
      
      if (MincFile->Frames > 0)
        coord[MincFile->TimeDim] = user_frame_list[frame]-1;
      
      miicv_get(MincFile->Icv, coord, count, (void *) dynamic_volume);
      dynamic_volume += MincFile->ImageSize;
      
      if (debug){
        fprintf(stderr, " Slice: %ld, Frame: %ld\n",
                       MincFile->Slices > 0 ? coord[MincFile->SliceDim]+1 : 1,
                       MincFile->Frames > 0 ? coord[MincFile->TimeDim]+1 : 1);
      }
      
      if (progress){
        if (image_no++ % notice_every == 0){
          fprintf(stderr, ".");
          fflush(stderr);            
        }
        
      }
      
    } /* for frame */
    
  } /* for slice */
  
  if (progress) fprintf(stderr, " done\n");
  
  miicv_inqdbl(MincFile->Icv, MI_ICV_NORM_MIN, &dbl_min);
  miicv_inqdbl(MincFile->Icv, MI_ICV_NORM_MAX, &dbl_max);
  
  *min = (float) dbl_min;
  *max = (float) dbl_max;
  
  /* 
   * If user requested frames
   */
  
  if (frames_present){
      char str_tmp[128];

      /* 
       * Get time domain 
       */
    
      count[0] = MincFile->Frames;
      coord[0] = 0;

      miattgetstr(MincFile->CdfId, MincFile->TimeId, MIspacing,
                  sizeof(str_tmp), str_tmp);
      if (!strcmp(str_tmp, MI_IRREGULAR)) {
          mivarget(MincFile->CdfId,
                   MincFile->TimeId,
                   coord,
                   count,
                   NC_FLOAT,
                   MI_SIGNED,
                   frame_time);
    
          mivarget(MincFile->CdfId,
                   MincFile->TimeWidthId,
                   coord,
                   count,
                   NC_FLOAT,
                   MI_SIGNED,
                   frame_length);
      }
      else {
          double start;
          double step;

          miattget1(MincFile->CdfId, MincFile->TimeId, MIstart,
                    NC_DOUBLE, &start);
          miattget1(MincFile->CdfId, MincFile->TimeId, MIstep,
                    NC_DOUBLE, &step);

          for (frame = 0;  frame < MincFile->Frames; frame++) {
              frame_time[frame] = frame*step;
              frame_length[frame] = step;
          }
      }
    
    /*
     * Calculate midframe time and frame_length.
     */
    
    for (frame = 0; frame < MincFile->Frames; frame++){
      frame_time[frame] = (frame_time[frame] + frame_length[frame]/2) / 60;

      if (frame < MincFile->Frames-1 && 
          frame_time[frame] > frame_time[frame + 1]) {
          fprintf(stderr, "WARNING: frame times are not monotonically increasing!\n");
      }
      if (debug) {
        fprintf(stderr, " Frame: %d, Time: %.2f, length: %.2f\n",
                       frame+1,
                       frame_time[frame],
                       frame_length[frame]);
      }
      
    }
    
    /*
     * Finally select those time points associated with the selected frames.
     */
    
    for (frame = 0; frame < user_frame_count; frame++){
      frame_time[frame] = frame_time[user_frame_list[frame]-1];
      if (debug) {
        fprintf(stderr, " User frame: %d, Study frame: %d, time: %.2f\n",
                       frame+1,
                       user_frame_list[frame],
                       frame_time[frame]);
      }
      
    }
    
  } /* if frames_present */
  
  return(NORMAL_STATUS);
  
}

void close_minc_file(MincInfo *MincFile)
{
  
  /* 
   * Free the image icv 
   */
  
  miicv_free(MincFile->Icv);
  
  /* 
   * Close the file and return
   */
  
  miclose(MincFile->CdfId);
  
}

void read_mni_data(MniInfo  *MniFile,
                  int      *user_slice_list,
                  int      user_slice_count,
                  int      *user_frame_list,
                  int      user_frame_count,
                  int      images_to_get,
                  VIO_BOOL  frames_present,
                  float    *frame_time,
                  float    *frame_length,
                  float    *min,
                  float    *max,
                  int      *min_slice,
                  int      *max_slice,
                  int      *min_frame,
                  int      *max_frame,
                  unsigned short *dynamic_volume,
                  VIO_BOOL  progress,
                  VIO_BOOL  debug)
     
{
  int 
    slice, frame,
    mni_image, user_image,
    pixel_count,
    notice_every;
  VIO_BOOL 
    scale = FALSE;
  float 
    image_min, image_max,
    range_factor,
    present_factor,
    present_term;
  unsigned char
    *present_byte;
  unsigned short
    *present_short;
  
  notice_every = images_to_get/MAX(MniFile->Slices,MniFile->Frames);
  if (notice_every == 0) notice_every = 1;

  if (progress){
    printf("Reading mni data ");
    fflush(stdout);
  }

  present_byte = (unsigned char *)dynamic_volume;

  for (slice = 0; slice < user_slice_count; slice++){
    
    for (frame = 0; frame < user_frame_count; frame++){
      
      mni_image = MniFile->Frames*(user_slice_list[slice]-1) + user_frame_list[frame]-1;

      image_max = MniFile->ImageParameters[mni_image].Qsc 
        * (float)(255 - MniFile->ImageParameters[mni_image].Isea);
      image_min = -MniFile->ImageParameters[mni_image].Qsc 
        * (float)MniFile->ImageParameters[mni_image].Isea;

      if (image_min < *min){
        *min = image_min;
        *min_frame = frame;
        *min_slice = slice;
      }
      if (image_max > *max){
        *max = image_max;
        *max_frame = frame;
        *max_slice = slice;
      }
      
      get_mni_slice(MniFile,
                    user_slice_list[slice]-1,
                    user_frame_list[frame]-1,
                    (void *)present_byte,
                    MNIchar,
                    scale);

      present_byte += MniFile->ImageSize;      
      
      if (progress && mni_image % notice_every == 0){
        fprintf(stderr, ".");
        fflush(stderr);
      }
      
    }
    
  }

  if (progress) fprintf(stderr, " done\n");

  notice_every = images_to_get/MAX(user_frame_count,user_slice_count);
  if (notice_every == 0) notice_every = 1;

  range_factor = (float)index_range / (*max - *min);
  present_short = dynamic_volume + images_to_get * MniFile->ImageSize - 1;
  present_byte = (unsigned char *)dynamic_volume + images_to_get * MniFile->ImageSize - 1;
  
  if (progress) fprintf(stderr, "Transforming ");
  
  for (slice = user_slice_count-1; slice >= 0; slice--){
    
    for (frame = user_frame_count-1; frame >= 0; frame--){
      
      mni_image = MniFile->Frames*(user_slice_list[slice]-1) + user_frame_list[frame]-1;
      user_image = slice * user_frame_count + frame;

      pixel_count = MniFile->ImageSize;
      present_factor = range_factor 
        * MniFile->ImageParameters[mni_image].Qsc;
      present_term = range_factor * MniFile->ImageParameters[mni_image].Qsc 
        * MniFile->ImageParameters[mni_image].Isea
        + *min * range_factor - index_min;

      while (pixel_count--) 
        *present_short-- = (unsigned short)
          (present_factor * (short)(*present_byte--) - present_term);

      if (progress && user_image % notice_every == 0){
        fprintf(stderr, ".");
        fflush(stderr);
      }
    }
  }

  if (progress) fprintf(stderr, " done\n");
  
  if (frames_present) {
    
    slice = user_slice_list[0]-1;
    for (frame = 0; frame < user_frame_count; frame++){
      user_image = slice * MniFile->Frames + user_frame_list[frame]-1;
      frame_length[frame] = MniFile->ImageParameters[user_image].Isctim/60.0;
      frame_time[frame] = MniFile->ImageParameters[user_image].Tsc + frame_length[frame]/2.0;
    }
  }
}

void read_float_data(FILE *input_fp,
                     int frames,
                     int slices,
                     int image_width,
                     int image_height,
                     int image_size,
                     int no_of_images,
                     float *min,
                     float *max,
                     int *min_frame,
                     int *min_slice,
                     int *max_frame,
                     int *max_slice,
                     float *frame_time,
                     float *frame_length,
                     int time_available,
                     unsigned short *dynamic_volume
                     )
{
  float *float_data, *p, factor, term; 
  int slice, frame, pixel;
  long total_pixels;
  
  total_pixels = image_size * no_of_images;
  
  if ((float_data = (float *)malloc((sizeof(float) * total_pixels))) == NULL){
    fprintf(stderr,"NOT ENOUGH MEMORY AVAILABLE\n");
    exit(ERROR_STATUS);
  }
  if (total_pixels != fread(float_data, sizeof(float), total_pixels, input_fp)){
    fprintf(stderr,"\007%d image(s) of size %d x %d not found\n",
                   no_of_images,image_width,image_height);
    exit(ERROR_STATUS);
  }
  
  p = float_data;
  *min = FLT_MAX; *max = -FLT_MAX;
  
  for (slice = 0; slice < slices; slice++)
    for (frame = 0; frame < frames; frame++){
      if (slice == 0 && time_available){
        frame_length[frame] = *(p+1);
        frame_time[frame] = *p + frame_length[frame]/120.0;
        *p = *(p+1) = *(p+2);
      }	 
      for (pixel = 0; pixel < image_size; pixel++){
        if (*p < *min){
          *min = *p;
          *min_frame = frame;
          *min_slice = slice;
        }
        if (*p > *max){
          *max = *p;
          *max_frame = frame;
          *max_slice = slice;
        }
        p++;
      }
    }
  
  factor = (float)index_range / (*max - *min);
  term = -factor * *min + (float)index_min;
  p = float_data;
  
  for (slice = 0; slice < slices; slice++)
    for (frame = 0; frame < frames; frame++)
      for (pixel = 0; pixel < image_size; pixel++)
        *dynamic_volume++ = (unsigned short)(*p++ * factor + term);
  
  free(float_data);
}

void spectral(float index, float *rgb)
{
  
  static float ramp[][RGB] = {
    {0.0000,0.0000,0.0000},
    {0.4667,0.0000,0.5333},
    {0.5333,0.0000,0.6000},
    {0.0000,0.0000,0.6667},
    {0.0000,0.0000,0.8667},
    {0.0000,0.4667,0.8667},
    {0.0000,0.6000,0.8667},
    {0.0000,0.6667,0.6667},
    {0.0000,0.6667,0.5333},
    {0.0000,0.6000,0.0000},
    {0.0000,0.7333,0.0000},
    {0.0000,0.8667,0.0000},
    {0.0000,1.0000,0.0000},
    {0.7333,1.0000,0.0000},
    {0.9333,0.9333,0.0000},
    {1.0000,0.8000,0.0000},
    {1.0000,0.6000,0.0000},
    {1.0000,0.0000,0.0000},
    {0.8667,0.0000,0.0000},
    {0.8000,0.0000,0.0000},
    {0.8000,0.8000,0.8000}
  };
  
  float base, slope; int floor;
  
  if (index >= 1.0){
    *rgb++ = ramp[20][R];
    *rgb++ = ramp[20][G];
    *rgb = ramp[20][B];
  }
  else if (index <= 0){
    *rgb++ = ramp[0][R];
    *rgb++ = ramp[0][G];
    *rgb = ramp[0][B];
  }
  else{
    base = index * 20;
    floor = (int)floorf(base);
    slope = base - floor;
    *rgb++ = ramp[floor][R] + (ramp[floor+1][R] - ramp[floor][R]) * slope;
    *rgb++ = ramp[floor][G] + (ramp[floor+1][G] - ramp[floor][G]) * slope;
    *rgb = ramp[floor][B] + (ramp[floor+1][B] - ramp[floor][B]) * slope;
  }
}

void hotmetal(float index, float *rgb)
{
  
  if (index >= 1.0){
    *rgb++ = 1.0;
    *rgb++ = 1.0;
    *rgb = 1.0;
  }
  else if (index <= 0){
    *rgb++ = 0.0;
    *rgb++ = 0.0;
    *rgb = 0.0;
  }
  else{
    float r,g,b;
    r = 2.0 * index;
    if ((g = 2.0 * index - 0.5) < 0) g = 0.0;
    if ((b = 2.0 * index - 1.0) < 0) b = 0.0;
    *rgb++ = (r < 1.0 ? r : 1.0);
    *rgb++ = (g < 1.0 ? g : 1.0);
    *rgb = (b < 1.0 ? b : 1.0);
  }
}

void gray(float index, float *rgb)
{
  if (index >= 1.0){
    *rgb++ = 1.0;
    *rgb++ = 1.0;
    *rgb = 1.0;
  }
  else if (index <= 0){
    *rgb++ = 0.0;
    *rgb++ = 0.0;
    *rgb = 0.0;
  }
  else{
    *rgb++ = index;
    *rgb++ = index;
    *rgb = index;
  }
}

#define xclr(x) ((_xmap == NULL) ? 0 : _xmap[x-index_min])

void 
change_color_map(void (*map)(float, float *), float low, float high)
{
    float rgb[RGB], col, m, b; 
    int index;
    XColor xcolor;

    m = 1.0 / (high - low);
    b = low / (low - high);

    if (_xmap != NULL) {
        XFreeColors(_xDisplay, _xColormap, _xmap, index_max - index_min, 0);
        free(_xmap);
    }
    _xmap = malloc(sizeof(unsigned long) * (index_max - index_min + 1));
    if (_xmap == NULL) {
        exit(-1);
    }
              
    for (index = index_min; index <= index_max; index++) {
        col = m*(index-index_min)/index_range+b;
        map(col, rgb);

        xcolor.flags = DoRGB;
        xcolor.red = RGB_MAX*rgb[R];
        xcolor.green = RGB_MAX*rgb[G];
        xcolor.blue = RGB_MAX*rgb[B];
        if (XAllocColor(_xDisplay, _xColormap, &xcolor)) {
            _xmap[index-index_min] = xcolor.pixel;
        }
    }
}

void
make_color_bar(unsigned short *color_bar)
{
    int x,y; 
    unsigned short color;
    float slope;
  
    slope = (float)index_range/COLOR_BAR_HEIGHT;
    for (y = 0; y <= COLOR_BAR_HEIGHT; y++) {
        color = slope * y + index_min + 0.5;
        for (x = 0; x < COLOR_BAR_WIDTH; x++)
            *color_bar++ = color;
    }
}

int
in_subwindow(int *position, int *window)
{
    return (position[X] > window[0] && 
            position[X] < window[1] && 
            position[Y] > window[2] && 
            position[Y] < window[3]
            );
}

#define COLOR_BAR_BOTTOM (COLOR_BAR_HEIGHT+COLOR_BAR_TOP)

float
get_scaled_position(short position)
{
    if (position > COLOR_BAR_BOTTOM) 
        position = COLOR_BAR_BOTTOM;
    if (position < COLOR_BAR_TOP) 
        position = COLOR_BAR_TOP;
  
    return (float)(COLOR_BAR_BOTTOM - position) / COLOR_BAR_HEIGHT;
}

float 
get_color(short position, float low, float high)
{
    float scaled_position, m, b;

    scaled_position = get_scaled_position(position);
  
    m = 1 / (high - low);
    b = -m * low;
  
    return (m * scaled_position + b);
}

void 
new_scale_slope(short position, float *low, float *high, int anchor, 
                float moving_color)
{
    float scaled_position, m, b;

    scaled_position = get_scaled_position(position);
    switch (anchor){
    case BOTTOM:
        b = (scaled_position - *low);
        if (b != 0.0) {
            m = moving_color / b;
            b = -*low * m;
            *high = (1.0 - b) / m;
        }
        break;
    case TOP:     
        b = (scaled_position - *high);
        if (b != 0.0) {
            m = (moving_color - 1.0) / b;
            b = 1.0 - *high * m;
            *low = -b / m;
        }
        break;
    }
    
    if (*high <= *low) {
        *high = *low + 0.01;
    }
}

void 
translate_scale(short position, float *low, float *high, float moving_color)
{
    float scaled_position, m, b;
  
    scaled_position = get_scaled_position(position);

    m = 1 / (*high - *low);
    b = moving_color - m * scaled_position;
    *low = -b / m;
    *high = (1.0 - b) / m;   

    if (*high <= *low) {
        *high = *low + 0.01;
    }
}

#if DO_RESAMPLE
unsigned char *
nearestNeighbour(unsigned short *refImage,
                 int rw, int rh,
                 int nw, int nh,
                 int newDims)
                        
{
    static unsigned char *newImage;
    static unsigned int *nx, *ny;
    static int set = False;
    static double xFactor, yFactor;

    unsigned int x, y, yOffset;
    int *iptr;
  
    if (newDims) {

        if (set) {
            /* free(newImage); */
            free(nx);
            free(ny);
            set = False;
        }

        xFactor = (double) (rw - 1) / (nw - 1);
        yFactor = (double) (rh - 1) / (nh - 1);

    }

    if (debug_level > 0) {
        printf ("In nearestNeighbour:\n");
        printf (" xFactor: %.3f, yFactor: %.3f\n", xFactor, yFactor);
        printf ("      rw: %5d,      rh: %5d\n", rw, rh);
        printf ("      nw: %5d,      nh: %5d\n", nw, nh);
    }

    if (!set) {
        newImage = (unsigned char *) malloc (sizeof (unsigned char) * 4 * nw * nh);
        nx = (unsigned int *) malloc (sizeof (unsigned int) * nw);
        ny = (unsigned int *) malloc (sizeof (unsigned int) * nh);
        set = True;
  }
    
  for (x = 0; x < nw; x++) {
    nx[x] = ROUND ( x * xFactor );
    if (debug_level > 2) (void) printf ("x: %03d, nx: %03d\n", x, nx[x]);
  }

  for (y = 0; y < nh; y++) {
    ny[y] = ROUND ( y * yFactor );
    if (debug_level > 2) (void) printf ("y: %03d, ny: %03d\n", y, ny[y]);
  }

  iptr = (int *) newImage;

  for (y = 0; y < nh; y++) {
      int iy = nh - (y + 1);
    yOffset = ny[iy] * rw;
    for (x = 0; x < nw; x++) {
        *iptr++ = xclr(*(refImage + yOffset + nx[x])); 
    }
  }

  return (newImage);
  
} /* end of nearestNeighbour */


/* ----------------------------------------------------------------------------
@NAME       : interpolateImage
@INPUT      : 
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION: 
@GLOBALS    : 
@CALLS      : 
@CREATED    : March, 1996 (GC Leger)
@MODIFIED   : 
---------------------------------------------------------------------------- */

unsigned char *
interpolateImage (unsigned short *refImage,
                  int rw, int rh,
                  int nw, int nh,
                  int newDims)
     
{

  static unsigned char *newImage;
  static unsigned int *bx, *by;
  static double *nx, *ny, x1, x2;
  static double *wx, *wy;
  static double xFactor, yFactor;
  static int set = False;

  unsigned int x, y, yOffset;
  int *iptr;
  unsigned short *xref1, *xref2;
  
  if (newDims) {

    if (set) {
        /* free (newImage); */
      free (nx);  free (ny);
      free (bx);  free (by);
      free (wx);  free (wy);
      set = False;
    }

    xFactor = (double) (rw - 1) / (nw - 1);
    yFactor = (double) (rh - 1) / (nh - 1);

  }

  if (!set) {
    newImage = (unsigned char *) malloc (4 * nw * nh);
    nx = (double *) malloc (sizeof (double) * nw);
    bx = (unsigned int *) malloc (sizeof (unsigned int) * nw);
    wx = (double *) malloc (sizeof (double) * nw);
    ny = (double *) malloc (sizeof (double) * nh);
    by = (unsigned int *) malloc (sizeof (unsigned int) * nh);
    wy = (double *) malloc (sizeof (double) * nh);
    set = True;
  }
    
  if (newDims) {

    for (x = 0; x < nw; x++) {
      bx[x] = floor ( nx[x] = x * xFactor );
      wx[x] = nx[x] - bx[x];
    }

    for (y = 0; y < nh; y++) {
      by[y] = floor ( ny[y] = y * yFactor );
      wy[y] = ny[y] - by[y];
    }

  }

  if (debug_level > 0) {
    (void) printf ("In interpolateImage:\n");
    (void) printf ("xFactor: %.4f, yFactor: %.4f\n", xFactor, yFactor);
    (void) printf ("rw: %d nw: %d, nx[nw-1]: %.4f\n", rw, nw, nx[nw-1]);
    (void) printf ("rh: %d nh: %d, ny[nh-1]: %.4f\n", rh, nh, ny[nh-1]);
  }

  iptr = (int *) newImage;

  for (y = 0; y < nh; y++) {
      int iy = nh - (y + 1);
    yOffset = by[iy] * rw;

    for (x = 0; x < nw; x++) {

      xref1 = refImage + yOffset + bx[x];
      xref2 = xref1 + rw;

      switch ((bx[x] + 1 == rw) + (by[iy] + 1 == rh)<<1) {

      case 0:
        x1 = *xref1 + wx[x] * (*(xref1 + 1) - *xref1);
        x2 = *xref2 + wx[x] * (*(xref2 + 1) - *xref2);
        *iptr++ = xclr(ROUND(x1 + wy[iy] * (x2 - x1)));
        break;

      case 1:
        *iptr++ = xclr(ROUND(*xref1 + wy[iy] * (*xref2 - *xref1)));
        break;

      case 2:
        *iptr++ = xclr(ROUND(*xref1 + wx[x] * (*(xref1 + 1) - *xref1)));
        break;

      case 3:
        *iptr++ = xclr(*xref1);
        break;
      }
    }
  }

  return (newImage);
  
} /* end of interpolateImage */

#endif /* DO_RESAMPLE */

void 
write_eval_pixel(struct postf_wind *wnd_ptr,
                 short x, short y, float v, int a, int redraw_only)
{
  char out_string[MAX_STRING_LENGTH];
  static short lx,ly;
  static int la;
  static float lv;

  if (!redraw_only){
    lx = x;
    ly = y;
    la = a;
    lv = v;
  }

  XSetForeground(_xDisplay, wnd_ptr->gc, ImageBackgroundColor);
  XFillRectangle(_xDisplay, wnd_ptr->draw, wnd_ptr->gc, 
                 WINDOW_WIDTH+COLOR_BAR_XOFFSET/2,
                 0,
                 COLOR_BAR_WINDOW-COLOR_BAR_XOFFSET/2,COLOR_BAR_YOFFSET);
  
  XSetForeground(_xDisplay, wnd_ptr->gc, TextColor);

  sprintf(out_string,"x=%d",lx);
  XDrawString(_xDisplay, wnd_ptr->draw, wnd_ptr->gc, 
              EVAL_XCOORD, 
              EVAL_YCOORD,
              out_string, strlen(out_string));
  
  sprintf(out_string,"y=%d",ly);
  XDrawString(_xDisplay, wnd_ptr->draw, wnd_ptr->gc, 
              EVAL_XCOORD, 
              EVAL_YCOORD+TEXT_SEP,
              out_string, strlen(out_string));
  
  
  sprintf(out_string,"v=%g",lv);
  XDrawString(_xDisplay, wnd_ptr->draw, wnd_ptr->gc, 
              EVAL_XCOORD, 
              EVAL_YCOORD+2*TEXT_SEP,
              out_string, strlen(out_string));
  
  sprintf(out_string,"a=%d",la);
  XDrawString(_xDisplay, wnd_ptr->draw, wnd_ptr->gc, 
              EVAL_XCOORD, 
              EVAL_YCOORD+3*TEXT_SEP,
              out_string, strlen(out_string));
}

void 
write_scale(struct postf_wind *wnd_ptr,
            float low, float high, 
            int frame, int slice, int frame_offset, int slice_offset)
{
    char out_string[MAX_STRING_LENGTH];

    XSetForeground(_xDisplay, wnd_ptr->gc, ImageBackgroundColor);
    XFillRectangle(_xDisplay, wnd_ptr->draw, wnd_ptr->gc, 
                   WINDOW_WIDTH+COLOR_BAR_XOFFSET/2, 
                   COLOR_BAR_TOP+COLOR_BAR_HEIGHT+3,
                   COLOR_BAR_WINDOW-COLOR_BAR_XOFFSET/2, 
                   COLOR_BAR_YOFFSET);
                 
    XSetForeground(_xDisplay, wnd_ptr->gc, TextColor);

    sprintf(out_string,"max = %3.0f%%", high * 100.0);
    XDrawString(_xDisplay, wnd_ptr->draw, wnd_ptr->gc,
                SCALE_XCOORD, SCALE_YCOORD, 
                out_string, strlen(out_string));
  
    sprintf(out_string,"min = %3.0f%%",low * 100.0);
    XDrawString(_xDisplay, wnd_ptr->draw, wnd_ptr->gc,
                SCALE_XCOORD, SCALE_YCOORD+TEXT_SEP, 
                out_string, strlen(out_string));
  
    sprintf(out_string,"f: %2d  s: %2d",frame+frame_offset,slice+slice_offset);
    XDrawString(_xDisplay, wnd_ptr->draw, wnd_ptr->gc,
                SCALE_XCOORD, SCALE_YCOORD+2*TEXT_SEP, 
                out_string, strlen(out_string));
}

void draw_axes(struct postf_wind *wnd_ptr,
               float abscisa_min,
               float abscisa_max,
               float min,
               float max,
               float x,
               float y,
               float z,
               int axes,
               float v,
               int plot_type,
               int initializing
               )
{
    static float  y_max, y_tic, y_lo, x_ts, x_l, x_te, y_te,y_min,yy,y_limit;
    static float px_max,px_tic,px_lo,py_ts,py_l,py_te,px_ls,px_min;
    static float dx_max,dx_tic,dx_lo,dy_ts,dy_l,dy_te,dx_ls,dx_min;
    float  x_min, x_max, x_tic, x_lo, y_ts, y_l, x_ls;
    XPoint pt[2];
  
    char axis_string[MAX_STRING_LENGTH];
    
    if (initializing){
        if (wnd_ptr != NULL) {
            XSetForeground(_xDisplay, wnd_ptr->gc, ProfileBackgroundColor);
            XFillRectangle(_xDisplay, wnd_ptr->draw, wnd_ptr->gc, 
                           0, 0,
                           AUXILLARY_WINDOW_WIDTH, AUXILLARY_WINDOW_HEIGHT);
        }

        y_min = min;
        y_max = max;
        y_tic = (y_max-y_min)/AXES_TICS;
        y_limit = y_max + y_tic*0.1;
        y_lo = y_tic/15.0;
        if (y_min < 0.0 && y_max > 0.0) {
            yy = 0.0; /* at y=0 if data spans 0 */
            x_ts = -y_tic/TIC_REDUCTION;
            x_te = 0.0;
            x_l = 4.0*x_ts;
        }
        else {
            yy = y_min; /* at y=y_min otherwise */
            x_ts = y_min - y_tic/TIC_REDUCTION;
            x_te = y_min;
            x_l = y_min - 3.0*y_tic/TIC_REDUCTION;
        }
    
        switch (plot_type) {
        case PROFILE:
            px_min = abscisa_min;
            px_max = abscisa_max;
            px_tic = (px_max-px_min)/AXES_TICS;
            px_lo = px_tic/6.0;
            py_ts = px_min-px_tic/TIC_REDUCTION;
            py_te = px_min;
            py_l = py_ts - 6.0*(py_te-py_ts);
            if (y_min < 0.0 && y_max > 0.0){
                px_ls = px_min + px_tic;
            }
            else{
                px_ls = 0.0;
            }
            break;
        case DYNAMIC:
            dx_min = abscisa_min;
            dx_max = abscisa_max;
            dx_tic = (dx_max-dx_min)/AXES_TICS;
            dx_lo = dx_tic/6.0;
            dy_ts = dx_min-dx_tic/TIC_REDUCTION;
            dy_te = dx_min;
            dy_l = dy_ts - 6.0*(dy_te-dy_ts);
            if (y_min < 0.0 && y_max > 0.0) {
                dx_ls = dx_min + dx_tic;
            }
            else{
                dx_ls = 0.0;
            }
            break;
        }
    
    }
    else{
        float point[XY];
    
        switch (plot_type) {
        case PROFILE:
            x_min = px_min;
            x_max = px_max;
            x_tic = px_tic;
            x_lo = px_lo;
            y_ts = py_ts;
            y_te = py_te;
            y_l = py_l;
            x_ls = px_ls;
            break;
        default:
        case DYNAMIC:
            x_min = dx_min;
            x_max = dx_max;
            x_tic = dx_tic;
            x_lo = dx_lo;
            y_ts = dy_ts;
            y_te = dy_te;
            y_l = dy_l;
            x_ls = dx_ls;
            break;
        }
    
        /* draw x axis */
        XSetForeground(_xDisplay, wnd_ptr->gc, AxisColor);

        pt[0].x = scalex(wnd_ptr, x_min);
        pt[0].y = scaley(wnd_ptr, yy);
        pt[1].x = scalex(wnd_ptr, x_max);
        pt[1].y = pt[0].y;

        XDrawLines(_xDisplay, wnd_ptr->draw, wnd_ptr->gc, pt, 2, 
                   CoordModeOrigin);

        for (point[X] = x_ls; point[X] <= x_max; point[X] += x_tic){
            pt[0].x = scalex(wnd_ptr, point[X]);
            pt[0].y = scaley(wnd_ptr, x_ts);
            pt[1].x = pt[0].x;
            pt[1].y = scaley(wnd_ptr, x_te);
            
            XDrawLines(_xDisplay, wnd_ptr->draw, wnd_ptr->gc, pt, 2, 
                       CoordModeOrigin);

            sprintf(axis_string,"%5.1f",point[X]);
            
            XDrawString(_xDisplay, wnd_ptr->draw, wnd_ptr->gc,
                        scalex(wnd_ptr, point[X] - x_lo), scaley(wnd_ptr, x_l),
                        axis_string, strlen(axis_string));
        }
    
        /* draw y axis */
        pt[0].x = scalex(wnd_ptr, x_min);
        pt[0].y = scaley(wnd_ptr, y_min);
        pt[1].x = pt[0].x;
        pt[1].y = scaley(wnd_ptr, y_max);

        XDrawLines(_xDisplay, wnd_ptr->draw, wnd_ptr->gc, pt, 2, 
                   CoordModeOrigin);

        for (point[Y] = y_min; point[Y] <= y_limit; point[Y] += y_tic) {
            pt[0].x = scalex(wnd_ptr, y_ts);
            pt[0].y = scaley(wnd_ptr, point[Y]);
            pt[1].x = scalex(wnd_ptr, y_te);
            pt[1].y = pt[0].y;

            XDrawLines(_xDisplay, wnd_ptr->draw, wnd_ptr->gc, pt, 2, 
                       CoordModeOrigin);

            sprintf(axis_string,"%5.1f",point[Y]);
            XDrawString(_xDisplay, wnd_ptr->draw, wnd_ptr->gc,
                        scalex(wnd_ptr, y_l), scaley(wnd_ptr, point[Y] - y_lo),
                        axis_string, strlen(axis_string));
        }

        /* Draw the value marker on the Y axis */
        pt[0].x = scalex(wnd_ptr, y_ts);
        pt[0].y = scaley(wnd_ptr, v);
        pt[1].x = scalex(wnd_ptr, y_te);
        pt[1].y = pt[0].y;

        XDrawLines(_xDisplay, wnd_ptr->draw, wnd_ptr->gc, pt, 2, 
                   CoordModeOrigin);
    
        if ((plot_type == DYNAMIC) || (axes & XAXIS)){
            if (plot_type == DYNAMIC) {
                XSetForeground(_xDisplay, wnd_ptr->gc, DynamicColor);
            }
            else {
                XSetForeground(_xDisplay, wnd_ptr->gc, ProfileXColor);
            }
            point[X] = x;
            if (plot_type == PROFILE) {
                point[X]++;
            }

            pt[0].x = scalex(wnd_ptr, point[X]);
            pt[0].y = scaley(wnd_ptr, 0.0);
            pt[1].x = pt[0].x;
            pt[1].y = scaley(wnd_ptr, v);

            XDrawLines(_xDisplay, wnd_ptr->draw, wnd_ptr->gc, pt, 2, 
                       CoordModeOrigin);
    }
    
    if (plot_type == PROFILE){
      if (axes & YAXIS){
        XSetForeground(_xDisplay, wnd_ptr->gc, ProfileYColor);

        pt[0].x = scalex(wnd_ptr, y+1.0);
        pt[0].y = scaley(wnd_ptr, 0.0);
        pt[1].x = pt[0].x;
        pt[1].y = scaley(wnd_ptr, v);

        XDrawLines(_xDisplay, wnd_ptr->draw, wnd_ptr->gc, pt, 2, CoordModeOrigin);
      }
      
      if (axes & ZAXIS){
        XSetForeground(_xDisplay, wnd_ptr->gc, ProfileZColor);

        pt[0].x = scalex(wnd_ptr, z+1.0);
        pt[0].y = scaley(wnd_ptr, 0.0);
        pt[1].x = pt[0].x;
        pt[1].y = scaley(wnd_ptr, v);

        XDrawLines(_xDisplay, wnd_ptr->draw, wnd_ptr->gc, pt, 2, CoordModeOrigin);
      }
    }
  }
}

void draw_profile(struct postf_wind *wnd_ptr,
                  unsigned short *image,
                  unsigned short *dynamic_volume,
                  int width,
                  int height,
                  short x,
                  short y,
                  float v,
                  float cfact,
                  float cterm,
                  int frames,
                  int slices,
                  int frame,
                  int slice,
                  int axes,
                  int initialize,
                  int redraw_only
                  )
{
  static int plot_present,iframes,z_step;
  static int islices,lframe,is,iw,ih;
  static short lx,ly,lz;
  static unsigned short *ld;
  static float lv,icf,ict,x_step;
  static unsigned short *id;
  
  if (initialize){
    iframes = frames;
    islices = slices;
    iw = width;
    ih = height;
    is = width * height;
    icf = cfact;
    ict = cterm;
    id = dynamic_volume;
    x_step = (float)MAX(iw,ih)/islices;
    z_step = iframes*is;
  }
  else{
    if (!redraw_only){
      ld = image;
      lframe = frame;
      lx = x;
      ly = y;
      lz = (float)slice * iw / islices;
      lv = v;
    }

    XSetForeground(_xDisplay, wnd_ptr->gc, ProfileBackgroundColor);
    XFillRectangle(_xDisplay, wnd_ptr->draw, wnd_ptr->gc, 0, 0,
                   wnd_ptr->vw, wnd_ptr->vh);

    if (plot_present || !redraw_only){
        XPoint pt[512];         /* TODO: Count better not be > 512! */
        int i;
        unsigned short *p;
        float point[XY];
        int count;
      
      if (axes & XAXIS){
        point[X] = 1.0;
        p = ld+(ly*iw);
        count = iw;
        
        XSetForeground(_xDisplay, wnd_ptr->gc, ProfileXColor);
        for (i = 0; i < count; i++) {
            pt[i].x = scalex(wnd_ptr, point[X]);
            point[Y] = (*p++ * icf + ict);
            pt[i].y = scaley(wnd_ptr, point[Y]);
            point[X]++;
        }
        XDrawLines(_xDisplay, wnd_ptr->draw, wnd_ptr->gc,
                   pt, count, CoordModeOrigin);
      }
      
      if (axes & YAXIS){
        point[X] = 1.0;
        p = ld+lx;
        count = ih;

        XSetForeground(_xDisplay, wnd_ptr->gc, ProfileYColor);
        for (i = 0; i < count; i++) {
            point[Y] = *p * icf + ict;
            pt[i].x = scalex(wnd_ptr, point[X]);
            pt[i].y = scaley(wnd_ptr, point[Y]);
            point[X]++;
            p+=iw;
        }
        XDrawLines(_xDisplay, wnd_ptr->draw, wnd_ptr->gc,
                   pt, count, CoordModeOrigin);
      }
      
      if (axes & ZAXIS){
        point[X] = 1.0;
        p = id + lframe*is + ly*iw + lx;
        count = islices;
        
        XSetForeground(_xDisplay, wnd_ptr->gc, ProfileZColor);
        for (i = 0; i < count; i++) {
            point[Y] = *p * icf + ict;
            pt[i].x = scalex(wnd_ptr, point[X]);
            pt[i].y = scaley(wnd_ptr, point[Y]);
            point[X]+=x_step;
            p+=z_step;
        }
        XDrawLines(_xDisplay, wnd_ptr->draw, wnd_ptr->gc,
                   pt, count, CoordModeOrigin);
      }
      
      draw_axes(wnd_ptr,
                NIL,NIL,NIL,NIL,lx,ly,lz,axes,lv,PROFILE,NOT_INITIALIZE);
      if (!plot_present) {
          plot_present = TRUE;
      }
    }
    swapbuffers(wnd_ptr);
  }
}

int get_roi_tac(unsigned short *dynamic_volume,
                 int frames,
                 int frame,
                 int slice,
                 int x,
                 int y,
                 int image_width,
                 int image_height,
                 int image_size,
                 float cfact,
                 float cterm,
                 int roi_radius,
                 float *roi_tac,
                 float *roi_std
                 )
{
  unsigned short *position;
  int i, j, present_frame, roi_area, *mask_p;
  float value, mv, sv, svv, diff;

  static int
    lx = -1, ly = -1,
    lroi_area = -1, lroi_radius = -1,
    istart, jstart, istop, jstop,
    n, r2,
    roi_mask[MAX_ROI_AREA];

  if (lroi_radius != roi_radius || lx != x || ly != y){

    lroi_radius = roi_radius;
    lx = x; ly = y;

    istart = MAX(0, x-roi_radius);
    istop  = MIN(image_width-1, x+roi_radius);
    jstart = MAX(0, y-roi_radius);
    jstop  = MIN(image_height-1, y+roi_radius);

    r2 = roi_radius * roi_radius;
    roi_area = (istart-istop+1) * (jstart-jstop+1);

    if (lroi_area != roi_area){
      lroi_area = roi_area;
      mask_p = roi_mask;

      n = 0;
      for (j = jstart; j <= jstop; j++)
        for (i = istart; i <= istop; i++)
          if ((*mask_p++ = ((i-x)*(i-x)+(j-y)*(j-y) <= r2)) != 0)
            n++;
    }
  }

  for (present_frame = 0; present_frame < frames; present_frame++){
    
    svv = sv = 0; mask_p = roi_mask;
    
    for (j = jstart; j <= jstop; j++){
      for (i = istart; i <= istop; i++){
        if (*mask_p++){
          position = 
            dynamic_volume + (slice*frames + present_frame) * image_size + j * image_width + i;
          value = *position * cfact + cterm;
          sv += value;
          svv += value * value;
        }
      }
    }

    mv = sv/n;
    *roi_tac++ = mv;
    diff = svv - n*mv*mv;
    if (diff < 0) diff = -diff;
    *roi_std++ = sqrt(diff / (n-1));

  }

  return(n);

}

void draw_dynamic(struct postf_wind *wnd_ptr,
                  unsigned short *dynamic_volume,
                  float *time,
                  int frames,
                  int frame,
                  int slice,
                  int x,
                  int y,
                  int width,
                  int height,
                  float v,
                  float cfact,
                  float cterm,
                  int roi_radius,
                  int initialize,
                  int redraw_only,
                  int roi_dump
                  )
{
  static int plot_present,offset;
  static int is,iw,ih,lx,ly,iframes,lframe,lslice,lroi_radius;
  static unsigned short *id;
  static float lv,icf,ict,*itime,*roi_tac,*roi_std;
  XPoint pt[512];               /* iframes better not be > 512 */
  
  if (initialize){
    iw = width;
    ih = height;
    is = width * height;
    icf = cfact;
    ict = cterm;
    iframes = frames;
    itime = time;
    roi_tac = (float *)malloc(sizeof(float)*(iframes));
    roi_std = (float *)malloc(sizeof(float)*(iframes));
    id = dynamic_volume;
  }
  else{
    if (!redraw_only){
      lv = v;
      lx = x;
      ly = y;
      lslice = slice;
      lframe = frame;
      lroi_radius = roi_radius;
      offset = slice * iframes * is + x + iw * y;
    }

    XSetForeground(_xDisplay, wnd_ptr->gc, ProfileBackgroundColor);
    XFillRectangle(_xDisplay, wnd_ptr->draw, wnd_ptr->gc, 0, 0,
                   wnd_ptr->vw, wnd_ptr->vh);

    if (plot_present || !redraw_only){
      
      int present_frame;
      float point[XY];
      float *t;
      
      t = itime;

      XSetForeground(_xDisplay, wnd_ptr->gc, DynamicColor);
      
      if (lroi_radius == 0){
        if (roi_dump) printf("%5d\t%10d\n", x+1, y+1);
        for (present_frame = 0; present_frame < iframes; present_frame++){
            point[Y] = *(id + offset + present_frame * is) * icf + ict;
            point[X] = *t++;
            pt[present_frame].x = scalex(wnd_ptr, point[X]);
            pt[present_frame].y = scaley(wnd_ptr, point[Y]);
            if (roi_dump) printf("%5.2f\t%10f\n", point[X], point[Y]);
        }
        XDrawLines(_xDisplay, wnd_ptr->draw, wnd_ptr->gc, 
                   pt, iframes, CoordModeOrigin);
      } else {
        int roi_area;
        roi_area = get_roi_tac(id,
                               iframes,
                               lframe,
                               lslice,
                               lx,
                               ly,
                               iw,
                               ih,
                               is,
                               icf,
                               ict,
                               lroi_radius,
                               roi_tac,
                               roi_std);
        if (roi_dump) printf("%5d\t%10d\t%10d\n", x+1, y+1, roi_area);
        for (present_frame = 0; present_frame < iframes; present_frame++){
            point[Y] = *(roi_tac + present_frame);
            point[X] = *t++;
            pt[present_frame].x = scalex(wnd_ptr, point[X]);
            pt[present_frame].y = scaley(wnd_ptr, point[Y]);
            if (roi_dump) 
              printf("%5.2f\t%10f\t%10f\n", point[X], point[Y], *(roi_std+present_frame));
        }
        XDrawLines(_xDisplay, wnd_ptr->draw, wnd_ptr->gc, 
                   pt, iframes, CoordModeOrigin);
        lv = *(roi_tac+lframe);
      }
      
      draw_axes(wnd_ptr, NIL,NIL,NIL,NIL,itime[lframe],NIL,NIL,NIL,lv,DYNAMIC,NOT_INITIALIZE);
      if (!plot_present) plot_present = TRUE;
    }
    swapbuffers(wnd_ptr);
  }
}

void 
evaluate_pixel(int position[XY],
               unsigned short *image,
               int frame,
               int slice,
               float cfact,
               float cterm,
               int width,
               int height,
               int size,
               int roi_radius,
               int profiling,
               int profile_frozen,
               int dynamic,
               int dynamic_frozen,
               int axes,
               struct postf_wind *main_window,
               struct postf_wind *profile_window,
               struct postf_wind *dynamic_window,
               int roi_dump
               )
{
    static short lx,ly;
    int roi_area;
    short x,y;
    float v, std;
  
    x = position[X];
    y = position[Y];
  
    if (x >= 0 && x < width && y >= 0 && y < height){
        lx = x; ly = y;
        if (roi_radius != 0){
            roi_area = get_roi_tac(image,1,0,0,lx,ly,width,height,size,
                                   cfact,cterm,roi_radius,&v,&std);
        }
        else{
            roi_area = 1;
            v = *(image+lx+width*ly) * cfact + cterm;
        }
        frontbuffer(main_window, 1);
        write_eval_pixel(main_window, lx + 1, ly + 1, v, roi_area, FALSE);
        frontbuffer(main_window, 0);
        if (profile_window != NULL && !profile_frozen) {
            draw_profile(profile_window, 
                         image,NIL,NIL,NIL,lx,ly,v,NIL,NIL,NIL,NIL,
                         frame,slice,axes,NOT_INITIALIZE,NOT_REDRAW_ONLY);
        }
        if (dynamic_window != NULL && !dynamic_frozen) {
            draw_dynamic(dynamic_window, 
                         NIL,NIL,NIL,frame,slice,lx,ly,NIL,NIL,v,NIL,NIL,
                         roi_radius,NOT_INITIALIZE,NOT_REDRAW_ONLY,roi_dump);
        }
    }
}

void
calculate_conversions(float min,float max,float *cfact,float *cterm)
{
    float dynamic_range;
    dynamic_range = max - min;
    *cfact = dynamic_range / index_range;
    *cterm = -index_min * dynamic_range / index_range + min;
}

void 
draw_cursor(struct postf_wind *wnd_ptr, int *position, int radius)
{
    static int o_r = 0;
    static int o_x = -1, o_y = -1;
    XPoint pt[2];

    XSetFunction(_xDisplay, wnd_ptr->gc, GXxor);
    XSetForeground(_xDisplay, wnd_ptr->gc, WhitePixel(_xDisplay, _xScreen));

    if (o_x > 0 && o_y > 0) {
        if (o_r != 0) {
            XDrawArc(_xDisplay, wnd_ptr->wind, wnd_ptr->gc,
                     o_x-o_r,
                     o_y-o_r,
                     2*o_r,
                     2*o_r,
                     0,
                     360*64);
        }
        else {
            pt[0].x = 0;
            pt[0].y = o_y;
            pt[1].x = wnd_ptr->vw-COLOR_BAR_WINDOW;
            pt[1].y = o_y;
            XDrawLines(_xDisplay, wnd_ptr->wind, wnd_ptr->gc,
                       pt, 2, CoordModeOrigin);

            pt[0].x = o_x;
            pt[0].y = 0;
            pt[1].x = o_x;
            pt[1].y = wnd_ptr->vh;
            XDrawLines(_xDisplay, wnd_ptr->wind, wnd_ptr->gc,
                       pt, 2, CoordModeOrigin);
        }
        o_x = -1;
        o_y = -1;
    }
  
    if (position != NULL) {
        if (position[X] > 0 && position[X] < wnd_ptr->vw && 
            position[Y] > 0 && position[Y] < wnd_ptr->vh){
 
            if (radius != 0) {
                XDrawArc(_xDisplay, wnd_ptr->wind, wnd_ptr->gc,
                         position[X]-radius,
                         position[Y]-radius,
                         2*radius,
                         2*radius,
                         0,
                         360*64);
            }
            else {
                pt[0].x = 0;
                pt[0].y = position[Y];
                pt[1].x = wnd_ptr->vw-COLOR_BAR_WINDOW;
                pt[1].y = position[Y];

                XDrawLines(_xDisplay, wnd_ptr->wind, wnd_ptr->gc,
                           pt, 2, CoordModeOrigin);

                pt[0].x = position[X];
                pt[0].y = 0;
                pt[1].x = position[X];
                pt[1].y = wnd_ptr->vh;

                XDrawLines(_xDisplay, wnd_ptr->wind, wnd_ptr->gc,
                           pt, 2, CoordModeOrigin);
            }
            o_x = position[X];
            o_y = position[Y];
            o_r = radius;
        }
    }
    XSetFunction(_xDisplay, wnd_ptr->gc, GXcopy);
}

void 
erase_cursor(struct postf_wind *wnd_ptr)
{
    draw_cursor(wnd_ptr, NULL, 0);
}

void 
draw_color_bar(struct postf_wind *wnd_ptr,
               unsigned short *color_bar, float min, float max)
{
    int x,y; 
    char scale_string[MAX_STRING_LENGTH]; 
    int step; 
    float range;
    XImage *xi;
    
    xi = XCreateImage(_xDisplay, _xVisual, _xDepth, ZPixmap,
                      0, NULL, COLOR_BAR_WIDTH, COLOR_BAR_HEIGHT, 8, 0);
    xi->data = malloc(xi->bytes_per_line * COLOR_BAR_HEIGHT);
    for (x = 0; x < COLOR_BAR_WIDTH; x++) {
        for (y = 0; y < COLOR_BAR_HEIGHT; y++) {
            int iy = COLOR_BAR_HEIGHT - (y + 1);
            XPutPixel(xi, x, iy, xclr(color_bar[x + (y * COLOR_BAR_WIDTH)]));
        }
    }
    XPutImage(_xDisplay, wnd_ptr->draw, wnd_ptr->gc, xi, 0, 0,
              WINDOW_WIDTH+COLOR_BAR_XOFFSET,
              COLOR_BAR_TOP, COLOR_BAR_WIDTH, COLOR_BAR_HEIGHT);
    XDestroyImage(xi);
    
    x = WINDOW_WIDTH+COLOR_BAR_XOFFSET+COLOR_BAR_WIDTH;
    range = (max - min) / SCALE_TICS;
    XSetForeground(_xDisplay, wnd_ptr->gc, TextColor);
    for (step = 0; step <= SCALE_TICS; step++){
        y = (float)step * SCALE_STEP + COLOR_BAR_YOFFSET;
        sprintf(scale_string,"-%-7.1f", range * step + min);
        XDrawString(_xDisplay, wnd_ptr->draw, wnd_ptr->gc,
                    x, wnd_ptr->vh - (y-4) - 1,
                    scale_string, strlen(scale_string));
    }

    XDrawRectangle(_xDisplay, wnd_ptr->draw, wnd_ptr->gc,
                   WINDOW_WIDTH+COLOR_BAR_XOFFSET,
                   COLOR_BAR_TOP,
                   COLOR_BAR_WIDTH,
                   COLOR_BAR_HEIGHT);
}

void
clip_image(short *anchor,
           unsigned short *image,
           unsigned short *clipping_buffer,
           long width,
           long height,
           long new_width,
           long new_height)
{
    short y;
  
    for (y = 0; y < new_height; y++)
        memcpy(&clipping_buffer[y * new_width],
               &image[(y + anchor[Y]) * width + anchor[X]],
               new_width * sizeof(unsigned short));
}


void 
draw_image(struct postf_wind *wnd_ptr,
           int *draw_origin,
           unsigned short *image,
           unsigned short *clipping_buffer,
           long image_width,
           long image_height,
           float zoom)
{
    short
        anchor[XY],
        new_origin[XY],
        new_width,
        new_height;
    XImage *xi;
    int x, y;
    unsigned short *tptr;
    unsigned long *lptr;
    double izoom;               /* inverse of zoom */

    XSetForeground(_xDisplay, wnd_ptr->gc, ImageBackgroundColor);
    XFillRectangle(_xDisplay, wnd_ptr->draw, wnd_ptr->gc, 0, 0, 
                   wnd_ptr->vw-COLOR_BAR_WINDOW, wnd_ptr->vh);

    if (draw_origin[X] < 0 || draw_origin[Y] < 0){
        if (draw_origin[X] < 0){
            anchor[X] = -draw_origin[X]/zoom;
            new_origin[X] = 0;
            new_width = image_width - anchor[X];
        }
        else {
            anchor[X] = 0;
            new_origin[X] = draw_origin[X];
            new_width = image_width;
        }
    
        if (draw_origin[Y] < 0) {
            anchor[Y] = -draw_origin[Y]/zoom;
            new_origin[Y] = 0;
            new_height = image_height - anchor[Y];
        }
        else{
            anchor[Y] = 0;
            new_origin[Y] = draw_origin[Y];
            new_height = image_height;
        }
    
        if (new_width > 0 && new_height > 0) {

            clip_image(anchor,
                       image,
                       clipping_buffer,
                       image_width,
                       image_height,
                       new_width,
                       new_height);
#if !DO_RESAMPLE
            image_height = new_height;
            image_width = new_width;
#endif /* DO_RESAMPLE */

            new_width *= zoom;
            new_height *= zoom;
            tptr = clipping_buffer;
        }
        else {
            return;             /* Can't do anything useful, so bail. */
        }
    
    }
    else {
        new_width = image_width*zoom;
        new_height = image_height*zoom;
        new_origin[X] = draw_origin[X];
        new_origin[Y] = draw_origin[Y];
        tptr = image;
    }

    xi = XCreateImage(_xDisplay,
                      _xVisual,
                      _xDepth,
                      ZPixmap,  /* Format */
                      0,        /* Offset */
#if DO_RESAMPLE
                      resample_function(image,
                                        image_width,
                                        image_height,
                                        new_width,
                                        new_height,
                                        True),
#else
                      NULL,	/* Data */
#endif /* DO_RESAMPLE */
                      new_width, /* Width */
                      new_height, /* Height */
                      8,        /* BitmapPad */
                      0);       /* BytesPerLine */

#if !DO_RESAMPLE
    xi->data = (char*) malloc(xi->bytes_per_line * new_height);
    if (xi->data == NULL) {
        fprintf(stderr, "yikes: can't allocate memory.\n");
        exit(1);
    }
    lptr = (unsigned long *) xi->data;
    izoom = 1.0 / zoom;

    for (y = new_height-1; y >= 0; y--) {
        unsigned short *xptr = &tptr[image_width * (int)(izoom * y)];
        for (x = 0; x < new_width; x++) {
            *lptr++ = xclr(xptr[(int)(izoom * x)]);
        }
    }
#endif /* DO_RESAMPLE */
    XPutImage(_xDisplay, wnd_ptr->draw, wnd_ptr->gc,
              xi, 0, 0, 
              new_origin[X], new_origin[Y],
              new_width, 
              new_height);
    XDestroyImage(xi);
}

void
usage(char *program_name)
{
    fprintf(stderr,"Usage: %s [option [...]] [filename]\n", program_name);
    exit(ERROR_STATUS);
}

struct postf_wind *
create_basic_window(char *window_name, int width, int height)
{
    struct postf_wind *wnd_ptr;
    int a, b;
    int blackColor = BlackPixel(_xDisplay, _xScreen);
    XSizeHints hints;

    wnd_ptr = malloc(sizeof(struct postf_wind));
    if (wnd_ptr == NULL) {
        return (NULL);
    }

    wnd_ptr->display = _xDisplay;
    wnd_ptr->visual = _xVisual;
    wnd_ptr->wind = XCreateSimpleWindow(_xDisplay, 
                                        DefaultRootWindow(_xDisplay), 
                                        0, 0, 
                                        width,
                                        height,
                                        0,
                                        blackColor,
                                        blackColor);
    XStoreName(_xDisplay, wnd_ptr->wind, window_name);
    wnd_ptr->gc = XCreateGC(_xDisplay, wnd_ptr->wind, 0, NULL);

    hints.flags = (PMinSize | PMaxSize);
    hints.min_width = width;
    hints.max_width = width;
    hints.min_height = height;
    hints.max_height = height;
    XSetWMNormalHints(_xDisplay, wnd_ptr->wind, &hints);

    if (XdbeQueryExtension(_xDisplay, &a, &b)) {
        wnd_ptr->draw = XdbeAllocateBackBufferName(_xDisplay, 
                                                   wnd_ptr->wind, 
                                                   XdbeUndefined);
    }

    wnd_ptr->wbuf[0] = wnd_ptr->wind;
    wnd_ptr->wbuf[1] = wnd_ptr->draw;

    XMapWindow(_xDisplay, wnd_ptr->wind);

    wnd_ptr->vl = 0;
    wnd_ptr->vb = 0;
    wnd_ptr->vw = width;
    wnd_ptr->vh = height;
    return (wnd_ptr);
}

struct postf_wind *
create_window(char *main_title,
              float x_min,
              float x_max,
              float y_min,
              float y_max,
              int axes,
              int plot_type,
              int *origin,
              int *size
              )
{
    struct postf_wind *wnd_ptr;
    char title_string[MAX_STRING_LENGTH];
    float x_range, y_range;
    static int prof_init = FALSE;
    static int dyna_init = FALSE;

    x_range = x_max - x_min;
    y_range = y_max - y_min;

    sprintf(title_string,"%s: %s profile",
            main_title, (plot_type == PROFILE) ? "slice" : "dynamic");

    wnd_ptr = create_basic_window(title_string,
                                  AUXILLARY_WINDOW_WIDTH,
                                  AUXILLARY_WINDOW_HEIGHT);

    if ((plot_type == PROFILE && !prof_init) || 
        (plot_type == DYNAMIC && !dyna_init)){
        switch (plot_type){
        case PROFILE:
            prof_init = TRUE;
            break;
        case DYNAMIC:
            dyna_init = TRUE;
            break;
        }
    }
    /* Set up orthographic projection for this window.
     */
    my_ortho2(wnd_ptr, 
              x_min-x_range/(AXES_TICS*EXPAND),
              x_max+x_range/(AXES_TICS*EXPAND),
              y_min-y_range/(AXES_TICS*EXPAND),
              y_max+y_range/(AXES_TICS*EXPAND)
              );

    switch (plot_type) {
    case PROFILE:
        draw_profile(wnd_ptr, NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,
                     NIL,NIL,NIL,axes,NOT_INITIALIZE,REDRAW_ONLY);
        break;
    case DYNAMIC:
        draw_dynamic(wnd_ptr, NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,
                     NIL,NIL,NIL,NOT_INITIALIZE,REDRAW_ONLY,NIL);
        break;
    }
    return (wnd_ptr);
}

void 
destroy_window(struct postf_wind *wnd_ptr)
{
    XEvent ev;

    XdbeDeallocateBackBufferName(_xDisplay, wnd_ptr->wbuf[1]);
    XUnmapWindow(_xDisplay, wnd_ptr->wind);
    XFreeGC(_xDisplay, wnd_ptr->gc);
    XDestroyWindow(_xDisplay, wnd_ptr->wind);
    XSync(_xDisplay, 0);
    while (XCheckWindowEvent(_xDisplay, wnd_ptr->wind, POSTF_X_EVENT_MASK, &ev)) {
      fprintf(stderr, "winclose: dropped %d for window 0x%x\n",
	      ev.type, (unsigned int) wnd_ptr->wind);
    }
    free(wnd_ptr);
}

void 
draw_new_slice(struct postf_wind *wnd_ptr,
               int *draw_origin,
               int frame,
               int slice,
               int frame_offset,
               int slice_offset,
               int image_width,
               int image_height,
               float min,
               float max,
               float low,
               float high,
               unsigned short *image,
               unsigned short *clipping_buffer,
               float zoom,
               unsigned short *color_bar,
               int redraw)
{
    draw_image(wnd_ptr, draw_origin,image,clipping_buffer,image_width,image_height,zoom);
    draw_color_bar(wnd_ptr, color_bar,min,max);
    write_eval_pixel(wnd_ptr, 0,0,0,0,redraw);
    write_scale(wnd_ptr, low,high,frame,slice,frame_offset,slice_offset);
    swapbuffers(wnd_ptr);
}

char *
get_image_name(char *path_name)
{
  char *last;
  
  last = path_name;
  while(*path_name) if (*path_name++ == '/') last = path_name;
  return(last);
}

int
main(int argc, char *argv[])
{
    static char *window_title = NULL; /* Main window title */
    static int debug = FALSE;
    static int progress = TRUE;
    static int indices[] = {INDEX_MIN,INDEX_MAX};
#define nindices (sizeof(indices)/sizeof(indices[0]))
    static int frames = 0;
    static int frame = START_FRAME;
    static int rframes[] = {0,0};
#define nrframes (sizeof(rframes)/sizeof(rframes[0]))
    static int rslices[] = {0,0};
#define nrslices (sizeof(rframes)/sizeof(rslices[0]))
    static int slices = 0;
    static int slice = START_SLICE;
    static int image_width = IMAGE_WIDTH;
    static int image_height = IMAGE_HEIGHT;
    static int roi_radius = ROI_RADIUS;
    static  int time_available = FALSE;
    static int no_of_images = NO_OF_IMAGES;
    static void (*present_scale)(float, float *) = 0;
  
    static ArgvInfo argTable[] = {
#ifdef ARGV_VERINFO             /* For pre-MINC-1.3 compilation */
        { NULL, ARGV_VERINFO, PACKAGE_VERSION, NULL, NULL },
#endif /* ARGV_VERINFO defined */
        { "-title", ARGV_STRING, (char *) NULL, (char *) &window_title,
          "Window title" },
        { "-debug", ARGV_CONSTANT, (char *) TRUE, (char *) &debug,
          "Output minc file information" },
        { "-noprogress", ARGV_CONSTANT, (char *) FALSE, (char *) &progress,
          "Do not show progress during minc image input" },
        { "-cindex", ARGV_INT, (char *) nindices, (char *) &indices,
          "Minimum and Maximum lookup table indices" },
        { "-frames", ARGV_INT, (char *) NULL, (char *) &frames,
          "Number of data frames" },
        { "-frame", ARGV_INT, (char *) NULL, (char *) &frame,
          "Frame to be displayed" },
        { "-rframes", ARGV_INT, (char *) nrframes, (char *) &rframes,
          "First and last frames to be read" },
        { "-slices", ARGV_INT, (char *) NULL, (char *) &slices,
          "Number of data slices" },
        { "-slice", ARGV_INT, (char *) NULL, (char *) &slice,
          "Slice to be displayed" },
        { "-rslices", ARGV_INT, (char *) nrslices, (char *) &rslices,
          "First and last slices to be read" },
        { "-width", ARGV_INT, (char *) NULL, (char *) &image_width,
          "Image width" },
        { "-height", ARGV_INT, (char *) NULL, (char *) &image_height,
          "Image height" },
        { "-roi_radius", ARGV_INT, (char *) NULL, (char *) &roi_radius,
          "ROI Size" },
        { "-dynamic", ARGV_CONSTANT, (char *) TRUE, (char *) &time_available,
          "Time info available within float data" },
        { "-images", ARGV_INT, (char *) NULL, (char *) &no_of_images,
          "Number of images" },
        { "-spectral", ARGV_CONSTANT, (char *) spectral, (char *) &present_scale,
          "Use spectral scale" },
        { "-hotmetal", ARGV_CONSTANT, (char *) hotmetal, (char *) &present_scale,
          "Use hotmetal scale" },
        { "-grayscale", ARGV_CONSTANT, (char *) gray, (char *) &present_scale,
          "Use gray scale" },
        { "-describe", ARGV_FUNC, (char *) describe_postf, (char *) NULL,
          "Print out a description of the postf interface."},
        { (char *) NULL, ARGV_END, (char *) NULL, (char *) NULL, (char *) NULL },
    };
  
    MincInfo *MincFile = NULL;
    MniInfo *MniFile = NULL;
    static char default_window_title[] = "stdin";
  
    VIO_BOOL quit;
  
    FILE *input_fp;
  
    float *frame_length;
    float *frame_time;
    float cterm, cfact; /* for reconverting color indices into floats */
    float initial_zoom;
    float zoom;
    float low = 0.0;        /* threshold for start of scale ramping */
    float high = 1.0;         /* threshold for end of scale ramping */
    float min, max;             /* min and max data values */
    static float moving_color; /* color selected using mouse pointer during rescaling */
  
    struct postf_wind *main_window = NULL;
    struct postf_wind *profile_window = NULL;
    struct postf_wind *dynamic_window = NULL;

    int profile_origin[XY];
    int profile_size[XY];
    int dynamic_origin[XY];
    int dynamic_size[XY];
  
    int min_frame = START_FRAME;
    int min_slice = START_SLICE;
    int max_frame = START_FRAME;
    int max_slice = START_SLICE;
    int frames_present = FALSE;
    int frame_range_specified = FALSE;
    int slice_range_specified = FALSE;
    int profile_frozen = FALSE;
    int dynamic_frozen = FALSE;
    int axes = XAXIS | YAXIS;   /* axes displayed in profile window */
    int stride = 1;
    int shiftkey_down = FALSE;
    int ctrlkey_down = FALSE;
    int altkey_down = FALSE;
    int update_needed = FALSE;
    int roi_dump = FALSE;
    int must_redraw_main = FALSE;
    int must_redraw_profile = FALSE;
    int must_redraw_dynamic = FALSE;
    int must_evaluate_pixel = FALSE;
    int must_change_color_map = FALSE;
    int must_write_scale = FALSE;
    int must_syncronize_mouse = FALSE;
    int evaluating_pixel = FALSE;
    int moving_origin = FALSE;
    int moving_scale = FALSE;
    int moving_top_scale = FALSE;
    int moving_bottom_scale = FALSE;
    int user_slice_count = SLICES;
    int user_frame_count = FRAMES;
    int user_image_count = SLICES * FRAMES;
    int *user_slice_list;
    int *user_frame_list;
    int image_size = IMAGE_SIZE;
    int frames_specified = FALSE;
    int slices_specified = FALSE;
  
    static int  image_subwindow[2*XY] = {0,WINDOW_WIDTH-1,0,WINDOW_HEIGHT-1};
    static int scale_subwindow[2*XY] = {
        WINDOW_WIDTH+COLOR_BAR_XOFFSET-LOOSE,
        WINDOW_WIDTH+COLOR_BAR_XOFFSET+COLOR_BAR_WIDTH+LOOSE,
        COLOR_BAR_TOP-LOOSE,
        COLOR_BAR_TOP+COLOR_BAR_HEIGHT+LOOSE
    };
        
    int mouse_position[XY];
    int evaluate_position[XY];
    int draw_origin[XY] = {0,0};
    int last_origin[XY];
    int start_origin[XY];
    int is_press;
  
    unsigned short *image;
    unsigned short *clipping_buffer;
    unsigned short *dynamic_volume;
    unsigned short color_bar[COLOR_BAR_WIDTH*(COLOR_BAR_HEIGHT+1)];
  
    char *input_fn;

#if DO_RESAMPLE
    resample_function = nearestNeighbour;
#endif /* DO_RESAMPLE */

    if (ParseArgv(&argc, argv, argTable, 0)){
        usage(argv[0]);
    }
  
  /* 
   * Check to see if indices were changed, if so, 
   * update index_min and index_max accordingly. 
   */
  
  if (indices[0] != INDEX_MIN || indices[1] != INDEX_MAX){
    index_min = indices[0];
    index_max = indices[1];
    index_range = index_max - index_min;
    if (index_range < 0){
      index_min = index_max;
      index_max = index_min - index_range;
      index_range = -index_range;
    }
  }
  
  if (frames != 0) frames_specified = TRUE; else frames = FRAMES;
  if (slices != 0) slices_specified = TRUE; else slices = SLICES;

  /* 
   * if file name is present: either minc or mni format 
   */
  
  if (argc == 2){
    
    input_fn = argv[1];
    if (window_title == NULL) window_title = get_image_name(input_fn);
    
    /*
     * Assume minc file format 
     */
    
    if ((MincFile = open_minc_file(input_fn, progress, debug)) != NULL){
      
      image_width = MincFile->ImageWidth;
      image_height = MincFile->ImageHeight;
      image_size = MincFile->ImageSize;
      slices = MincFile->Slices;
      frames = MincFile->Frames;
      no_of_images = slices * frames;
      
      if (frames == 0) frames = 1;
      if (slices == 0) slices = 1;

    } else if ((MniFile = open_mni_file(input_fn, debug, MNI_DUMP_HEADER)) != NULL){
      
      /* 
       * file was not minc, so assume mni format
       */

      if (progress && !debug) fprintf(stderr, "\nAttempting to read mni file ...\n");

      image_width = MniFile->ImageWidth;
      image_height = MniFile->ImageHeight;
      image_size = MniFile->ImageSize;

      if (slices_specified) MniFile->Slices = slices; else slices = MniFile->Slices; 
      if (frames_specified) MniFile->Frames = frames; else frames = MniFile->Frames;

      no_of_images = slices * frames;
      
    }
    else{
      
      usage(argv[0]);
      
    }      

  } else {

    /*
     * No filename specified, so assuming data available from stdin.
     * This requires image size and number information.  Check to see
     * if defaults have been modified, and update dependancies.
     */

    if (frames != FRAMES || slices != SLICES){
      if (no_of_images == NO_OF_IMAGES){
        no_of_images = slices * frames;
      }
      else if (no_of_images != slices * frames){
        fprintf(stderr,"%s: # of images != slices * frames\n", argv[0]);
        usage(argv[0]);
      }	 
    }
    
    image_size = image_width * image_height;
    
  }

  if (frames > 1) frames_present = TRUE;

  /* 
   * Check to see if frame or slice ranges were specified by user.
   * If so, update corresponding Booleans and create frame and slice 
   * lists.
   */
  
  if (rframes[1] != 0) {
    frame_range_specified = TRUE;
  }
  else{
    rframes[0] = 1;
    rframes[1] = frames;
  }
  
  {
    int frame_counter;
    user_frame_count = rframes[1] - rframes[0] + 1;
    user_frame_list = malloc(sizeof(*user_frame_list)*user_frame_count);
    for (frame_counter = 0; frame_counter < user_frame_count; frame_counter++) {
      user_frame_list[frame_counter] = rframes[0] + frame_counter;
    }
  }
  
  if (rslices[1] != 0) {
    slice_range_specified = TRUE;
  }
  else{
    rslices[0] = 1;
    rslices[1] = slices;
  }
  
  {
    int slice_counter;
    user_slice_count = rslices[1] - rslices[0] + 1;
    user_slice_list = malloc(sizeof(*user_slice_list)*user_slice_count);
    for (slice_counter = 0; slice_counter < user_slice_count; slice_counter++) {
      user_slice_list[slice_counter] = rslices[0] + slice_counter;
    }
  }
  
  user_image_count = user_slice_count * user_frame_count;
  
  frame_time = (float *)malloc(sizeof(float)*(frames));
  frame_length = (float *)malloc(sizeof(float)*(frames));
  
  if ((dynamic_volume = 
       (unsigned short *)malloc(sizeof(unsigned short)*image_size*user_image_count))
      == NULL){
    fprintf(stderr,"%s: NOT ENOUGH MEMORY AVAILABLE\n",argv[0]);
    exit(ERROR_STATUS);
  }

  if (MincFile != NULL) {
    
    read_minc_data(MincFile,
                   user_slice_list,
                   user_slice_count,
                   user_frame_list,
                   user_frame_count,
                   user_image_count,                  
                   frames_present,
                   frame_time,
                   frame_length,
                   &min,
                   &max,
                   dynamic_volume,
                   progress,
                   debug);
    
  } else if (MniFile != NULL){

    read_mni_parameters(MniFile,
                        user_slice_list,
                        user_slice_count,
                        user_frame_list,
                        user_frame_count,
                        debug,
                        MNI_DUMP_TABLE);
      
    read_mni_data(MniFile,
                  user_slice_list,
                  user_slice_count,
                  user_frame_list,
                  user_frame_count,
                  user_image_count,
                  frames_present,
                  frame_time,
                  frame_length,
                  &min,
                  &max,
                  &min_slice,
                  &max_slice,
                  &min_frame,
                  &max_frame,
                  dynamic_volume,
                  progress,
                  debug);

  }
  else{
    
    input_fp = stdin;
    read_float_data(input_fp,
                    frames,
                    slices,
                    image_width,
                    image_height,
                    image_size,
                    no_of_images,
                    &min,
                    &max,
                    &min_frame,
                    &min_slice,
                    &max_frame,
                    &max_slice,
                    frame_time,
                    frame_length,
                    time_available,
                    dynamic_volume);
    
  }
  
  if (slice != START_SLICE) slice--;
  if (frame != START_FRAME) frame--;
  
  if (slice >= user_slice_count || slice < 0){
    fprintf(stderr,"%s: chosen slice (%d) > # of slices (%d)\n",
                   argv[0],slice+1,slices);
    usage(argv[0]);
  }
  if(frame >= user_frame_count || frame < 0){
    fprintf(stderr,"%s: chosen frame (%d) > # of frames (%d)\n", 
                   argv[0],frame+1,frames);
    usage(argv[0]);
  }
  
  calculate_conversions(min,max,&cfact,&cterm);
  
  /* 
   *  fprintf(stderr, "      Number of slices: %d\n", slices);
   *  fprintf(stderr, "      Number of frames: %d\n", frames);
   *  fprintf(stderr, "Total number of images: %d\n", no_of_images);
   *  fprintf(stderr, "            Volume min: %f\n", min);
   *  fprintf(stderr, "            Volume max: %f\n", max);   
   *  fprintf(stderr, "Conversion constants: %f and %f\n",cfact,cterm);
   */
  
  /* if (debug) exit(NORMAL_STATUS); */
  
  if (window_title == NULL) window_title = default_window_title;

  xinit();

  main_window = create_basic_window(window_title, 
                                    WINDOW_WIDTH+COLOR_BAR_WINDOW,
                                    WINDOW_HEIGHT);

  if (present_scale == 0) {
      present_scale = spectral;
  }
  change_color_map(present_scale, low, high);
  make_color_bar(color_bar);

  XSetForeground(_xDisplay, main_window->gc, ImageBackgroundColor);
  XFillRectangle(_xDisplay, main_window->draw, main_window->gc, 0, 0, 
                 WINDOW_WIDTH+COLOR_BAR_WINDOW, WINDOW_HEIGHT);
  
  initial_zoom = zoom = (float)MIN(WINDOW_WIDTH/image_width,WINDOW_HEIGHT/image_height);
  image = dynamic_volume + (slice * user_frame_count + frame) * image_size;
  clipping_buffer = malloc(sizeof(*clipping_buffer) * image_size);
  draw_axes(NULL, 0, MAX(image_width, image_height), min, max,
            NIL,NIL,NIL,NIL,NIL,PROFILE,INITIALIZE);
  draw_axes(NULL, frame_time[0], frame_time[user_frame_count-1],
            min,max,NIL,NIL,NIL,NIL,NIL,DYNAMIC,INITIALIZE);
  
  draw_profile(NULL,
               NIL,
               dynamic_volume,
               image_width,
               image_height,
               NIL,NIL,NIL,
               cfact,
               cterm,
               user_frame_count,
               user_slice_count,
               NIL,NIL,NIL,
               INITIALIZE,
               NOT_REDRAW_ONLY);

  draw_dynamic(NULL,
               dynamic_volume,
               frame_time,
               user_frame_count,NIL,NIL,NIL,NIL,
               image_width,
               image_height,
               NIL,
               cfact,
               cterm,
               roi_radius,
               INITIALIZE,NOT_REDRAW_ONLY,
               roi_dump);

  draw_new_slice(main_window, 
                 draw_origin,
                 frame,
                 slice,
                 *rframes,
                 *rslices,
                 image_width,
                 image_height,
                 min,
                 max,
                 low,
                 high,
                 image,
                 clipping_buffer,
                 zoom,
                 color_bar,
                 NOT_REDRAW_ONLY);
  

  get_mouse_ptr(main_window, mouse_position);

  XSelectInput(_xDisplay, main_window->wind, POSTF_X_EVENT_MASK);
  quit = FALSE;
  while (!quit){
      XEvent event;
      XNextEvent(_xDisplay, &event);

      switch (event.type) {
      case Expose:
          if (debug) {
              printf("Expose\n");
          }
          if (event.xexpose.count == 0) {
              if (main_window != NULL && 
                  event.xexpose.window == main_window->wind) {
                  must_redraw_main = TRUE;
              }
              else if (profile_window != NULL && 
                       event.xexpose.window == profile_window->wind) {
                  must_redraw_profile = TRUE;
              }
              else if (dynamic_window != NULL && 
                       event.xexpose.window == dynamic_window->wind) {
                  must_redraw_dynamic = TRUE;
              }
          }
          break;

      case MapNotify:
          if (debug) {
              printf("MapNotify\n");
          }
          break;

      case ReparentNotify:
          if (debug) {
              printf("ReparentNotify\n");
          }
          break;

      case ConfigureNotify:
          if (debug) {
              printf("ConfigureNotify %d %d %d %d\n",
                     event.xconfigure.x,
                     event.xconfigure.y,
                     event.xconfigure.width,
                     event.xconfigure.height);
          }
          if (event.xconfigure.window == main_window->wind) {
              if (main_window->vw != event.xconfigure.width ||
                  main_window->vh != event.xconfigure.height) {
                  main_window->vw = event.xconfigure.width;
                  main_window->vh = event.xconfigure.height;
                  zoom = (float)MIN((main_window->vw-COLOR_BAR_WINDOW)/image_width,main_window->vh/image_height);
                  must_redraw_main = TRUE;
              }
          }
          break;

      case ButtonPress:
      case ButtonRelease:
          shiftkey_down = (event.xbutton.state & ShiftMask);
          ctrlkey_down = (event.xbutton.state & ControlMask);
          altkey_down = (event.xbutton.state & Mod1Mask);
          is_press = (event.xbutton.type == ButtonPress);
          switch (event.xbutton.button) {
          case Button1:
              if (is_press) {
                  if (in_subwindow(mouse_position,scale_subwindow)) {
                      moving_color = get_color(mouse_position[Y], low, high);
                      moving_top_scale = TRUE;
                  }
                  else if (in_subwindow(mouse_position, image_subwindow) && 
                           !altkey_down) {
#if 0
                      curstype(C16X1);
                      defcursor(1,glyph);
                      setcursor(1,0,0);
#endif
                      evaluating_pixel = TRUE;
                      must_evaluate_pixel = TRUE;
                  }
              }
              else{
                  if (moving_top_scale) 
                      moving_top_scale = FALSE;
                  if (evaluating_pixel) {
                      erase_cursor(main_window);
#if 0
                      setcursor(0,0,0);
#endif
                      evaluating_pixel = FALSE;
                      must_evaluate_pixel = FALSE;
                  }
              }
              break;

          case Button2:
              if (is_press) {
                  if (in_subwindow(mouse_position,scale_subwindow)) {
                      moving_color = get_color(mouse_position[Y], low, high);
                      moving_scale = TRUE;
                  }
                  else if (in_subwindow(mouse_position,image_subwindow)) {
                      start_origin[X] = mouse_position[X];
                      start_origin[Y] = mouse_position[Y];
                      moving_origin = TRUE;
                  }
              }
              else{
                  if (moving_scale)
                      moving_scale = FALSE;
                  if (moving_origin) {
                      moving_origin = FALSE;
                      last_origin[X] = draw_origin[X];
                      last_origin[Y] = draw_origin[Y];
                  }
              }
              break;
      
          case Button3:
              if (is_press) {
                  if (in_subwindow(mouse_position,scale_subwindow)) {
                      moving_color = get_color(mouse_position[Y], low, high);
                      moving_bottom_scale = TRUE;
                  }
                  else if (evaluating_pixel && dynamic_window != NULL){
                      must_evaluate_pixel = TRUE;
                      roi_dump = TRUE;
                  }
              }
              else {
                  if (moving_bottom_scale) {
                      moving_bottom_scale = FALSE;
                  }
              }
              break;
          default:
              fprintf(stderr, "Unknown mouse button %d\n", event.xbutton.button);
              break;
          }
          break;

      case KeyPress:
      case KeyRelease:
          {
#define MAX_MAPPED_STRING_LENGTH 10
              int buffersize = MAX_MAPPED_STRING_LENGTH;
              int charcount;
              char buffer[MAX_MAPPED_STRING_LENGTH];
              KeySym keysym;
              XComposeStatus compose;
        
              charcount = XLookupString (&event.xkey,
                                         buffer,
                                         buffersize,
                                         &keysym,
                                         &compose);

              is_press = (event.xkey.type == KeyPress);
              shiftkey_down = (event.xkey.state & ShiftMask);
              ctrlkey_down = (event.xkey.state & ControlMask);
              altkey_down = (event.xkey.state & Mod1Mask);

              switch (keysym) {

              case XK_Up:
                  if (!is_press) {
                      break;
                  }
                  if (ctrlkey_down) {
                      mouse_position[Y] += stride * zoom;
                      if (in_subwindow(mouse_position,image_subwindow)) {
                          must_evaluate_pixel = TRUE;
                      }
                      else {
                          mouse_position[Y] -= stride * zoom;
                      }
                  }
                  else if (altkey_down) {
                      roi_radius = MIN(MAX_ROI_RADIUS,roi_radius+1);
                      draw_cursor(main_window, mouse_position, roi_radius * zoom);
                      if (in_subwindow(mouse_position,image_subwindow)){
                          must_evaluate_pixel = TRUE;
                      }
                  }
                  else {
                      slice++;
                      if (slice == user_slice_count) slice = 0;
                      if (!shiftkey_down){
                          image = dynamic_volume + (slice * user_frame_count + frame) * image_size;
                          must_redraw_main = TRUE;
                          if (in_subwindow(mouse_position,image_subwindow)){
                              must_evaluate_pixel = TRUE;
                          }
                      }
                      else{
                          update_needed = TRUE;
                          must_write_scale = TRUE;
                      }
                  }
                  break;

              case XK_Right:
                  if (is_press){
                      if (ctrlkey_down){
                          mouse_position[X] += stride * zoom;
                          if (in_subwindow(mouse_position,image_subwindow)){
                              must_evaluate_pixel = TRUE;
                          }
                          else{
                              mouse_position[X] -= stride * zoom;
                          }
                      }
                      else if (altkey_down){
                          roi_radius = MIN(MAX_ROI_RADIUS,roi_radius+1);
                          draw_cursor(main_window, mouse_position, roi_radius * zoom);
                          if (in_subwindow(mouse_position,image_subwindow)){
                              must_evaluate_pixel = TRUE;
                          }
                      }
                      else{
                          frame++;
                          if (frame == user_frame_count) frame = 0;
                          if (!shiftkey_down){
                              image = dynamic_volume + (slice * user_frame_count + frame) * image_size;
                              must_redraw_main = TRUE;		  
                              if (in_subwindow(mouse_position,image_subwindow)){
                                  must_evaluate_pixel = TRUE;
                              }
                          }
                          else{
                              update_needed = TRUE;
                              must_write_scale = TRUE;
                          }
                      }
                  }
                  break;

              case XK_Down:
                  if (is_press){
                      if (ctrlkey_down){
                          mouse_position[Y] -= stride * zoom;
                          if (in_subwindow(mouse_position,image_subwindow)){
                              must_evaluate_pixel = TRUE;
                          }
                          else{
                              mouse_position[Y] += stride * zoom;
                          }
                      }
                      else if (altkey_down){
                          roi_radius = MAX(0,roi_radius-1);
                          must_redraw_main = TRUE;
                          if (in_subwindow(mouse_position,image_subwindow)){
                              must_evaluate_pixel = TRUE;
                          }
                      }
                      else{
                          slice--;
                          if (slice < 0) slice = user_slice_count-1;
                          if (!shiftkey_down){
                              image = dynamic_volume + (slice * user_frame_count + frame) * image_size;
                              must_redraw_main = TRUE;
                              if (in_subwindow(mouse_position,image_subwindow)){
                                  must_evaluate_pixel = TRUE;
                              }
                          }
                          else{
                              update_needed = TRUE;
                              must_write_scale = TRUE;
                          }
                      }
                  }
                  break;
            
              case XK_Left:
                  if (is_press){
                      if (ctrlkey_down){
                          mouse_position[X] -= stride * zoom;
                          if (in_subwindow(mouse_position,image_subwindow)){
                              must_evaluate_pixel = TRUE;
                          }
                          else{
                              mouse_position[X] += stride * zoom;
                          }
                      }
                      else if (altkey_down){
                          roi_radius = MAX(0,roi_radius-1);
                          draw_cursor(main_window, mouse_position, roi_radius * zoom);
                          if (in_subwindow(mouse_position,image_subwindow)){
                              must_evaluate_pixel = TRUE;
                          }
                      }
                      else{
                          frame--;
                          if (frame < 0) frame = user_frame_count-1;
                          if (!shiftkey_down){
                              image = dynamic_volume + (slice * user_frame_count + frame) * image_size;
                              must_redraw_main = TRUE;
                              if (in_subwindow(mouse_position,image_subwindow)){
                                  must_evaluate_pixel = TRUE;
                              }
                          }
                          else{
                              update_needed = TRUE;
                              must_write_scale = TRUE;
                          }
                      }
                  }
                  break;

#if DO_RESAMPLE
              case XK_I:
              case XK_i:
                  resample_function = interpolateImage;
                  must_redraw_main = TRUE;
                  break;

              case XK_N:
              case XK_n:
                  resample_function = nearestNeighbour;
                  must_redraw_main = TRUE;
                  break;
#endif /* DO_RESAMPLE */

              case XK_C:
              case XK_c:
                  break;

              case XK_D:
              case XK_d:
                  if (!is_press) {
                      break;
                  }
                  if (user_frame_count > 1 && dynamic_window == NULL) {
                      dynamic_window = create_window(window_title,
                                                     frame_time[0],
                                                     frame_time[user_frame_count-1],
                                                     min,
                                                     max,
                                                     axes,
                                                     DYNAMIC,
                                                     dynamic_origin,
                                                     dynamic_size);
                  }
                  else if (dynamic_window != NULL && !dynamic_frozen){
                      destroy_window(dynamic_window);
                      dynamic_window = NULL;
                  }
                  break;
                  
              case XK_G:
              case XK_g:
                  if (is_press){
                      present_scale = gray;
                      must_change_color_map = TRUE;
                  }
                  break;
      
              case XK_H:
              case XK_h:
                  if (is_press){
                      present_scale = hotmetal;
                      must_change_color_map = TRUE;
                  }
                  break;

              case XK_M:
              case XK_m:
                  if (is_press){
                      if (keysym == XK_M){
                          frame = max_frame;
                          slice = max_slice;
                      }
                      else{
                          frame = min_frame;
                          slice = min_slice;
                      }
                      image = dynamic_volume + (slice * user_frame_count + frame) * image_size;
                      must_redraw_main = TRUE;
                  }
                  break;

              case XK_P:
              case XK_p:
                  if (!is_press) {
                      break;
                  }
                  if (profile_window == NULL) {
                      profile_window = create_window(window_title,
                                                     0,
                                                     MAX(image_width,image_height),
                                                     min,
                                                     max,
                                                     axes,
                                                     PROFILE,
                                                     profile_origin,
                                                     profile_size);
                  }
                  else if (profile_window != NULL && !profile_frozen) {
                      destroy_window(profile_window);
                      profile_window = NULL;
                  }
                  break;

              case XK_R:
              case XK_r:
                  if (is_press) {
                      if (keysym == XK_R){
                          zoom = initial_zoom;
                          draw_origin[X] = last_origin[X] = 0;
                          draw_origin[Y] = last_origin[Y] = 0;	       
                          must_redraw_main = TRUE;
                      }
                      else{    
                          low = 0.0; high = 1.0;
                          must_change_color_map = TRUE;
                          must_write_scale = TRUE;	
                      }
                  }
                  break;
      
              case XK_S:
              case XK_s:
                  if (is_press){
                      present_scale = spectral;
                      must_change_color_map = TRUE;
                  }
                  break;
      
              case XK_T:
              case XK_t:
                  break;
      
              case XK_X:
              case XK_x:
                  if (is_press) axes ^= XAXIS;
                  break;
      
              case XK_Y:
              case XK_y:
                  if (is_press) axes ^= YAXIS;
                  break;
      
              case XK_Z:
              case XK_z:
                  if (is_press) axes ^= ZAXIS;
                  break;
      
              case XK_minus:
                  if (is_press && in_subwindow(mouse_position,image_subwindow)) {
                      if (zoom >= 2.0){
                          evaluate_position[X] = (mouse_position[X] - draw_origin[X]) / zoom;
                          evaluate_position[Y] = (mouse_position[Y] - draw_origin[Y]) / zoom;
                          zoom--;
                          last_origin[X] = draw_origin[X] = mouse_position[X] - zoom * evaluate_position[X];
                          last_origin[Y] = draw_origin[Y] = mouse_position[Y] - zoom * evaluate_position[Y];
                          must_redraw_main = TRUE;
                      }
                  }
                  break;
      
              case XK_plus:
                  if (is_press && in_subwindow(mouse_position,image_subwindow)){
                      evaluate_position[X] = (mouse_position[X] - draw_origin[X]) / zoom;
                      evaluate_position[Y] = (mouse_position[Y] - draw_origin[Y]) / zoom;
                      zoom++;
                      last_origin[X] = draw_origin[X] = mouse_position[X] - zoom * evaluate_position[X];
                      last_origin[Y] = draw_origin[Y] = mouse_position[Y] - zoom * evaluate_position[Y];
                      must_redraw_main = TRUE;
                  }
                  break;
      
              case XK_space:
                  if (is_press && evaluating_pixel && dynamic_window != NULL) {
                      must_evaluate_pixel = TRUE;
                      roi_dump = TRUE;
                  }
                  break;
      
              case XK_Q:
              case XK_q:
              case XK_Escape:
                  quit = TRUE;
                  break;
                  
              case XK_Shift_L:
              case XK_Shift_R:
                  if (is_press){
                      shiftkey_down = TRUE;
                      if (ctrlkey_down) stride = 4;
                  }
                  else{
                      shiftkey_down = FALSE;
                      if (ctrlkey_down) stride = 1;
                      if (update_needed){
                          update_needed = FALSE;
                          image = dynamic_volume + (slice * user_frame_count + frame) * image_size;
                          must_redraw_main = TRUE;
                          must_write_scale = TRUE;
                      }
                  }
                  break;
      
              case XK_Control_L:
              case XK_Control_R:
                  if (is_press && !ctrlkey_down){
                      ctrlkey_down = TRUE;
                      if (shiftkey_down) 
                          stride = 4;
                      if (in_subwindow(mouse_position,image_subwindow)) {
#if 0
                          curstype(C16X1);
                          defcursor(1,glyph);
                          setcursor(1,0,0);
#endif
                          draw_cursor(main_window, mouse_position, roi_radius * zoom);
                          evaluating_pixel = TRUE;
                      }
                  }
                  else if (!is_press && ctrlkey_down) {
                      ctrlkey_down = FALSE;
                      if (shiftkey_down) 
                          stride = 1;
                      if (!altkey_down) {
                          must_syncronize_mouse = FALSE;
                          get_mouse_ptr(main_window, mouse_position);
                          erase_cursor(main_window);
#if 0
                          setcursor(0,0,0);
#endif
                          evaluating_pixel = FALSE;
                      }
                      else{
                          must_syncronize_mouse = TRUE;
                      }
                  }
                  break;
      
              case XK_Alt_L:
              case XK_Alt_R:
                  if (is_press && !altkey_down && !evaluating_pixel){
                      altkey_down = TRUE;
                      if (in_subwindow(mouse_position,image_subwindow)){
#if 0
                          curstype(C16X1);
                          defcursor(1,glyph);
                          setcursor(1,0,0);
#endif
                          draw_cursor(main_window, mouse_position, roi_radius * zoom);
                          evaluating_pixel = TRUE;
                      }
                  }
                  else if (!is_press && altkey_down){
                      altkey_down = FALSE;
                      if (!ctrlkey_down) {
                          if (must_syncronize_mouse){
                              must_syncronize_mouse = FALSE;
                              get_mouse_ptr(main_window, mouse_position);
                          }
                          erase_cursor(main_window);
#if 0
                          setcursor(0,0,0);
#endif
                          evaluating_pixel = FALSE;
                      }
                  }
                  break;
      
              default:
                  fprintf(stderr, "Unknown keysym %d\n", (int) keysym);
                  break;
              }
          }
          break;

      case MotionNotify:
          if (debug) {
              printf("MotionNotify %d %d\n",
                     event.xmotion.x,
                     event.xmotion.y);
          }
          mouse_position[X] = event.xmotion.x;
          mouse_position[Y] = event.xmotion.y;

          if (evaluating_pixel) {
              must_evaluate_pixel = TRUE;
          }
          else if (moving_origin) {
              draw_origin[X] = last_origin[X] + (mouse_position[X] - start_origin[X]);
              draw_origin[Y] = last_origin[Y] + (mouse_position[Y] - start_origin[Y]);
              must_redraw_main = TRUE;
          }
          else if (moving_scale) {
              translate_scale(mouse_position[Y], &low, &high, moving_color);
              must_change_color_map = TRUE;
              must_write_scale = TRUE;
          }
          else if (moving_bottom_scale) {
              new_scale_slope(mouse_position[Y],
                              &low, &high, TOP, moving_color);
              must_change_color_map = TRUE;
              must_write_scale = TRUE;
          }
          else if (moving_top_scale) {
              new_scale_slope(mouse_position[Y],
                              &low, &high, BOTTOM, moving_color);
              must_change_color_map = TRUE;
              must_write_scale = TRUE;
          }
          break;

      default:
          fprintf(stderr, "unknown event type: %d\n", event.type);
          break;
      }

      if (must_change_color_map) {
          must_change_color_map = FALSE;
          change_color_map(present_scale, low, high);
          must_redraw_main = TRUE;
      }
      
      if (must_redraw_main) {
          must_redraw_main = FALSE;
          draw_new_slice(main_window, 
                         draw_origin,
                         frame,
                         slice,
                         *rframes,
                         *rslices,
                         image_width,
                         image_height,
                         min,
                         max,
                         low,
                         high,
                         image,
                         clipping_buffer,
                         zoom,
                         color_bar,
                         NOT_REDRAW_ONLY);
      }
      
      if (must_write_scale) {
          must_write_scale = FALSE;
          frontbuffer(main_window, 1);
          write_scale(main_window, low, high, frame, slice, *rframes, *rslices);
          frontbuffer(main_window, 0);
      }

      if (must_redraw_profile) {
          must_redraw_profile = FALSE;
          draw_profile(profile_window, 
                       NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,
                       NIL,NIL, axes, NOT_INITIALIZE, REDRAW_ONLY);
      }
      
      if (must_redraw_dynamic) {
          must_redraw_dynamic = FALSE;
          draw_dynamic(dynamic_window, 
                       NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,
                       NIL,NIL,NOT_INITIALIZE,REDRAW_ONLY, roi_dump);
      }
      
      if (must_evaluate_pixel) {
          must_evaluate_pixel = FALSE;
          if (evaluating_pixel) 
              draw_cursor(main_window, mouse_position, roi_radius * zoom);
          evaluate_position[X] = (mouse_position[X] - draw_origin[X])/zoom;
          evaluate_position[Y] = (mouse_position[Y] - draw_origin[Y])/zoom;
          evaluate_position[Y] = image_height-evaluate_position[Y]-1;
          evaluate_pixel(evaluate_position,
                         image,
                         frame,
                         slice,
                         cfact,
                         cterm,
                         image_width,
                         image_height,
                         image_size,
                         roi_radius,
                         (profile_window != NULL),
                         profile_frozen,
                         (dynamic_window != NULL),
                         dynamic_frozen,
                         axes,
                         main_window,
                         profile_window,
                         dynamic_window,
                         roi_dump);
          if (roi_dump) roi_dump = FALSE;
      }

  }

  if (profile_window != NULL) {
      destroy_window(profile_window);
      profile_window = NULL;
  }
  if (dynamic_window != NULL) {
      destroy_window(dynamic_window);
      dynamic_window = NULL;
  }
  if (main_window != NULL) {
      destroy_window(main_window);
      main_window = NULL;
  }
  xexit();
  return(NORMAL_STATUS);
  
}
