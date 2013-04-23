
/**
 * animenu - lirc menu system
 *
 *  copyright (c) 2009, 2012-2013 by Pete Beardmore <pete.beardmore@msn.com>
 *  copyright (c) 2001-2003 by Alastair M. Robinson <blackfive@fakenhamweb.co.uk>
 *
 *  licensed under GNU General Public License 2.0 or later
 *  some rights reserved. see COPYING, AUTHORS
 */

#ifndef TOKENIZE_H
#define TOKENIZE_H

struct easyoption {
  int id;
  const char *name;
  int args, argsread;
  int value1, value2, value3;
};

struct easyoption *parseoption(struct easyoption *optlist, const char *cmd);

#endif
