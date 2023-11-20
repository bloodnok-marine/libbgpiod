# ----------
#  Makefile
# 
#       Makefile for libbgpiod
# 
#       Copyright (c) 2023 Marc Munro
#       Fileset: libbgpiod - basic/bloodnok gpio device library
#       Author:  Marc Munro
# 	License: GPL-3.0
#  
#  ----------
#
# Note that the bgpiod directory hierarchy does not use recursive make
# (see the article "Recursive Make Considered Harmful for a
# rationale).  Instead, this is the sole target-defining Makefile.  In
# subdirectories you may find links to GNUmakefile.  If make is run in
# such a directory it will change directory to the parent directory
# and run the same make command, meaning that this makefile will be
# used, even from subdirectories.  This allows builds to be done from
# any directory, with proper tracking of where the build takes place
# for tools such as emacs' compile and next-error.
#
# Note that the examples directory has its own simple, dedicated
# Makefile for building the examples.


# Phony targets are those that do not create a file of the same name.
# These will always be considered out of date and will always be
# built.
#
.PHONY:	DEFAULT all xfer prep_for_xfer deps \
	unit runit systest rsystest \
	gitdocs pages doc docs man help \
	install uninstall \
	tarball tar \
	deb \
	tidy do_tidy clean do_clean distclean

# Do not use make's built-in rules
# (this improves performance and avoids hard-to-debug behaviour).
#
MAKEFLAGS += -r

# Include definitions created by configure.
#
ifndef NODEPS
include Makefile.global
endif

################################################################
# Definitions
#

MAJOR_VERSION=$(shell echo $(PKG_VERSION) | cut -d. -f1)

LIBNAME = $(PKG_BASENAME)
SONAME = $(LIBNAME).so.$(MAJOR_VERSION)

# Tools are the executables shipped with the package.
# They are defined in the tools directory, with executables being
# placed into the root directory.  Note that they all begin with the
# prefix 'bgpio'.  This distinguishes them from files containing
# 'helper' functions. 
#
TOOL_SOURCES = $(wildcard tools/bgpio*.c)
TOOLS = $(subst tools/,,$(TOOL_SOURCES:%.c=%))
ALL_TOOL_SOURCES = $(wildcard tools/*.c)
TOOL_OBJECTS = $(ALL_TOOL_SOURCES:%.c=%.o)

HELPER_SOURCES = $(filter-out $(TOOL_SOURCES),$(wildcard tools/*.c))
HELPER_OBJECTS = $(HELPER_SOURCES:%.c=%.o)

# The sources for the library can be found in the lib directory.
#
LIB_SOURCES = $(wildcard lib/*.c)
LIB_HEADERS = $(wildcard lib/*.h)
LIB_OBJECTS = $(LIB_SOURCES:%.c=%.o)

SOURCE_DIRS = lib tools
SOURCES = $(foreach dir,$(SOURCE_DIRS),$(wildcard $(dir)/*.c))
HEADERS = $(foreach dir,$(SOURCE_DIRS),$(wildcard $(dir)/*.h))
ALL_SOURCES = $(SOURCES) $(HEADERS)
ALL_OBJECTS = $(TOOL_OBJECTS) $(LIB_OBJECTS)

# man pages are built in the man directory, using help2man, from
# running the tools with the -h option.  From the manpages, we also
# build some html pages which get included into the doxygen
# documentation. 
#
MANPAGES = $(TOOLS:%=man/%.1)
MANPAGES_HTML = $(TOOLS:%=man/%.html)
MANPAGE_FILES = $(notdir $(MANPAGES))

OTHER_DOC_SOURCES = doc/libbgpiod.md $(MANPAGES_HTML) \
	$(wildcard examples/*.c)

# Files that configure manipulates.  If any of these get modified, we
# have to re-run configure.
#
CONFIGURE_INFILES = Makefile.global.in tools/bgpiotools.h.in 
HEADERS += tools/bgpiotools.h.in

ifdef DOXYGEN
  CONFIGURE_INFILES += doc/Doxyfile.in
endif

CONFIGURE_TARGETS = $(CONFIGURE_INFILES:%.in=%)

# The examples directory contains some simple example programs.  These
# are referenced from the documentation.
#
EXAMPLES = $(wildcard examples/*.c) examples/Makefile

# Shared library
SHLIB = $(LIBNAME).so

# Static library
STLIB = $(LIBNAME).a

LIBS = $(SHLIB).$(PKG_VERSION) $(STLIB)

# Everything we will build for the default target,
#
DEFAULT_TARGETS = $(TOOLS) $(STLIB) $(SHLIB) man doc

# Everything that will be build by the "all" target.
#
ALL_TARGETS = $(DEFAULT_TARGETS) doc

# Dependency files.  These are suto-generated by this Makefile and
# removed by the clean target.
#
DEPS = $(SOURCES:%.c=%.d) man/man.d  tools/tools.d

# Remote host connect string for ssh, for remote testing
#
REMOTE = lab

# test files
#
ALL_TESTS = $(shell find tests -type f ! -name '*~')
ALL_TESTTOOLS = bin/shunit2

# Define glob patterns for garbage files that "tidy" targets should
# remove.
#
garbage := \\\#*  .\\\#*  *~ 
ALL_GARBAGE = $(garbage) $(garbage:%=lib/%) \
	      $(garbage:%=tools/%) $(garbage:%=doc/%) \
	      $(garbage:%=man/%) $(garbage:%=examples/%) \
	      $(garbage:%=tests/%) $(garbage:%=tests/*/%) \
	      $(garbage:%=debian/%) $(garbage:%=bin/%)


