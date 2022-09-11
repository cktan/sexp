CC ?= gcc
CFLAGS += -I../ext/include -Wmissing-declarations -Wall -Wextra -mavx2 -mfma -mbmi2 -MMD

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
CFILES = sexpr.c

OBJS = $(CFILES:.c=.o)
EXECS = 

all: libsexpr.a $(EXECS) 

#xsanitize: xsanitize.cpp libexx.a sql_parser/libpg_query.a
#	$(CXX) $(CXXFLAGS) -o $@ $(filter-out %.hpp %.h, $^) $(LDFLAGS) $(LDLIBS)


libsexpr.a: $(OBJS)
	ar -rcs $@ $^

-include $(OBJS:%.o=%.d) $(EXECS:%=%.d)

clean:
	rm -f *.a *.o *.d $(EXECS)

format:
	clang-format -i *.[ch]

.PHONY: all clean format

