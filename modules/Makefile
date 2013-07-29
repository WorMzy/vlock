include ../config.mk

MODULES += $(EXTRA_MODULES)

.PHONY: all
all: $(MODULES)

.PHONY: install
install: $(addprefix install-, $(MODULES))

MODULE_GROUP = $(ROOT_GROUP)
MODULE_MODE = 0755

override CFLAGS += -I../src -fPIC

#special build rules

caca.so : override LDLIBS += -lcaca -lncurses

all.o: all.c ../src/console_switch.h

#generic build rule

%.so : override LDFLAGS += -shared
%.so: %.o
	$(LINK.o) -shared $^ $(LOADLIBES) $(LDLIBS) -o $@

# special installation rules

install-new.so : MODULE_GROUP=$(VLOCK_GROUP)
install-new.so : MODULE_MODE=$(VLOCK_MODULE_MODE)
install-nosysrq.so : MODULE_GROUP=$(VLOCK_GROUP)
install-nosysrq.so : MODULE_MODE=$(VLOCK_MODULE_MODE)

# generic installation rule

.PHONY: install-%.so
install-%.so: %.so
	$(MKDIR_P) -m 755 $(DESTDIR)$(MODULEDIR)
	$(INSTALL) -m $(MODULE_MODE) -o root -g $(MODULE_GROUP) $< $(DESTDIR)$(MODULEDIR)/$<

.PHONY: clean
clean:
	$(RM) $(wildcard *.o) $(wildcard *.so)
