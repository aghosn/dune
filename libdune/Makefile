CC      = gcc
CFLAGS  = -Wall -g -O3 -MD
LDFLAGS = -static -L. -ldune

SANDBOX_OBJS = $(patsubst %.c, %.o, $(wildcard **/*.c))
OBJ     = entry.o dune.o vsyscall.o elf.o util.o procmap.o debug.o 
NOFPU_OBJ = trap.o

#$(info $$SANDBOX_OBJS is [${SANDBOX_OBJS}])

$(NOFPU_OBJ): EXTRA_FLAGS := -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -msoft-float

all: libdune.a

libdune.a: $(OBJ) $(NOFPU_OBJ) $(SANDBOX_OBJS)
	$(AR) crD $(@) $(OBJ) $(NOFPU_OBJ) $(SANDBOX_OBJS)

clean:
	rm -f *.o test *.d libdune.a

-include *.d

%.o: %.c
	$(CC) $(CFLAGS) $(EXTRA_FLAGS) -o $@ -c $<

