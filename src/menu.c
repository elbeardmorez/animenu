
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

#define _freecfg(A) free((void*)A[0]),free((void*)A)

/* globals */
const char *playall = "| play all |";

struct animenuitem *animenu_createitem(enum animenuitem_type type,
                                              char *title, char *path, char *regex,
                                              char *command, int recurse);
struct animenucontext *animenu_createmenu(const char *path);
struct animenucontext *animenu_createfilesystem(char *path, char *regex,
                                                       char *command, int recurse);
int animenu_additem(struct animenucontext *menu, struct animenuitem *item);

void animenu_disposeitem(struct animenuitem *mi);
void animenu_disposemenu(struct animenucontext *menu);

int animenu_genosd(struct animenucontext *menu);
void animenu_show(struct animenucontext *menu);
void animenu_showcurrent(struct animenucontext *menu);
void animenu_hide(struct animenucontext *menu);
void animenu_hideframe(struct animenucontext *menu, int frame);

void animenu_go(struct animenuitem *mi);
void animenu_select(struct animenuitem *mi);
void animenu_prev(struct animenucontext *menu);
void animenu_next(struct animenucontext *menu);

void *animenu_thread(void *ud);
void *animenu_idcallback(void *ud, struct osditemdata **osdid);
int animenu_readmenufile(FILE *f, char ***item);
char *animenu_stripwhitespace(char *string);

char *rx_start(char *s, char **first);
int rx_compare(const char *s1, const char *s2);

int animenu_initialise(struct animenucontext **rootmenu, const char *path) {
  int success = TRUE;

  /* create root menu */
  if ((*rootmenu = animenu_createmenu(path)))
    /* generate osd frames */
    animenu_genosd(*rootmenu);
  else {
    success = FALSE;
    (*rootmenu)->dispose(*rootmenu);
  }

  return(success);
}

void animenu_dump(struct animenucontext *menu) {
  int tab = 0;
  struct animenuitem *item;
  int i;

  if (menu) {
    item = menu->firstitem;
    while (item) {
      for (i = 0; i < tab; ++i)
        printf("..");
      if (item->type == animenuitem_command)
        printf("[%s], [%s]\n", item->title, item->command);
      if (item->type == animenuitem_menu) {
        ++tab;
        printf("[%s], menu:\n", item->title);
        animenu_dump(item->menu);
        --tab;
      }
      item = item->next;
    }
  }
}

/* create item */
struct animenuitem *animenu_createitem(enum animenuitem_type type, char *title,
                                       char *path, char* regex, char *command, int recurse) {
  struct animenuitem *item;
  if (!(item = malloc(sizeof(struct animenuitem))))
    return(NULL);

  item->go = animenu_go;
  item->select = animenu_select;
  item->dispose = animenu_disposeitem;

  item->title = NULL;
  item->path = NULL;
  item->regex = NULL;
  item->command = NULL;
  item->menu = NULL;

  item->next = NULL;
  item->prev = NULL;

  item->type = type;
  if (title) {
    if (!(item->title = strdup(title))) {
      item->dispose(item);
      return(NULL);
    }
  }
  item->osddata.title = item->title;

  if (path) {
    if (!(item->path = strdup(path))) {
      item->dispose(item);
      return(NULL);
    }
  }
  if (regex) {
    if (!(item->regex = strdup(regex))) {
      item->dispose(item);
      return(NULL);
    }
  }
  if (command) {
    if (!(item->command = strdup(command))) {
      item->dispose(item);
      return(NULL);
    }
  }

  /* create menu */
  if (type == animenuitem_menu) {
    struct animenucontext *menu;
    if ((menu = animenu_createmenu(path))) {
      /* connect new sub-menu to its item */
      item->menu = menu;
      /* generate osd frames */
      animenu_genosd(menu);
    } else {
      menu->dispose(menu);
      item->dispose(item);
      return(NULL);
    }
  }

  /* create stub for dynamic filesystem menu */
  if (type == animenuitem_filesystem)
    item->recurse = recurse;

  return(item);
}

/* create menu content */
struct animenucontext *animenu_createmenu(const char *path) {

  struct animenucontext *menu;
  struct animenuitem *item;
  char *type, *title;
  char **itemcfg;
  char pathbase[PATH_MAX + 1];
  FILE *f;

  struct animenu_options* options = get_options();

  if (!(menu = malloc(sizeof(struct animenucontext))))
    return(NULL);
  memset(menu, 0, sizeof(struct animenucontext));

