ifndef VERSION
DATE         = $(shell git log -1 --format='%cd' --date=format:'%F')
DATE_TIME    = $(DATE) 00:00
COMMIT_COUNT = $(shell git rev-list --count HEAD --since="$(DATE_TIME)")
VERSION      = 6.4.$(shell date -d "$(DATE)" +'%Y%m%d')_$(COMMIT_COUNT)
endif
