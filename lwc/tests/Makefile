CC = gcc
CFLAGS = -Wall -O3 -pthread

APPS = vqtest

all: $(APPS)

$(APPS):

$(APPS): %: %.o
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f *.o *.d $(APPS)

-include *.d