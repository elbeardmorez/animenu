#ifndef OPTIONS_H
#define OPTIONS_H

extern char rc_fontspec[256];
extern char rc_color[256];
extern char rc_selcolor[256];

char *makefontspec(char *font, char *size);
int read_config();

#endif
