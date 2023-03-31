
static char RcsHeader[] = "$Header: /usr/home/nakashim/proj-arm64/src/csim/RCS/xdisp.c,v 1.3 2021/06/14 03:53:22 nakashim Exp nakashim $";

/*                          Copyright (C) 2013- by NAIST */
/*                           Primary writer: Y.Nakashima */
/*                                  nakashim@is.naist.jp */
/* xdisp.c 2002/4/22 */ 

#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xdbe.h>
#include "csim.h"

#define WD           320
#define HT           240
#define BITMAP       (WD*HT)
#define SCRWD        4
#define SCRHT        3
unsigned int         X[BITMAP*SCRWD*SCRHT]; /* mapped on csim */

/***********/
/* for X11 */
/***********/

typedef struct {
  unsigned int  width;  /* width of image in pixels */
  unsigned int  height; /* height of image in pixels */
  unsigned char *data;  /* data rounded to full byte for each row */
} Image;

typedef struct {
  Display  *disp;       /* destination display */
  int       scrn;       /* destination screen */
  int       depth;      /* depth of drawable we want/have */
  int       dpixlen;    /* bitsPerPixelAtDepth */
  Drawable  drawable;   /* drawable to send image to */
  Colormap  cmap;       /* colormap used for image */
  GC        gc;         /* cached gc for sending image */
  XImage   *ximage;     /* ximage structure */
} XImageInfo;

union {
  XEvent              event;
  XAnyEvent           any;
  XButtonEvent        button;
  XExposeEvent        expose;
  XMotionEvent        motion;
  XResizeRequestEvent resize;
  XClientMessageEvent message;
} event;

unsigned int          redvalue[256], greenvalue[256], bluevalue[256];
XImageInfo            ximageinfo;
Image                 imageinfo;  /* image that will be sent to the display */
unsigned int          bitsPerPixelAtDepth();
void                  imageInWindow();
void                  bestVisual();
XdbeBackBuffer	      backBuffer;	/* Back buffer */
XdbeBackBufferAttributes  *backAttr;	/* Back buffer attributes */
XdbeSwapInfo	      swapInfo;		/* Swap info */

#define TRUE_RED(PIXVAL)      (((PIXVAL) & 0xff0000) >> 16)
#define TRUE_GREEN(PIXVAL)    (((PIXVAL) &   0xff00) >>  8)
#define TRUE_BLUE(PIXVAL)     (((PIXVAL) &     0xff)      )

void x11_open()
{
  if (!(flag & IMAGEWIN))
    return;

  if (!(ximageinfo.disp = XOpenDisplay(NULL))) {
    printf("%s: Cannot open display\n", XDisplayName(NULL));
    onintr_exit(1);
  }
  ximageinfo.scrn = DefaultScreen(ximageinfo.disp);
  imageinfo.width = WD*4;
  imageinfo.height= HT*3;
  /*imageinfo.data  = X;*/
  imageInWindow(&ximageinfo, &imageinfo);
}

void x11_update()
{
  if (!(flag & IMAGEWIN))
    return;

  XPutImage(ximageinfo.disp, ximageinfo.drawable, ximageinfo.gc,
	    ximageinfo.ximage, 0, 0, 0, 0, imageinfo.width, imageinfo.height);
}

int x11_checkevent()
{
  if (!(flag & IMAGEWIN))
    return(0);

  XNextEvent(ximageinfo.disp, &event.event);
  switch (event.any.type) {
  case KeyPress:
    return (0);
  default:
    x11_update();
    return (1);
  }
}

void x11_close()
{
  if (!(flag & IMAGEWIN))
    return;

  XCloseDisplay(ximageinfo.disp);
}

