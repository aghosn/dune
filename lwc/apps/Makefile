SUBDIRS = openssl
CLEANDIRS = $(SUBDIRS:%=clean-%)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean: $(CLEANDIRS)

$(CLEANDIRS):
	$(MAKE) -C $(@:clean-%=%) clean

.PHONY: all clean $(SUBDIRS) $(CLEANDIRS)