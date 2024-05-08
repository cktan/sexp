ARCH=$(shell uname -m)
CC ?= gcc
CFLAGS = -I../ext/include -Wmissing-declarations -Wall -Wextra -MMD
ifeq ($(ARCH), x86_64)
	ifeq ($(MARCH), )
		CFLAGS += -march=broadwell
	else
		CFLAGS += -march=$(MARCH)
	endif
else ifeq ($(ARCH), aarch64)
	CFLAGS += -D__ARM_NEON__
endif



ifdef DEBUG
    CFLAGS += -O0 -g
else
    CFLAGS += -O3 -funroll-loops -ftree-vectorize -DNDEBUG 
endif

ifdef PROFILE
    CFLAGS += -pg
endif

CFLAGS += -std=c99
LDFLAGS = -L../ext/lib -Wl,-rpath='$$ORIGIN'/../ext/lib
LDLIBS = 
CFILES = xexpr.c

OBJS = $(CFILES:.c=.o)
EXECS = 

all: libxexpr.a $(EXECS) 

#xsanitize: xsanitize.cpp libexx.a sql_parser/libpg_query.a
#	$(CXX) $(CXXFLAGS) -o $@ $(filter-out %.hpp %.h, $^) $(LDFLAGS) $(LDLIBS)


libxexpr.a: $(OBJS)
	ar -rcs $@ $^

-include $(OBJS:%.o=%.d) $(EXECS:%=%.d)

clean:
	rm -f *.a *.o *.d $(EXECS)

format:
	clang-format -i *.[ch]

.PHONY: all clean format

