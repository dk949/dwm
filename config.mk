# dwm version
VERSION = 6.2

# Customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man


# Xinerama and libasound. Both optional, comment out to remove.
XINERAMALIBS  = -lXinerama
XINERAMAFLAGS = -DXINERAMA

ASOUNDLIBS = -lasound
ASOUNDFLAGS = -DASOUND

# xbacklight
XBACKLIGHTLIBS = -lxcb-randr -lxcb -lxcb-util -lxcb-res

X11LIBS = -lX11 -lX11-xcb

# freetype
FREETYPELIBS = -lfontconfig -lXft
FREETYPEINC = /usr/include/freetype2

# includes and libs
INCS = -I${FREETYPEINC}
LIBS = ${X11LIBS} ${XINERAMALIBS} ${FREETYPELIBS} ${XBACKLIGHTLIBS} ${ASOUNDLIBS}

# flags
CPPFLAGS = -D_DEFAULT_SOURCE \
		   -D_BSD_SOURCE \
		   -D_POSIX_C_SOURCE=200809L \
		   -DVERSION=\"${VERSION}\" \
		   ${XINERAMAFLAGS}\
		   ${ASOUNDFLAGS}

CFLAGS   = -std=c99 -Wpedantic -Wall -Werror -Os ${INCS} ${CPPFLAGS}
LDFLAGS  = ${LIBS}

# compiler and linker
CC = cc
