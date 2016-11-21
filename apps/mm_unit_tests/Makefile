DUNE = ../../
DUNE_LIB = $(DUNE)/libdune/libdune.a
CFLAGS = -Wall -O3 -pthread -I$(DUNE)/libdune
CC = gcc

APPS = test_vm

all: $(APPS)

$(APPS):

$(APPS): %: %.o
	$(CC) $(CFLAGS) $^ -o $@ $(DUNE_LIB)

clean:
	rm -f *.o *.d $(APPS)

-include *.d

