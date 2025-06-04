all: $(VENDORDIR)/noticeboard/out/lib/libnoticeboard.a

$(VENDORDIR)/noticeboard.zip:
	@mkdir -p $(VENDORDIR)
	curl -L $(NB_LINK) -o '$@'


$(VENDORDIR)/noticeboard/ready: $(VENDORDIR)/noticeboard.zip
	cd $(VENDORDIR) && unzip noticeboard.zip
	mv $(VENDORDIR)/noticeboard-* $(VENDORDIR)/noticeboard
	touch $@


$(NB_OUT): $(VENDORDIR)/noticeboard/ready
	CPPFLAGS= CXXFLAGS= CFLAGS= LDFLAGS= cmake -S $(VENDORDIR)/noticeboard --preset default -B $(VENDORDIR)/noticeboard/build -DCMAKE_BUILD_TYPE=Release
	CPPFLAGS= CXXFLAGS= CFLAGS= LDFLAGS= cmake --build $(VENDORDIR)/noticeboard/build
	cmake --install $(VENDORDIR)/noticeboard/build --prefix $(VENDORDIR)/noticeboard/out
