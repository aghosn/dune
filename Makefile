LIBC	= eglibc-2.14/eglibc-build/libc.so
SUBDIRS	= kern libdune lwc

all: $(SUBDIRS)
libc: $(LIBC)

lwc: libdune

$(SUBDIRS):
	$(MAKE) -C $(@)

$(LIBC):
	sh build-eglibc.sh

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $(@); \
	done

distclean: clean
	rm -fr eglibc-2.14

.PHONY: $(SUBDIRS) clean distclean
