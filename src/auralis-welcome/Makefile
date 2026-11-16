# ==============================================================================
# AuralisOS Apprenticeship Tour Makefile (Standalone Native Build)
# ==============================================================================

CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -D_GNU_SOURCE
PREFIX ?= /usr/local

PKGS := gtk4
PKG_CFLAGS := $(shell pkg-config --cflags $(PKGS) 2>/dev/null)
PKG_LIBS   := $(shell pkg-config --libs $(PKGS) 2>/dev/null) -lm

TARGET := auralis-welcome
SRC := main.c

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(PKG_CFLAGS) -o $@ $(SRC) $(PKG_LIBS)

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/
	ln -sf $(PREFIX)/bin/$(TARGET) $(DESTDIR)$(PREFIX)/bin/auralis-tour
	install -d $(DESTDIR)/usr/share/auralis-welcome
	install -m 644 style.css $(DESTDIR)/usr/share/auralis-welcome/

clean:
	rm -f $(TARGET) *.o