  menu->dispose = animenu_disposemenu;
  menu->next = animenu_next;
  menu->prev = animenu_prev;
  menu->show = animenu_show;
  menu->showcurrent = animenu_showcurrent;
  menu->hide = animenu_hide;
  menu->additem = animenu_additem;

  menu->firstitem = NULL;
  menu->currentitem = NULL;
  menu->osd = NULL;
  menu->menuanimation = options->menuanimation;

  if (!(f = fopen(path, "r"))) {
    /* cannot open file */
    menu->dispose(menu);
    return(NULL);
  }

  while (animenu_readmenufile(f, &itemcfg)) {
    /* create an item */
    type = itemcfg[0];
    title = itemcfg[1];
    if (strcmp(type, "item") == 0) {
      /* set up a command item */
      if ((item = animenu_createitem(animenuitem_command, title, NULL, NULL, itemcfg[2], 0)))
        menu->additem(menu, item);
      else
        fprintf(stderr, "cannot create item '%s'\n", title);
    } else if (strcmp(type, "menu") == 0) {
      /* set up a menu item */
      snprintf(pathbase, PATH_MAX, "%s/.animenu/%s", getenv("HOME"), itemcfg[2]);
      if ((item = animenu_createitem(animenuitem_menu, title, pathbase, NULL, NULL, 0)))
        menu->additem(menu, item);
      else
        fprintf(stderr, "cannot create sub menu '%s' from '%s'\n", title, itemcfg[2]);
    } else if (strncasecmp(type, "browse", 6) == 0) {
      /* set up a browse item */
      int recurse;
      if (strncasecmp(type, "browse_recurse", 14) == 0) {
        /* return pointer 14 chars beyond the start of the buffer, thus the path */
        strcpy(pathbase, animenu_stripwhitespace(type + 14));
        recurse = 1;
      } else {
        strcpy(pathbase, animenu_stripwhitespace(type + 6));
        recurse = 0;
      }
      /* the path is later derived from the regular expression */
      if ((item = animenu_createitem(animenuitem_filesystem, title, NULL, pathbase, itemcfg[2], recurse)))
        menu->additem(menu, item);
      else
        fprintf(stderr, "cannot create filesystem menu '%s' from '%s'\n", title, itemcfg[2]);
    } else
      fprintf(stderr, "skipping '%s', unknown type '%s'!\n", title, type);
    /* clean up config item */
    _freecfg(itemcfg);
  } /* end while */
  fclose(f);

  return(menu);
}

