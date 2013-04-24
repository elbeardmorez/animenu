
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
#include <getopt.h>
#include <limits.h>
#include <lirc/lirc_client.h>

#include "animenu.h"
#include "tokenize.h"
#include "osd.h"
#include "menu.h"
#include "options.h"

char *progname = PACKAGE "-" VERSION;

enum myids {id_null, id_show, id_next, id_prev, id_select, id_back, id_forward};

struct easyoption myoptions[] = {
  {id_show, "show", 0, 0, 0, 0, 0},
  {id_next, "next", 0, 0, 0, 0, 0},
  {id_prev, "prev", 0, 0, 0, 0, 0},
  {id_select, "select", 0, 0, 0, 0, 0},
  {id_back, "back", 0, 0, 0, 0, 0},
  {id_forward, "forward", 0, 0, 0, 0, 0},
  {0, NULL, 0, 0, 0, 0, 0}
};

enum optresults {opt_null, opt_daemonise, opt_exit_failure, opt_exit_success};

/* globals */
static long osd_utime;
static struct animenucontext *rootmenu;
static struct animenucontext *currentmenu;
static int alarm_pending;
char *fontname = "-misc-fixed-medium-r-normal--36-*-75-75-c-*-iso8859-*";
char *fgcolor = "rgb:00/ff/00", *bgcolor = "black", *selectfgcolor = "rgb:bf/ff/bf";

int menutimeout = 5, menuanimation = 10000;
int dump = 0;

void sigalarm_handler(int junk) {
  if (alarm_pending) {
    alarm_pending = 0;
    rootmenu->hide(rootmenu);
  }
}

int process_options(int argc, char *argv[]) {
  int daemonise = 0;
  read_config();

  while (1) {
    int c;
    static struct option long_options[] = {
      {"help", no_argument, NULL, 'h'},
      {"version", no_argument, NULL, 'v'},
      {"daemon", no_argument, NULL, 'd'},
      {"font", required_argument, NULL, 'f'},
      {"fontname", required_argument, NULL, 'n'},
      {"fgcolor", required_argument, NULL, 'c'},
/*
      {"bgcolor",required_argument,NULL,'b'},
*/
      {"selectfgcolor", required_argument, NULL, 's'},
      {"menutimeout", required_argument, NULL, 't'},
      {"menuanimation", required_argument, NULL, 'a'},
      {"dump", no_argument, NULL, 'M'},
      {"debug", optional_argument, NULL, 'D'},
      {0, 0, 0, 0}
    };
    c = getopt_long(argc, argv, "hvdf:n:c:s:t:a:M:D::", long_options, NULL);
    if (c == -1)
      break;
    switch (c) {
      case 'h':
        printf("usage: %s [options]\n", argv[0]);
        printf("\t -h --help\t\tdisplay this message\n");
        printf("\t -v --version\t\tdisplay version\n");
        printf("\t -d --daemon\t\trun in background\n");
        printf("\t -f --font\t\tuse specified font (xfontsel format)\n");
        printf("\t -n --fontname\t\tspecify font by family name only\n");
        printf("\t -c --fgcolor\t\tuse specified foreground color\n");
/*
        printf("\t -b --bgcolor\t\tuse specified background color\n");
*/
        printf("\t -s --selectfgcolor\t\tcolor of selected item\n");
        printf("\t -t --menutimeout\t\thow long before menu dissapears (0 for no timeout)\n");
        printf("\t -a --menuanimation\t\tmenu animation speed (microseconds)\n");
        printf("\t -M --dump\t\tdump menu structure to screen\n");
        printf("\t -D[x] --debug=[x]\tenable log. optional verbosity [1 .. 2] (default: 1)\n");
        return(opt_exit_success);
      case 'v':
        printf("%s\n", progname);
        return(opt_exit_success);
      case 'd':
        daemonise = 1;
        break;
      case 'f':
        fontname = strdup(optarg);
        break;
      case 'n':
        fontname = makefontspec(optarg, "36");
        break;
      case 'c':
        fgcolor = strdup(optarg);
        break;
/*
      case 'b':
        bgcolor = strdup(optarg);
        break;
*/
      case 's':
        selectfgcolor = strdup(optarg);
        break;
      case 't':
        menutimeout = atoi(optarg);
        break;
      case 'a':
        menuanimation = atoi(optarg);
        break;
      case 'M':
        dump = 1;
        break;
      case 'D':
        debug = optarg ? atoi(optarg) : 1;
        break;
      default:
        printf("usage: %s [options]\n", argv[0]);
        return(opt_exit_failure);
    }
  }
  if (optind < argc - 1) {
    fprintf(stderr, "%s: too many arguments\n", progname);
    return(opt_exit_failure);
  }
  return((daemonise ? opt_daemonise : opt_null));
}

