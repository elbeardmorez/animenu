#ifndef animenuCONTEXT_H
#define animenuCONTEXT_H

#include "osd.h"

enum animenuitem_type {animenuitem_null, animenuitem_command, animenuitem_submenu};

struct animenucontext {
  /* public functions */
  void (*dispose) (struct animenucontext *menu);
  void (*next) (struct animenucontext *menu);
  void (*prev) (struct animenucontext *menu);
  void (*select) (struct animenucontext *menu);
  void (*show) (struct animenucontext *menu);
  void (*showcurrent) (struct animenucontext *menu);
  void (*hide) (struct animenucontext *menu);
  /* private data */
  struct animenuitem *firstitem, *lastitem;
  struct animenuitem *currentitem;
  struct animenucontext *parent;
  struct osdcontext *osd;
  int visible;
};

struct animenuitem {
  /* public functions */
  void (*dispose) (struct animenuitem *mi);
  void (*go) (struct animenuitem *mi);
  int (*setitem) (struct animenuitem *mi, char *title, char *command);
  int (*setsubmenu) (struct animenuitem *mi, char *title, struct animenucontext *menu);
  /* private data follows */
  struct animenuitem *next, *prev;
  struct animenucontext *parent;
  enum animenuitem_type type;
  char *title;
  char *command;  /* if type is animenuitem_command */
  struct animenucontext *submenu;  /* if type is animenuitem_submenu */
  struct osditemdata osddata;
};

struct animenucontext *animenu_create(struct animenucontext *parent, const char *filename);

void animenu_test();

#endif