void imageInWindow(ximageinfo, image)
     XImageInfo   *ximageinfo;
     Image        *image;
{
  Window                ViewportWin;
  Visual               *visual;
  unsigned int          depth;
  unsigned int          dpixlen;
  XSetWindowAttributes  swa_view;
  XSizeHints            sh;
  unsigned int pixval;
  unsigned int redcolors, greencolors, bluecolors;
  unsigned int redstep, greenstep, bluestep;
  unsigned int redbottom, greenbottom, bluebottom;
  unsigned int redtop, greentop, bluetop;
  XColor        xcolor;
  unsigned int  a;
  XGCValues gcv;

  bestVisual(ximageinfo->disp, ximageinfo->scrn, &visual, &depth);
  dpixlen = (bitsPerPixelAtDepth(ximageinfo->disp, depth) + 7) / 8;

  ximageinfo->depth   = depth;
  ximageinfo->dpixlen = dpixlen;
  ximageinfo->drawable= None;
  ximageinfo->gc      = NULL;
  ximageinfo->ximage  = XCreateImage(ximageinfo->disp, visual, depth, ZPixmap, 0,
                                     NULL, image->width, image->height,
                                     8, 0);
  ximageinfo->ximage->data= (char*)malloc(image->width * image->height * dpixlen);
  ximageinfo->ximage->byte_order= MSBFirst; /* trust me, i know what
                                             * i'm talking about */

  if (visual == DefaultVisual(ximageinfo->disp, ximageinfo->scrn))
    ximageinfo->cmap= DefaultColormap(ximageinfo->disp, ximageinfo->scrn);
  else
    ximageinfo->cmap= XCreateColormap(ximageinfo->disp, RootWindow(ximageinfo->disp, ximageinfo->scrn), visual, AllocNone);

  redcolors= greencolors= bluecolors= 1;
  for (pixval= 1; pixval; pixval <<= 1) {
    if (pixval & visual->red_mask)
      redcolors <<= 1;
    if (pixval & visual->green_mask)
      greencolors <<= 1;
    if (pixval & visual->blue_mask)
      bluecolors <<= 1;
  }

  redtop   = 0;
  greentop = 0;
  bluetop  = 0;
  redstep  = 256 / redcolors;
  greenstep= 256 / greencolors;
  bluestep = 256 / bluecolors;
  redbottom= greenbottom= bluebottom= 0;
  for (a= 0; a < visual->map_entries; a++) {
    if (redbottom < 256)
      redtop= redbottom + redstep;
    if (greenbottom < 256)
      greentop= greenbottom + greenstep;
    if (bluebottom < 256)
      bluetop= bluebottom + bluestep;

    xcolor.flags= DoRed | DoGreen | DoBlue;
    xcolor.red  = (redtop - 1) << 8;
    xcolor.green= (greentop - 1) << 8;
    xcolor.blue = (bluetop - 1) << 8;
    XAllocColor(ximageinfo->disp, ximageinfo->cmap, &xcolor);

    while ((redbottom < 256) && (redbottom < redtop))
      redvalue[redbottom++]= xcolor.pixel & visual->red_mask;
    while ((greenbottom < 256) && (greenbottom < greentop))
      greenvalue[greenbottom++]= xcolor.pixel & visual->green_mask;
    while ((bluebottom < 256) && (bluebottom < bluetop))
      bluevalue[bluebottom++]= xcolor.pixel & visual->blue_mask;
  }

  swa_view.background_pixel= WhitePixel(ximageinfo->disp, ximageinfo->scrn);
  swa_view.backing_store= WhenMapped;
  swa_view.cursor= XCreateFontCursor(ximageinfo->disp, XC_watch);
  swa_view.event_mask= ButtonPressMask | Button1MotionMask | KeyPressMask |
    StructureNotifyMask | EnterWindowMask | LeaveWindowMask | ExposureMask;
  swa_view.save_under= False;
  swa_view.bit_gravity= NorthWestGravity;
  swa_view.save_under= False;
  swa_view.colormap= ximageinfo->cmap;
  swa_view.border_pixel= 0;
  ViewportWin= XCreateWindow(ximageinfo->disp, RootWindow(ximageinfo->disp, ximageinfo->scrn), 0, 0,
                             image->width, image->height, 0,
                             DefaultDepth(ximageinfo->disp, ximageinfo->scrn), InputOutput,
                             DefaultVisual(ximageinfo->disp, ximageinfo->scrn),
                             CWBackingStore | CWBackPixel |
                             CWEventMask | CWSaveUnder,
                             &swa_view);
  ximageinfo->drawable= ViewportWin;

  gcv.function= GXcopy;
  ximageinfo->gc= XCreateGC(ximageinfo->disp, ximageinfo->drawable, GCFunction, &gcv);

  sh.width= image->width;
  sh.height= image->height;
  sh.min_width= image->width;
  sh.min_height= image->height;
  sh.max_width= image->width;
  sh.max_height= image->height;
  sh.width_inc= 1;
  sh.height_inc= 1;
  sh.flags= PMinSize | PMaxSize | PResizeInc | PSize;
  XSetNormalHints(ximageinfo->disp, ViewportWin, &sh);

  XStoreName(ximageinfo->disp, ViewportWin, "csim");
  XMapWindow(ximageinfo->disp, ViewportWin);
  XSync(ximageinfo->disp, False);
}

void bestVisual(disp, scrn, rvisual, rdepth)
     Display       *disp;
     int            scrn;
     Visual       **rvisual;
     unsigned int  *rdepth;
{
  unsigned int  depth, a;
  Screen       *screen;
  XVisualInfo template, *info;
  int nvisuals;

  /* figure out the best depth the server supports.  note that some servers
   * (such as the HP 11.3 server) actually say they support some depths but
   * have no visuals that support that depth.  seems silly to me....
   */
  depth = 0;
  screen= ScreenOfDisplay(disp, scrn);
  for (a= 0; a < screen->ndepths; a++) {
    if (screen->depths[a].nvisuals &&
        ((!depth ||
          ((depth < 24) && (screen->depths[a].depth > depth)) ||
          ((screen->depths[a].depth >= 24) &&
           (screen->depths[a].depth < depth)))))
      depth= screen->depths[a].depth;
  }
  template.screen= scrn;
  template.class= TrueColor;
  template.depth= depth;
  if (! (info= XGetVisualInfo(disp, VisualScreenMask | VisualClassMask | VisualDepthMask, &template, &nvisuals)))
    *rvisual= NULL; /* no visuals of this depth */
  else {
    *rvisual= info->visual;
    XFree((char *)info);
  }
  *rdepth= depth;
}

unsigned int bitsPerPixelAtDepth(disp, depth)
     Display      *disp;
     unsigned int  depth;
{
  XPixmapFormatValues *xf;
  unsigned int nxf, a;

  xf = XListPixmapFormats(disp, (int *)&nxf);
  for (a = 0; a < nxf; a++)
    if (xf[a].depth == depth)
      return(xf[a].bits_per_pixel);

  fprintf(stderr, "bitsPerPixelAtDepth: Can't find pixmap depth info!\n");
  exit(1);
}

void BGR_to_X(int id, Uint *from)
{
  int i, j;
  Uint *to;

  to = (Uint*)(ximageinfo.ximage->data)+BITMAP*SCRWD*(id/SCRWD)+WD*(id%SCRWD);
  for (i=0; i<HT; i++,to+=WD*(SCRWD-1)) {
    for (j=0; j<WD; j++)
      *to++ = *from++;
  }
}
