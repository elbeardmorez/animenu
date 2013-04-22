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
