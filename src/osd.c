
/*
 * animenu - lirc menu system
 *
 *  copyright (c) 2009, 2012-2013 by Pete Beardmore <pete.beardmore@msn.com>
 *  copyright (c) 2001-2003 by Alastair M. Robinson <blackfive@fakenhamweb.co.uk>
 *
 *  licensed under GNU General Public License 2.0 or later
 *  some rights reserved. see COPYING, AUTHORS
 */

#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/ioctl.h>

/* x11 includes */
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xmd.h>
#include <X11/extensions/shape.h>

#ifdef HAVE_LIBXFT
#include <X11/Xft/Xft.h>
#endif /* HAVE_LIBXFT */

#include "osd.h"

extern int menuanimation;

struct osdprivate {
  struct osdcontext *parent;
  Display *display;
  Window win;
  GC greengc, lightgrngc;
  int left, top;
  int width, height;
  int itemcount;
  int mapped;
  Pixmap bg_initial, bg_shaded;
  XFontStruct *font;
  int fontascent, fontdescent;
  int itemheight;
  int itemoffset;
#ifdef HAVE_LIBXFT
  XftDraw *xftdraw;
  XftColor bgcolor, fgcolor;
#endif  /* HAVE_LIBXFT */
  int frame;
  void *userdata;
  void *(*osdidcallback) (void *ud, struct osditemdata **osdid);
};

static unsigned long getcolor(struct osdcontext *osd, char *colorname) {
  XColor color;
  XWindowAttributes winattr;
  XGetWindowAttributes(osd->priv->display, RootWindow(osd->priv->display, DefaultScreen(osd->priv->display)), &winattr);

  color.pixel = 0;
  if ((XParseColor(osd->priv->display, winattr.colormap, colorname, &color)) == 0)
    fprintf(stderr, "XParseColor: cannot resolve colorname %s.\n", colorname);
  color.flags = DoRed | DoGreen | DoBlue;
  XAllocColor(osd->priv->display, winattr.colormap, &color);
  return color.pixel;
}

static void osd_sync(struct osdcontext *osd) {
  XFlush(osd->priv->display);
}

static void osd_showselected(struct osdcontext *osd, int selected) {
  int items, i, edge, frame = 0;
  float t;
  int stepsize = 30;
  struct osditemdata *oid = NULL;
  void *ud = osd->priv->userdata;

  i = 0;
  while (ud) {
    ud = osd->priv->osdidcallback(ud, &oid);
    if (oid) {
      if (i == selected) {
        XDrawString(osd->priv->display, osd->priv->win,
                    osd->priv->lightgrngc, 16, i * osd->priv->itemheight + osd->priv->itemoffset,
                    oid->title, strlen(oid->title));
      } else {
        XDrawString(osd->priv->display, osd->priv->win,
                    osd->priv->greengc, 16, i * osd->priv->itemheight + osd->priv->itemoffset,
                    oid->title, strlen(oid->title));
      }
    }
    ++i;
  }
  osd_sync(osd);
}

static void osd_initanim(struct osdcontext *osd) {
  XMapRaised(osd->priv->display, osd->priv->win);

  XCopyArea(osd->priv->display, osd->priv->win, osd->priv->bg_initial,
            osd->priv->greengc, 0, 0, osd->priv->width, osd->priv->height, 0, 0);

  XCopyArea(osd->priv->display, osd->priv->win, osd->priv->bg_shaded,
            osd->priv->greengc, 0, 0, osd->priv->width, osd->priv->height, 0, 0);

#ifdef HAVE_LIBXFT
  XftDrawRect(osd->priv->xftdraw, &osd->priv->bgcolor, 0, 0, osd->priv->width, osd->priv->height);
#endif /* HAVE_LIBXFT */

  osd->priv->mapped = 1;
}

static void osd_showframe(struct osdcontext *osd, int frame) {
  int items, i, edge;
  float t;
  struct osditemdata *oid = NULL;
  void *ud = osd->priv->userdata;

  if (osd->priv->mapped == 0)
    osd_initanim(osd);

  items = osd->priv->height / osd->priv->itemheight;

  if (frame <= 1) {
    i = 0;
    while (ud) {
      ud = osd->priv->osdidcallback(ud, &oid);
      if (oid) {
        oid->frameoffset = -(1000 / items) * i;
        oid->lastedge = 0;
      }
      ++i;
    }
  }

  ud = osd->priv->userdata;
  i = 0;
  while (ud) {
    ud = osd->priv->osdidcallback(ud, &oid);
    if (oid) {
      edge = 1000 - (frame + oid->frameoffset);
      if ((edge >= -50) && (edge <= 1000)) {
        if (edge < 0)
          edge = 0;
        t = edge;
        t /= 1000;
        t = pow(t, 3);
        edge = osd->priv->width * (1 - t);
        if (edge > oid->lastedge) {
          XCopyArea(osd->priv->display, osd->priv->bg_shaded, osd->priv->win,
                    osd->priv->greengc, 0, i * osd->priv->itemheight,
                    edge, osd->priv->itemheight, 0, i * osd->priv->itemheight);
          XDrawString(osd->priv->display, osd->priv->win,
                      osd->priv->greengc, edge - (osd->priv->width - 16),
                      i * osd->priv->itemheight + osd->priv->itemoffset, oid->title, strlen(oid->title));
          oid->lastedge = edge;
        }
      }
    }
    ++i;
  }
  osd_sync(osd);
}

