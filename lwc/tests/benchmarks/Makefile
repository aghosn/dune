DUNE = ../../..
DUNE_LIB = $(DUNE)/libdune/libdune.a
CC = gcc
CFLAGS = -Wall -O3 -pthread -I$(DUNE)/libdune -I../../liblwc

APPS = revert revert_fork one_create revert_vfork revert_lwc

all: $(APPS)

$(APPS):

$(APPS): %: %.o
	$(CC) $(CFLAGS) $^ -o $@ $(DUNE_LIB)

clean:
	rm -f *.o *.d $(APPS)

-include *.d