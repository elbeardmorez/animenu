
/**
 * animenu - lirc menu system
 *
 *  copyright (c) 2009, 2012-2013 by Pete Beardmore <pete.beardmore@msn.com>
 *  copyright (c) 2001-2003 by Alastair M. Robinson <blackfive@fakenhamweb.co.uk>
 *
 *  licensed under GNU General Public License 2.0 or later
 *  some rights reserved. see COPYING, AUTHORS
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <lirc/lirc_client.h>

#include "animenu.h"
#include "osd.h"
#include "menu.h"
#include "options.h"


enum command_ids {id_null, id_show, id_next, id_prev, id_select, id_back, id_forward};

struct lirc_command {
  int id;
  const char *name;
  int args, argsread;
  int value1, value2, value3;
};
struct lirc_command lirc_commands[] = {
  {id_show, "show", 0, 0, 0, 0, 0},
  {id_next, "next", 0, 0, 0, 0, 0},
  {id_prev, "prev", 0, 0, 0, 0, 0},
  {id_select, "select", 0, 0, 0, 0, 0},
  {id_back, "back", 0, 0, 0, 0, 0},
  {id_forward, "forward", 0, 0, 0, 0, 0},
  {0, NULL, 0, 0, 0, 0, 0}
};

/* globals */
static struct animenucontext *rootmenu;
static struct animenucontext *currentmenu;
static int alarm_pending;

void sigalarm_handler(int junk) {
  if (alarm_pending) {
    alarm_pending = 0;
    rootmenu->hide(rootmenu);
  }
}

struct lirc_command * parse_codes(struct lirc_command* cmds, const char *cmd) {
  struct lirc_command *c = NULL;
  char cmdpart[32];
  int i;
  if (strlen(cmd) > 31) {
    printf("error: lirc code line too long\n");
    return(NULL);
  }
  if (1 == sscanf(cmd, "%s", cmdpart)) {
    i = 0;
    while (cmds[i].name) {
      if (0 == strcasecmp(cmds[i].name, cmdpart)) {
        c = &cmds[i];
        c->value1 = c->value2 = c->value3 = 0;
        c->argsread = sscanf(cmd, "%*s %d %d %d", &c->value1, &c->value2, &c->value3);
        return (c);
      }
      ++i;
    }
  } else
    printf("error: cannot parse lirc code line '%s'\n", cmd);

  return(NULL);
}

int main(int argc, char *argv[]) {
  struct lirc_config *config;
  struct stat fstat, fstat_lircrc;

  switch (process_options(argc, argv)) {
    case option_exitsuccess:
      return(EXIT_SUCCESS);
      break;
    case option_exitfailure:
      return(EXIT_FAILURE);
      break;
  }
  struct animenu_options* options = get_options();

  if (options->menutimeout < 0 || options->menuanimation < 0) {
    fprintf(stderr, "menutimeout and menuanimation must be >= 0\n");
    return(EXIT_FAILURE);
  }

  /* create root menu */
  char file[PATH_MAX] = "";
  snprintf(file, PATH_MAX - 1, "%s/.animenu/%s", getenv("HOME"), "root.menu");
  if (!(rootmenu = animenu_create(NULL, animenuitem_submenu, file))) {
    fprintf(stderr, "cannot create root menu!\n");
    return(EXIT_FAILURE);
  }
  if (options->dump) {
    animenu_dump(rootmenu);
    rootmenu->dispose(rootmenu);
    exit(0);
  }

  if (options->menutimeout != 0)
    signal(SIGALRM, sigalarm_handler);

  if (lirc_init(options->progname, options->debug) == -1)
    exit(EXIT_FAILURE);

  if (options->daemonise) {
    if (daemon(0, 0) == -1) {
      fprintf(stderr, "%s: can't daemonise\n", options->progname);
      perror(options->progname);
      lirc_freeconfig(config);
      lirc_deinit();
      exit(EXIT_FAILURE);
    }
  }

  char *code;
  char *c;
  int ret;
  config = NULL;
  while (lirc_nextcode(&code) == 0) {
    if (code == NULL)
      continue;

    /* test config */
    fstat = (struct stat){0};
    stat((const char*)options->lircrcfile, &fstat);
    if (config && (long unsigned int)difftime(fstat.st_mtime, fstat_lircrc.st_mtime) > 0) {
      /* release config */
      if (options->debug > 0)
        fprintf(stderr, "lircrc file '%s' has changed, re-reading\n", options->lircrcfile);
      lirc_freeconfig(config);
      config = NULL;
    }
    if (config == NULL) {
      /* (re)read config */
      lirc_readconfig(options->lircrcfile, &config, NULL);
      fstat_lircrc = (struct stat){0};
      stat((const char*)options->lircrcfile, &fstat_lircrc);
    }
    if (config == NULL)
      continue;

    while ((ret = lirc_code2char(config, code, &c)) == 0 && c != NULL) {
      struct lirc_command * cmd = parse_codes(lirc_commands,c);
      if (!cmd)
        fprintf(stderr, "command not recognised: %s\n", c);
      else {
        if (options->menutimeout != 0)
          alarm(0);
        switch (cmd->id) {
          case id_show:
            if (rootmenu->visible) {
              rootmenu->hide(rootmenu);
              currentmenu = NULL;
            } else {
              rootmenu->show(rootmenu);
              if (options->debug > 0)
                printf("root | current item: '%s'\n",
                       rootmenu->currentitem ? rootmenu->currentitem->title : "NULL");
              rootmenu->next(rootmenu);
              if (options->debug > 0)
                printf("root | current item: '%s'\n",
                       rootmenu->currentitem ? rootmenu->currentitem->title : "NULL");
              /* navigate based on currentmenu */
              currentmenu = rootmenu;
            }
            break;
          case id_next:
            if (currentmenu != NULL)
              currentmenu->next(currentmenu);
            break;
          case id_prev:
            if (currentmenu != NULL)
              currentmenu->prev(currentmenu);
            break;
          case id_select:
            if (currentmenu != NULL && currentmenu->currentitem != NULL)
              currentmenu->currentitem->go(currentmenu->currentitem);
            break;
          case id_back:
            if (currentmenu != NULL && currentmenu->parent != NULL) {
              currentmenu->hide(currentmenu);
              currentmenu = currentmenu->parent;
              currentmenu->showcurrent(currentmenu);
            }
            break;
          case id_forward:
            if (currentmenu != NULL && currentmenu->currentitem != NULL) {
              currentmenu->currentitem->select(currentmenu->currentitem);
              if ((currentmenu->currentitem->submenu != NULL) &&
                  (currentmenu->currentitem->submenu->visible)) {
                currentmenu = currentmenu->currentitem->submenu;
                if (strstr(currentmenu->currentitem->title, playall) != 0)
                  currentmenu->next(currentmenu);
                if (options->debug > 0)
                  printf("current item: '%s'\n", currentmenu->currentitem->title);
              }
            }
            break;
        }
        if ((cmd->id == id_show) && !(rootmenu->visible))
          ; /* no-op */
        else if (options->menutimeout != 0) {
          alarm_pending = 1;
          alarm(options->menutimeout);
        }
      } /* valid command */
    } /* while code2char */
    free(code);
    if (ret == -1)
      fprintf(stderr, "animenu: lirc failed to process code: \n'%s'\n", code);
  } /* while waiting for next lirc code */
  lirc_freeconfig(config);

  /* close lirc connection */
  lirc_deinit();

  exit(EXIT_SUCCESS);
}
