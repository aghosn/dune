DUNE = ../../..
DUNE_LIB = $(DUNE)/libdune/libdune.a
CC = gcc
CFLAGS = -Wall -O3 -pthread -I$(DUNE)/libdune -I../../liblwc

APPS = hello shared2 mmap double_switch registers gva_hva shared private cow readonly shared_increase shared3

all: $(APPS)

$(APPS):

$(APPS): %: %.o
	$(CC) $(CFLAGS) $^ -o $@ $(DUNE_LIB)

clean:
	rm -f *.o *.d $(APPS)

-include *.d