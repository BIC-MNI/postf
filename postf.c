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
   @MODIFIED   : Revision 1.1  2005-02-11 22:06:51  bert
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
   ---------------------------------------------------------------------------- */

/*
 * Some definitions for the use of Dave globals routines
 */

#define  GLOBALS_LOOKUP_NAME  globals_list
#include <bicpl/globals.h>
#include <volume_io.h>

#undef X
#undef Y

#ifndef lint
static char rcsid[] = "$Header: /private-cvsroot/visualization/postf/postf.c,v 1.1 2005-02-11 22:06:51 bert Exp $";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

/*
 * Patch to avoid problems with definitions
 * in Davids globals include files
 */

#undef   BLACK
#undef   WHITE
#undef   RED
#undef   GREEN
#undef   BLUE
#undef   CYAN
#undef   MAGENTA
#undef   YELLOW

#define  window   GL_window
#define  normal   GL_normal
#define  poly     GL_poly

#include <X11/Ygl.h>
#include <X11/X.h>

#undef   window
#undef   normal
#undef   poly

/*
 * And some standard defs
 */

/*
#include <gl/device.h>
#include <fmclient.h>
*/

#include "glhacks.h"

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
#define EVAL_YCOORD (WINDOW_HEIGHT-(COLOR_BAR_YOFFSET/4))
#define SCALE_XCOORD (WINDOW_WIDTH+COLOR_BAR_XOFFSET)
#define SCALE_YCOORD (COLOR_BAR_YOFFSET*2/3)
#define TEXT_SEP (COLOR_BAR_YOFFSET/4)
#define LOOSE 10

#define INDEX_MIN 64
#define INDEX_MAX 255
#define INDEX_RANGE (INDEX_MAX-INDEX_MIN)
#define RGB_MAX 255

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

short 
  index_min = INDEX_MIN,
  index_max = INDEX_MAX,
  index_range = INDEX_RANGE;


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
  (void) fputs(output_text, stdout);
  
  exit(EXIT_FAILURE);
}

