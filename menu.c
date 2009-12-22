
/* Menu.c - part of the animenu package, which is copyright (c) 2001
 * by Alastair M. Robinson.
 *
 * This program is released under the terms of the GNU Public License,
 * version 2 or later.  See the file 'COPYING' for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <regex.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>

#include "menu.h"

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#define _strncpy(A,B,C) strncpy(A,B,C), *(A+(C)-1)='\0'
#define _strncat(A,B,C) strncat(A,B,C), *(A+(C)-1)='\0'

static void animenuitem_dispose(struct animenuitem *mi);
static int animenuitem_setitem(struct animenuitem *mi, char *title, char *command);
static int animenuitem_setsubmenu(struct animenuitem *mi, char *title, struct animenucontext *submenu);
static void *animenuitem_thread(void *ud);
static void animenuitem_select(struct animenuitem *mi);
static void animenuitem_go(struct animenuitem *mi);
static void animenu_dispose(struct animenucontext *menu);
static void animenu_showcurrent(struct animenucontext *menu);
static void animenu_next(struct animenucontext *menu);
static void animenu_prev(struct animenucontext *menu);
static void animenu_show(struct animenucontext *menu);
static void animenu_hideframe(struct animenucontext *menu, int frame);
static void animenu_hide(struct animenucontext *menu);
static char *animenu_stripwhitespace(char *string);
static int animenu_readtriple(FILE *f, char *typebuf, char *titlebuf, char *cmdbuf);
void *animenu_idcallback(void *ud, struct osditemdata **osdid);
int animenu_genosd(struct animenucontext *menu);
void animenu_dump(struct animenucontext *menu);

static void animenuitem_dispose(struct animenuitem *mi) {
  if (mi) {
    if (mi->next)
      mi->next->prev = mi->prev;
    else
      mi->parent->lastitem = mi->prev;
    if (mi->prev)
      mi->prev->next = mi->next;
    else
      mi->parent->firstitem = mi->next;

    if (mi->title)
      free(mi->title);
    if (mi->command)
      free(mi->command);
    if (mi->submenu)
      mi->submenu->dispose(mi->submenu);

    free(mi);
  }
}

static int animenuitem_setitem(struct animenuitem *mi, char *title, char *command) {
  int success = TRUE;
  if (mi->title)
    free(mi->title);
  if (!(mi->title = strdup(title)))
    success = FALSE;

  mi->osddata.title = mi->title;

  if (mi->command)
    free(mi->command);
  if (!(mi->command = strdup(command)))
    success = FALSE;

  mi->type = animenuitem_command;
  if (mi->submenu)
    mi->submenu->dispose(mi->submenu);
  mi->submenu = NULL;
  return(success);
}

static int animenuitem_setsubmenu(struct animenuitem *mi, char *title, struct animenucontext *submenu) {
  int success = TRUE;
  if (mi->title)
    free(mi->command);
  if (!(mi->title = strdup(title)))
    success = FALSE;

  mi->osddata.title = mi->title;

  if (mi->command)
    free(mi->command);
  mi->command = NULL;

  mi->submenu = submenu;
  submenu->parent = mi->parent;
  mi->type = animenuitem_submenu;
  return(success);
}

static void *animenuitem_thread(void *ud) {
  char *cmd = (char *) ud;
  system(cmd);
  return(NULL);
}

static void animenuitem_go(struct animenuitem *mi) {
  /* hide the menu hierarchy */
  struct animenucontext *parent = NULL;
  parent = mi->parent;
  /* iterate back through the context/menu hierarchy to the root */
  while (parent->parent)
    parent = parent->parent;
  parent->hide(parent);

  /* execute the item's command */
  pthread_t thread;
  pthread_create(&thread, NULL, animenuitem_thread, mi->command);
}

struct animenuitem *animenuitem_create(struct animenucontext *menu) {
  struct animenuitem *mi;

  if (!(mi = malloc(sizeof(struct animenuitem))))
    return(NULL);

  mi->dispose = animenuitem_dispose;
  mi->go = animenuitem_go;
  mi->select = animenuitem_select;
  mi->setitem = animenuitem_setitem;
  mi->setsubmenu = animenuitem_setsubmenu;

  mi->title = mi->command = NULL;
  mi->submenu = NULL;

  mi->parent = menu;
  mi->next = NULL;
  mi->prev = NULL;

  if (!(menu->firstitem))
    menu->firstitem = mi;
  if (menu->lastitem) {
    menu->lastitem->next = mi;
    mi->prev = menu->lastitem;
  }
  menu->lastitem = mi;

  return(mi);
}