/* create filesystem menu content */
struct animenucontext *animenu_createfilesystem(char *path, char *regex,
                                                       char *command, int recurse) {

  struct animenucontext *menu;
  struct animenuitem *item;
  DIR *d = NULL;
  struct stat statbuf;
  struct dirent *dirent;
  char *title, *commandall;
  char pathbase[BUFSIZE + 1], pathcur[BUFSIZE + 1];
  unsigned int size;
  struct files {
    char file[PATH_MAX + 1];
    struct files *next;
  } *filecur, *fileroot = NULL, *fileprev = NULL;

  if (path)
    /* path already set, so use that */
    _strncpy(pathbase, path, BUFSIZE + 1);
  else {
    if (regex == NULL) {
      fprintf(stderr, "cannot set base path");
      return(FALSE);
    } else if (*regex != '/') {
      fprintf(stderr, "cannot set base path from '%s', ensure it is fully qualified", regex);
      return(FALSE);
    }
    /* set base path */
    _strncpy(pathbase, regex, BUFSIZE + 1);
    char *rxs;
    if (rx_start(pathbase, &rxs))
      *rxs = '\0';
    /* return pointer to last occurence of char in array
     * and use this to terminate the string */
    *strrchr(pathbase, '/') = '\0';
  }

  if (!(d = opendir(pathbase))) {
    fprintf(stderr, "invalid base path '%s'", pathbase);
    return(FALSE);
  }
  /* fail if we can't allocate enough memory for the menu struct (known size) */
  if (!(menu = malloc(sizeof(struct animenucontext))))
    return(FALSE);
  memset(menu, 0, sizeof(struct animenucontext));

  /* function pointers */
  menu->dispose = animenu_disposemenu;
  menu->next = animenu_next;
  menu->prev = animenu_prev;
  menu->show = animenu_show;
  menu->showcurrent = animenu_showcurrent;
  menu->hide = animenu_hide;
  menu->additem = animenu_additem;

  menu->firstitem = NULL;
  menu->currentitem = NULL;
  menu->osd = NULL;

  for (*pathcur = '\0'; (dirent = readdir(d));) {
    /* use 'back' navigation to move up through the file hierarchy instead */
    if (!strcmp(dirent->d_name, "..") || !strcmp(dirent->d_name, "."))
      continue;

    /* set 'cur' as the current full path for consideration */
    snprintf(pathcur, BUFSIZE + 1, "%s/%s", pathbase, dirent->d_name);

    if (stat(pathcur, &statbuf) != -1) {
      if (S_ISREG(statbuf.st_mode)) {
        if (!rx_compare(pathcur, regex)) {
          /* match the regex path */
          if (!(filecur = malloc(sizeof(struct files))))
            continue;
          if (fileroot == NULL)
            fileroot = filecur;
          else
            fileprev->next = filecur;

          /* copy the path into the file struct's file variable */
          _strncpy(filecur->file, pathcur, BUFSIZE + 1);
          filecur->next = NULL;
          fileprev = filecur;
        }
      } else if (S_ISDIR(statbuf.st_mode) || S_ISLNK(statbuf.st_mode)) {
        if (recurse)
          ; /* no-op */
        else {
          /* add the dir/link regardless of match */
          if (!(filecur = malloc(sizeof(struct files))))
            continue;
          if (fileroot == NULL)
            fileroot = filecur;
          else
            fileprev->next = filecur;
          /* copy the path into our struct's file variable */
          _strncpy(filecur->file, pathcur, BUFSIZE + 1);
          filecur->next = NULL;
          fileprev = filecur;
        }
      }
    }
  }
  closedir(d);

  /* create the browse menu's items
   * either 'command' items or 'browse' (menu) items */
  if (!*pathcur) {
    menu->dispose(menu);
    return(FALSE);
  }
  if (fileroot) {
    /* get the final size for the command + args string */
    for (size = strlen(command) + 1, filecur = fileroot; filecur != NULL; filecur = filecur->next)
      size += strlen(filecur->file) + 3;

    if (!(commandall = malloc(size + 1))) {
      menu->dispose(menu);
      return(FALSE);
    }
    _strncpy(commandall, command, size);

    for (filecur = fileroot; filecur != NULL; filecur = filecur->next) {
      _strncat(commandall, " \"", size);
      _strncat(commandall, filecur->file, size);
      _strncat(commandall, "\"", size);
    }

    item = animenu_createitem(animenuitem_command, (char*)playall, NULL, NULL, commandall, 0);
    menu->additem(menu, item);
    for (filecur = fileroot; filecur != NULL; filecur = filecur->next) {
      title = strrchr(filecur->file, '/');
      stat(filecur->file, &statbuf);
      if (S_ISREG(statbuf.st_mode)) {
        /* create command item */
        snprintf(commandall, size, "%s \"%s\"", command, filecur->file);
        item = animenu_createitem(animenuitem_command, ++title, NULL, NULL, commandall, 0);
        menu->additem(menu, item);
      } else if (S_ISDIR(statbuf.st_mode) || S_ISLNK(statbuf.st_mode)) {
        /* create menu item */
        item = animenu_createitem(animenuitem_filesystem, title, filecur->file, regex, command, recurse);
        menu->additem(menu, item);
      }
    }
  } else {
    /* create empty item for empty menu */
    item = animenu_createitem(animenuitem_null, NULL, NULL, NULL, NULL, 0);
    menu->additem(menu, item);
  }

  return(menu);
}

int animenu_additem(struct animenucontext *menu, struct animenuitem *item) {
  int success = TRUE;

  item->parent = menu;
  if (!(menu->firstitem))
    menu->firstitem = item;
  if (menu->lastitem) {
    menu->lastitem->next = item;
    item->prev = menu->lastitem;
  }
  menu->lastitem = item;
  if (item->menu)
    item->menu->parent = menu;

  return(success);
}

void animenu_disposeitem(struct animenuitem *mi) {
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
    if (mi->menu)
      mi->menu->dispose(mi->menu);

    free(mi);
  }
}

void animenu_disposemenu(struct animenucontext *menu) {
  if (menu) {
    if (menu->osd)
      menu->osd->dispose(menu->osd, menu->menuanimation);
    while (menu->firstitem)
      menu->firstitem->dispose(menu->firstitem);
    free(menu);
  }
}

