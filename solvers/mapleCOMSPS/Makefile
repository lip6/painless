###################################################################################################

.PHONY:	r d p sh cr cd cp csh lr ld lp lsh config all install install-headers install-lib\
        install-bin clean distclean
all:	r lr lsh

## Load Previous Configuration ####################################################################

-include config.mk

## Configurable options ###########################################################################

# Directory to store object files, libraries, executables, and dependencies:
BUILD_DIR      ?= build

# Include debug-symbols in release builds
MAPLE_RELSYM ?= -g

# Sets of compile flags for different build types
MAPLE_REL    ?= -O3 -D NDEBUG
MAPLE_DEB    ?= -O0 -D DEBUG
MAPLE_PRF    ?= -O3 -D NDEBUG
MAPLE_FPIC   ?= -fpic

# GNU Standard Install Prefix
prefix         ?= /usr/local

## Write Configuration  ###########################################################################

config:
	@( echo 'BUILD_DIR?=$(BUILD_DIR)'           ; \
	   echo 'MAPLE_RELSYM?=$(MAPLE_RELSYM)' ; \
	   echo 'MAPLE_REL?=$(MAPLE_REL)'       ; \
	   echo 'MAPLE_DEB?=$(MAPLE_DEB)'       ; \
	   echo 'MAPLE_PRF?=$(MAPLE_PRF)'       ; \
	   echo 'MAPLE_FPIC?=$(MAPLE_FPIC)'     ; \
	   echo 'prefix?=$(prefix)'                 ) > config.mk

## Configurable options end #######################################################################

INSTALL ?= install

# GNU Standard Install Variables
exec_prefix ?= $(prefix)
includedir  ?= $(prefix)/include
bindir      ?= $(exec_prefix)/bin
libdir      ?= $(exec_prefix)/lib
datarootdir ?= $(prefix)/share
mandir      ?= $(datarootdir)/man

# Target file names
MAPLE      = mapleCOMSPS#       Name of MiniSat main executable.
MAPLE_CORE = mapleCOMSPS_core#  Name of simplified MiniSat executable (only core solver support).
MAPLE_SLIB = lib$(MAPLE).a#  Name of MiniSat static library.
MAPLE_DLIB = lib$(MAPLE).so# Name of MiniSat shared library.

# Shared Library Version
SOMAJOR=2
SOMINOR=1
SORELEASE?=.0#   Declare empty to leave out from library file name.

MAPLE_CXXFLAGS = -I. -I../../libs/m4ri-20200125 -D __STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS -Wall -Wno-parentheses -Wextra -std=c++11
MAPLE_LDFLAGS  = -Wall -lz -L../../libs/m4ri-20200125/.libs -lm4ri

ECHO=@echo
ifeq ($(VERB),)
VERB=@
else
VERB=
endif

