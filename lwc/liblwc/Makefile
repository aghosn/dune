# Copyright 2013-16 Board of Trustees of Stanford University
# Copyright 2013-16 Ecole Polytechnique Federale Lausanne (EPFL)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

# A Makefile for LWC.
#
# path for this Makefile to work properly.

DUNE = ../../
INC = -I$(DUNE)/libdune -include$(DUNE)/kern/dune.h

CC = gcc
CFLAGS = -g -Wall -fno-dwarf2-cfi-asm -fno-asynchronous-unwind-tables -O3 -mno-red-zone $(INC) -D__KERNEL__ $(EXTRA_CFLAGS)

LD = gcc
LDFLAGS = -T lwc.ld
LDLIBS = -lrt -lpthread -lm -lnuma -ldl -lconfig

SRCS = 
DIRS = syscalls core


define register_dir
SRCS += $(patsubst %, $(1)/%, $(2))
endef

include $(patsubst %, %/dir.mk, $(DIRS))

%.generated.S: %.c
	$(CC) $(CFLAGS) -o $@ -S $<

all: lwc

OBJS=$(subst .c,.o,$(SRCS))

DUNE_LIB=$(DUNE)/libdune/libdune.a

depend: .depend

.depend: $(SRCS)
	bash -c "cat $(foreach SRC,$(SRCS),<($(CC) $(CFLAGS) -MM -MT $(SRC:.c=.o) $(SRC))) > ./.depend"

ifneq ($(MAKECMDGOALS),clean)
-include .depend
endif

lwc: $(DEPENDENCY) $(OBJS) $(DUNE_LIB) lwc.ld
	$(LD) $(LDFLAGS) -o lwc $(OBJS) $(DUNE_LIB) $(LDLIBS)

clean:
	rm -f $(OBJS) lwc .depend

dist-clean: clean
	rm *~
