all: vendor/noticeboard/out/lib/libnoticeboard.a
NB_LINK=https://github.com/dk949/noticeboard/archive/160612ca4f9cf4c982311fabfcd37de35b2ccf5e.zip
NB_OUT = vendor/noticeboard/out/lib/libnoticeboard.a
NB_INC = vendor/noticeboard/out/include
NB_LIB = noticeboard
NB_LDIR = vendor/noticeboard/out/lib/

vendor/noticeboard.zip:
	@mkdir -p vendor
	curl -L $(NB_LINK) -o '$@'


vendor/noticeboard/ready: vendor/noticeboard.zip
	cd vendor && unzip noticeboard.zip
	mv vendor/noticeboard-* vendor/noticeboard
	touch $@


$(NB_OUT): vendor/noticeboard/ready
	cmake -S vendor/noticeboard --preset default -B vendor/noticeboard/build
	cmake --build vendor/noticeboard/build
	cmake --install vendor/noticeboard/build --prefix vendor/noticeboard/out