/*
 recursive generation of the OSD contexts
 called after static menu tree creation and whenever a filesystem
 menu is created
 if each menu's OSD was built in the animenu_createmenu() call
 the order of generation would be from leaf to root, so the parent
 geometry wouldn't be available
*/
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
    if (item->type == animenuitem_menu) {
      result &= animenu_genosd(item->menu);
    }
    item = item->next;
   }
  return(result);
}

void animenu_show(struct animenucontext *menu) {
  if ((menu) && (menu->osd)) {
    if (menu->visible) {
      struct animenuitem *item = menu->currentitem;
      if (item) {
        struct animenucontext *submenu = NULL;
        if (item->type == animenuitem_menu)
          submenu = item->menu;
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

void animenu_showcurrent(struct animenucontext *menu) {
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

void animenu_hide(struct animenucontext *menu) {
  int frame;
  for (frame = 0; frame < OSD_MAXANIMFRAME; frame += 40) {
    animenu_hideframe(menu, frame);
    usleep(menu->menuanimation);
  }
}

void animenu_hideframe(struct animenucontext *menu, int frame) {
  struct animenuitem *item = menu->firstitem;
  while (item) {
    if (item->menu != NULL && item->menu->visible)
      animenu_hideframe(item->menu, frame);
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

void animenu_go(struct animenuitem *mi) {
  /* hide the menu hierarchy */
  struct animenucontext *parent = NULL;
  parent = mi->parent;
  /* iterate back through the context/menu hierarchy to the root */
  while (parent->parent)
    parent = parent->parent;
  parent->hide(parent);

  /* execute the item's command */
  pthread_t thread;
  pthread_create(&thread, NULL, animenu_thread, mi->command);
}

void animenu_select(struct animenuitem *mi) {
  if (mi->type == animenuitem_menu) {
    mi->menu->currentitem = mi->menu->firstitem;
    mi->menu->show(mi->menu);
    animenu_showcurrent(mi->menu);
  } else if (mi->type == animenuitem_filesystem) {
    /* create dynamic filesystem item content */
    struct animenucontext *menu;
    if ((menu = animenu_createfilesystem(mi->path, mi->regex, mi->command, mi->recurse))) {
      /* attach new sub menu */
      mi->menu = menu;
      mi->osddata.title = mi->title;
      menu->parent = mi->parent;
      /* generate osd frames */
      animenu_genosd(menu);
      /* select menu */
      mi->menu->currentitem = mi->menu->firstitem;
      mi->menu->show(mi->menu);
      animenu_showcurrent(mi->menu);
    } else {
      fprintf(stderr, "cannot create filesystem menu\n");
    }
  }
}

void animenu_prev(struct animenucontext *menu) {
  struct animenuitem *item;
  int done = FALSE;
  if (!(menu->visible)) {
    menu->show(menu);
  }

  while (!(done)) {
    if ((item = menu->currentitem)) {
      if ((item->type == animenuitem_menu) && (item->menu->visible)) {
        menu = item->menu;
        item = menu->currentitem;
      } else {
        menu->currentitem = item->prev;
        animenu_showcurrent(menu);
        done = TRUE;
      }
    } else {
      menu->currentitem = menu->lastitem;
      animenu_showcurrent(menu);
      done = TRUE;
    }
  }
}

void animenu_next(struct animenucontext *menu) {
  struct animenuitem *item;
  int done = FALSE;
  if (!(menu->visible)) {
    menu->show(menu);
  }

  while (!(done)) {
    if ((item = menu->currentitem)) {
      if ((item->type == animenuitem_menu) && (item->menu->visible)) {
        menu = item->menu;
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

void *animenu_thread(void *ud) {
  char *cmd = (char *) ud;
  system(cmd);
  return(NULL);
}

void *animenu_idcallback(void *ud, struct osditemdata **osdid) {
  struct animenuitem *item = (struct animenuitem *) ud;
  if (item) {
    *osdid = &item->osddata;
    item = item->next;
  }
  return(item);
}

char *animenu_stripwhitespace(char *str) {
  while (isspace(*str))
    ++str;
  while (isspace(*(str + strlen(str) - 1)))
    *(str + strlen(str) - 1) = '\0';
  return str;
}

/* read a structure with controlled by whitespace/indentation */
int animenu_readmenufile(FILE *f, char ***item) {

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
