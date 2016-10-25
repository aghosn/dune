# LIBDUNE	= ../libdune
# CFLAGS	= -Wall -g -MD -I$(LIBDUNE) -fomit-frame-pointer
# RANLIB	= ranlib
# LIBS	= -ldune -lpthread
# CC = gcc

# all: lwC.a test

# lwC.a: lwC.o
# 	$(AR) cru $(@) $(<)
# 	$(RANLIB) $(@)

# test: test.o lwC.a
# 	$(CC) $(CFLAGS) -o $(@) $(<) -L. -L$(LIBDUNE) $(LIBS)

# clean:
# 	rm -f *.o *.d test lwC.a

# -include *.d

CC      = gcc
LIBDUNE = ../libdune
CFLAGS  = -Wall -g -MD -O3 -I$(LIBDUNE) -pthread 

APPS = test

all: $(APPS) 
#$(SOURCE)

$(APPS): $(LIBDUNE)/libdune.a

$(APPS): %: %.o lwc_vm.o lwC.o
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f *.o *.d $(APPS)

-include *.d