static void animenu_dispose(struct animenucontext *menu) {
  if (menu) {
    if (menu->osd)
      menu->osd->dispose(menu->osd);

    while (menu->firstitem)
      menu->firstitem->dispose(menu->firstitem);

    free(menu);
  }
}

static void animenu_showcurrent(struct animenucontext *menu) {
  int i = 0;
  struct animenuitem *item = menu->firstitem;
  while (item) {
    if (item == menu->currentitem) {
      menu->osd->showselected(menu->osd, i);
      return;
    }
    item = item->next;
    ++i;
  }
  menu->osd->showselected(menu->osd, -1);
}

static void animenu_next(struct animenucontext *menu) {
  struct animenuitem *item;
  int done = FALSE;
  if (!(menu->visible)) {
    menu->show(menu);
  }

  while (!(done)) {
    if ((item = menu->currentitem)) {
      if ((item->type == animenuitem_submenu) && (item->submenu->visible)) {
        menu = item->submenu;
        item = menu->currentitem;
      } else {
        menu->currentitem = item->next;
        animenu_showcurrent(menu);
        done = TRUE;
      }
    } else {
      menu->currentitem = menu->firstitem;
      animenu_showcurrent(menu);
      done = TRUE;
    }
  }
}

static void animenu_prev(struct animenucontext *menu) {
  struct animenuitem *item;
  int done = 0;
  if (!(menu->visible)) {
    menu->show(menu);
  }

  while (!(done)) {
    if ((item = menu->currentitem)) {
      if ((item->type == animenuitem_submenu) && (item->submenu->visible)) {
        menu = item->submenu;
        item = menu->currentitem;
      } else {
        menu->currentitem = item->prev;
        animenu_showcurrent(menu);
        done = 1;
      }
    } else {
      menu->currentitem = menu->lastitem;
      animenu_showcurrent(menu);
      done = 1;
    }
  }
}

static void animenuitem_select(struct animenuitem *mi) {
  if (mi->type == animenuitem_submenu) {
    mi->submenu->currentitem = mi->submenu->firstitem;
    mi->submenu->show(mi->submenu);
    animenu_showcurrent(mi->submenu);
  }
}

/* Routine to toggle visibility of the menus.
   The algorithm is as follows:
   If the menu is currently off-screen,
     we show it.
   If the menu is visible,
     we first check to see if its currently selected
     item is a submenu,
       and if so, whether the submenu is visible.
         If it is, we call this routine recursively
         on the submenu.
     If the submenu is invisible, or there isn't one,
       we hide the current menu instead.
*/
static void animenu_show(struct animenucontext *menu) {
  if ((menu) && (menu->osd)) {
    if (menu->visible) {
      struct animenuitem *item = menu->currentitem;
      if (item) {
        struct animenucontext *submenu = NULL;
        if (item->type == animenuitem_submenu)
          submenu = item->submenu;
        if (submenu && submenu->visible)
          submenu->show(submenu);
        else
          menu->hide(menu);
      } else
        menu->hide(menu);
    } else {
      menu->visible = TRUE;
      menu->osd->show(menu->osd);
    }
  }
}

static void animenu_hideframe(struct animenucontext *menu, int frame) {
  struct animenuitem *item = menu->firstitem;
  while (item) {
    if (item->submenu != NULL && item->submenu->visible)
      animenu_hideframe(item->submenu, frame);
    item = item->next;
  }
  if ((menu->visible) && (menu->osd)) {
    menu->osd->hideframe(menu->osd, frame);
    if (frame > 1950) {
      menu->visible = FALSE;
      menu->currentitem = FALSE;
    }
  }
}

extern int menuanimation;

static void animenu_hide(struct animenucontext *menu) {
  int frame;
  for (frame = 0; frame < OSD_MAXANIMFRAME; frame += 40) {
    animenu_hideframe(menu, frame);
    usleep(menuanimation);
  }
}

static char *animenu_stripwhitespace(char *str) {
  while (isspace(*str))
    ++str;
  while (isspace(*(str + strlen(str) - 1)))
    *(str + strlen(str) - 1) = '\0';
  return str;
}

static int animenu_readtriple(FILE *f, char *typebuf, char *titlebuf, char *cmdbuf) {
  do {
    if (!(fgets(typebuf, 256, f)))
      return(FALSE);
  } while (strlen(typebuf) < 2);
  typebuf[strlen(typebuf) - 1] = 0;

  do {
    if (!(fgets(titlebuf, 256, f)))
      return(FALSE);
  } while (strlen(titlebuf) < 2);
  titlebuf[strlen(titlebuf) - 1] = 0;

  do {
    if (!(fgets(cmdbuf, 256, f)))
      return(FALSE);
  } while (strlen(cmdbuf) < 2);
  cmdbuf[strlen(cmdbuf) - 1] = 0;

  return(TRUE);
}

