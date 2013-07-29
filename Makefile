# vlock makefile

include config.mk

VPATH = src

VLOCK_VERSION = 2.2.3

PROGRAMS = vlock vlock-main

.PHONY: all
all: $(PROGRAMS)

.PHONY: debug
debug:
	@$(MAKE) DEBUG=y

ifeq ($(ENABLE_PLUGINS),yes)
all: plugins
endif

.PHONY: plugins
plugins: modules scripts

.PHONY: modules
modules:
	@$(MAKE) -C modules

.PHONY: scripts
scripts:
	@$(MAKE) -C scripts

.PHONY: check memcheck
check memcheck:
	@$(MAKE) -C tests $@

### configuration ###

config.mk:
	$(info )
	$(info ###################################################)
	$(info # Creating default configuration.                 #)
	$(info # Run ./configure or edit config.mk to customize. #)
	$(info ###################################################)
	$(info )
	@./configure --quiet

### installation rules ###

.PHONY: install
install: install-programs install-man

ifeq ($(ENABLE_PLUGINS),yes)
install: install-plugins
endif

.PHONY: install-programs
install-programs: $(PROGRAMS)
	$(MKDIR_P) -m 755 $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -m 755 -o root -g $(ROOT_GROUP) vlock $(DESTDIR)$(BINDIR)/vlock
	$(MKDIR_P) -m 755 $(DESTDIR)$(PREFIX)/sbin
	$(INSTALL) -m 4711 -o root -g $(ROOT_GROUP) vlock-main $(DESTDIR)$(SBINDIR)/vlock-main

.PHONY: install-plugins
install-plugins: install-modules install-scripts

.PHONY: install-modules
install-modules:
	@$(MAKE) -C modules install

.PHONY: install-scripts
install-scripts:
	@$(MAKE) -C scripts install

.PHONY: install-man
install-man:
	$(MKDIR_P) -m 755 $(DESTDIR)$(MANDIR)/man1
	$(INSTALL) -m 644 -o root -g $(ROOT_GROUP) man/vlock.1 $(DESTDIR)$(MANDIR)/man1/vlock.1
	$(MKDIR_P) -m 755 $(DESTDIR)$(MANDIR)/man8
	$(INSTALL) -m 644 -o root -g $(ROOT_GROUP) man/vlock-main.8 $(DESTDIR)$(MANDIR)/man8/vlock-main.8
	$(MKDIR_P) -m 755 $(DESTDIR)$(MANDIR)/man5
	$(INSTALL) -m 644 -o root -g $(ROOT_GROUP) man/vlock-plugins.5 $(DESTDIR)$(MANDIR)/man5/vlock-plugins.5


### build rules ###

vlock: vlock.sh config.mk Makefile
	$(BOURNE_SHELL) -n $<
	sed \
		-e 's,%BOURNE_SHELL%,$(BOURNE_SHELL),' \
		-e 's,%PREFIX%,$(PREFIX),' \
		-e 's,%VLOCK_VERSION%,$(VLOCK_VERSION),' \
		-e 's,%VLOCK_ENABLE_PLUGINS%,$(ENABLE_PLUGINS),' \
		$< > $@.tmp
	mv -f $@.tmp $@

override CFLAGS += -Isrc

vlock-main: vlock-main.o prompt.o auth-$(AUTH_METHOD).o console_switch.o util.o

auth-pam.o: auth-pam.c prompt.h auth.h
auth-shadow.o: auth-shadow.c prompt.h auth.h
prompt.o: prompt.c prompt.h
vlock-main.o: vlock-main.c auth.h prompt.h util.h
plugins.o: plugins.c tsort.h plugin.h plugins.h list.h util.h
module.o : override CFLAGS += -DVLOCK_MODULE_DIR="\"$(MODULEDIR)\""
module.o: module.c plugin.h list.h util.h
script.o : override CFLAGS += -DVLOCK_SCRIPT_DIR="\"$(SCRIPTDIR)\""
script.o: script.c plugin.h process.h list.h util.h
plugin.o: plugin.c plugin.h list.h util.h
tsort.o: tsort.c tsort.h list.h
list.o: list.c list.h util.h
console_switch.o: console_switch.c console_switch.h
process.o: process.c process.h
util.o: util.c util.h

ifneq ($(ENABLE_ROOT_PASSWORD),yes)
vlock-main.o : override CFLAGS += -DNO_ROOT_PASS
endif

ifeq ($(AUTH_METHOD),pam)
vlock-main : override LDLIBS += $(PAM_LIBS)
endif

ifeq ($(AUTH_METHOD),shadow)
vlock-main : override LDLIBS += $(CRYPT_LIB)
endif

ifeq ($(ENABLE_PLUGINS),yes)
vlock-main: plugins.o plugin.o module.o process.o script.o tsort.o list.o
# -rdynamic is needed so that the all plugin can access the symbols from console_switch.o
vlock-main : override LDFLAGS += -rdynamic
vlock-main : override LDLIBS += $(DL_LIB)
vlock-main.o : override CFLAGS += -DUSE_PLUGINS
vlock-main.o: plugins.h
endif

.PHONY: realclean
realclean: clean
	$(RM) config.mk

.PHONY: clean
clean:
	$(RM) $(PROGRAMS) $(wildcard *.o)
	@$(MAKE) -C modules clean
	@$(MAKE) -C scripts clean
	@$(MAKE) -C tests clean