###########
# Verbosity control.  Define VERBOSE on the command line to show the
# full compilation, etc commands.  If VERBOSE is defined $(FEEDBACK)
# will do nothing and $(AT) will have no effect, otherwise $(FEEDBACK)
# will perform an echo and $(AT) will make the command that follows it
# execute quietly.
# FEEDBACK2 is used in multi-line shell commands where part of the
# command can usefully provide feedback.
#
VERBOSE = 1
ifdef VERBOSE
    FEEDBACK = @true
    FEEDBACK2 = true
    AT =
else
    FEEDBACK = @echo
    FEEDBACK2 = echo
    AT = @
endif

# For compilation feedback
CCNAME := $(shell echo $(CC) | tr '[a-z]' '[A-Z]')


################################################################
# Default target.  Must appear as first target in this Makefile
#

DEFAULT: $(DEFAULT_TARGETS)

all: $(ALL_TARGETS)


# The following include must not precede the first target as
# dependencies are specified as targets, and we need the DEFAULT
# target to appear first.
#
# Including $(DEPS) here will cause all dependencies to be rebuilt as
# needed.  Any compilation failures will prevent the build from
# succeeding, so the user may define NODEPS=y on the command line to
# prevent this, allowing targets that require no compilations to run
# regardless.  This may help in debugging the Makefile.
#
ifndef NODEPS
  -include $(DEPS)
endif

# Explicit deps target.  Primarily for debugging.
#
deps: $(DEPS)

$(DEPS): $(CONFIGURE_TARGETS)

# Dependency file for each executable being based on its namesake
# object in the tools dir.
#
tools/tools.d: $(TOOL_SOURCES) $(SHLIB)
	$(FEEDBACK) "  MAKE DEP" $@
	$(AT) echo $(TOOL_SOURCES) | xargs -n 1 | \
	    sed -e 's!\(.*/\(.*\)\).c!\2: \1.o $(SHLIB)!' >$@ || rm $@

# Create dependency file for each man page being based on its namesake
# executable.
#
man/man.d:
	$(FEEDBACK) "  MAKE DEP" $@
	$(AT)echo "$(TOOLS)" | xargs -n 1 | \
	    sed -e 's!\(.*\)!man/\1.1: \1!' >$@ || rm $@


################################################################
# xfer.  Transfer files to remote host and build there.
#
xfer: .xfer $(DEFAULT_TARGETS) Makefile
	@ssh $(REMOTE) "cd bgpio; make prep_for_xfer  NODEPS=y" || \
	    scp Makefile $(REMOTE):bgpio; \
	    ssh $(REMOTE) "cd bgpio; make prep_for_xfer NODEPS=y"
	@echo Copying `cat .xfer`...
	@xargs -n 1 <.xfer | sort -u | xargs -i scp {} $(REMOTE):bgpio/{}
	@echo >.xfer
	@echo Performing remote build...
	@# (Also run a minimal test)
	@ssh $(REMOTE) "cd bgpio; make"

