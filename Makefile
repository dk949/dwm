# dwm - dynamic window manager
# See LICENSE file for copyright and license details.

include config.mk

SRC = drw.c                        \
	  dwm.c                        \
	  util.c                       \
	  $(if $(ASOUNDFLAGS),volc.c,) \
	  $(if $(XBACKLIGHTFLAGS),xbacklight.c,alt_backlight.c)

HDR = backlight.h common.h config.h drw.h util.h $(if $(ASOUNDFLAGS),volc.h,)

OBJ = ${SRC:.c=.o}

all: options dwm

options:
	@echo dwm build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: ${HDR} config.mk MODE

dwm: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f dwm ${OBJ}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	install dwm ${DESTDIR}${PREFIX}/bin
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	sed "s/VERSION/${VERSION}/g" < dwm.1 > ${DESTDIR}${MANPREFIX}/man1/dwm.1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/dwm.1

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/dwm \
		${DESTDIR}${MANPREFIX}/man1/dwm.1

.PHONY: all options clean install uninstall
.PHONY: phony
MODE: phony
	@[ "`cat $@ 2>&1`" = '$($@)' ] || echo -n $($@) >$@
