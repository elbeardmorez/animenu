## Process this file with automake to produce Makefile.in

@include@

bin_PROGRAMS = animenu

## simple programs
animenu_SOURCES = animenu.c animenu.h osd.c osd.h menu.c menu.h options.c options.h

animenu_LDADD = $(LIBS)

EXTRA_DIST = \
  examples/animenurc \
  examples/root.menu \
  examples/emulators.menu \
  examples/graphics.menu \
  examples/audio.menu \
  examples/internet.menu

CLEANFILES = *~

