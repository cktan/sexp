ARCH=$(shell uname -m)
CC ?= gcc
CFLAGS = -I../src -Wmissing-declarations -Wall -Wextra -MMD
ifeq ($(ARCH), x86_64)
	ifeq ($(MARCH), )
		CFLAGS += -march=broadwell
	else
		CFLAGS += -march=$(MARCH)
	endif
else ifeq ($(ARCH), aarch64)
	CFLAGS += -D__ARM_NEON__
endif

CFILES = xparse.c
OBJ = $(CFILES:.c=.o)
EXECS = $(CFILES:.c=)

ifdef DEBUG
    CFLAGS += -O0 -g
else
    CFLAGS += -O3 -funroll-loops -ftree-vectorize -DNDEBUG 
endif

CFLAGS += -std=c99

all: $(EXECS)

xparse: xparse.c ../src/libxexpr.a
	$(CC) $(CFLAGS) -o $@ $(filter-out %.h, $^) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f *.o *.a *.d $(EXECS)

format:
	clang-format -i *.[ch]

-include $(EXECS:%=%.d)

.PHONY: clean format
