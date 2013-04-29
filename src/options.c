
/**
 * animenu - lirc menu system
 *
 *  copyright (c) 2009, 2012-2013 by Pete Beardmore <pete.beardmore@msn.com>
 *  copyright (c) 2002 by Massimo Maricchiolo <mmaricch@yahoo.com>
 *  copyright (c) 2001-2003 by Alastair M. Robinson <blackfive@fakenhamweb.co.uk>
 *
 *  licensed under GNU General Public License 2.0 or later
 *  some rights reserved. see COPYING, AUTHORS
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <getopt.h>

#include "options.h"

char *progname = PACKAGE "-" VERSION;
static struct animenu_options options;

struct animenu_options* get_options() {
  return (&options);
};

void makefontspec() {
  snprintf(options.fontspec, BUFSIZE + 1,
           "-*-%s-*-*-*--%s-*-*-*-*-*-*-*",
           options.fontname, options.fontsize);
}

int read_config() {

  char buf[BUFSIZE + 1];
  char *tmp, *key, *val;
  int len, pos, i;
  FILE *f;

  buf[0] = '\0';
  strcat(buf, getenv("HOME"));
  strcat(buf, "/.animenu/animenurc");

  if (!(f = fopen(buf, "r")))
    return(0);

  while (fgets(buf, sizeof(buf), f) != NULL) {
    len = strlen(buf);
    for (i = 0; i < len; i++)
      buf[i] = tolower(buf[i]);

    if (buf[0] == '#')
      continue;
    if (len < 5)
      continue;

    len--;
    if (buf[len] == '\n')
      buf[len] = '\0';
    len--;
    if (buf[len] == '\r')
      buf[len] = '\0';

    key = strtok(buf, "= \t");
    for (i = 0; i < len; i++) {
      /* generic */
      pos = strlen(key) + 1;
      while (buf[pos] == '=' || buf[pos] == '\t' || buf[pos] == ' ')
        pos++;
      tmp = &buf[pos];
      val = strtok(tmp, "\t #");
      /* option specific */
      if (strcmp(key, "fontspec") == 0) {
        strcpy(options.fontspec, val);
      } else if (strcmp(key, "fontname") == 0) {
        strcpy(options.fontname, val);
        makefontspec();
      } else if (strcmp(key, "fontsize") == 0) {
        strcpy(options.fontsize, val);
        makefontspec();
      } else if (strcmp(key, "bgcolour") == 0) {
        strcpy(options.bgcolour, val);
      } else if (strcmp(key, "fgcolour") == 0) {
        strcpy(options.fgcolour, val);
      } else if (strcmp(key, "fgcoloursel") == 0) {
        strcpy(options.fgcoloursel, val);
      }
    }
  }

  fclose(f);

  return (0);
}

int process_options(int argc, char *argv[]) {

  /* set defaults */
  strcpy(options.progname, PACKAGE);
  strcpy(options.progver, PACKAGE "-" VERSION);
  strcpy(options.fontspec, "-misc-fixed-medium-r-normal--24-*-75-75-c-*-iso8859-*");
  strcpy(options.fontname, "fixed");
  strcpy(options.fontsize, "24");
  strcpy(options.bgcolour, "black");
  strcpy(options.fgcolour, "rgb:88/88/88");
  strcpy(options.fgcoloursel, "white");
  sprintf(options.lircrcfile, "%s/.lircrc", getenv("HOME"));
  options.menutimeout = 5;
  options.menuanimation = 1000;
  options.daemonise = 0;
  options.dump = 0;
  options.debug = 0;

  /* read rc file */
  read_config();

  /* process command line args */
  while (1) {
    int c;
    static struct option long_options[] = {
      {"help", no_argument, NULL, 'h'},
      {"version", no_argument, NULL, 'v'},
      {"daemon", no_argument, NULL, 'd'},
      {"fontspec", required_argument, NULL, 'f'},
      {"fontname", required_argument, NULL, 'n'},
      {"fontsize", required_argument, NULL, 'z'},
      {"bgcolour", required_argument, NULL, 'b'},
      {"fgcolour", required_argument, NULL, 'c'},
      {"fgcoloursel", required_argument, NULL,'s'},
      {"menutimeout", required_argument, NULL, 't'},
      {"menuanimation", required_argument, NULL, 'a'},
      {"dump", no_argument, NULL, 'M'},
      {"debug", optional_argument, NULL, 'D'},
      {0, 0, 0, 0}
    };
    c = getopt_long(argc, argv, "hvdf:n:z:b:c:s:t:a:M:D::", long_options, NULL);
    if (c == -1)
      break;
    switch (c) {
      case 'h':
        printf("Usage: %s [options]\n", argv[0]);
        printf("\t -h    --help\t\tdisplay this message\n");
        printf("\t -v    --version\t\tdisplay version\n");
        printf("\t -d    --daemon\t\trun in background\n");
        printf("\t -f    --fontspec\t\tuse specified font (xfontsel format)\n");
        printf("\t -n    --fontname\t\tspecify font by family name only\n");
        printf("\t -z    --fontsize\t\tset the font size\n");
        printf("\t -b    --bgcolour\t\tuse specified background colour\n");
        printf("\t -c    --fgcolour\t\tuse specified foreground colour\n");
        printf("\t -s    --fgcoloursel\t\tcolour of selected item\n");
        printf("\t -t    --menutimeout\t\thow long before menu dissapears (0 for no timeout)\n");
        printf("\t -a    --menuanimation\t\tmenu animation speed (microseconds)\n");
        printf("\t -M    --dump\t\tdump menu structure to screen\n");
        printf("\t -D[x] --debug=[x]\tenable log. optional verbosity [1 .. 2] (default: 1)\n");
        return (option_exitsuccess);
      case 'v':
        printf("%s\n", options.progname);
        return (option_exitsuccess);
      case 'd':
        options.daemonise = 1;
        break;
      case 'f':
        strcpy(options.fontspec, optarg);
        break;
      case 'n':
        strcpy(options.fontname, optarg);
        makefontspec();
        break;
      case 'z':
        strcpy(options.fontsize, optarg);
        makefontspec();
        break;
      case 'b':
        strcpy(options.bgcolour, optarg);
        break;
      case 'c':
        strcpy(options.fgcolour, optarg);
        break;
      case 's':
        strcpy(options.fgcoloursel, optarg);
        break;
      case 't':
        options.menutimeout = atoi(optarg);
        break;
      case 'a':
        options.menuanimation = atoi(optarg);
        break;
      case 'M':
        options.dump = 1;
        break;
      case 'D':
        options.debug = optarg ? atoi(optarg) : 1;
        break;
      default:
        printf("Usage: %s [options]\n", argv[0]);
        return (option_exitfailure);
    }
  }
  /* lircrc path */
  if (optind == argc - 1)
    strcpy(options.lircrcfile, argv[optind]);
  else if (optind < argc - 1) {
    fprintf(stderr, "%s: too many arguments\n", options.progname);
    return (option_exitfailure);
  }
  return ((options.daemonise ? option_daemonise : option_null));

}


