#DUNE = ../..
#DUNE_LIB = $(DUNE)/libdune/libdune.a
#CC = gcc
#CFLAGS = -Wall -O3 -pthread -I$(DUNE)/libdune -I../liblwc
#
#APPS = hello shared2 mmap double_switch registers gva_hva shared private cow readonly
#
#all: $(APPS)
#
#$(APPS):
#
#$(APPS): %: %.o
#	$(CC) $(CFLAGS) $^ -o $@ $(DUNE_LIB)
#
#clean:
#	rm -f *.o *.d $(APPS)
#
#-include *.d
SUBDIRS = units benchmarks
CLEANDIRS = $(SUBDIRS:%=clean-%)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean: $(CLEANDIRS)

$(CLEANDIRS):
	$(MAKE) -C $(@:clean-%=%) clean

.PHONY: all clean $(SUBDIRS) $(CLEANDIRS)
