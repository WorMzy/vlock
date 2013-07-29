include ../config.mk

VPATH = ../src

override CFLAGS+=-I../src

export VLOCK_TEST_OUTPUT_MODE
VLOCK_TEST_OUTPUT_MODE = verbose

.PHONY: all
all: check

TESTED_SOURCES = list.c tsort.c util.c process.c
TESTED_OBJECTS = $(TESTED_SOURCES:.c=.o)

TEST_SOURCES = $(TESTED_SOURCES:%=test_%)
TEST_OBJECTS = $(TEST_SOURCES:.c=.o)

vlock-test : override LDFLAGS+=-lcunit
vlock-test: vlock-test.o $(TEST_OBJECTS) $(TESTED_OBJECTS)

vlock-test.o: $(TEST_SOURCES:.c=.h)

ifeq ($(COVERAGE),y)
vlock-test : override LDFLAGS+=--coverage
$(TESTED_OBJECTS) : override CFLAGS+=--coverage
endif

.PHONY: check
check: vlock-test
	@./vlock-test

.PHONY: memcheck
memcheck : VLOCK_TEST_OUTPUT_MODE=silent
memcheck: vlock-test
	@valgrind \
		--tool=memcheck \
		--suppressions=.valgrind-supressions \
		--error-exitcode=1 \
		--leak-check=full \
		--show-reachable=yes \
		--track-fds=yes \
		--child-silent-after-fork=yes \
		./vlock-test

.PHONY: clean
clean:
	$(RM) vlock-test $(wildcard *.o)
	$(RM) $(wildcard *.gcno) $(wildcard *.gcda) $(wildcard *.gcov)
