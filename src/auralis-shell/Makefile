# ==============================================================================
# AuralisOS Shell Makefile (Standalone Native Build)
# ==============================================================================

CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -D_GNU_SOURCE
PREFIX ?= /usr/local

PKGS := gtk4 gtk4-layer-shell-0
PKG_CFLAGS := $(shell pkg-config --cflags $(PKGS) 2>/dev/null)
PKG_LIBS   := $(shell pkg-config --libs $(PKGS) 2>/dev/null) -lm

TARGET := auralis-shell
SRC := main.c

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(PKG_CFLAGS) -o $@ $(SRC) $(PKG_LIBS)

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/
	install -d $(DESTDIR)/usr/share/auralis-shell
	install -m 644 style.css $(DESTDIR)/usr/share/auralis-shell/

clean:
	rm -f $(TARGET) *.o