# Update the .xfer file with all updated sources.  These can then be
# copied across by the xfer target.  This means that we only transfer
# files that are sources and newer than the files we last transfered.
# Since this may not be entirely reliable the .xfer file may be
# removed by running the tidy or clean targets, to cause xfer to
# transfer all relevant files.
#
.xfer:  $(ALL_SOURCES) $(CONFIGURE_INFILES) \
	    $(EXAMPLES) $(MANPAGES) Makefile configure 
	@echo $? >>.xfer


# Ensure that target subdirectories exist and that the configure
# generated files are writable (can be overwritten).
prep_for_xfer:
	@echo Preparing for xfer to `hostname`
	@mkdir -p lib tools man doc examples tests \
		tests/common tests/Potato bin
	@chmod +w -f tools/bgpiotools.h Makefile.global || true
	@[ -f doc/Doxyfile ] && chmod +w -f doc/Doxyfile || true

runit: .xfer_tests xfer
	@echo "Copying tests to $(REMOTE)..."
	@xargs -n 1 <.xfer_tests | sort -u | xargs -i scp {} $(REMOTE):bgpio/{}
	@echo >.xfer_tests
	@echo "Running remote tests..."
	@ssh $(REMOTE) "cd bgpio; make unit"

rsystest: .xfer_tests xfer
	@echo "Copying tests to $(REMOTE)..."
	@xargs -n 1 <.xfer_tests | sort -u | xargs -i scp {} $(REMOTE):bgpio/{}
	@echo >.xfer_tests
	@echo "Running remote tests..."
	@ssh $(REMOTE) "cd bgpio; make systest"

.xfer_tests: $(ALL_TESTS) $(ALL_TESTTOOLS)
	@echo $? >>.xfer_tests


################################################################
# Test targets
#
unit: $(DEFAULT_TARGETS)
	@echo "Running tests..."
	@tests/unit

systest: $(DEFAULT_TARGETS)
	@echo "Running tests..."
	@tests/system


################################################################
# Install targets
#

# ARCH will be defined in Multiarch Debian build environments
#
ARCH = $(DEB_HOST_MULTIARCH)

