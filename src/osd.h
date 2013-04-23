
/**
 * animenu - lirc menu system
 *
 *  copyright (c) 2009, 2012-2013 by Pete Beardmore <pete.beardmore@msn.com>
 *  copyright (c) 2001-2003 by Alastair M. Robinson <blackfive@fakenhamweb.co.uk>
 *
 *  licensed under GNU General Public License 2.0 or later
 *  some rights reserved. see COPYING, AUTHORS
 */

#ifndef IRMIX_OSD_H
#define IRMIX_OSD_H

#define OSD_MAXANIMFRAME 2000

struct osditemdata {
  char *title;
  int frameoffset;
  int lastedge;
};

struct osdcontext {
  void (*dispose) (struct osdcontext *osd);
  void (*show) (struct osdcontext *osd);
  void (*showframe) (struct osdcontext *osd, int frame);
  void (*showselected) (struct osdcontext *osd, int selected);
  void (*hide) (struct osdcontext *osd);
  void (*hideframe) (struct osdcontext *osd, int frame);
  void (*setstringcallback) (struct osdcontext *osd,
                             void *(*stringcallback) (void *userdata, struct osditemdata **osdid),
                             void *userdata);
  struct osdprivate *priv;
};

struct osdcontext *osd_create(struct osdcontext *parent,
                              void *(*idcallback) (void *userdata, struct osditemdata **osdid),
                              void *userdata);

#endif
