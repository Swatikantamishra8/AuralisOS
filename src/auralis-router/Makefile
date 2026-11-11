# ==============================================================================
# AuralisOS Binary Router Makefile (Static Zero-Dependency Build)
# ==============================================================================

GO ?= go
TARGET := auralis-router
PREFIX ?= /usr

.PHONY: all static clean install test

all: static

static:
	CGO_ENABLED=0 GOOS=linux GOARCH=amd64 $(GO) build \
		-ldflags="-s -w -extldflags '-static'" \
		-trimpath \
		-o $(TARGET) main.go

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/
	install -d $(DESTDIR)/usr/local/bin
	ln -sf $(PREFIX)/bin/$(TARGET) $(DESTDIR)/usr/local/bin/$(TARGET)

clean:
	rm -f $(TARGET)