static void osd_show(struct osdcontext *osd) {
  int frame;
  for (frame = 0; frame < OSD_MAXANIMFRAME; frame += 30) {
    osd->showframe(osd, frame);
    usleep(menuanimation);
  }
}

static void osd_hideframe(struct osdcontext *osd, int frame) {
  int items, i, edge;
  float t;
  struct osditemdata *oid = NULL;
  void *ud = osd->priv->userdata;

  if (osd->priv->mapped) {
    items = osd->priv->height / osd->priv->itemheight;
    if (frame <= 1) {
      i = 0;
      while (ud) {
        ud = osd->priv->osdidcallback(ud, &oid);
        oid->frameoffset = (-1000 / items) * (items - i);
        oid->lastedge = 0;
        ++i;
      }
    }

    ud = osd->priv->userdata;
    i = 0;
    while (ud) {
      ud = osd->priv->osdidcallback(ud, &oid);
      edge = (frame + oid->frameoffset);
      if ((edge >= 0) && (edge <= 1000 + 50)) {
        if (edge > 1000)
          edge = 1000;
        t = edge;
        t /= 1000;
        t = pow(t, 3);
        edge = t * osd->priv->width;
        if (edge > oid->lastedge) {
          XCopyArea(osd->priv->display, osd->priv->bg_initial, osd->priv->win,
                    osd->priv->greengc, 0, i * osd->priv->itemheight,
                    edge, osd->priv->itemheight, 0, i * osd->priv->itemheight);
          XCopyArea(osd->priv->display, osd->priv->bg_shaded, osd->priv->win,
                    osd->priv->greengc, edge, i * osd->priv->itemheight,
                    osd->priv->width - edge, osd->priv->itemheight, edge, i * osd->priv->itemheight);
          XDrawString(osd->priv->display, osd->priv->win,
                      osd->priv->greengc, edge + 16, i * osd->priv->itemheight + osd->priv->itemoffset,
                      oid->title, strlen(oid->title));
          oid->lastedge = edge;
        }
      }
      ++i;
    }
    osd_sync(osd);

    if (frame >= 1950) {
      XUnmapWindow(osd->priv->display, osd->priv->win);
      XFlush(osd->priv->display);
      osd->priv->mapped = 0;
    }
  }
}

static void osd_hide(struct osdcontext *osd) {
  int frame;
  for (frame = 0; frame < OSD_MAXANIMFRAME; frame += 40) {
    osd->hideframe(osd, frame);
    usleep(menuanimation);
  }
}

static void osd_dispose(struct osdcontext *osd) {
  if (osd->priv->mapped)
    osd->hide(osd);
  free(osd);
}

#ifdef HAVE_LIBXFT
void setup_xft(struct osdcontext *osd) {
  XftDraw *xftdraw;
  XftColor color_bg, color_fg, colortwo;
  XRenderColor colortmp;
  int screen_num = DefaultScreen(osd->priv->display);

  osd->priv->xftdraw =
    XftDrawCreate(osd->priv->display,
                  (Drawable) osd->priv->bg_shaded,
                  DefaultVisual(osd->priv->display, screen_num), DefaultColormap(osd->priv->display, screen_num));

  if (!osd->priv->xftdraw)
    return;

  colortmp.red = 0x0;
  colortmp.green = 0x0;
  colortmp.blue = 0x0;
  colortmp.alpha = 0x006000;
  XftColorAllocValue(osd->priv->display,
                     DefaultVisual(osd->priv->display, screen_num),
                     DefaultColormap(osd->priv->display, screen_num), &colortmp, &osd->priv->bgcolor);

  colortmp.red = 0x0;
  colortmp.green = 0x0;
  colortmp.blue = 0x0;
  colortmp.alpha = 0x00ffff;
  XftColorAllocValue(osd->priv->display,
                     DefaultVisual(osd->priv->display, screen_num),
                     DefaultColormap(osd->priv->display, screen_num), &colortmp, &osd->priv->fgcolor);

}
#endif /* HAVE_LIBXFT */

static void osd_calcdimensions(struct osdcontext *osd) {
  int width = 0, height = 0, count = 0;
  struct osdprivate *osdp = osd->priv;
  if (osdp->osdidcallback) {
    int w = 0;
    struct osditemdata *osdid;
    void *ud = osdp->userdata;
    while (ud) {
      ud = osdp->osdidcallback(ud, &osdid);
      if (osdid->title)
        w = XTextWidth(osdp->font, osdid->title, strlen(osdid->title));
      if (w > width)
        width = w;
      height += osd->priv->itemheight;
      ++count;
    }
  }
  osdp->width = width;
  osdp->height = height;
  osdp->itemcount = count;
}

