CFILES = xparse.c
OBJ = $(CFILES:.c=.o)
EXECS = $(CFILES:.c=)

CC ?= gcc
CFLAGS += -I../src -Wmissing-declarations -Wall -Wextra -mavx2 -mfma -mbmi2 -MMD

ifdef DEBUG
    CFLAGS += -O0 -g
else
    CFLAGS += -O3 -funroll-loops -ftree-vectorize -DNDEBUG 
endif

CFLAGS += -std=c99

all: $(EXECS)

xparse: xparse.c ../src/libsexpr.a
	$(CC) $(CFLAGS) -o $@ $(filter-out %.h, $^) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f *.o *.a *.d $(EXECS)

format:
	clang-format -i *.[ch]

-include $(EXECS:%=%.d)

.PHONY: clean format
