
/**
 * animenu - lirc menu system
 *
 *  copyright (c) 2009, 2012-2013 by Pete Beardmore <pete.beardmore@msn.com>
 *  copyright (c) 2001-2003 by Alastair M. Robinson <blackfive@fakenhamweb.co.uk>
 *
 *  licensed under GNU General Public License 2.0 or later
 *  some rights reserved. see COPYING, AUTHORS
 */

#ifndef ANIMENU_H
#define ANIMENU_H

#define _max(A,B) A>B?A:B
#define _strncpy(A,B,C) strncpy(A,B,C), *(A+(C)-1)='\0'
#define _strncat(A,B,C) strncat(A,B,C), *(A+(C)-1)='\0'

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef BUFSIZE
#define BUFSIZE 1024
#endif

#endif

