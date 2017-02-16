CC = gcc
CFLAGS = -Wall -O3 -pthread

switcher : switcher.o bench.o
	$(CC) $(CXXFLAGS) switcher.o bench.o -o $@


clean:
	rm -f *.o *.d switcher

-include *.d