void *animenu_idcallback(void *ud, struct osditemdata **osdid) {
  struct animenuitem *item = (struct animenuitem *) ud;
  if (item) {
    *osdid = &item->osddata;
    item = item->next;
  }
  return(item);
}

/* Generation of the OSD contexts.
   This is done in one operation after the menu tree has been
   created, because to extract position information from the
   parent OSD structure, the tree must be built from root to
   leaves; if each menu's OSD were built in the animenu_create call,
   the order of generation would be from leaf to root, so the parent
   geometry wouldn't be available. */

int animenu_genosd(struct animenucontext *menu) {
  int result = TRUE;
  struct animenuitem *item;
  struct osdcontext *parent = NULL;
  if (menu->parent)
    parent = menu->parent->osd;

  if (!(menu->osd = osd_create(parent, animenu_idcallback, menu->firstitem)))
    return(FALSE);
  item = menu->firstitem;
  while (item) {
    if (item->type == animenuitem_submenu) {
      result &= animenu_genosd(item->submenu);
    }
    item = item->next;
  }
  return(result);
}

int regexcmp(const char *s1, const char *s2) {
  regex_t *usrflt_regex = NULL;
  int regex_errcode = REG_NOMATCH;

  if ((s1) && (s2) && (usrflt_regex = (regex_t *) malloc(sizeof(regex_t))) &&
      !regcomp(usrflt_regex, s2, REG_EXTENDED | REG_ICASE | REG_NOSUB)) {
    regex_errcode = regexec(usrflt_regex, s1, 0, NULL, 0);
  }

  if (usrflt_regex)
    free(usrflt_regex);
  return regex_errcode;
}

static struct animenucontext *animenu_create_browse(struct animenucontext *parent,
                                                    const char *regexpath,
                                                    const char *INIT_path,
                                                    const char *cmd,
                                                    int recurse) {
  struct files {
    char file[PATH_MAX + 1];
    struct files *next;
  } *file_cur, *file_root = NULL, *file_prev = NULL;

  struct animenucontext *menu;
  DIR *d = NULL;
  struct stat statbuf;
  struct dirent *dirent;
  char cur[PATH_MAX + 1], *all_cmd, *title, path[PATH_MAX + 1];
  unsigned int size;
  struct animenuitem *item;

  if (!regexpath || *regexpath != '/')
    return NULL;
  if (!INIT_path) {
    _strncpy(path, regexpath, PATH_MAX);
    *strrchr(path, '/') = '\0';
  } else
    _strncpy(path, INIT_path, PATH_MAX);

  if (!(d = opendir(path)))
    return NULL;
  if (!(menu = malloc(sizeof(struct animenucontext))))
    return(NULL);
  memset(menu, 0, sizeof(struct animenucontext));

  /* function pointers */
  menu->dispose = animenu_dispose;
  menu->next = animenu_next;
  menu->prev = animenu_prev;
  menu->show = animenu_show;
  menu->showcurrent = animenu_showcurrent;
  menu->hide = animenu_hide;

  menu->firstitem = NULL;
  menu->currentitem = NULL;
  menu->parent = parent;
  menu->osd = NULL;

  for (*cur = '\0'; (dirent = readdir(d));) {
    if (!strcmp(dirent->d_name, "..") || !strcmp(dirent->d_name, "."))
      continue;

    snprintf(cur, PATH_MAX, "%s/%s", path, dirent->d_name);

    if (stat(cur, &statbuf) != -1) {
      if (S_ISDIR(statbuf.st_mode)) {
        if (recurse) {
          struct animenucontext *submenu;
          item = animenuitem_create(menu);
          if ((submenu = animenu_create_browse(menu, regexpath, cur, cmd, recurse))) {
            if (submenu->firstitem)
              item->setsubmenu(item, dirent->d_name, submenu);
            else {
              fprintf(stderr, "submenu empty - discarding\n");
              submenu->dispose(submenu);
              item->dispose(item);
            }
          } else {
            item->dispose(item);
            fprintf(stderr, "cannot create submenu\n");
          }
        }
      } else if (S_ISREG(statbuf.st_mode) || S_ISLNK(statbuf.st_mode)) {
        if (!regexcmp(cur, regexpath)) {
          if (!(file_cur = malloc(sizeof(struct files))))
            continue;
          if (file_root == NULL)
            file_root = file_cur;
          else
            file_prev->next = file_cur;
          _strncpy(file_cur->file, cur, PATH_MAX);
          file_cur->next = NULL;
          file_prev = file_cur;
        }
      }
    }
  }
  closedir(d);

  if (!*cur)
    return NULL;
  if (file_root) {
    for (size = strlen(cmd) + 1, file_cur = file_root; file_cur != NULL; file_cur = file_cur->next)
      size += strlen(file_cur->file) + 3;

    if (!(all_cmd = malloc(size + 1)))
      return NULL;
    _strncpy(all_cmd, cmd, size);

    for (file_cur = file_root; file_cur != NULL; file_cur = file_cur->next) {
      _strncat(all_cmd, " \"", size);
      _strncat(all_cmd, file_cur->file, size);
      _strncat(all_cmd, "\"", size);
    }

    item = animenuitem_create(menu);
    item->setitem(item, "Play all", all_cmd);
    for (file_cur = file_root; file_cur != NULL; file_cur = file_cur->next) {
      item = animenuitem_create(menu);
      snprintf(all_cmd, size, "%s \"%s\"", cmd, file_cur->file);
      title = strrchr(file_cur->file, '/');
      item->setitem(item, ++title, all_cmd);
    }
  }

  if (parent == NULL)
    /* generate the osd tree */
    animenu_genosd(menu);
  return(menu);
}

