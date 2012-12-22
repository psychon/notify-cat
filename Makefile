CC        = gcc
PKGS      = libnotify gio-2.0 gio-unix-2.0
PKG_FLAGS = $(shell pkg-config --cflags --libs $(PKGS))
WFLAGS    = -Wall -Wextra -pedantic -std=gnu99 -Wno-unused-parameter -Wdeclaration-after-statement -Wmissing-declarations -Werror-implicit-function-declaration -Wnested-externs -Wpointer-arith -Wwrite-strings -Wsign-compare -Wstrict-prototypes -Wmissing-prototypes -Wpacked -Wswitch-enum -Wmissing-format-attribute -Wbad-function-cast -Wvolatile-register-var -Wstrict-aliasing=2 -Winit-self -Wunsafe-loop-optimizations -Winline
CFLAGS    = $(WFLAGS) -g -DG_DISABLE_DEPRECATED=1 $(PKG_FLAGS)

all: notify-cat

notify-cat: notify-cat.c
	$(CC) -o $@ $(LDFLAGS) $< $(CFLAGS) $(LIBS)
