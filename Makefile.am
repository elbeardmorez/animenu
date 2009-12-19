## Process this file with automake to produce Makefile.in

bin_PROGRAMS = animenu

## simple programs
animenu_SOURCES = animenu.c animenu.h tokenize.c tokenize.h osd.c osd.h menu.c menu.h options.c options.h

animenu_LDADD = -lX11

EXTRA_DIST = \
  examplemenus/animenurc \
  examplemenus/root.menu \
  examplemenus/emulators.menu \
  examplemenus/graphics.menu \
  examplemenus/sound.menu \
  examplemenus/internet.menu

CLEANFILES = *~
