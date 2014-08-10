
/**
 * animenu - lirc menu system
 *
 *  copyright (c) 2009, 2012-2014 by Pete Beardmore <pete.beardmore@msn.com>
 *  copyright (c) 2001-2003 by Alastair M. Robinson <blackfive@fakenhamweb.co.uk>
 *
 *  licensed under GNU General Public License 2.0 or later
 *  some rights reserved. see COPYING, AUTHORS
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>

#include "menu.h"

/* globals */
int debug = 0;
const char *playall = "| play all |";

static void animenuitem_dispose(struct animenuitem *mi);
static int animenuitem_setitem(struct animenuitem *mi, enum animenuitem_type type,
                               char *title, char *path, char* regex, char *command, int recurse);
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
static int animenu_readmenufile(FILE *f, char ***item);
void *animenu_idcallback(void *ud, struct osditemdata **osdid);
int animenu_genosd(struct animenucontext *menu);
void animenu_dump(struct animenucontext *menu);
char *rx_start(char *s, char **first);
int rx_compare(const char *s1, const char *s2);

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
    if (mi->path)
      free(mi->path);
    if (mi->regex)
      free(mi->regex);
    if (mi->submenu)
      mi->submenu->dispose(mi->submenu);

    free(mi);
  }
}

static int animenuitem_setitem(struct animenuitem *mi, enum animenuitem_type type,
                               char *title, char *path, char* regex, char *command, int recurse) {
  int success = TRUE;
  if (mi->title)
    free(mi->title);
  if (title) {
    if (!(mi->title = strdup(title)))
      success = FALSE;
  } else
    mi->title = "";
  mi->osddata.title = mi->title;

  if (mi->command)
    free(mi->command);
  if (command) {
    if (!(mi->command = strdup(command)))
      success = FALSE;
  }

  mi->type = type;
  mi->path = path;
  if (mi->submenu)
    mi->submenu->dispose(mi->submenu);
  mi->submenu = NULL;

  if (type == animenuitem_browse) {
    if (regex) {
      mi->regex = malloc(BUFSIZE + 1);
      strncpy(mi->regex, regex, BUFSIZE + 1);
    } else
      success = FALSE;
  }
  mi->recurse = recurse;

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

  mi->title = NULL;
  mi->path = NULL;
  mi->regex = NULL;
  mi->command = NULL;

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
      menu->osd->dispose(menu->osd, menu->menuanimation);
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
  } else if (mi->type == animenuitem_browse) {
    if (animenu_browse(mi)) {
      mi->osddata.title = mi->title;
      mi->submenu->currentitem = mi->submenu->firstitem;
      mi->submenu->show(mi->submenu);
      animenu_showcurrent(mi->submenu);
    } else {
      fprintf(stderr, "cannot create browse menu\n");
    }
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
      menu->osd->show(menu->osd, menu->menuanimation);
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

static void animenu_hide(struct animenucontext *menu) {
  int frame;
  for (frame = 0; frame < OSD_MAXANIMFRAME; frame += 40) {
    animenu_hideframe(menu, frame);
    usleep(menu->menuanimation);
  }
}

static char *animenu_stripwhitespace(char *str) {
  while (isspace(*str))
    ++str;
  while (isspace(*(str + strlen(str) - 1)))
    *(str + strlen(str) - 1) = '\0';
  return str;
}

/* read a structure with controlled by whitespace/indentation */
#define _freecfg(A) free((void*)A[0]),free((void*)A)
static int animenu_readmenufile(FILE *f, char ***item) {

  /* append valid lines to a dynamic buff buffer, determining the
   * max length and number of lines for the current config item.
   * allocate memory and then split when hitting the next item or eof
   * caller must free the memory allocation, and can use _freecfg
   */

  int i;
  int idx = 0; /* item index */
  int maxlen = 0; /* max length seen */
  int maxlenline = BUFSIZE + 1; /* max line length */
  int len;
  char s[maxlenline];
  char *s2;
  char *b = malloc(maxlenline); /* flattened item buffer */
  const char* rx_comment = "^\\s*#.*";
  const char* rx_empty = "^\\s*$";
  const char* rx_nested = "^\\s+.*";

  struct animenu_options* options = get_options();

  /* read item to buffer*/
  b[0] = '\0';
  do {
    s[0] = '\0';
    if (!(fgets(s, maxlenline - 1, f))) {
      /* eof */
      if (idx > 0)
        /* process final item */
        break;
      else {
        /* nothing left to read */
        free(b);
        return(FALSE);
      }
    }
    if (rx_compare(s, rx_empty) == 0 ||
        rx_compare(s, rx_comment) == 0)
      continue;
    if (rx_compare(s, rx_nested) != 0) {
      if (idx > 0) {
        fseek(f, -strlen(s), SEEK_CUR);
        break;
      }
    }
    s2 = animenu_stripwhitespace(s);
    len = strlen(s2);
    maxlen = _max(len, maxlen);
    if (idx == 0) {
      /* force lower case 'type' */
      for (i = 0; b[i]; i++)
        b[i] = tolower(b[i]);
    }
    if (strlen(s) + len + 1 > sizeof(s)) {
      /* enlarge buffer */
      char *b2 = strdup(b);
      b = realloc(b, sizeof(b) + maxlenline);
      strcpy(b, b2);
      free(b2);
    }
    sprintf(b + strlen(b), "%s\n", s2);
    idx++;
  } while (1);

  /* create item from flat buffer */
  (*item) = malloc(idx * sizeof(char*));
  (*item)[0] = malloc(idx * (maxlen + 1) * sizeof(char));
  int pos = 0;
  char *n = NULL;
  if (options->debug > 0)
    fprintf(stderr, "parsed config item:\n");
  for (i = 0; pos < strlen(b); i++) {
    (*item)[i] = (*item)[0] + i * (maxlen + 1);
    n = strchr(b + pos, '\n');
    _strncpy((*item)[i], b + pos, n - (b + pos) + 1);
    pos += n - (b + pos) + 1;
    if (options->debug > 0)
      fprintf(stderr, "[%d] %s\n", i, (*item)[i]);
  }
  free(b);

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

int rx_compare(const char *s1, const char *s2) {
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

char *rx_start(char *s, char **first) {
  char *s2, *m, *m2;
  char rx[] = "*.[]()|^$?+";
  char rxc[2];
  s2 = strdup(s);
  int l = 0;
  while (l < strlen(rx) - 1) {
    sprintf(rxc, "\\%c", rx[l]);
    /* locate first occurence of rx char and if it's not
     * prefixed with '\', truncate the buffer */
    while ((m = strstr(s2, rxc + 1))) {
      m2 = strstr(s2, rxc);
      if (m && (!m2 || (m2 && m2 + 1 != m)))
        *m = '\0';
    }
    l++;
  }
  if (strlen(s2) != strlen(s))
    *first = s + strlen(s2) + 1;
  else
    *first = NULL;
  free(s2);
  return(*first);
}

/* set browse content */
int animenu_browse(struct animenuitem *mi) {
  int success = TRUE;

  struct files {
    char file[BUFSIZE + 1];
    struct files *next;
  } *file_cur, *file_root = NULL, *file_prev = NULL;

  struct animenucontext *menu;
  struct animenuitem *mi2;
  DIR *d = NULL;
  struct stat statbuf;
  struct dirent *dirent;
  /* note that the regexp is not kept seperately */
  char cur[BUFSIZE + 1], *all_cmd, *title, path[BUFSIZE + 1];
  unsigned int size;

  const struct animenu_options* options = get_options();

  if (mi->path)
    /* path already set, so use that */
    _strncpy(path, mi->path, BUFSIZE + 1);
  else {
    if (mi->regex == NULL) {
      fprintf(stderr, "empty path");
      return(FALSE);
    } else if (*mi->regex != '/') {
      fprintf(stderr, "invalid path '%s', ensure it is fully qualified", mi->regex);
      return(FALSE);
    }
    /* set base path */
    _strncpy(path, mi->regex, BUFSIZE + 1);
    char *rxs;
    if (rx_start(path, &rxs))
      *rxs = '\0';
    /* return pointer to last occurence of char in array
     * and use this to terminate the string */
    *strrchr(path, '/') = '\0';
  }

  if (!(d = opendir(path))) {
    fprintf(stderr, "invalid path '%s'", path);
    return(FALSE);
  }
  /* fail if we can't allocate enough memory for the menu struct (known size) */
  if (!(menu = malloc(sizeof(struct animenucontext))))
    return(FALSE);
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
  menu->parent = mi->parent;
  menu->osd = NULL;
  menu->menuanimation = options->menuanimation;

  for (*cur = '\0'; (dirent = readdir(d));) {
    /* use 'back' navigation to move up through the file hierarchy instead */
    if (!strcmp(dirent->d_name, "..") || !strcmp(dirent->d_name, "."))
      continue;

    /* set 'cur' as the current full path for consideration */
    snprintf(cur, BUFSIZE + 1, "%s/%s", path, dirent->d_name);

    if (stat(cur, &statbuf) != -1) {
      if (S_ISREG(statbuf.st_mode)) {
        if (!rx_compare(cur, mi->regex)) {
          /* match the regex path */
          if (!(file_cur = malloc(sizeof(struct files))))
            continue;
          if (file_root == NULL)
            file_root = file_cur;
          else
            file_prev->next = file_cur;

          /* copy the path into the file struct's file variable */
          _strncpy(file_cur->file, cur, BUFSIZE + 1);
          file_cur->next = NULL;
          file_prev = file_cur;
        }
      } else if (S_ISDIR(statbuf.st_mode) || S_ISLNK(statbuf.st_mode)) {
        if (mi->recurse)
          ; /* no-op */
        else {
          /* add the dir/link regardless of match */
          if (!(file_cur = malloc(sizeof(struct files))))
            continue;
          if (file_root == NULL)
            file_root = file_cur;
          else
            file_prev->next = file_cur;
          /* copy the path into our struct's file variable */
          _strncpy(file_cur->file, cur, BUFSIZE + 1);
          file_cur->next = NULL;
          file_prev = file_cur;
        }
      }
    }
  }
  closedir(d);

  /* create the browse menu's items
   * either 'command' items or 'browse' (menu) items */
  if (!*cur)
    return(FALSE);
  if (file_root) {
    /* get the final size for the command + args string */
    for (size = strlen(mi->command) + 1, file_cur = file_root; file_cur != NULL; file_cur = file_cur->next)
      size += strlen(file_cur->file) + 3;

    if (!(all_cmd = malloc(size + 1)))
      return(FALSE);
    _strncpy(all_cmd, mi->command, size);

    for (file_cur = file_root; file_cur != NULL; file_cur = file_cur->next) {
      _strncat(all_cmd, " \"", size);
      _strncat(all_cmd, file_cur->file, size);
      _strncat(all_cmd, "\"", size);
    }

    mi2 = animenuitem_create(menu);
    mi2->setitem(mi2, animenuitem_command, (char*)playall, NULL, NULL, all_cmd, 0);
    for (file_cur = file_root; file_cur != NULL; file_cur = file_cur->next) {
      title = strrchr(file_cur->file, '/');
      stat(file_cur->file, &statbuf);
      if (S_ISREG(statbuf.st_mode)) {
        /* create command item */
        mi2 = animenuitem_create(menu);
        snprintf(all_cmd, size, "%s \"%s\"", mi->command, file_cur->file);
        mi2->setitem(mi2, animenuitem_command, ++title, NULL, NULL, all_cmd, 0);
      } else if (S_ISDIR(statbuf.st_mode) || S_ISLNK(statbuf.st_mode)) {
        /* create submenu item */
        mi2 = animenuitem_create(menu);
        mi2->setitem(mi2, animenuitem_browse, title, file_cur->file, mi->regex, mi->command, mi->recurse);
      }
    }
  } else {
    /* create empty item for empty menu */
    mi2 = animenuitem_create(menu);
    mi2->setitem(mi2, animenuitem_null, NULL, NULL, NULL, NULL, 0);
  }

  /* generate the osd tree */
  animenu_genosd(menu);

  /* attach new sub menu */
  if (menu->firstitem != NULL) {
    mi->submenu = menu;
    success = TRUE;
  }

  return(success);
}

struct animenucontext *animenu_create(struct animenucontext *parent, enum animenuitem_type type, const char *filename) {
  char file[BUFSIZE + 1] = "";
  struct animenucontext *menu;
  FILE *f;
  char **itemcfg;
  char typebuf[BUFSIZE + 1];
  char titlebuf[BUFSIZE + 1];
  char cmdbuf[BUFSIZE + 1];

  struct animenu_options* options = get_options();

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
  menu->menuanimation = options->menuanimation;

  if (!(f = fopen(filename, "r"))) {
    /* cannot open file */
    menu->dispose(menu);
    return(NULL);
  }

  if (type == animenuitem_submenu) {
    while (animenu_readmenufile(f, &itemcfg)) {
      _strncpy(typebuf, itemcfg[0], strlen(itemcfg[0]) + 1);
      _strncpy(titlebuf, itemcfg[1], strlen(itemcfg[1]) + 1);
      _strncpy(cmdbuf, itemcfg[2], strlen(itemcfg[2]) + 1);
      /* create an item and adds it to the menu */
      struct animenuitem *item = animenuitem_create(menu);

      if (strcasecmp(animenu_stripwhitespace(typebuf),"item") == 0)
        /* set up a command item */
        item->setitem(item, animenuitem_command, animenu_stripwhitespace(titlebuf),
                       NULL, NULL, animenu_stripwhitespace(cmdbuf), 0);
      else if (strcasecmp(animenu_stripwhitespace(typebuf), "menu") == 0) {
        /* set up a submenu */
        snprintf(file, BUFSIZE + 1, "%s/.animenu/%s", getenv("HOME"), animenu_stripwhitespace(cmdbuf));
        struct animenucontext *submenu;
        if ((submenu = animenu_create(menu, animenuitem_submenu, file)))
          item->setsubmenu(item, animenu_stripwhitespace(titlebuf), submenu);
        else {
          item->dispose(item);
          fprintf(stderr, "cannot create submenu\n");
        }
      } else if (strncasecmp(animenu_stripwhitespace(typebuf), "browse", 6) == 0) {
        int recurse;
        char *path;
        if (strncasecmp(animenu_stripwhitespace(typebuf), "browse_recurse", 14) == 0) {
          /* return pointer 14 chars beyond the start of the buffer, thus the path */
          path = animenu_stripwhitespace(typebuf + 14);
          recurse = 1;
        } else {
          path = animenu_stripwhitespace(typebuf + 6);
          recurse = 0;
        }
        /* set up a browse item
         * initially the path is the regular expression */
        item->setitem(item, animenuitem_browse, animenu_stripwhitespace(titlebuf),
                      NULL, path, animenu_stripwhitespace(cmdbuf), recurse);
      } else {
        item->dispose(item);
        fprintf(stderr, "cannot create submenu\n");
      }
      /* clean up config item */
      _freecfg(itemcfg);
    } /* end while */
    fclose(f);
  } else
    fprintf(stderr, "unknown type!\n");

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
