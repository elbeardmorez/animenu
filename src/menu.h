
/**
 * animenu - lirc menu system
 *
 *  copyright (c) 2009, 2012-2014 by Pete Beardmore <pete.beardmore@msn.com>
 *  copyright (c) 2001-2003 by Alastair M. Robinson <blackfive@fakenhamweb.co.uk>
 *
 *  licensed under GNU General Public License 2.0 or later
 *  some rights reserved. see COPYING, AUTHORS
 */

#ifndef ANIMENU_MENU_H
#define ANIMENU_MENU_H

#ifndef ANIMENU_H
#include "animenu.h"
#endif
#include "osd.h"

/* globals */
const char *playall;

enum animenuitem_type {animenuitem_null, animenuitem_command, animenuitem_menu, animenuitem_filesystem};

struct animenuitem {
  /* public functions */
  void (*dispose) (struct animenuitem *mi);
  void (*go) (struct animenuitem *mi);
  void (*select) (struct animenuitem *mi);
  /* private data follows */
  struct animenuitem *next, *prev;
  struct animenucontext *parent;
  enum animenuitem_type type;
  char *title;
  char *path;
  char *regex;
  char *command;
  int recurse;
  struct animenucontext *menu;
  struct osditemdata osddata;
};

struct animenucontext {
  /* public functions */
  void (*dispose) (struct animenucontext *menu);
  void (*next) (struct animenucontext *menu);
  void (*prev) (struct animenucontext *menu);
  void (*show) (struct animenucontext *menu);
  void (*showcurrent) (struct animenucontext *menu);
  void (*hide) (struct animenucontext *menu);
  int (*additem) (struct animenucontext *menu, struct animenuitem *mi);
  /* private data */
  struct animenuitem *firstitem, *lastitem;
  struct animenuitem *currentitem;
  struct animenucontext *parent;
  struct osdcontext *osd;
  int menuanimation;
  int visible;
};

int animenu_initialise(struct animenucontext **rootmenu, const char *filename);
void animenu_dump(struct animenucontext *menu);

#endif
