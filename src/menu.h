
/**
 * animenu - lirc menu system
 *
 *  copyright (c) 2009, 2012-2013 by Pete Beardmore <pete.beardmore@msn.com>
 *  copyright (c) 2001-2003 by Alastair M. Robinson <blackfive@fakenhamweb.co.uk>
 *
 *  licensed under GNU General Public License 2.0 or later
 *  some rights reserved. see COPYING, AUTHORS
 */

#ifndef animenuCONTEXT_H
#define animenuCONTEXT_H

#include "osd.h"

/* globals */
int debug;
const char *playall;

enum animenuitem_type {animenuitem_null, animenuitem_command, animenuitem_submenu, animenuitem_browse};

struct animenucontext {
  /* public functions */
  void (*dispose) (struct animenucontext *menu);
  void (*next) (struct animenucontext *menu);
  void (*prev) (struct animenucontext *menu);
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
  void (*select) (struct animenuitem *mi);
  int (*setitem) (struct animenuitem *mi, enum animenuitem_type type,
                  char *title, char *path, char *regex, char *command, int recurse);
  int (*setsubmenu) (struct animenuitem * mi, char *title, struct animenucontext *menu);
  /* private data follows */
  struct animenuitem *next, *prev;
  struct animenucontext *parent;
  enum animenuitem_type type;
  char *title;
  char *path;
  char *regex;
  char *command;
  int recurse;
  struct animenucontext *submenu;
  struct osditemdata osddata;
};

int animenu_browse(struct animenuitem *mi);
struct animenucontext *animenu_create(struct animenucontext *parent,
                                      enum animenuitem_type type, const char *filename);
struct animenuitem *animenuitem_create(struct animenucontext *menu);

void animenu_test();

#endif