int main(int argc, char *argv[]) {
  struct lirc_config *config;
  int daemonise = 0;
  int currentchannel = 0;

  switch (process_options(argc, argv)) {
    case opt_exit_success:
      return(EXIT_SUCCESS);
      break;
    case opt_exit_failure:
      return(EXIT_FAILURE);
      break;
    case opt_daemonise:
      daemonise = 1;
      break;
  }
  if (menutimeout < 0 || menuanimation < 0) {
    fprintf(stderr, "menutimeout and menuanimation must be >= 0\n");
    return(EXIT_FAILURE);
  }

  char file[PATH_MAX] = "";
  snprintf(file, PATH_MAX - 1, "%s/.animenu/%s", getenv("HOME"), "root.menu");
  if (!(rootmenu = animenu_create(NULL, animenuitem_submenu, file))) {
    fprintf(stderr, "cannot create root menu!\n");
    return(EXIT_FAILURE);
  }
  if (dump) {
    animenu_dump(rootmenu);
    rootmenu->dispose(rootmenu);
    exit(0);
  }

  if (menutimeout != 0)
    signal(SIGALRM, sigalarm_handler);

  if (lirc_init("animenu", debug) == -1)
    exit(EXIT_FAILURE);

  if (lirc_readconfig(optind != argc ? argv[optind] : NULL, &config, NULL) == 0) {
    char *code;
    char *c;
    int ret;

    if (daemonise) {
      if (daemon(0, 0) == -1) {
        fprintf(stderr, "%s: cannot daemonise\n", progname);
        perror(progname);
        lirc_freeconfig(config);
        lirc_deinit();
        exit(EXIT_FAILURE);
      }
    }

    while (lirc_nextcode(&code) == 0) {
      if (code == NULL)
        continue;
      while ((ret = lirc_code2char(config, code, &c)) == 0 && c != NULL) {
        struct easyoption *opt = parseoption(myoptions, c);
        if (!opt)
          fprintf(stderr, "command not recognised: %s\n", c);
        else {
          if (menutimeout != 0)
            alarm(0);
          switch (opt->id) {
            case id_show:
              if (rootmenu->visible) {
                rootmenu->hide(rootmenu);
                currentmenu = NULL;
              } else {
                rootmenu->show(rootmenu);
                if (debug > 0)
                  printf("root | current item: '%s'\n",
                         rootmenu->currentitem ? rootmenu->currentitem->title : "NULL");
                rootmenu->next(rootmenu);
                if (debug > 0)
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
              if(currentmenu != NULL && currentmenu->parent != NULL) {
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
                  if (debug > 0)
                    printf("current item: '%s'\n", currentmenu->currentitem->title);
                }
              }
              break;
          }
          if ((opt->id == id_show) && !(rootmenu->visible))
            ; /* no-op */
          else if (menutimeout != 0) {
            alarm_pending = 1;
            alarm(menutimeout);
          }
        } /* valid command */
      } /* while code2char */
      free(code);
      if (ret == -1)
        break;
    } /* while waiting for next lirc code */

    lirc_freeconfig(config);
  } /* config read */

  /* close lirc connection */
  lirc_deinit();

  exit(EXIT_SUCCESS);
}