SRCS = $(wildcard mapleCOMSPS/core/*.cc) $(wildcard mapleCOMSPS/simp/*.cc) $(wildcard mapleCOMSPS/utils/*.cc)
HDRS = $(wildcard mapleCOMSPS/mtl/*.h) $(wildcard mapleCOMSPS/core/*.h) $(wildcard mapleCOMSPS/simp/*.h) $(wildcard mapleCOMSPS/utils/*.h)
OBJS = $(filter-out %Main.o, $(SRCS:.cc=.o))

r:	$(BUILD_DIR)/release/bin/$(MAPLE)
d:	$(BUILD_DIR)/debug/bin/$(MAPLE)
p:	$(BUILD_DIR)/profile/bin/$(MAPLE)
sh:	$(BUILD_DIR)/dynamic/bin/$(MAPLE)

cr:	$(BUILD_DIR)/release/bin/$(MAPLE_CORE)
cd:	$(BUILD_DIR)/debug/bin/$(MAPLE_CORE)
cp:	$(BUILD_DIR)/profile/bin/$(MAPLE_CORE)
csh:	$(BUILD_DIR)/dynamic/bin/$(MAPLE_CORE)

lr:	$(BUILD_DIR)/release/lib/$(MAPLE_SLIB)
ld:	$(BUILD_DIR)/debug/lib/$(MAPLE_SLIB)
lp:	$(BUILD_DIR)/profile/lib/$(MAPLE_SLIB)
lsh:	$(BUILD_DIR)/dynamic/lib/$(MAPLE_DLIB).$(SOMAJOR).$(SOMINOR)$(SORELEASE)

## Build-type Compile-flags:
$(BUILD_DIR)/release/%.o:			MAPLE_CXXFLAGS +=$(MAPLE_REL) $(MAPLE_RELSYM)
$(BUILD_DIR)/debug/%.o:				MAPLE_CXXFLAGS +=$(MAPLE_DEB) -g
$(BUILD_DIR)/profile/%.o:			MAPLE_CXXFLAGS +=$(MAPLE_PRF) -pg
$(BUILD_DIR)/dynamic/%.o:			MAPLE_CXXFLAGS +=$(MAPLE_REL) $(MAPLE_FPIC)

## Build-type Link-flags:
$(BUILD_DIR)/profile/bin/$(MAPLE):		MAPLE_LDFLAGS += -pg
$(BUILD_DIR)/release/bin/$(MAPLE):		MAPLE_LDFLAGS += --static $(MAPLE_RELSYM)
$(BUILD_DIR)/profile/bin/$(MAPLE_CORE):	MAPLE_LDFLAGS += -pg
$(BUILD_DIR)/release/bin/$(MAPLE_CORE):	MAPLE_LDFLAGS += --static $(MAPLE_RELSYM)

## Executable dependencies
$(BUILD_DIR)/release/bin/$(MAPLE):	 	$(BUILD_DIR)/release/mapleCOMSPS/simp/Main.o $(BUILD_DIR)/release/lib/$(MAPLE_SLIB)
$(BUILD_DIR)/debug/bin/$(MAPLE):	 	$(BUILD_DIR)/debug/mapleCOMSPS/simp/Main.o $(BUILD_DIR)/debug/lib/$(MAPLE_SLIB)
$(BUILD_DIR)/profile/bin/$(MAPLE):	 	$(BUILD_DIR)/profile/mapleCOMSPS/simp/Main.o $(BUILD_DIR)/profile/lib/$(MAPLE_SLIB)
# need the main-file be compiled with fpic?
$(BUILD_DIR)/dynamic/bin/$(MAPLE):	 	$(BUILD_DIR)/dynamic/mapleCOMSPS/simp/Main.o $(BUILD_DIR)/dynamic/lib/$(MAPLE_DLIB)

## Executable dependencies (core-version)
$(BUILD_DIR)/release/bin/$(MAPLE_CORE):	$(BUILD_DIR)/release/mapleCOMSPS/core/Main.o $(BUILD_DIR)/release/lib/$(MAPLE_SLIB)
$(BUILD_DIR)/debug/bin/$(MAPLE_CORE):	 	$(BUILD_DIR)/debug/mapleCOMSPS/core/Main.o $(BUILD_DIR)/debug/lib/$(MAPLE_SLIB)
$(BUILD_DIR)/profile/bin/$(MAPLE_CORE):	$(BUILD_DIR)/profile/mapleCOMSPS/core/Main.o $(BUILD_DIR)/profile/lib/$(MAPLE_SLIB)
# need the main-file be compiled with fpic?
$(BUILD_DIR)/dynamic/bin/$(MAPLE_CORE): 	$(BUILD_DIR)/dynamic/mapleCOMSPS/core/Main.o $(BUILD_DIR)/dynamic/lib/$(MAPLE_DLIB)

## Library dependencies
$(BUILD_DIR)/release/lib/$(MAPLE_SLIB):	$(foreach o,$(OBJS),$(BUILD_DIR)/release/$(o))
$(BUILD_DIR)/debug/lib/$(MAPLE_SLIB):		$(foreach o,$(OBJS),$(BUILD_DIR)/debug/$(o))
$(BUILD_DIR)/profile/lib/$(MAPLE_SLIB):	$(foreach o,$(OBJS),$(BUILD_DIR)/profile/$(o))
$(BUILD_DIR)/dynamic/lib/$(MAPLE_DLIB).$(SOMAJOR).$(SOMINOR)$(SORELEASE)\
 $(BUILD_DIR)/dynamic/lib/$(MAPLE_DLIB).$(SOMAJOR)\
 $(BUILD_DIR)/dynamic/lib/$(MAPLE_DLIB):	$(foreach o,$(OBJS),$(BUILD_DIR)/dynamic/$(o))

## Compile rules (these should be unified, buit I have not yet found a way which works in GNU Make)
$(BUILD_DIR)/release/%.o:	%.cc
	$(ECHO) Compiling: $@
	$(VERB) mkdir -p $(dir $@)
	$(VERB) $(CXX) $(MAPLE_CXXFLAGS) $(CXXFLAGS) -c -o $@ $< -MMD -MF $(BUILD_DIR)/release/$*.d

$(BUILD_DIR)/profile/%.o:	%.cc
	$(ECHO) Compiling: $@
	$(VERB) mkdir -p $(dir $@)
	$(VERB) $(CXX) $(MAPLE_CXXFLAGS) $(CXXFLAGS) -c -o $@ $< -MMD -MF $(BUILD_DIR)/profile/$*.d

$(BUILD_DIR)/debug/%.o:	%.cc
	$(ECHO) Compiling: $@
	$(VERB) mkdir -p $(dir $@)
	$(VERB) $(CXX) $(MAPLE_CXXFLAGS) $(CXXFLAGS) -c -o $@ $< -MMD -MF $(BUILD_DIR)/debug/$*.d

$(BUILD_DIR)/dynamic/%.o:	%.cc
	$(ECHO) Compiling: $@
	$(VERB) mkdir -p $(dir $@)
	$(VERB) $(CXX) $(MAPLE_CXXFLAGS) $(CXXFLAGS) -c -o $@ $< -MMD -MF $(BUILD_DIR)/dynamic/$*.d

## Linking rule
$(BUILD_DIR)/release/bin/$(MAPLE) $(BUILD_DIR)/debug/bin/$(MAPLE) $(BUILD_DIR)/profile/bin/$(MAPLE) $(BUILD_DIR)/dynamic/bin/$(MAPLE)\
$(BUILD_DIR)/release/bin/$(MAPLE_CORE) $(BUILD_DIR)/debug/bin/$(MAPLE_CORE) $(BUILD_DIR)/profile/bin/$(MAPLE_CORE) $(BUILD_DIR)/dynamic/bin/$(MAPLE_CORE):
	$(ECHO) Linking Binary: $@
	$(VERB) mkdir -p $(dir $@)
	$(VERB) $(CXX) $^ $(MAPLE_LDFLAGS) $(LDFLAGS) -o $@

## Static Library rule
%/lib/$(MAPLE_SLIB):
	$(ECHO) Linking Static Library: $@
	$(VERB) mkdir -p $(dir $@)
	$(VERB) $(AR) -rcs $@ $^

## Shared Library rule
$(BUILD_DIR)/dynamic/lib/$(MAPLE_DLIB).$(SOMAJOR).$(SOMINOR)$(SORELEASE)\
 $(BUILD_DIR)/dynamic/lib/$(MAPLE_DLIB).$(SOMAJOR)\
 $(BUILD_DIR)/dynamic/lib/$(MAPLE_DLIB):
	$(ECHO) Linking Shared Library: $@
	$(VERB) mkdir -p $(dir $@)
	$(VERB) $(CXX) $(MAPLE_LDFLAGS) $(LDFLAGS) -o $@ -shared -Wl,-soname,$(MAPLE_DLIB).$(SOMAJOR) $^
	$(VERB) ln -sf $(MAPLE_DLIB).$(SOMAJOR).$(SOMINOR)$(SORELEASE) $(BUILD_DIR)/dynamic/lib/$(MAPLE_DLIB).$(SOMAJOR)
	$(VERB) ln -sf $(MAPLE_DLIB).$(SOMAJOR) $(BUILD_DIR)/dynamic/lib/$(MAPLE_DLIB)

install:	install-headers install-lib install-bin
install-debug:	install-headers install-lib-debug

install-headers:
#       Create directories
	$(INSTALL) -d $(DESTDIR)$(includedir)/mapleCOMSPS
	for dir in mtl utils core simp; do \
	  $(INSTALL) -d $(DESTDIR)$(includedir)/mapleCOMSPS/$$dir ; \
	done
#       Install headers
	for h in $(HDRS) ; do \
	  $(INSTALL) -m 644 $$h $(DESTDIR)$(includedir)/$$h ; \
	done

install-lib-debug: $(BUILD_DIR)/debug/lib/$(MAPLE_SLIB)
	$(INSTALL) -d $(DESTDIR)$(libdir)
	$(INSTALL) -m 644 $(BUILD_DIR)/debug/lib/$(MAPLE_SLIB) $(DESTDIR)$(libdir)

install-lib: $(BUILD_DIR)/release/lib/$(MAPLE_SLIB) $(BUILD_DIR)/dynamic/lib/$(MAPLE_DLIB).$(SOMAJOR).$(SOMINOR)$(SORELEASE)
	$(INSTALL) -d $(DESTDIR)$(libdir)
	$(INSTALL) -m 644 $(BUILD_DIR)/dynamic/lib/$(MAPLE_DLIB).$(SOMAJOR).$(SOMINOR)$(SORELEASE) $(DESTDIR)$(libdir)
	ln -sf $(MAPLE_DLIB).$(SOMAJOR).$(SOMINOR)$(SORELEASE) $(DESTDIR)$(libdir)/$(MAPLE_DLIB).$(SOMAJOR)
	ln -sf $(MAPLE_DLIB).$(SOMAJOR) $(DESTDIR)$(libdir)/$(MAPLE_DLIB)
	$(INSTALL) -m 644 $(BUILD_DIR)/release/lib/$(MAPLE_SLIB) $(DESTDIR)$(libdir)

install-bin: $(BUILD_DIR)/dynamic/bin/$(MAPLE)
	$(INSTALL) -d $(DESTDIR)$(bindir)
	$(INSTALL) -m 755 $(BUILD_DIR)/dynamic/bin/$(MAPLE) $(DESTDIR)$(bindir)

clean:
	rm -rf $(foreach t, release debug profile dynamic, $(foreach o, $(SRCS:.cc=.o), $(BUILD_DIR)/$t/$o)) \
          $(foreach t, release debug profile dynamic, $(foreach d, $(SRCS:.cc=.d), $(BUILD_DIR)/$t/$d)) \
	  $(foreach t, release debug profile dynamic, $(BUILD_DIR)/$t/bin/$(MAPLE_CORE) $(BUILD_DIR)/$t/bin/$(MAPLE)) \
	  $(foreach t, release debug profile, $(BUILD_DIR)/$t/lib/$(MAPLE_SLIB)) \
	  $(BUILD_DIR)/dynamic/lib/$(MAPLE_DLIB).$(SOMAJOR).$(SOMINOR)$(SORELEASE)\
	  $(BUILD_DIR)/dynamic/lib/$(MAPLE_DLIB).$(SOMAJOR)\
	  $(BUILD_DIR)/dynamic/lib/$(MAPLE_DLIB) \
     $(BUILD_DIR)

distclean:	clean
	rm -f config.mk

## Include generated dependencies
-include $(foreach s, $(SRCS:.cc=.d), $(BUILD_DIR)/release/$s)
-include $(foreach s, $(SRCS:.cc=.d), $(BUILD_DIR)/debug/$s)
-include $(foreach s, $(SRCS:.cc=.d), $(BUILD_DIR)/profile/$s)
-include $(foreach s, $(SRCS:.cc=.d), $(BUILD_DIR)/dynamic/$s)
