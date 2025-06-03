XINERAMAFLAGS = $(shell pkg-config xinerama --cflags --silence-errors && echo "-DXINERAMA")
XINERAMALIBS  = $(shell pkg-config xinerama --libs --silence-errors)

ASOUNDFLAGS = $(shell pkg-config alsa --cflags --silence-errors && echo -DASOUND)
ASOUNDLIBS  = $(shell pkg-config alsa --libs --silence-errors)

PKG_CONFIG_LIBS = x11 x11-xcb xcb-res xft fontconfig
# Include directories (-I/dir/name or -isystem/dir/name)
INC=$(XINERAMAFLAGS) $(ASOUNDFLAGS) -I$(NB_INC)
# Libraries to be linked (-llibname or -L/dir/name)
LIBS=$(XINERAMALIBS) $(ASOUNDLIBS) -L$(NB_LDIR) -l$(NB_LIB)