struct animenucontext *animenu_create(struct animenucontext *parent, const char *filename) {
  char fntemp[PATH_MAX + 1] = "";
  struct animenucontext *menu;
  FILE *f;
  char typebuf[256];
  char titlebuf[256];
  char cmdbuf[256];

  if (!(menu = malloc(sizeof(struct animenucontext))))
    return(NULL);
  memset(menu, 0, sizeof(struct animenucontext));

  menu->dispose = animenu_dispose;
  menu->next = animenu_next;
  menu->prev = animenu_prev;
  menu->show = animenu_show;
  menu->showcurrent = animenu_showcurrent;
  menu->hide = animenu_hide;

  menu->firstitem = NULL;
  menu->currentitem = NULL;
  menu->parent = parent;
  menu->osd = NULL;

  if (!(f = fopen(filename, "r"))) {
    snprintf(fntemp, PATH_MAX, "%s/.animenu/%s", getenv("HOME"), filename);
    if (!(f = fopen(fntemp, "r"))) {
      menu->dispose(menu);
      return(NULL);
    }
  }

  while (animenu_readtriple(f, typebuf, titlebuf, cmdbuf)) {
    struct animenuitem *item = animenuitem_create(menu);

    if (strcasecmp(animenu_stripwhitespace(typebuf), "item") == 0) {
      item->setitem(item, animenu_stripwhitespace(titlebuf), animenu_stripwhitespace(cmdbuf));
    } else if (strcasecmp(animenu_stripwhitespace(typebuf), "menu") == 0) {
      struct animenucontext *submenu;
      if ((submenu = animenu_create(menu, animenu_stripwhitespace(cmdbuf))))
        item->setsubmenu(item, animenu_stripwhitespace(titlebuf), submenu);
      else {
        item->dispose(item);
        fprintf(stderr, "cannot create submenu\n");
      }
    } else if (strncasecmp(animenu_stripwhitespace(typebuf), "browse", 6) == 0) {
      struct animenucontext *submenu;
      int recurse;
      char *path = animenu_stripwhitespace(typebuf + 6);
      if (strncasecmp(animenu_stripwhitespace(typebuf), "browse_recurse", 14) == 0) {
        recurse = 1;
        path = animenu_stripwhitespace(typebuf + 14);
      } else
        recurse = 0;
      if ((submenu = animenu_create_browse(menu, path, NULL, animenu_stripwhitespace(cmdbuf), recurse))) {
        if (submenu->firstitem)
          item->setsubmenu(item, animenu_stripwhitespace(titlebuf), submenu);
      } else {
        item->dispose(item);
        fprintf(stderr, "cannot create submenu\n");
      }
    } else
      fprintf(stderr, "unknown type!\n");
  }
  fclose(f);

  if (parent == NULL)
    /* generate the osd tree */
    animenu_genosd(menu);  
  return(menu);
}

void animenu_dump(struct animenucontext *menu) {
  static int tab = 0;
  struct animenuitem *item;
  int i;

  if (menu) {
    item = menu->firstitem;
    while (item) {
      for (i = 0; i < tab; ++i)
        printf("..");
      if (item->type == animenuitem_command)
        printf("[%s], [%s]\n", item->title, item->command);
      if (item->type == animenuitem_submenu) {
        ++tab;
        printf("[%s], submenu:\n", item->title);
        animenu_dump(item->submenu);
        --tab;
      }
      item = item->next;
    }
  }
}
