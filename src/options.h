
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

#ifndef OPTIONS_H
#define OPTIONS_H

#ifndef ANIMENU_H
#include "animenu.h"
#endif

enum option_results {option_null, option_daemonise, option_exitfailure, option_exitsuccess};
struct animenu_options {
  char progname[BUFSIZE + 1];
  char progver[BUFSIZE + 1];
  char fontspec[BUFSIZE + 1];
  char fontname[BUFSIZE + 1];
  char fontsize[BUFSIZE + 1];
  char bgcolour[BUFSIZE + 1];
  char fgcolour[BUFSIZE + 1];
  char fgcoloursel[BUFSIZE + 1];
  int menutimeout;
  int menuanimation;
  int daemonise;
  int dump;
  int debug;
};

int process_options(int argc, char *argv[]);
struct animenu_options* get_options();

#endif
