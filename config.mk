# dwm version

ifndef VERSION
DATE         = $(shell git log -1 --format='%cd' --date=format:'%F')
DATE_TIME    = $(DATE) 00:00
COMMIT_COUNT = $(shell git rev-list --count HEAD --since="$(DATE_TIME)")
VERSION      = 6.2.$(shell date -d "$(DATE)" +'%Y%m%d')_$(COMMIT_COUNT)
endif

# Customize below to fit your system

# paths
DESTDIR   ?=
PREFIX    ?= /usr/local
MANPREFIX  = $(PREFIX)/share/man
MODE      ?= RELEASE

# Optional dependencies:
# 	libxinerama
#   libasound
#   st
# Will be included if installed
XINERAMAFLAGS = $(shell pkg-config xinerama --cflags --silence-errors && echo "-DXINERAMA")
XINERAMALIBS  = $(shell pkg-config xinerama --libs --silence-errors)

ASOUNDFLAGS = $(shell pkg-config alsa --cflags --silence-errors && echo -DASOUND)
ASOUNDLIBS  = $(shell pkg-config alsa --libs --silence-errors)

STFLAGS = $(shell command -v st >/dev/null && echo -DST_INTEGRATION)

# Optional features:
# 	Setting backlight with X
# Deprecated. Uncomment to reenable
# XBACKLIGHTLIBS = `pkg-config xcb xcb-randr xcb-util --cflags --silence-errors && echo "-DXBACKLIGHT"`
# XBACKLIGHTFLAGS = `pkg-config xcb xcb-randr xcb-util --libs --silence-errors`


REQ_LIBS = x11 x11-xcb xcb-res xft fontconfig

# includes and libs
LIBFLAGS = $(XINERAMAFLAGS) $(ASOUNDFLAGS) $(XBACKLIGHTFLAGS) `pkg-config $(REQ_LIBS) --cflags`
LIBS     = $(XINERAMALIBS)  $(ASOUNDLIBS)  $(XBACKLIGHTLIBS)  `pkg-config $(REQ_LIBS) --libs`

RELEASE_CPPFLAGS = -DNDEBUG
DEBUG_CPPFLAGS   =

# flags
CPPFLAGS = -D_DEFAULT_SOURCE         \
		   -D_GNU_SOURCE             \
		   -D_POSIX_C_SOURCE=200809L \
		   -DVERSION=\"$(VERSION)\"  \
		   $(STFLAGS)	             \
		   $($(MODE)_CPPFLAGS)

RELEASE_CFLAGS = -Os -flto
DEBUG_CFLAGS   = -Og -g -fsanitize=address
RELEASE_LDFLAGS = -flto
DEBUG_LDFLAGS   = -lg -fsanitize=address

CFLAGS         = -std=c23 -Wpedantic -Wall -Werror $($(MODE)_CFLAGS) $(LIBFLAGS) $(CPPFLAGS)

LDFLAGS  = $($(MODE)_LDFLAGS) $(LIBS)

# compiler and linker
CC ?= gcc
