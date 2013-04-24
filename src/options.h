
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

extern char rc_fontspec[256];
extern char rc_color[256];
extern char rc_selcolor[256];

char *makefontspec(char *font, char *size);
int read_config();

#endif
