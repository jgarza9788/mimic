PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share
CC ?= cc
BUILD ?= release

CSTD = -std=c11
WARN = -Wall -Wextra -Wpedantic -Wshadow -Wformat=2
CFLAGS_COMMON = $(CSTD) $(WARN) -Iinclude

XCB_BASE_LIBS := -lxcb
XCB_KEYSYMS_LIB := $(shell pkg-config --libs xcb-keysyms 2>/dev/null)
XCB_ICCCM_LIB := $(shell pkg-config --libs xcb-icccm 2>/dev/null)
XCB_EWMH_LIB := $(shell pkg-config --libs xcb-ewmh 2>/dev/null)
LDFLAGS_COMMON = $(XCB_BASE_LIBS) $(XCB_KEYSYMS_LIB) $(XCB_ICCCM_LIB) $(XCB_EWMH_LIB)

ifeq ($(BUILD),debug)
  CFLAGS = $(CFLAGS_COMMON) -O0 -g3 -DDEBUG
else
  CFLAGS = $(CFLAGS_COMMON) -O2 -DNDEBUG
endif

SRC = src/main.c src/wm.c
OBJ = $(SRC:.c=.o)
TARGET = mimicwm

.PHONY: all clean install uninstall debug

all: $(TARGET)

debug:
	$(MAKE) BUILD=debug

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS_COMMON)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	install -d $(DESTDIR)$(DATADIR)/xsessions
	install -m 644 assets/mimicwm.desktop $(DESTDIR)$(DATADIR)/xsessions/mimicwm.desktop
	install -d $(DESTDIR)$(DATADIR)/mimicwm
	install -m 644 config/picom.conf $(DESTDIR)$(DATADIR)/mimicwm/picom.conf
	install -m 644 config/config.toml $(DESTDIR)$(DATADIR)/mimicwm/config.toml
	install -m 755 scripts/start-mimicwm-session.sh $(DESTDIR)$(DATADIR)/mimicwm/start-mimicwm-session.sh

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -f $(DESTDIR)$(DATADIR)/xsessions/mimicwm.desktop
	rm -rf $(DESTDIR)$(DATADIR)/mimicwm

clean:
	rm -f $(OBJ) $(TARGET)