install: $(DEFAULT_TARGETS)
	$(FEEDBACK) INSTALL $(LIBS)
	$(AT) install -D \
	    --target-directory=$(DESTDIR)$(prefix)/lib/$(ARCH) $(LIBS)
	@cd $(DESTDIR)$(prefix)/lib/$(ARCH); \
	  ln -s $(SHLIB).$(PKG_VERSION) $(SHLIB) 2>/dev/null; \
	  ln -s $(SHLIB).$(PKG_VERSION) $(SONAME) 2>/dev/null; true
	$(FEEDBACK) INSTALL $(LIB_HEADERS)
	$(AT) install -D \
	    --target-directory=$(DESTDIR)$(prefix)/include/bgpiod \
	    $(LIB_HEADERS)
	$(FEEDBACK) INSTALL $(TOOLS)
	$(AT) install -D \
	    --target-directory=$(DESTDIR)$(prefix)/bin/ $(TOOLS) 
	$(FEEDBACK) INSTALL $(MANPAGES)
	-$(AT) install -D \
	    --target-directory=$(DESTDIR)$(prefix)/share/man/man1 \
	    $(MANPAGES) 
	$(FEEDBACK) INSTALL doc/html
	-$(AT) install -D \
	    --target-directory=$(DESTDIR)$(prefix)/share/doc/libbgpiod/html \
	    doc/html/*
	@echo You should now run ldconfig

uninstall:
	$(FEEDBACK) UNINSTALL $(LIBS)
	$(AT) rm $(LIBS:%=$(DESTDIR)$(prefix)/lib/%) || true
	$(AT) rm $(SHLIB:%=$(DESTDIR)$(prefix)/lib/%) || true
	$(AT) rm $(STLIB:%=$(DESTDIR)$(prefix)/lib/%) || true
	$(FEEDBACK) UNINSTALL $(LIB_HEADERS)
	$(AT) rm -rf $(DESTDIR)$(prefix)/include/bgpio || true
	$(FEEDBACK) UNINSTALL $(TOOLS)
	$(AT) rm $(TOOLS:%=$(DESTDIR)$(prefix)/bin/%) || true
	$(FEEDBACK) UNINSTALL $(MANPAGE_FILES)
	$(AT) rm $(MANPAGE_FILES:%=$(DESTDIR)$(prefix)/share/man/man1/%) || true
	$(FEEDBACK) UNINSTALL doc/html
	$(AT)rm -rf $(DESTDIR)$(prefix)/share/doc/libbgpiod || true

tarball tar: clean $(TARNAME)

$(TARNAME):
	@echo "Creating $(TARNAME)..."
	@cd ..; \
	 	 tar --create \
	     --exclude='.git*' \
	     --exclude='*gz' \
	     --exclude='autom4te.cache' \
	     --exclude=TODO \
	     --exclude=deb-build \
	     --exclude=doc/doxygen.log \
	     --transform='s!^$(DIRNAME)\(/\|$$\)!$(PKGNAME)\1!' \
	     -f - "$(DIRNAME)" | xz -z - >$(TARNAME)
	@mv ../$(TARNAME) .
	@echo Done

# Create debian package
#
DEB_NAME  = $(PKG_BASENAME)_$(PKG_VERSION)
DEB_TAR   = $(DEB_NAME).orig.tar.gz
DEB_DIR   = packaging/$(PKGNAME)/debian
deb: clean $(TARNAME)
	set -x; cd deb-build; \
	cp ../$(TARNAME) .; \
	xzcat $(TARNAME) | tar xvf -; \
	cd $(PKGNAME) && \
	debmake --targz=tar.xz --yes; \
	debuild -us -uc

debclean:
	@rm -rf deb-build/* 2>/dev/null || true


################################################################
# Clean (and tidy) targets
#

# Tidy is a clean-up target that just removes garbage files from
# everywhere
tidy: do_tidy
	@echo Done

# Target to ensure that running make tidy will give some feedback even
# when nothing is to be done
#
do_tidy:
	@ echo Removing garbage...
	@rm -f .xfer .xfer_tests $(ALL_GARBAGE)  2>/dev/null
	@rm -f $(TARNAME) 2>/dev/null || true
	@rm -f config.log config.status  2>/dev/null 

clean: do_clean
	@echo Done

do_clean: do_tidy debclean
	@ echo Removing generated files...
	@rm -rf doc/html 2>/dev/null || true
	@rm -f $(ALL_TARGETS) $(ALL_OBJECTS) $(SHLIB) $(STLIB) $(LIBS) \
		$(MANPAGES) $(MANPAGES_HTML) $(DEPS) \
		$(CONFIGURE_TARGETS) 2>/dev/null || true
	@rm -rf external 2>/dev/null || true

distclean: do_clean 
	@ echo Removing autoconf generated files...
	@rm -rf Makefile.global configure doc/Doxyfile \
	        aclocal.m4 autom4te.cache 2>/dev/null || true
	@echo Done




################################################################
# Autconf related targets
# These provide dependencies to determine when autoconf, configure,
# etc need to be re-run.  This means that you do not need to manually
# run configure, but it will do no harm.  Make should be the only tool
# you need to run.

# Automatically re-run configure when needed
#

tools/bgpiotools.h: tools/bgpiotools.h.in
Makefile.global: Makefile.global.in

# Run ./configure when any of the configure input files have been
# updated.
# Note that this makes the configure-generated files read-only
# to discourage accidentally editing them.
# Note also the use of "grouped targets" ("&:").
#
$(CONFIGURE_TARGETS)  &: configure
	./configure
	chmod -w -f tools/bgpiotools.h Makefile.global
	-[ -f doc/Doxyfile ] && chmod -w -f doc/Doxyfile

# Automatically regenerate configure when configure.ac has been
# updated.  This rule allows for the non-existence of configure.ac and
# aclocal.m4, which will be the case with a distributed tarball,
# allowing builds in the absence of autotools.
#
configure: configure.ac 
	@if [ -f configure.ac ]; then \
	    if [ ! -d aclocal.m4 ]; then \
	        echo aclocal; \
	        aclocal; \
	    fi; \
	    echo autoconf; \
	    autoconf; \
	fi

# Do nothing.  This allows make to work in the absence of configure.ac
# and autoconf.
#
configure.ac:
	@>/dev/null

# Regenerate the aclocal m4 directory for autoconf
#
aclocal.m4:
	aclocal


################################################################
# Main targets
#

$(STLIB): $(LIB_OBJECTS)
$(SHLIB): $(LIB_OBJECTS)

# All tools depend on the tools objects and the static library
$(TOOLS): tools/utils.o tools/vectors.o $(STLIB)

# Link command for each tool executable
$(TOOLS): $(SHLIB)
	$(FEEDBACK) "  LINK" $@
	$(AT) $(CC) $(LDFLAGS) -o $@ tools/$@.c \
	    tools/utils.o tools/vectors.o $(STLIB)


################################################################
# Per file-type targets
#

# Compile and create dependency and object files

%.o: %.c
	$(FEEDBACK) "  $(CCNAME)" $@
	$(AT) $(CC) -c $(CFLAGS) $(CPPFLAGS) -fPIC $*.c -o $*.o

%.d: %.c
	$(FEEDBACK) "  MAKE DEP" $@
	$(AT) $(CC) -M -MT $*.o $(CFLAGS) $(CPPFLAGS) $*.c > $*.d

%.so:
	$(FEEDBACK) "  $(CCNAME)" $@
	$(AT) $(CC) $(LDFLAGS) -shared -fPIC -Wl,-soname,$(SONAME) \
	    -o $@.$(PKG_VERSION) $<
	@ln -s $@.$(PKG_VERSION) $@ 2>/dev/null || true

%.a:
	$(FEEDBACK) "  AR" $@
	$(AT) ar rcs $@ $<

%.1:
	$(FEEDBACK) "  HELP2MAN" $@
	$(AT) name=`grep "#define.*SUMMARY" tools/$<.c | cut -d" " -f3-`; \
	    help2man --name="$${name:- from libbgpiod}" \
	    --no-info ./$(*F) >$@ || rm $@

# Building html versions of man pages.  Note that we change h1 to h3
# in the output, as that renders better and is easier than messing with
# the css.
%.html: %.1
	pandoc --from man --to html $< | sed -e 's/<h1>/<h3>/' >$@

################################################################
# Documentation targets

ifdef DOXYGEN

# Create a local copy of gpio.h so that its structures can be
# documented.
external/gpio.h: lib/bgpiod.d
	@[ -d external ] || mkdir external
	$(AT) if [ ! -h external/gpio.h ]; then \
	    $(FEEDBACK2) CP $@; \
	    cat lib/bgpiod.d | xargs -n 1 | grep gpio.h | \
	      xargs -i cp {} external; \
	fi

doc/Doxyfile: doc/Doxyfile.in

doc/html: $(ALL_SOURCES) $(OTHER_DOC_SOURCES) doc/Doxyfile \
	external/gpio.h
	@echo Building html documentation using doxygen...
	$(DOXYGEN) doc/Doxyfile >doc/doxygen.log 2>&1
	@touch $@

else
doc/html: $(ALL_SOURCES) 
	@echo "\nDoxygen is not installed - cannot build docs.\n"
endif

doc docs: doc/html

gitdocs pages: doc
	@echo "Preparing for release to github-pages..."
	git checkout gh-pages
	( \
	  mv doc dox; \
	  mv dox/html doc; \
	  git add doc; \
	  git commit -a -m "Doc release `date +%Y%m%d-%H%M%S`"; \
	  git push; \
	  echo "Returning to master branch..."; \
	  rm -rf dox doc 2>&1; \
	  true )
	git checkout -f master

ifdef HELP2MAN
man:	$(MANPAGES)
else
man:
	@echo "help2man is not available: unable to build man pages."
endif


################################################################
# Provide some helpful clues for developers
#
help:
	@echo "\nMajor targets of bgpio's Makefile:"
	@echo "  all         - build everything (lib, tools, doc)"
	@echo "  clean       - remove all generated, backup and target files"
	@echo "  distclean   - as clean but also remove all auto-generated files"
	@echo "  doc        - build doxygen documentation (into doc/html)"
	@echo "  gitdocs     - release docs to github-pages"
	@echo "  help        - list major makefile targets"
	@echo "  install     - install lib, headers and tools"
	@echo "  man         - build man pages"
	@echo "  pages       - release docs to github-pages (alias for gitdocs)"
	@echo "  runit       - run unit tests on target host"
	@echo "  rsystest    - run systest on target host"
	@echo "  systest     - run system integration/consistency tests"
	@echo "  tar         - create a tarball for distribution"
	@echo "  tarball     - create a tarball for distribution"
	@echo "  tidy        - remove all garbage files (such as emacs backups)"
	@echo "  unit        - run unit tests"
	@echo "  uninstall   - de-install lib, headers and tools"
	@echo "  xfer        - transfer sources to target host and build"
	@echo "\nTo increase feedback, define VERBOSE=y on the command line."
	@echo "To prevent dependencies being rebuilt, define NODEPS=y."
	@echo "\nFor more information read this Makefile."