struct osdcontext *osd_create(struct osdcontext *parent,
                              void *(*osdidcallback) (void *userdata, struct osditemdata **osdid),
                              void *userdata) {
  struct osdcontext *osd;
  struct osdprivate *osdp;
  XGCValues gcval;
  GC gc;
  XSizeHints sizehints;
  XSetWindowAttributes xattributes;
  XCharStruct extent;
  int txt_direction;
  int width, screen_num;

  if (!(osd = malloc(sizeof(struct osdcontext)))) {
    fprintf(stderr, "cannot allocate osdcontext!\n");
    return(NULL);
  }

  if (!(osdp = malloc(sizeof(struct osdprivate)))) {
    fprintf(stderr, "cannot allocate osdcontext!\n");
    return(NULL);
  }

  osd->priv = osdp;

  osdp->mapped = 0;
  osd->priv->osdidcallback = osdidcallback;
  osd->priv->userdata = userdata;
  osd->priv->parent = parent;

  osd->dispose = osd_dispose;
  osd->show = osd_show;
  osd->showframe = osd_showframe;
  osd->showselected = osd_showselected;
  osd->hide = osd_hide;
  osd->hideframe = osd_hideframe;
  if (!(osd->priv->display = XOpenDisplay(NULL))) {
    fprintf(stderr, "unable to open display\n");
    exit(EXIT_FAILURE);
  }

  screen_num = DefaultScreen(osdp->display);

  osd->priv->font = XLoadQueryFont(osd->priv->display, fontname);

  if (osd->priv->font == NULL) {
    fprintf(stderr, "trying alternate font\n");
    osd->priv->font = XLoadQueryFont(osd->priv->display,
           "-sony-fixed-medium-r-normal--36-*-100-100-c-*-iso8859-*");
    if (osd->priv->font == NULL) {
      fprintf(stderr, "trying \"fixed\" font\n");
      osd->priv->font = XLoadQueryFont(osd->priv->display, "fixed");
      if (osd->priv->font == NULL) {
        fprintf(stderr, "your X server is probably broken\n");
        return(NULL);
      }
    }
  }

  XTextExtents(osd->priv->font, "The quick brown fox jumps over the lazy dog!", 44,
               &txt_direction, &osd->priv->fontascent, &osd->priv->fontdescent, &extent);

  osd->priv->itemheight = osd->priv->fontascent + osd->priv->fontdescent + 2;
  osd->priv->itemoffset = osd->priv->fontascent + 2;

  osd_calcdimensions(osd);

  osd->priv->width += 40;

  if (parent) {
    osd->priv->top = parent->priv->top + (3 * osd->priv->itemheight) / 2;
    osd->priv->left = parent->priv->left + parent->priv->width;
  } else {
    osd->priv->top = 32;
    osd->priv->left = 32;
  }
  sizehints.flags = USSize | USPosition;

  sizehints.x = osd->priv->left;
  sizehints.y = osd->priv->top;

  sizehints.width = osd->priv->width;
  sizehints.height = osd->priv->height;
  xattributes.save_under = True;
  xattributes.override_redirect = True;
  xattributes.cursor = None;

  osd->priv->win = XCreateWindow(osd->priv->display,
                                 DefaultRootWindow(osd->priv->display),
                                 sizehints.x, sizehints.y,
                                 osd->priv->width, osd->priv->height, 0,
                                 CopyFromParent,   // depth
                                 CopyFromParent,   // class
                                 CopyFromParent,   // visual
                                 0,  // valuemask
                                 0); // attributes

  XSetWMNormalHints(osd->priv->display, osd->priv->win, &sizehints);
  XChangeWindowAttributes(osd->priv->display, osd->priv->win, CWSaveUnder | CWOverrideRedirect, &xattributes);
  XStoreName(osd->priv->display, osd->priv->win, "osd");

  gcval.foreground = getcolor(osd, fgcolor);
  gcval.background = getcolor(osd, bgcolor);
  gcval.graphics_exposures = 0;

  osd->priv->greengc = XCreateGC(osd->priv->display, osd->priv->win,
                                 GCForeground | GCBackground | GCGraphicsExposures, &gcval);

  gcval.foreground = getcolor(osd, selectfgcolor);
  osd->priv->lightgrngc = XCreateGC(osd->priv->display, osd->priv->win,
                                    GCForeground | GCBackground | GCGraphicsExposures, &gcval);

  XSetFont(osd->priv->display, osd->priv->greengc, osd->priv->font->fid);
  XSetFont(osd->priv->display, osd->priv->lightgrngc, osd->priv->font->fid);

  osd->priv->bg_initial = XCreatePixmap(osd->priv->display,
                                        DefaultRootWindow(osd->priv->display),
                                        osd->priv->width, osd->priv->height,
                                        DefaultDepth(osd->priv->display, screen_num));

  osd->priv->bg_shaded = XCreatePixmap(osd->priv->display,
                                       DefaultRootWindow(osd->priv->display),
                                       osd->priv->width, osd->priv->height,
                                       DefaultDepth(osd->priv->display, screen_num));

#ifdef HAVE_LIBXFT
  setup_xft(osd);
#endif

  return(osd);
}
