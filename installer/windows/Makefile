# ==============================================================================
# AuralisOS Windows Loopback Installer Makefile
# ==============================================================================

GO ?= go
TARGET := Auralis-Installer.exe

.PHONY: all clean

all: $(TARGET)

$(TARGET): main.go
	CGO_ENABLED=0 GOOS=windows GOARCH=amd64 $(GO) build \
		-ldflags="-s -w" \
		-trimpath \
		-o $(TARGET) main.go

clean:
	rm -f $(TARGET)