void find_min_max(float *matrix, int size, float *min, float *max, int step)
{
  *min = FLT_MAX;
  *max = -FLT_MAX;
  while(size--){
    if (*matrix < *min) *min = *matrix;
    if (*matrix > *max) *max = *matrix;
    matrix += step;
  }
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

MincInfo *open_minc_file(char *minc_filename, BOOLEAN progress, BOOLEAN debug)
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
  BOOLEAN
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
  
  (void) ncvarinq(MincFile->CdfId, 
                  MincFile->ImgId, 
                  image_name, 
                  &datatype, 
                  &MincFile->nDim, 
                  dim_ids, 
                  &natts);
  
  
  if (debug){
    (void) fprintf(stderr, " Image variable name: %s, attributes: %d, dimensions: %d\n", 
                   image_name, natts, MincFile->nDim);
    (void) fprintf(stderr, " Attributes:\n");
    for (attribute = 0; attribute < natts; attribute++){
      (void) ncattname(MincFile->CdfId, MincFile->ImgId, attribute, attname);
      (void) fprintf(stderr, "  %s\n", attname);
    }
    (void) fprintf(stderr, " Dimension variables:\n");
  }
  
  /*
   * For each dimension, inquire about name and get dimension length
   */
  
  for (dimension = 0; dimension < MincFile->nDim; dimension++){
    
    (void) ncdiminq(MincFile->CdfId,
                    dim_ids[dimension],
                    dim_names[dimension],
                    &dim_length[dimension]);
    
    if (debug){
      (void) fprintf(stderr, "  %d\t%d\t%s\t%ld\n", 
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
      
    }else if (dimension == MincFile->nDim-3 || dimension == MincFile->nDim-4){
      MincFile->SliceDim = dimension;
      MincFile->Slices = dim_length[dimension];
      slices_present = TRUE;
      
    }else if (dimension == MincFile->nDim-2){
      MincFile->HeightImageDim = dimension;
      MincFile->ImageHeight = dim_length[dimension];
      
    }else if (dimension == MincFile->nDim-1){
      MincFile->WidthImageDim = dimension;
      MincFile->ImageWidth = dim_length[dimension];
      
    }else{
      (void) fprintf(stderr, "  Too many dimensions, skipping dimension <%s>\n",
                     dim_names[dimension]);
      
    }
    
  }
  
  MincFile->ImageSize = MincFile->ImageWidth * MincFile->ImageHeight;
  
  /*
   * Create the image icv 
   */
  
  MincFile->Icv = miicv_create();
  (void) miicv_setint(MincFile->Icv, MI_ICV_DO_NORM, TRUE);
  (void) miicv_setint(MincFile->Icv, MI_ICV_TYPE, NC_SHORT);
  (void) miicv_setstr(MincFile->Icv, MI_ICV_SIGN, MI_UNSIGNED);
  (void) miicv_setdbl(MincFile->Icv, MI_ICV_VALID_MAX, index_max);
  (void) miicv_setdbl(MincFile->Icv, MI_ICV_VALID_MIN, index_min);
  (void) miicv_setint(MincFile->Icv, MI_ICV_DO_FILLVALUE, TRUE);
  (void) miicv_setint(MincFile->Icv, MI_ICV_DO_DIM_CONV, TRUE);
  (void) miicv_setint(MincFile->Icv, MI_ICV_XDIM_DIR, MI_ICV_POSITIVE);
  (void) miicv_setint(MincFile->Icv, MI_ICV_YDIM_DIR, MI_ICV_POSITIVE);
  (void) miicv_setint(MincFile->Icv, MI_ICV_ZDIM_DIR, MI_ICV_POSITIVE);
  
  /* 
   * Attach icv to image variable 
   */
  
  (void) miicv_attach(MincFile->Icv, MincFile->CdfId, MincFile->ImgId);
  
  /*
   * If frames are present, then get variable id for time
   */
  
  if (frames_present) {
    MincFile->TimeId = ncvarid(MincFile->CdfId, dim_names[MincFile->TimeDim]);  
    MincFile->TimeWidthId = ncvarid(MincFile->CdfId, MItime_width);  
  }else{
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
                   BOOLEAN  frames_present,
                   float    *frame_time,
                   float    *frame_length,
                   float    *min,
                   float    *max,
                   unsigned short *dynamic_volume,
                   BOOLEAN  progress,
                   BOOLEAN  debug)
{
  int
    frame,
    slice;
  
  long 
    coord[MAX_VAR_DIMS],
    count[MAX_VAR_DIMS];
  
  int notice_every;
  int image_no;
  
  double
    dbl_min,
    dbl_max;
  
  progress = progress && !debug;
  
  /* 
   * Modify count and coord 
   */
  
  (void) miset_coords(MincFile->nDim, (long) 0, coord);
  (void) miset_coords(MincFile->nDim, (long) 1, count);
  count[MincFile->nDim-1] = MincFile->ImageWidth;
  count[MincFile->nDim-2] = MincFile->ImageHeight;
  coord[MincFile->nDim-1] = 0;
  coord[MincFile->nDim-2] = 0;
  
  if (progress){
    (void) fprintf(stderr, "Reading minc data ");
    notice_every = images_to_get/MAX(user_slice_count,user_frame_count);
    if (notice_every == 0) notice_every = 1;
    image_no = 0;
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
      
      (void) miicv_get(MincFile->Icv, coord, count, (void *) dynamic_volume);
      dynamic_volume += MincFile->ImageSize;
      
      if (debug){
        (void) fprintf(stderr, " Slice: %d, Frame: %d\n",
                       MincFile->Slices > 0 ? coord[MincFile->SliceDim]+1 : 1,
                       MincFile->Frames > 0 ? coord[MincFile->TimeDim]+1 : 1);
      }
      
      if (progress){
        if (image_no++ % notice_every == 0){
          (void) fprintf(stderr, ".");
          (void) fflush(stderr);            
        }
        
      }
      
    } /* for frame */
    
  } /* for slice */
  
  if (progress) (void) fprintf(stderr, " done\n");
  
  (void) miicv_inqdbl(MincFile->Icv, MI_ICV_NORM_MIN, &dbl_min);
  (void) miicv_inqdbl(MincFile->Icv, MI_ICV_NORM_MAX, &dbl_max);
  
  *min = (float) dbl_min;
  *max = (float) dbl_max;
  
  /* 
   * If user requested frames
   */
  
  if (frames_present){
    
    /* 
     * Get time domain 
     */
    
    count[0] = MincFile->Frames;
    coord[0] = 0;
    
    (void) mivarget(MincFile->CdfId,
                    MincFile->TimeId,
                    coord,
                    count,
                    NC_FLOAT,
                    MI_SIGNED,
                    frame_time);
    
    (void) mivarget(MincFile->CdfId,
                    MincFile->TimeWidthId,
                    coord,
                    count,
                    NC_FLOAT,
                    MI_SIGNED,
                    frame_length);
    
    /*
     * Calculate midframe time and frame_length.
     */
    
    for (frame = 0; frame < MincFile->Frames; frame++){
      frame_time[frame] = (frame_time[frame] + frame_length[frame]/2) / 60;
      if (debug) {
        (void) fprintf(stderr, " Frame: %d, Time: %.2f, length: %.2f\n",
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
        (void) fprintf(stderr, " User frame: %d, Study frame: %d, time: %.2f\n",
                       frame+1,
                       user_frame_list[frame],
                       frame_time[frame]);
      }
      
    }
    
  } /* if frames_present */
  
  return(NORMAL_STATUS);
  
}

int close_minc_file(MincInfo *MincFile)
{
  
  /* 
   * Free the image icv 
   */
  
  (void) miicv_free(MincFile->Icv);
  
  /* 
   * Close the file and return
   */
  
  (void) miclose(MincFile->CdfId);
  
}

int read_mni_data(MniInfo  *MniFile,
                  int      *user_slice_list,
                  int      user_slice_count,
                  int      *user_frame_list,
                  int      user_frame_count,
                  int      images_to_get,
                  BOOLEAN  frames_present,
                  float    *frame_time,
                  float    *frame_length,
                  float    *min,
                  float    *max,
                  int      *min_slice,
                  int      *max_slice,
                  int      *min_frame,
                  int      *max_frame,
                  unsigned short *dynamic_volume,
                  BOOLEAN  progress,
                  BOOLEAN  debug)
     
{
  int 
    slice, frame,
    mni_image, user_image,
    pixel_count,
    notice_every;
  BOOLEAN 
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
        (void) fprintf(stderr, ".");
        (void) fflush(stderr);
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
        (void) fprintf(stderr, ".");
        (void) fflush(stderr);
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
    (void) fprintf(stderr,"NOT ENOUGH MEMORY AVAILABLE\n");
    exit(ERROR_STATUS);
  }
  if (total_pixels != fread(float_data, sizeof(float), total_pixels, input_fp)){
    (void) fprintf(stderr,"\007%d image(s) of size %d x %d not found\n",
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

void initialize_gl_fonts()
{
#if 0
  fmfonthandle font_handle, font_handle_scaled;
  
  fminit();
  if ((font_handle = fmfindfont("Helvetica")) == 0){
    (void) fprintf(stderr,"Fonts not found ...");
    return;
  }
  if (font_handle != (fmfonthandle) 0){
    font_handle_scaled = fmscalefont(font_handle,8.0);
    fmsetfont(font_handle_scaled);
  }
#endif
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
  
  float base, slope; short floor;
  
  if (index >= 1.0){
    *rgb++ = ramp[20][R];
    *rgb++ = ramp[20][G];
    *rgb = ramp[20][B];
  }else if (index <= 0){
    *rgb++ = ramp[0][R];
    *rgb++ = ramp[0][G];
    *rgb = ramp[0][B];
  }else{
    base = index * 20;
    floor = (short)ftrunc(base);
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
  }else if (index <= 0){
    *rgb++ = 0.0;
    *rgb++ = 0.0;
    *rgb = 0.0;
  }else{
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
  }else if (index <= 0){
    *rgb++ = 0.0;
    *rgb++ = 0.0;
    *rgb = 0.0;
  }else{
    *rgb++ = index;
    *rgb++ = index;
    *rgb = index;
  }
}

void change_color_map(void (*map)(float, float *), float low, float high)
{
  float rgb[RGB], col, m, b; Colorindex index;
  
  m = 1.0 / (high - low);
  b = low / (low - high);
  
  for (index = index_min; index <= index_max; index++){
    col = m*(index-index_min)/index_range+b;
    map(col, rgb);
    mapcolor((Colorindex)index,
             (short)RGB_MAX*rgb[R],
             (short)RGB_MAX*rgb[G],
             (short)RGB_MAX*rgb[B]
             );
  }
}

void make_color_bar(unsigned short *color_bar)
{
  int x,y; unsigned short color; float slope;
  
  slope = (float)index_range/COLOR_BAR_HEIGHT;
  for (y = 0; y <= COLOR_BAR_HEIGHT; y++){
    color = slope * y + index_min + 0.5;
    for (x = 0; x < COLOR_BAR_WIDTH; x++)
      *color_bar++ = color;
  }
}

int in_subwindow(short *position, short *window)
{
  return(position[X] > window[0] && 
         position[X] < window[1] && 
         position[Y] > window[2] && 
         position[Y] < window[3]
         );
}

float get_color(short position, float low, float high)
{
  static float
    bottom = COLOR_BAR_YOFFSET,
    top = COLOR_BAR_YOFFSET+COLOR_BAR_HEIGHT,
    range = COLOR_BAR_HEIGHT;
  float scaled_position, m, b;
  
  m = 1 / (high - low);
  b = -m * low;
  
  if (position < bottom) position = bottom;
  if (position > top) position = top;
  
  scaled_position = (position - bottom) / range;
  return(m * scaled_position + b);
}

void new_scale_slope(short position, float *low, float *high, int anchor, float moving_color)
{
  float scaled_position, m, b;

  scaled_position = (float)(position - COLOR_BAR_YOFFSET) / COLOR_BAR_HEIGHT;
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

void translate_scale(short position, float *low, float *high, float moving_color)
{
  float scaled_position, m, b;
  
  scaled_position = (float)(position - COLOR_BAR_YOFFSET) / COLOR_BAR_HEIGHT;
  m = 1 / (*high - *low);
  b = moving_color - m * scaled_position;
  *low = -b / m;
  *high = (1.0 - b) / m;   

  if (*high <= *low) {
      *high = *low + 0.01;
  }
}

void write_eval_pixel(short x, short y, float v, int a, int redraw_only)
{
  static Screencoord
    cleft = WINDOW_WIDTH+COLOR_BAR_XOFFSET/2,
    cright = WINDOW_WIDTH+COLOR_BAR_WINDOW,
    cbottom = WINDOW_HEIGHT-COLOR_BAR_YOFFSET,
    ctop = WINDOW_HEIGHT;
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
  
  pushviewport();
  viewport(cleft,cright,cbottom,ctop);
  color(ImageBackgroundColor); clear();
  popviewport();
  
  color(TextColor);
  
  cmov2i(EVAL_XCOORD,EVAL_YCOORD);
  (void) sprintf(out_string,"x=%d",lx);
  fmprstr(out_string);
  
  cmov2i(EVAL_XCOORD,EVAL_YCOORD-TEXT_SEP);
  (void) sprintf(out_string,"y=%d",ly);
  fmprstr(out_string);
  
  cmov2i(EVAL_XCOORD,EVAL_YCOORD-2*TEXT_SEP);
  (void) sprintf(out_string,"v=%g",lv);
  fmprstr(out_string);

  cmov2i(EVAL_XCOORD,EVAL_YCOORD-3*TEXT_SEP);
  (void) sprintf(out_string,"a=%d",la);
  fmprstr(out_string);
}

void write_scale(float low, float high, int frame, int slice, int frame_offset, int slice_offset)
{
  static Screencoord
    cleft = WINDOW_WIDTH+COLOR_BAR_XOFFSET/2,
    cright = WINDOW_WIDTH+COLOR_BAR_WINDOW,
    cbottom = 0,
    ctop = COLOR_BAR_YOFFSET - TEXT_SEP + 1;
  char out_string[MAX_STRING_LENGTH];
  
  pushviewport();
  viewport(cleft,cright,cbottom,ctop);
  color(ImageBackgroundColor); clear();
  popviewport();
  
  color(TextColor);
  
  cmov2i(SCALE_XCOORD,SCALE_YCOORD);
  (void) sprintf(out_string,"max = %3.0f%%",high * 100.0);
  fmprstr(out_string);
  
  cmov2i(SCALE_XCOORD,SCALE_YCOORD-TEXT_SEP);
  (void) sprintf(out_string,"min = %3.0f%%",low * 100.0);
  fmprstr(out_string);
  
  cmov2i(SCALE_XCOORD,SCALE_YCOORD-2*TEXT_SEP);
  (void) sprintf(out_string,"f: %2d  s: %2d",frame+frame_offset,slice+slice_offset);
  fmprstr(out_string);
}

void draw_axes(float abscisa_min,
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
  
  char axis_string[MAX_STRING_LENGTH];
  
  if (initializing){
    color(ProfileBackgroundColor);
    clear();
    y_min = min;
    y_max = max;
    y_tic = (y_max-y_min)/AXES_TICS;
    y_limit = y_max + y_tic*0.1;
    y_lo = y_tic/15.0;
    if (y_min < 0.0 && y_max > 0.0){
      yy = 0.0; /* at y=0 if data spans 0 */
      x_ts = -y_tic/TIC_REDUCTION;
      x_te = 0.0;
      x_l = 4.0*x_ts;
    }else{
      yy = y_min; /* at y=y_min otherwise */
      x_ts = y_min - y_tic/TIC_REDUCTION;
      x_te = y_min;
      x_l = y_min - 3.0*y_tic/TIC_REDUCTION;
    }
    
    switch(plot_type){
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
      }else{
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
      if (y_min < 0.0 && y_max > 0.0){
        dx_ls = dx_min + dx_tic;
      }else{
        dx_ls = 0.0;
      }
      break;
    }
    
  }else{
    float point[XY];
    
    switch(plot_type){
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
    
    color(AxisColor);
    
    /* draw x axis */
    point[X] = x_min;
    point[Y] = yy;
    bgnline();
    {
      v2f(point);
      point[X] = x_max;
      v2f(point);
    }
    endline(); /* x tics */
    for (point[X] = x_ls; point[X] <= x_max; point[X] += x_tic){
      point[Y] = x_ts;
      bgnline();
      {
        v2f(point);
        point[Y] = x_te;
        v2f(point);
      }
      endline();
      cmov2(point[X]-x_lo,x_l);
      (void) sprintf(axis_string,"%5.1f",point[X]);
      fmprstr(axis_string);
    }
    
    /* draw y axis */
    point[X] = x_min;
    point[Y] = y_min;
    bgnline();
    {
      v2f(point);
      point[Y] = y_max;
      v2f(point);
    }
    endline(); /* y tics */
    for (point[Y] = y_min; point[Y] <= y_limit; point[Y] += y_tic){
      point[X] = y_ts;
      bgnline();
      {
        v2f(point);
        point[X] = y_te;
        v2f(point);
      }
      endline();
      cmov2(y_l,point[Y]-y_lo);
      (void) sprintf(axis_string,"%5.1f",point[Y]);
      fmprstr(axis_string);
    }
    
    point[X] = y_ts;
    point[Y] = v;
    bgnline();
    {
      v2f(point);
      point[X] = y_te;
      v2f(point);
    }
    endline();
    
    if ((plot_type == DYNAMIC) || (axes & XAXIS)){
      if (plot_type == DYNAMIC) 
        color(DynamicColor);
      else
        color(ProfileXColor);
      point[X] = x;
      if (plot_type == PROFILE) point[X]++;
      point[Y] = 0;
      bgnline();
      {
        v2f(point);
        point[Y] = v;
        v2f(point);
      }
      endline();
    }
    
    if (plot_type == PROFILE){
      if (axes & YAXIS){
        color(ProfileYColor);
        point[X] = y+1.0;
        point[Y] = 0;
        bgnline();
        {
          v2f(point);
          point[Y] = v;
          v2f(point);
        }
        endline();
      }
      
      if (axes & ZAXIS){
        color(ProfileZColor);
        point[X] = z+1.0;
        point[Y] = 0;
        bgnline();
        {
          v2f(point);
          point[Y] = v;
          v2f(point);
        }
        endline();
      }
    }
  }
}

void draw_profile(unsigned short *image,
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
  }else{
    if (!redraw_only){
      ld = image;
      lframe = frame;
      lx = x;
      ly = y;
      lz = (float)slice * iw / islices;
      lv = v;
    }
    
    color(ProfileBackgroundColor); clear();
    
    if (plot_present || !redraw_only){
      
      unsigned short *p;
      float point[XY];
      int count;
      
      if (axes & XAXIS){
        point[X] = 1.0;
        p = ld+(ly*iw);
        count = iw;
        
        color(ProfileXColor);
        bgnline();
        {
          while(count--){
            point[Y] = *p++ * icf + ict;
            v2f(point);
            point[X]++;
          }
        }
        endline();
      }
      
      if (axes & YAXIS){
        point[X] = 1.0;
        p = ld+lx;
        count = ih;
        
        color(ProfileYColor);
        bgnline();
        {
          while(count--){
            point[Y] = *p * icf + ict;
            v2f(point);
            point[X]++;
            p+=iw;
          }
        }
        endline();
      }
      
      if (axes & ZAXIS){
        point[X] = 1.0;
        p = id + lframe*is + ly*iw + lx;
        count = islices;
        
        color(ProfileZColor);
        bgnline();
        {
          while(count--){
            point[Y] = *p * icf + ict;
            v2f(point);
            point[X]+=x_step;
            p+=z_step;
          }
        }
        endline();
      }
      
      draw_axes(NIL,NIL,NIL,NIL,lx,ly,lz,axes,lv,PROFILE,NOT_INITIALIZE);
      if (!plot_present) plot_present = TRUE;
    }
    swapbuffers();
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
          if (*mask_p++ = (i-x)*(i-x)+(j-y)*(j-y) <= r2)
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
    *roi_std++ = sqrtf(diff / (n-1));

  }

  return(n);

}

void draw_dynamic(unsigned short *dynamic_volume,
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
  }else{
    if (!redraw_only){
      lv = v;
      lx = x;
      ly = y;
      lslice = slice;
      lframe = frame;
      lroi_radius = roi_radius;
      offset = slice * iframes * is + x + iw * y;
    }
    color(ProfileBackgroundColor); clear();
    
    if (plot_present || !redraw_only){
      
      int present_frame;
      float point[XY];
      float *t;
      
      t = itime;
      
      color(DynamicColor);
      
      if (lroi_radius == 0){
        if (roi_dump) printf("%5d\t%10d\n", x+1, y+1);
        bgnline();
        {
          for (present_frame = 0; present_frame < iframes; present_frame++){
            point[Y] = *(id + offset + present_frame * is) * icf + ict;
            point[X] = *t++;
            if (roi_dump) printf("%5.2f\t%10f\n", point[X], point[Y]);
            v2f(point);
          }
        }
        endline();
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
        bgnline();
        {
          for (present_frame = 0; present_frame < iframes; present_frame++){
            point[Y] = *(roi_tac + present_frame);
            point[X] = *t++;
            if (roi_dump) 
              printf("%5.2f\t%10f\t%10f\n", point[X], point[Y], *(roi_std+present_frame));
            v2f(point);
          }
        }
        endline();
        lv = *(roi_tac+lframe);
      }
      
      draw_axes(NIL,NIL,NIL,NIL,itime[lframe],NIL,NIL,NIL,lv,DYNAMIC,NOT_INITIALIZE);
      if (!plot_present) plot_present = TRUE;
    }
    swapbuffers();
  }
}

void evaluate_pixel(short position[XY],
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
                    long main_window,
                    long profile_window,
                    long dynamic_window,
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
    }else{
      roi_area = 1;
      v = *(image+lx+width*ly) * cfact + cterm;
    }
    frontbuffer(TRUE);
    write_eval_pixel(lx+1,ly+1,v,roi_area,FALSE);
    frontbuffer(FALSE);
    if (profiling && !profile_frozen){
      winset(profile_window);
      draw_profile(image,NIL,NIL,NIL,lx,ly,v,NIL,NIL,NIL,NIL,
                   frame,slice,axes,NOT_INITIALIZE,NOT_REDRAW_ONLY);
      winset(main_window);
    }
    if (dynamic && !dynamic_frozen){
      winset(dynamic_window);
      draw_dynamic(NIL,NIL,NIL,frame,slice,lx,ly,NIL,NIL,v,NIL,NIL,
                   roi_radius,NOT_INITIALIZE,NOT_REDRAW_ONLY,roi_dump);
      winset(main_window);
    }
  }
}

void calculate_conversions(float min,float max,float *cfact,float *cterm)
{
  float dynamic_range;
  dynamic_range = max - min;
  *cfact = dynamic_range / index_range;
  *cterm = -index_min * dynamic_range / index_range + min;
}

void draw_cursor(short *position, int radius)
{
    static short
        left = 0,
        right = WINDOW_WIDTH-1,
        bottom = 0,
        top = WINDOW_HEIGHT-1;
    long point[XY];
    static int o_r = 0;
    static Screencoord o_x = -1, o_y = -1;

    drawmode(OVERDRAW);
    XSetFunction(getXdpy(), getXgc(), GXxor);
    color(WHITE);

    if (o_x > 0 && o_y > 0) {
        if (o_r != 0) {
            circs(o_x, o_y, (short)o_r);
        }
        else {
            bgnline();
            point[X] = left;
            point[Y] = o_y;
            v2i(point);
            point[X] = right;
            v2i(point);
            endline();

            bgnline();
            point[X] = o_x;
            point[Y] = bottom;
            v2i(point);
            point[Y] = top;
            v2i(point);
            endline();
        }
        o_x = -1;
        o_y = -1;
    }
  
    if (position != NULL) {
        if (position[X] > left && position[X] < right && 
            position[Y] > bottom && position[Y] < top){
 
            if (radius != 0) {
                circs(position[X], position[Y], (short)(radius));
            }
            else {
                bgnline();
                point[X] = left;
                point[Y] = position[Y];
                v2i(point);
                point[X] = right;
                v2i(point);
                endline();

                bgnline();
                point[X] = position[X];
                point[Y] = bottom;
                v2i(point);
                point[Y] = top;
                v2i(point);
                endline();
            }
            o_x = position[X];
            o_y = position[Y];
            o_r = radius;
        }
    }
    XSetFunction(getXdpy(), getXgc(), GXcopy);
    drawmode(NORMALDRAW);
}

void erase_cursor()
{
    draw_cursor(NULL, 0);
}

void draw_color_bar(unsigned short *color_bar, float min, float max)
{
  Screencoord x,y; char scale_string[MAX_STRING_LENGTH]; int step; float range;
  static long
    v1[XY] = {WINDOW_WIDTH+COLOR_BAR_XOFFSET,COLOR_BAR_YOFFSET-1},
    v2[XY] = {WINDOW_WIDTH+COLOR_BAR_XOFFSET+COLOR_BAR_WIDTH-1,COLOR_BAR_YOFFSET-1},
    v3[XY] = {WINDOW_WIDTH+COLOR_BAR_XOFFSET+COLOR_BAR_WIDTH-1,COLOR_BAR_YOFFSET+COLOR_BAR_HEIGHT-1},
    v4[XY] = {WINDOW_WIDTH+COLOR_BAR_XOFFSET,COLOR_BAR_YOFFSET+COLOR_BAR_HEIGHT-1};
  
  rectwrite((Screencoord)v1[X],(Screencoord)v1[Y],(Screencoord)v3[X],(Screencoord)v3[Y],(Colorindex *)color_bar);
  x = v2[X];
  range = (max - min) / SCALE_TICS;
  color(TextColor);
  for (step = 0; step <= SCALE_TICS; step++){
    y = (float)step * SCALE_STEP + COLOR_BAR_YOFFSET;
    cmov2i(x,y-4.0);
    (void) sprintf(scale_string,"-%-7.1f", range * step + min);
    fmprstr(scale_string);
  }
  bgnline();
  {
    v2i(v1);
    v2i(v2);
    v2i(v3);
    v2i(v4);
    v2i(v1);
  }
  endline();
}

void clip_image(short *anchor,
                unsigned short *image,
                unsigned short *clipping_buffer,
                long width,
                long height,
                long new_width,
                long new_height)
{
  short y;
  
  for (y = 0; y < new_height; y++)
    (void) memcpy(&clipping_buffer[y * new_width],
                  &image[(y + anchor[Y]) * width + anchor[X]],
                  new_width * sizeof(unsigned short));
}


void draw_image(short *draw_origin,
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
  static Screencoord
    cleft = 0,
    cright = WINDOW_WIDTH,
    cbottom = 0,
    ctop = WINDOW_HEIGHT;
  
  color(ImageBackgroundColor); clear();
  pushviewport();
  viewport(cleft,cright,cbottom,ctop);
  rectzoom(zoom,zoom);
  
  if (draw_origin[X] < 0 || draw_origin[Y] < 0){
    
    if (draw_origin[X] < 0){
      anchor[X] = -draw_origin[X]/zoom;
      new_origin[X] = 0;
      new_width = image_width - anchor[X];
    }else{
      anchor[X] = 0;
      new_origin[X] = draw_origin[X];
      new_width = image_width;
    }
    
    if (draw_origin[Y] < 0){
      anchor[Y] = -draw_origin[Y]/zoom;
      new_origin[Y] = 0;
      new_height = image_height - anchor[Y];
    }else{
      anchor[Y] = 0;
      new_origin[Y] = draw_origin[Y];
      new_height = image_height;
    }
    
    if (new_width > 0 && new_height > 0){
      clip_image(anchor,
                 image,
                 clipping_buffer,
                 image_width,
                 image_height,
                 new_width,
                 new_height);         
      rectwrite((Screencoord)new_origin[X],
                (Screencoord)new_origin[Y],
                (Screencoord)new_width-1+new_origin[X],
                (Screencoord)new_height-1+new_origin[Y],
                (Colorindex *)clipping_buffer);
    }
    
  }else{
    
    rectwrite((Screencoord)draw_origin[X],
              (Screencoord)draw_origin[Y],
              (Screencoord)image_width-1+draw_origin[X],
              (Screencoord)image_height-1+draw_origin[Y],
              (Colorindex *)image);
    
  }
  
  rectzoom(1.0,1.0);
  popviewport();
  
}

void usage(char *program_name)
{
  (void) fprintf(stderr,"Usage: %s [option [...]] [filename]\n", program_name);
  exit(ERROR_STATUS);
}

long create_window(char *main_title,
                   float x_min,
                   float x_max,
                   float min,
                   float max,
                   int axes,
                   int plot_type,
                   long *origin,
                   long *size
                   )
{
  long window_id; char title_string[MAX_STRING_LENGTH];
  float x_range, y_range;
  static int prof_init = FALSE;
  static int dyna_init = FALSE;
  
  x_range = x_max - x_min;
  y_range = max - min;
  (void) sprintf(title_string,"%s: %s profile",
                 main_title,(plot_type == PROFILE)?"slice":"dynamic");
  if (plot_type == PROFILE && !prof_init || plot_type == DYNAMIC && !dyna_init){
    size[X] = AUXILLARY_WINDOW_WIDTH;
    size[Y] = AUXILLARY_WINDOW_HEIGHT;
    prefsize(size[X],size[Y]);
  }else{
    prefposition(origin[X],origin[X]+size[X]-1,origin[Y],origin[Y]+size[Y]-1);
  }
  window_id = winopen(title_string);
  doublebuffer(); gconfig();
  if (plot_type == PROFILE && !prof_init || plot_type == DYNAMIC && !dyna_init){
    getorigin(&origin[X],&origin[Y]);
    switch (plot_type){
    case PROFILE:
      prof_init = TRUE;
      break;
    case DYNAMIC:
      dyna_init = TRUE;
      break;
    }
  }
  ortho2((Coord)(x_min-x_range/(AXES_TICS*EXPAND)),
         (Coord)(x_max+x_range/(AXES_TICS*EXPAND)),
         (Coord)(min-y_range/(AXES_TICS*EXPAND)),
         (Coord)(max+y_range/(AXES_TICS*EXPAND))
         );
  /* winconstraints(); winconstraints(); */
  switch (plot_type){
  case PROFILE:
    draw_profile(NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,
                 NIL,NIL,NIL,axes,NOT_INITIALIZE,REDRAW_ONLY);
    break;
  case DYNAMIC:
    draw_dynamic(NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,
                 NIL,NIL,NIL,NOT_INITIALIZE,REDRAW_ONLY,NIL);
    break;
  }
  return(window_id);
}

void destroy_window(long window_id)
{
  winclose(window_id);
}

void set_overlay()
{
}

void draw_new_slice(short *draw_origin,
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
  draw_image(draw_origin,image,clipping_buffer,image_width,image_height,zoom);
  draw_color_bar(color_bar,min,max);
  write_eval_pixel(0,0,0,0,redraw);
  write_scale(low,high,frame,slice,frame_offset,slice_offset);
  swapbuffers();
}

void make_map_mode_icon(unsigned short *image,
                        unsigned short *sicon,
                        int image_width,
                        int image_height,
                        int icon_width,
                        int icon_height)
{
  int
    x, x_step, x_skip,
    y, y_step, y_skip;
  
  x_step = image_width / icon_width;
  y_step = image_height / icon_height;
  
  x_skip = (image_width % icon_width) / 2;
  y_skip = (image_height % icon_height) / 2;
  
  if (x_step == 0){
    x_step = 1;
    x_skip = 0;
  }
  if (y_step == 0){
    y_step = 1;
    y_skip = 0;
  }
  
  for (y = 0; y < icon_height; y++)
    for (x = 0; x < icon_width; x++)
      *sicon++ = *(image + image_width * (y_step * y + y_skip) + x_step * x + x_skip);
  
}


char *get_image_name(char *path_name)
{
  char *last;
  
  last = path_name;
  while(*path_name) if (*path_name++ == '/') last = path_name;
  return(last);
}


main(int argc, char *argv[])
{
  
  static char *
    window_title = NULL;
  static int 
    mni_data = FALSE,
    debug = FALSE,
    progress = TRUE,
    minc_data = FALSE,
    float_data = FALSE,
    indices[] = {INDEX_MIN,INDEX_MAX},
#define nindices (sizeof(indices)/sizeof(indices[0]))
    frames = 0,
    frame = START_FRAME,
    rframes[] = {0,0},
#define nrframes (sizeof(rframes)/sizeof(rframes[0]))
    rslices[] = {0,0},
#define nrslices (sizeof(rframes)/sizeof(rslices[0]))
    slices = 0,
    slice = START_SLICE,
    keep_foreground = FALSE,
    use_icon = FALSE,
    image_width = IMAGE_WIDTH,
    image_height = IMAGE_HEIGHT,
    roi_radius = ROI_RADIUS,
    time_available = FALSE,
    no_of_images = NO_OF_IMAGES;
  
  static void 
    (*present_scale)(float, float *) = 0;
  
  static ArgvInfo argTable[] = {
    { "-title", ARGV_STRING, (char *) NULL, (char *) &window_title,
        "Window title" },
    { "-mni", ARGV_CONSTANT, (char *) TRUE, (char *) &mni_data,
        "File is an mni file" },
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
    { "-foreground", ARGV_CONSTANT, (char *) TRUE, (char *) &keep_foreground,
        "Stay in foreground" },
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
    { "-icon", ARGV_CONSTANT, (char *) TRUE, (char *) &use_icon,
        "Use special premade icon (user selected or default)" },
    { "-describe", ARGV_FUNC, (char *) describe_postf, (char *) NULL,
        "Print out a description of the postf interface."},
    { (char *) NULL, ARGV_END, (char *) NULL, (char *) NULL, (char *) NULL },
  };
  
  MincInfo
    *MincFile;

  MniInfo
    *MniFile;
  
  char 
    matrix_type;
  
  static char
    default_window_title[] = "stdin";
  
  BOOLEAN quit;
  
  FILE *input_fp;
  
  Device 
    event;
  
  static Device
    mouse[] = {MOUSEX,MOUSEY};
  
  float
    *frame_length,
    *frame_time,
    cterm,cfact,  /* for reconverting color indices into floats */
    initial_zoom,
    zoom,
    low = 0.0,    /* threshold for start of scale ramping */
    high = 1.0,   /* threshold for end of scale ramping */
    min, max,     /* min and max data values */
    moving_color; /* color selected using mouse pointer during rescaling */
  
  long
    main_window,
    profile_window,
    dynamic_window,
    main_origin[XY],
    profile_origin[XY],
    profile_size[XY],
    dynamic_origin[XY],
    dynamic_size[XY];
  
  int
    min_frame = START_FRAME,
    min_slice = START_SLICE,
    max_frame = START_FRAME,
    max_slice = START_SLICE,
    offset_to_images, /* in blocks */
    frames_present = FALSE,
    frame_range_specified = FALSE,
    slice_range_specified = FALSE,
    main_frozen = FALSE,
    profile_frozen = FALSE,
    profiling = FALSE,
    dynamic_frozen = FALSE,
    axes = XAXIS | YAXIS,
    dynamic = FALSE,
    stride = 1,
    shiftkey_down = FALSE,
    ctrlkey_down = FALSE,
    altkey_down = FALSE,
    update_needed = FALSE,
    roi_dump = FALSE,
    must_create_profile = FALSE,
    must_create_dynamic = FALSE,
    must_redraw_main = FALSE,
    must_redraw_profile = FALSE,
    must_redraw_dynamic = FALSE,
    must_evaluate_pixel = FALSE,
    must_change_color_map = FALSE,
    must_write_scale = FALSE,
    must_syncronize_mouse = FALSE,
    drawing_new_image = FALSE,
    evaluating_pixel = FALSE,
    moving_origin = FALSE,
    moving_scale = FALSE,
    moving_top_scale = FALSE,
    moving_bottom_scale = FALSE,
    user_slice_count = SLICES,
    user_frame_count = FRAMES,
    user_image_count = SLICES * FRAMES,
    icon_width = 85,
    icon_height = 67,
    icon_draw_mode = MAP_MODE,
    *user_slice_list,
    *user_frame_list,
    image_size = IMAGE_SIZE,
    frames_specified = FALSE,
    slices_specified = FALSE;
  
  static short
    image_subwindow[2*XY] = {0,WINDOW_WIDTH-1,0,WINDOW_HEIGHT-1},
    scale_subwindow[2*XY] = {
      WINDOW_WIDTH+COLOR_BAR_XOFFSET-LOOSE,
      WINDOW_WIDTH+COLOR_BAR_XOFFSET+COLOR_BAR_WIDTH+LOOSE,
      COLOR_BAR_YOFFSET-LOOSE,
      COLOR_BAR_YOFFSET+COLOR_BAR_HEIGHT+LOOSE
      };
  
  short
    mouse_position[XY],
    evaluate_position[XY],
    draw_origin[XY] = {0,0},
    last_origin[XY],
    start_origin[XY],
    valuator;
  
  unsigned short
    *sicon,
    *image,
    *clipping_buffer,
    *dynamic_volume,
    glyph[16],
    color_bar[COLOR_BAR_WIDTH*(COLOR_BAR_HEIGHT+1)];
  
  unsigned long *licon;
  
  char *input_fn;

  TextColor = 1;
  
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
  
  /*
   * Read in postf.defauts file for runtime global variable settings
   */
  
  {
    
    char globals_filename[] = "postf.defaults";
    char homepath[MAX_STRING_LENGTH];
    char libpath[MAX_STRING_LENGTH];
    
    sprintf(libpath,"/usr/local/mni/lib/%s", globals_filename);
    sprintf(homepath,"%s/.%s", getenv("HOME"), globals_filename);
    
    if (file_exists(libpath)){
      input_globals_file(SIZEOF_STATIC_ARRAY(globals_list),
                         globals_list,
                         libpath);
    }
    
    if (file_exists(homepath)){
      input_globals_file(SIZEOF_STATIC_ARRAY(globals_list),
                         globals_list,
                         homepath);
    }
    
    if (file_exists(globals_filename)){
      input_globals_file(SIZEOF_STATIC_ARRAY(globals_list),
                         globals_list,
                         globals_filename);
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
      
      minc_data = TRUE;
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

      mni_data = TRUE;
      image_width = MniFile->ImageWidth;
      image_height = MniFile->ImageHeight;
      image_size = MniFile->ImageSize;

      if (slices_specified) MniFile->Slices = slices; else slices = MniFile->Slices; 
      if (frames_specified) MniFile->Frames = frames; else frames = MniFile->Frames;

      no_of_images = slices * frames;
      
    }else{
      
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
      }else if (no_of_images != slices * frames){
        (void) fprintf(stderr,"%s: # of images != slices * frames\n", argv[0]);
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
  }else{
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
  }else{
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
    (void) fprintf(stderr,"%s: NOT ENOUGH MEMORY AVAILABLE\n",argv[0]);
    exit(ERROR_STATUS);
  }

  if (minc_data) {
    
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
    
  } else if (mni_data){

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

  }else{
    
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
    (void) fprintf(stderr,"%s: chosen slice (%d) > # of slices (%d)\n",
                   argv[0],slice+1,slices);
    usage(argv[0]);
  }
  if(frame >= user_frame_count || frame < 0){
    (void) fprintf(stderr,"%s: chosen frame (%d) > # of frames (%d)\n", 
                   argv[0],frame+1,frames);
    usage(argv[0]);
  }
  
  calculate_conversions(min,max,&cfact,&cterm);
  
  /* 
   *  (void) fprintf(stderr, "      Number of slices: %d\n", slices);
   *  (void) fprintf(stderr, "      Number of frames: %d\n", frames);
   *  (void) fprintf(stderr, "Total number of images: %d\n", no_of_images);
   *  (void) fprintf(stderr, "            Volume min: %f\n", min);
   *  (void) fprintf(stderr, "            Volume max: %f\n", max);   
   *  (void) fprintf(stderr, "Conversion constants: %f and %f\n",cfact,cterm);
   */
  
  if (use_icon){
    char
      *icon_file,
      default_icon[] = "/usr/people/gaby/csrc/post.ico";
    
    if ((icon_file = getenv("POST_ICON")) == NULL) icon_file = default_icon;
    
    if (file_exists(icon_file)){
      
      sizeofimage(icon_file,&icon_width,&icon_height);
      licon = (unsigned long *)longimagedata(icon_file);
      icon_draw_mode = TRUE_COLOR_MODE;
      
    } else {
      
      sicon = (unsigned short *)malloc(sizeof(unsigned short)*icon_width*icon_height);
      
    }
    
  } else {
    
    sicon = (unsigned short *)malloc(sizeof(unsigned short)*icon_width*icon_height);
    
  }
  
  iconsize(icon_width,icon_height);
  
  /* if (debug) exit(NORMAL_STATUS); */
  
  initialize_gl_fonts();
  
  if (window_title == NULL) window_title = default_window_title;
  prefsize((long)(WINDOW_WIDTH+COLOR_BAR_WINDOW),(long)WINDOW_HEIGHT);
  
  if (keep_foreground) foreground();
  
  main_window = winopen(window_title);
  set_overlay();
  doublebuffer(); gconfig();
  
  if (present_scale == 0) present_scale = spectral;
  change_color_map(present_scale,low,high);
  getorigin(&main_origin[X],&main_origin[Y]);
  make_color_bar(color_bar);
  
  initial_zoom = zoom = (float)MIN(WINDOW_WIDTH/image_width,WINDOW_HEIGHT/image_height);
  image = dynamic_volume + (slice * user_frame_count + frame) * image_size;
  clipping_buffer = malloc(sizeof(*clipping_buffer) * image_size);
  draw_axes(0,MAX(image_width,image_height),min,max,NIL,NIL,NIL,NIL,NIL,PROFILE,INITIALIZE);
  draw_axes(frame_time[0],frame_time[user_frame_count-1],
            min,max,NIL,NIL,NIL,NIL,NIL,DYNAMIC,INITIALIZE);
  
  draw_profile(NIL,dynamic_volume,image_width,image_height,NIL,NIL,NIL,cfact,cterm,
               user_frame_count,user_slice_count,NIL,NIL,NIL,INITIALIZE,NOT_REDRAW_ONLY);
  draw_dynamic(dynamic_volume,frame_time,user_frame_count,NIL,NIL,NIL,NIL,image_width,
               image_height,NIL,cfact,cterm,roi_radius,INITIALIZE,NOT_REDRAW_ONLY,roi_dump);
  draw_new_slice(draw_origin,frame,slice,*rframes,*rslices,image_width,image_height,
                 min,max,low,high,image,clipping_buffer,zoom,color_bar,NOT_REDRAW_ONLY);
  
  noise(MOUSEX,4);
  noise(MOUSEY,4);
  
  qdevice(DKEY);
  qdevice(GKEY);
  qdevice(HKEY);
  qdevice(MKEY);
  qdevice(PKEY);
  qdevice(QKEY);
  qdevice(RKEY);
  qdevice(SKEY);
  qdevice(TKEY);
  qdevice(XKEY);
  qdevice(YKEY);
  qdevice(ZKEY);
  qdevice(ESCKEY);
  qdevice(SPACEKEY);
  qdevice(MINUSKEY);
  qdevice(EQUALKEY);
  /* qdevice(WINSHUT); */
  qdevice(WINQUIT);
  qdevice(MOUSEX);
  qdevice(MOUSEY);
  qdevice(LEFTARROWKEY);
  qdevice(RIGHTARROWKEY);
  qdevice(UPARROWKEY);
  qdevice(DOWNARROWKEY);
  qdevice(LEFTSHIFTKEY);
  qdevice(RIGHTSHIFTKEY);
  qdevice(LEFTCTRLKEY);
  qdevice(RIGHTCTRLKEY);
  qdevice(LEFTALTKEY);
  qdevice(RIGHTALTKEY);
  qdevice(LEFTMOUSE);
  qdevice(MIDDLEMOUSE);
  qdevice(RIGHTMOUSE);
  /* qdevice(WINTHAW); */
  /* qdevice(WINFREEZE); */
  /* qdevice(REDRAWICONIC); */
  
  getdev(2,mouse,mouse_position);
  mouse_position[X] -= main_origin[X];
  mouse_position[Y] -= main_origin[Y];
  
  quit = FALSE;
  while (!quit){
    
    if (!qtest()){
      
      if (must_create_profile){
        must_create_profile = FALSE;
        iconsize(icon_width,icon_height);
        profile_window = create_window(window_title,0,MAX(image_width,image_height),min,
                                       max,axes,PROFILE,profile_origin,profile_size);
        winset(main_window);
      }
      
      if (must_create_dynamic){
        must_create_dynamic = FALSE;
        iconsize(icon_width,icon_height);
        dynamic_window = create_window(window_title,frame_time[0],
                                       frame_time[user_frame_count-1],
                                       min,max,axes,DYNAMIC,
                                       dynamic_origin,dynamic_size);
        winset(main_window);
      }
      
      if (must_redraw_main){
        must_redraw_main = FALSE;
        draw_new_slice(draw_origin,frame,slice,*rframes,*rslices,image_width,image_height,
                       min,max,low,high,image,clipping_buffer,zoom,color_bar,NOT_REDRAW_ONLY);
      }
      
      if (must_redraw_profile){
        must_redraw_profile = FALSE;
        winset(profile_window);
        getorigin(&profile_origin[X],&profile_origin[Y]);
        getsize(&profile_size[X],&profile_size[Y]);
        viewport((Screencoord)0,(Screencoord)profile_size[X],
                 (Screencoord)0,(Screencoord)profile_size[Y]);
        draw_profile(NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,
                     NIL,NIL,axes,NOT_INITIALIZE,REDRAW_ONLY);
        winset(main_window);
      }
      
      if (must_redraw_dynamic){
        must_redraw_dynamic = FALSE;
        winset(dynamic_window);
        getorigin(&dynamic_origin[X],&dynamic_origin[Y]);
        getsize(&dynamic_size[X],&dynamic_size[Y]);
        viewport((Screencoord)0,(Screencoord)dynamic_size[X],
                 (Screencoord)0,(Screencoord)dynamic_size[Y]);
        draw_dynamic(NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,
                     NIL,NIL,NOT_INITIALIZE,REDRAW_ONLY,roi_dump);
        winset(main_window);
      }
      
      if (must_evaluate_pixel){
        must_evaluate_pixel = FALSE;
        if (evaluating_pixel) draw_cursor(mouse_position, roi_radius * zoom);
        evaluate_position[X] = (mouse_position[X] - draw_origin[X])/zoom;
        evaluate_position[Y] = (mouse_position[Y] - draw_origin[Y])/zoom;
        evaluate_pixel(evaluate_position,image,frame,slice,cfact,cterm,image_width,
                       image_height,image_size,roi_radius,profiling,profile_frozen,
                       dynamic,dynamic_frozen,axes,main_window,
                       profile_window,dynamic_window,roi_dump);
        if (roi_dump) roi_dump = FALSE;
      }
      if (must_change_color_map){
        must_change_color_map = FALSE;
        change_color_map(present_scale,low,high);
        qenter(REDRAW, main_window);
      }
      
      if (must_write_scale){
        must_write_scale = FALSE;
        frontbuffer(TRUE);
        write_scale(low,high,frame,slice,*rframes,*rslices);
        frontbuffer(FALSE);
      }
      
      if (drawing_new_image){
        drawing_new_image = FALSE;
      }
    }
    
    event=qread(&valuator);
    if (main_frozen && 
        /* event != REDRAWICONIC &&  */
        event != WINTHAW && 
        event != WINQUIT /* && 
                            event != WINSHUT */
        ) continue;
    
    switch(event){
      
    case WINTHAW:
      if (valuator == main_window){
        main_frozen = FALSE;
        if (profile_frozen) profile_frozen = FALSE;
        if (dynamic_frozen) dynamic_frozen = FALSE;
        if (icon_draw_mode == TRUE_COLOR_MODE) {
          cmode(); 
          gconfig();
        }
        must_redraw_main = TRUE;
        if (profiling) must_create_profile = TRUE;
        if (dynamic) must_create_dynamic = TRUE;
      }else if (valuator == profile_window){
        profile_frozen = FALSE;
        winset(profile_window);
        if (icon_draw_mode == TRUE_COLOR_MODE) {
          cmode(); 
          gconfig();
        }
        draw_profile(NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,
                     NIL,NIL,NIL,axes,NOT_INITIALIZE,REDRAW_ONLY);
        winset(main_window);
      }else if (valuator == dynamic_window){
        dynamic_frozen = FALSE;
        winset(dynamic_window);
        if (icon_draw_mode == TRUE_COLOR_MODE) {
          cmode(); 
          gconfig();
        }
        draw_dynamic(NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,NIL,
                     NIL,NIL,NOT_INITIALIZE,REDRAW_ONLY,roi_dump);
        winset(main_window);
      }
      break;
      
    case REDRAW:
      if (valuator == main_window){
        getorigin(&main_origin[X],&main_origin[Y]);
        must_redraw_main = TRUE;
      }
      else if (profiling && valuator == profile_window) must_redraw_profile = TRUE;
      else if (dynamic && valuator == dynamic_window) must_redraw_dynamic = TRUE;
      break;
      
    case DKEY:
      if (user_frame_count > 1 && valuator && !dynamic) must_create_dynamic = dynamic = TRUE;
      else if(valuator && dynamic && !dynamic_frozen){
        destroy_window(dynamic_window);
        winset(main_window);
        dynamic = FALSE;
      }
      break;
      
    case GKEY:
      if (valuator){
        present_scale = gray;
        must_change_color_map = TRUE;
      }
      break;
      
    case HKEY:
      if (valuator){
        present_scale = hotmetal;
        must_change_color_map = TRUE;
      }
      break;
      
    case MKEY:
      if (valuator){
        if (shiftkey_down){
          frame = max_frame;
          slice = max_slice;
        }else{
          frame = min_frame;
          slice = min_slice;
        }
        image = dynamic_volume + (slice * user_frame_count + frame) * image_size;
        must_redraw_main = TRUE;
      }
      break;
      
    case PKEY:
      if (valuator && !profiling){
        must_create_profile = TRUE;
        profiling = TRUE;
      }else if(valuator && profiling && !profile_frozen){
        destroy_window(profile_window);
        winset(main_window);
        profiling = FALSE;
      }
      break;
      
    case RKEY:
      if (valuator){
        if (shiftkey_down){
          zoom = initial_zoom;
          draw_origin[X] = last_origin[X] = 0;
          draw_origin[Y] = last_origin[Y] = 0;	       
          must_redraw_main = TRUE;
        }else{    
          low = 0.0; high = 1.0;
          must_change_color_map = TRUE;
          must_write_scale = TRUE;	
        }
      }
      break;
      
    case SKEY:
      if (valuator){
        present_scale = spectral;
        must_change_color_map = TRUE;
      }
      break;
      
    case TKEY:
      break;
      
    case XKEY:
      if (valuator) axes ^= XAXIS;
      break;
      
    case YKEY:
      if (valuator) axes ^= YAXIS;
      break;
      
    case ZKEY:
      if (valuator) axes ^= ZAXIS;
      break;
      
    case MINUSKEY:
      if (valuator && !shiftkey_down && in_subwindow(mouse_position,image_subwindow))
        if (zoom >= 2.0){
          evaluate_position[X] = (mouse_position[X] - draw_origin[X]) / zoom;
          evaluate_position[Y] = (mouse_position[Y] - draw_origin[Y]) / zoom;
          zoom--;
          last_origin[X] = draw_origin[X] = mouse_position[X] - zoom * evaluate_position[X];
          last_origin[Y] = draw_origin[Y] = mouse_position[Y] - zoom * evaluate_position[Y];
          must_redraw_main = TRUE;
        }
      break;
      
    case EQUALKEY:
      if (valuator && shiftkey_down && in_subwindow(mouse_position,image_subwindow)){
        evaluate_position[X] = (mouse_position[X] - draw_origin[X]) / zoom;
        evaluate_position[Y] = (mouse_position[Y] - draw_origin[Y]) / zoom;
        zoom++;
        last_origin[X] = draw_origin[X] = mouse_position[X] - zoom * evaluate_position[X];
        last_origin[Y] = draw_origin[Y] = mouse_position[Y] - zoom * evaluate_position[Y];
        must_redraw_main = TRUE;
      }
      break;
      
    case MOUSEX:
      mouse_position[X] = valuator - main_origin[X];
      break;
      
    case MOUSEY:
      mouse_position[Y] = valuator - main_origin[Y];
      if(evaluating_pixel){
        must_evaluate_pixel = TRUE;
      }else if(moving_origin){
        draw_origin[X] = last_origin[X] + (mouse_position[X] - start_origin[X]);
        draw_origin[Y] = last_origin[Y] + (mouse_position[Y] - start_origin[Y]);
        must_redraw_main = TRUE;
      }else if (moving_scale){
        translate_scale(mouse_position[Y],&low,&high,moving_color);
        must_change_color_map = TRUE;
        must_write_scale = TRUE;
      }else if (moving_bottom_scale){
        new_scale_slope(mouse_position[Y],&low,&high,TOP,moving_color);
        must_change_color_map = TRUE;
        must_write_scale = TRUE;
      }else if (moving_top_scale){
        new_scale_slope(mouse_position[Y],&low,&high,BOTTOM,moving_color);
        must_change_color_map = TRUE;
        must_write_scale = TRUE;
      }
      break;
      
    case LEFTMOUSE:
      if (valuator){
        if (in_subwindow(mouse_position,scale_subwindow)){
          moving_color = get_color(mouse_position[Y], low, high);
          moving_top_scale = TRUE;
        }else if (in_subwindow(mouse_position,image_subwindow) && !altkey_down){
          curstype(C16X1);
          defcursor(1,glyph);
          setcursor(1,0,0);
          evaluating_pixel = TRUE;
          must_evaluate_pixel = TRUE;
        }
      }else{
        if (moving_top_scale) moving_top_scale = FALSE;
        if (evaluating_pixel){
          erase_cursor();
          setcursor(0,0,0);
          evaluating_pixel = FALSE;
          must_evaluate_pixel = FALSE;
        }
      }
      break;
      
    case MIDDLEMOUSE:
      if (valuator){
        if (in_subwindow(mouse_position,scale_subwindow)){
          moving_color = get_color(mouse_position[Y], low, high);
          moving_scale = TRUE;
        }else if (in_subwindow(mouse_position,image_subwindow)){
          start_origin[X] = mouse_position[X];
          start_origin[Y] = mouse_position[Y];
          moving_origin = TRUE;
        }
      }else{
        if (moving_scale) moving_scale = FALSE;
        if (moving_origin){
          moving_origin = FALSE;
          last_origin[X] = draw_origin[X];
          last_origin[Y] = draw_origin[Y];
        }
      }
      break;
      
    case RIGHTMOUSE:
      if (valuator){
        if (in_subwindow(mouse_position,scale_subwindow)){
          moving_color = get_color(mouse_position[Y], low, high);
          moving_bottom_scale = TRUE;
        }else if (evaluating_pixel && dynamic){
          must_evaluate_pixel = TRUE;
          roi_dump = TRUE;
        }
      }else{
        if (moving_bottom_scale) moving_bottom_scale = FALSE;
      }
      break;
      
    case SPACEKEY:
      if (valuator && evaluating_pixel && dynamic){
        must_evaluate_pixel = TRUE;
        roi_dump = TRUE;
      }
      break;
      
    case RIGHTARROWKEY:
      if (valuator){
        if (ctrlkey_down){
          mouse_position[X] += stride * zoom;
          if (in_subwindow(mouse_position,image_subwindow)){
            must_evaluate_pixel = TRUE;
          }else{
            mouse_position[X] -= stride * zoom;
          }
        }else if (altkey_down){
          roi_radius = MIN(MAX_ROI_RADIUS,roi_radius+1);
          draw_cursor(mouse_position, roi_radius * zoom);
          if (in_subwindow(mouse_position,image_subwindow)){
            must_evaluate_pixel = TRUE;
          }
        }else{
          frame++;
          if (frame == user_frame_count) frame = 0;
          if (!shiftkey_down){
            image = dynamic_volume + (slice * user_frame_count + frame) * image_size;
            must_redraw_main = TRUE;		  
            if (in_subwindow(mouse_position,image_subwindow)){
              must_evaluate_pixel = TRUE;
            }
          }else{
            update_needed = TRUE;
            must_write_scale = TRUE;
          }
        }
      }
      break;
      
    case LEFTARROWKEY:
      if (valuator){
        if (ctrlkey_down){
          mouse_position[X] -= stride * zoom;
          if (in_subwindow(mouse_position,image_subwindow)){
            must_evaluate_pixel = TRUE;
          }else{
            mouse_position[X] += stride * zoom;
          }
        }else if (altkey_down){
          roi_radius = MAX(0,roi_radius-1);
          draw_cursor(mouse_position, roi_radius * zoom);
          if (in_subwindow(mouse_position,image_subwindow)){
            must_evaluate_pixel = TRUE;
          }
        }else{
          frame--;
          if (frame < 0) frame = user_frame_count-1;
          if (!shiftkey_down){
            image = dynamic_volume + (slice * user_frame_count + frame) * image_size;
            must_redraw_main = TRUE;
            if (in_subwindow(mouse_position,image_subwindow)){
              must_evaluate_pixel = TRUE;
            }
          }else{
            update_needed = TRUE;
            must_write_scale = TRUE;
          }
        }
      }
      break;
      
    case DOWNARROWKEY:
      if (valuator){
        if (ctrlkey_down){
          mouse_position[Y] -= stride * zoom;
          if (in_subwindow(mouse_position,image_subwindow)){
            must_evaluate_pixel = TRUE;
          }else{
            mouse_position[Y] += stride * zoom;
          }
        }else if (altkey_down){
          roi_radius = MAX(0,roi_radius-1);
          must_redraw_main = TRUE;
          if (in_subwindow(mouse_position,image_subwindow)){
            must_evaluate_pixel = TRUE;
          }
        }else{
          slice--;
          if (slice < 0) slice = user_slice_count-1;
          if (!shiftkey_down){
            image = dynamic_volume + (slice * user_frame_count + frame) * image_size;
            must_redraw_main = TRUE;
            if (in_subwindow(mouse_position,image_subwindow)){
              must_evaluate_pixel = TRUE;
            }
          }else{
            update_needed = TRUE;
            must_write_scale = TRUE;
          }
        }
      }
      break;
      
    case UPARROWKEY:
      if (valuator){
        if (ctrlkey_down){
          mouse_position[Y] += stride * zoom;
          if (in_subwindow(mouse_position,image_subwindow)){
            must_evaluate_pixel = TRUE;
          }else{
            mouse_position[Y] -= stride * zoom;
          }
        }else if (altkey_down){
          roi_radius = MIN(MAX_ROI_RADIUS,roi_radius+1);
          draw_cursor(mouse_position, roi_radius * zoom);
          if (in_subwindow(mouse_position,image_subwindow)){
            must_evaluate_pixel = TRUE;
          }
        }else{
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
      }
      break;
      
    case LEFTSHIFTKEY:
    case RIGHTSHIFTKEY:
      if (valuator){
        shiftkey_down = TRUE;
        if (ctrlkey_down) stride = 4;
      }else{
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
      
    case LEFTCTRLKEY:
    case RIGHTCTRLKEY:
      if (valuator && !ctrlkey_down){
        ctrlkey_down = TRUE;
        if (shiftkey_down) stride = 4;
        if (in_subwindow(mouse_position,image_subwindow)){
          curstype(C16X1);
          defcursor(1,glyph);
          setcursor(1,0,0);
          draw_cursor(mouse_position, roi_radius * zoom);
          evaluating_pixel = TRUE;
        }
      }else if (!valuator && ctrlkey_down){
        ctrlkey_down = FALSE;
        if (shiftkey_down) stride = 1;
        if (!altkey_down){
          must_syncronize_mouse = FALSE;
          getdev(2,mouse,mouse_position);
          mouse_position[X] -= main_origin[X];
          mouse_position[Y] -= main_origin[Y];
          erase_cursor();
          setcursor(0,0,0);
          evaluating_pixel = FALSE;
        }else{
          must_syncronize_mouse = TRUE;
        }
      }
      break;
      
    case LEFTALTKEY:
    case RIGHTALTKEY:
      if (valuator && !altkey_down && !evaluating_pixel){
        altkey_down = TRUE;
        if (in_subwindow(mouse_position,image_subwindow)){
          curstype(C16X1);
          defcursor(1,glyph);
          setcursor(1,0,0);
          draw_cursor(mouse_position, roi_radius * zoom);
          evaluating_pixel = TRUE;
        }
      }else if (!valuator && altkey_down){
        altkey_down = FALSE;
        if (!ctrlkey_down) {
          if (must_syncronize_mouse){
            must_syncronize_mouse = FALSE;
            getdev(2,mouse,mouse_position);
            mouse_position[X] -= main_origin[X];
            mouse_position[Y] -= main_origin[Y];
          }
          erase_cursor();
          setcursor(0,0,0);
          evaluating_pixel = FALSE;
        }
      }
      break;
      
    case WINFREEZE:
      if (valuator == main_window){
        main_frozen = TRUE;
        if (profiling){
          destroy_window(profile_window);
          winset(main_window);
        }
        if (dynamic){
          destroy_window(dynamic_window);
          winset(main_window);
        }
      }else if (valuator == profile_window){
        profile_frozen = TRUE;
        winset(profile_window);
      }else if (valuator == dynamic_window){
        dynamic_frozen = TRUE;
        winset(dynamic_window);
        
      }
      if (icon_draw_mode == TRUE_COLOR_MODE) {
        RGBmode();
        gconfig();
      }else{
        make_map_mode_icon(image,sicon,image_width,image_height,icon_width,icon_height);
      }
      
      /* FALL THROUGH */

#if 0      
    case REDRAWICONIC:
#endif
      if (valuator == profile_window) winset(profile_window);
      if (valuator == dynamic_window) winset(dynamic_window);
      frontbuffer(TRUE);
      if (icon_draw_mode == TRUE_COLOR_MODE){
        lrectwrite((Screencoord)0,
                   (Screencoord)0,
                   (Screencoord)icon_width-1,
                   (Screencoord)icon_height-1,
                   licon);
      }else{
        rectwrite((Screencoord)0,
                  (Screencoord)0,
                  (Screencoord)icon_width-1,
                  (Screencoord)icon_height-1,
                  sicon);
      }
      frontbuffer(FALSE);
      if (valuator != main_window) winset(main_window);
      break;
      
      /* case WINSHUT: */
    case WINQUIT:
      if (valuator == profile_window){
        destroy_window(profile_window);
        winset(main_window);
        profiling = FALSE;
      }else if (valuator == dynamic_window){
        destroy_window(dynamic_window);
        winset(main_window);
        dynamic = FALSE;
      }else quit = TRUE;
      break;
      
    case QKEY:
    case ESCKEY:
      if (valuator){
        getdev(2,mouse,mouse_position);
        mouse_position[X] -= main_origin[X];
        mouse_position[Y] -= main_origin[Y];
        if (in_subwindow(mouse_position,image_subwindow) || 
            in_subwindow(mouse_position,scale_subwindow))
          quit = TRUE;
      }
      break;
    }
  }
  
  gexit();
  return(NORMAL_STATUS);
  
}
