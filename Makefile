# rawhide - find files using pretty C expressions
# https://raf.org/rawhide
# https://github.com/raforg/rawhide
# https://codeberg.org/raforg/rawhide
#
# Copyright (C) 1990 Ken Stauffer, 2022-2023 raf <raf@raf.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <https://www.gnu.org/licenses/>.
#
# 20230609 raf <raf@raf.org>

DESTDIR =
#PREFIX = /usr
#PREFIX = /usr/pkg
#PREFIX = /usr/local
#PREFIX = /opt/local
PREFIX = /usr/local

ETCDIR = /etc
#ETCDIR = $(PREFIX)/etc

APP_INSDIR = $(PREFIX)/bin
APP_CFGDIR = $(ETCDIR)
MAN_INSDIR = $(PREFIX)/share/man

APP_MANSECT = 1
APP_MANSECTNAME = User Commands
APP_MANDIR = $(MAN_INSDIR)/man$(APP_MANSECT)
FMT_MANSECT = 5
FMT_MANSECTNAME = File Formats
FMT_MANDIR = $(MAN_INSDIR)/man$(FMT_MANSECT)
MAN_GZIP = 0

RAWHIDE_NAME = rawhide
RAWHIDE_VERSION = 3.2
RAWHIDE_DATE = 20230609
RAWHIDE_PROG_NAME = rh
RAWHIDE_ID = $(RAWHIDE_NAME)-$(RAWHIDE_VERSION)
RAWHIDE_DIST = $(RAWHIDE_ID).tar.gz

RAWHIDE_CONF = rawhide.conf
RAWHIDE_CONFD = rawhide.conf.d
RAWHIDE_CONFD_ATTRIBUTES = attributes
RAWHIDE_CONFD_ATTRIBUTES_LINUX = attributes.linux
RAWHIDE_CONFD_ATTRIBUTES_RESERVE = attributes.reserve # Linux only
RAWHIDE_CONFD_ATTRIBUTES_FREEBSD = attributes.freebsd
RAWHIDE_CONFD_ATTRIBUTES_OPENBSD = attributes.openbsd
RAWHIDE_CONFD_ATTRIBUTES_NETBSD = attributes.netbsd
RAWHIDE_CONFD_ATTRIBUTES_MACOS = attributes.macos

RAWHIDE_APP_MANFILE = $(RAWHIDE_PROG_NAME).$(APP_MANSECT)
RAWHIDE_FMT_MANFILE = $(RAWHIDE_CONF).$(FMT_MANSECT)

RAWHIDE_PROG_NAME_UPPER = RH
RAWHIDE_CONF_UPPER = RAWHIDE.CONF

# Note: Changing the MAX defines breaks t12 parser error tests, so run them first.
# With MAX_*_SIZE at default values,   RSS is 6.5M and VSZ is 46.6M (on a VM).
# Reducing MAX_*_SIZE to 1/10 reduces  RSS to 4.5M and VSZ to 12.1M.
# Reducing MAX_*_SIZE to 1/100 reduces RSS to 2.6M and VSZ to 8.7M.
# They're all fine choices. The default is probably overkill.
# Reducing to 1/1000 made no further difference, and could be too small.
# With GNU find(1), RSS is 4.4M and VSZ is 9.2M for the same task.
#
# See ./configure --static=SIZE (large=default, small=1/10, tiny=1/100)

RAWHIDE_DEFINES = \
	-DETCDIR=\"$(ETCDIR)\" \
	-DRAWHIDE_NAME=\"$(RAWHIDE_NAME)\" \
	-DRAWHIDE_PROG_NAME=\"$(RAWHIDE_PROG_NAME)\" \
	-DRAWHIDE_VERSION=\"$(RAWHIDE_VERSION)\" \
	-DRAWHIDE_DATE=\"$(RAWHIDE_DATE)\" \
#	-DMAX_PROGRAM_SIZE=2000000 \
#	-DMAX_STACK_SIZE=1000000 \
#	-DMAX_DATA_SIZE=200000 \
#	-DMAX_FILEREF_SIZE=10000 \
#	-DMAX_IDENT_LENGTH=200

# Perl-compatible regular expressions (regexes)
#PCRE2_DEFINES =
#PCRE2_CFLAGS =
#PCRE2_LDFLAGS =

# Access control lists
#ACL_DEFINES =
#ACL_CFLAGS =
#ACL_LDFLAGS =

# Extended attributes
#EA_DEFINES =
#EA_CFLAGS =
#EA_LDFLAGS =

# Linux ext2-style file attributes (like FreeBSD/OpenBSD/NetBSD/macOS flags)
#ATTR_DEFINES =
#ATTR_CFLAGS =
#ATTR_LDFLAGS =

# FreeBSD/OpenBSD/NetBSD/macOS file flags (like Linux attributes)
#FLAG_DEFINES =
#FLAG_CFLAGS =
#FLAG_LDFLAGS =

# File type and mimetype identification by libmagic
#MAGIC_DEFINES =
#MAGIC_CFLAGS =
#MAGIC_LDFLAGS =

# For major() and minor() on Linux
#HAVE_SYS_SYSMACROS = -DHAVE_SYS_SYSMACROS=1

# For major() and minor() on Solaris
#HAVE_SYS_MKDEV = -DHAVE_SYS_MKDEV=1

# Uncomment this to exclude debug code (~10%/12KiB smaller stripped binary)
# Note: This breaks the "invalid -? option argument: wrong" test in t20 (OK)
#DEBUG_DEFINES = -DNDEBUG

# Test coverage (gcov): Uncomment this, run tests (as non-root and as root),
# then run gcov *.c then examine *.c.gcov or run gcov_summary (97.12% on Linux)
#GCOV_CFLAGS = -fprofile-arcs -ftest-coverage

# Undefined behaviour checks: Uncomment this, run tests (as non-root and as root)
#UBSAN_CFLAGS = -fsanitize=undefined
#UBSAN_LDFLAGS = -fsanitize=undefined

CC = cc
#CC = gcc
#CC = other
CPPFLAGS = $(RAWHIDE_DEFINES) $(PCRE2_DEFINES) $(ACL_DEFINES) $(EA_DEFINES) $(ATTR_DEFINES) $(FLAG_DEFINES) $(MAGIC_DEFINES) $(DEBUG_DEFINES) $(HAVE_SYS_SYSMACROS) $(HAVE_SYS_MKDEV)
ALL_CFLAGS = -O3 -g -Wall -pedantic $(CFLAGS) $(CPPFLAGS) $(PCRE2_CFLAGS) $(ACL_CFLAGS) $(EA_CFLAGS) $(ATTR_CFLAGS) $(FLAG_CFLAGS) $(MAGIC_CFLAGS) $(GCOV_CFLAGS) $(UBSAN_CFLAGS)
ALL_LDFLAGS = $(LDFLAGS) $(PCRE2_LDFLAGS) $(ACL_LDFLAGS) $(EA_LDLAGS) $(ATTR_LDFLAGS) $(FLAG_LDFLAGS) $(MAGIC_LDFLAGS) $(UBSAN_LDFLAGS)

OBJS = rhcmds.o rh.o rhparse.o rhdir.o rhdata.o rhstr.o rherr.o

$(RAWHIDE_PROG_NAME): Makefile $(OBJS)
	$(CC) $(ALL_CFLAGS) -o $(RAWHIDE_PROG_NAME) $(OBJS) $(ALL_LDFLAGS)

rh.o: Makefile rh.c rh.h rhparse.h rhdata.h rhdir.h rhstr.h rherr.h
	$(CC) $(ALL_CFLAGS) -c rh.c

rhcmds.o: Makefile rhcmds.c rh.h rhdir.h rherr.h rhstr.h
	$(CC) $(ALL_CFLAGS) -c rhcmds.c

rhdata.o: Makefile rhdata.c rh.h rhdata.h rhcmds.h rherr.h
	$(CC) $(ALL_CFLAGS) -c rhdata.c

rhdir.o: Makefile rhdir.c rh.h rhdata.h rhcmds.h rhdir.h rhstr.h rherr.h
	$(CC) $(ALL_CFLAGS) -c rhdir.c

rherr.o: Makefile rherr.c rh.h
	$(CC) $(ALL_CFLAGS) -c rherr.c

rhparse.o: Makefile rhparse.c rh.h rhdata.h rhcmds.h rhstr.h
	$(CC) $(ALL_CFLAGS) -c rhparse.c

rhstr.o: Makefile rhstr.c rh.h rhstr.h
	$(CC) $(ALL_CFLAGS) -c rhstr.c

clean:
	rm -f $(RAWHIDE_PROG_NAME) $(OBJS) tags $(RAWHIDE_APP_MANFILE).html $(RAWHIDE_FMT_MANFILE).html README.html
	@rm -f valgrind.out *.gcda *.gcno *.gcov

clobber: clean
	rm -f $(RAWHIDE_APP_MANFILE) $(RAWHIDE_FMT_MANFILE)

install: install-bin install-config install-man

install-bin:
	mkdir -p $(DESTDIR)$(APP_INSDIR)
	cp $(RAWHIDE_PROG_NAME) $(DESTDIR)$(APP_INSDIR)
	chmod 755 $(DESTDIR)$(APP_INSDIR)/$(RAWHIDE_PROG_NAME)
	strip $(DESTDIR)$(APP_INSDIR)/$(RAWHIDE_PROG_NAME)

install-config:
	mkdir -p $(DESTDIR)$(APP_CFGDIR)
	cp $(RAWHIDE_CONF) $(DESTDIR)$(APP_CFGDIR)
	chmod 644 $(DESTDIR)$(APP_CFGDIR)/$(RAWHIDE_CONF)
	mkdir -p $(DESTDIR)$(APP_CFGDIR)/$(RAWHIDE_CONFD)
	case "`uname`" in \
		Linux) \
			if ./$(RAWHIDE_PROG_NAME) -h | grep -wq attr; \
			then \
				cp $(RAWHIDE_CONFD_ATTRIBUTES_LINUX) $(DESTDIR)$(APP_CFGDIR)/$(RAWHIDE_CONFD)/$(RAWHIDE_CONFD_ATTRIBUTES); \
			else \
				cp $(RAWHIDE_CONFD_ATTRIBUTES_RESERVE) $(DESTDIR)$(APP_CFGDIR)/$(RAWHIDE_CONFD)/$(RAWHIDE_CONFD_ATTRIBUTES); \
			fi; \
			chmod 644 $(DESTDIR)$(APP_CFGDIR)/$(RAWHIDE_CONFD)/$(RAWHIDE_CONFD_ATTRIBUTES); \
			;; \
		FreeBSD) \
			cp $(RAWHIDE_CONFD_ATTRIBUTES_FREEBSD) $(DESTDIR)$(APP_CFGDIR)/$(RAWHIDE_CONFD)/$(RAWHIDE_CONFD_ATTRIBUTES); \
			chmod 644 $(DESTDIR)$(APP_CFGDIR)/$(RAWHIDE_CONFD)/$(RAWHIDE_CONFD_ATTRIBUTES); \
			;; \
		OpenBSD) \
			cp $(RAWHIDE_CONFD_ATTRIBUTES_OPENBSD) $(DESTDIR)$(APP_CFGDIR)/$(RAWHIDE_CONFD)/$(RAWHIDE_CONFD_ATTRIBUTES); \
			chmod 644 $(DESTDIR)$(APP_CFGDIR)/$(RAWHIDE_CONFD)/$(RAWHIDE_CONFD_ATTRIBUTES); \
			;; \
		NetBSD) \
			cp $(RAWHIDE_CONFD_ATTRIBUTES_NETBSD) $(DESTDIR)$(APP_CFGDIR)/$(RAWHIDE_CONFD)/$(RAWHIDE_CONFD_ATTRIBUTES); \
			chmod 644 $(DESTDIR)$(APP_CFGDIR)/$(RAWHIDE_CONFD)/$(RAWHIDE_CONFD_ATTRIBUTES); \
			;; \
		Darwin) \
			cp $(RAWHIDE_CONFD_ATTRIBUTES_MACOS) $(DESTDIR)$(APP_CFGDIR)/$(RAWHIDE_CONFD)/$(RAWHIDE_CONFD_ATTRIBUTES); \
			chmod 644 $(DESTDIR)$(APP_CFGDIR)/$(RAWHIDE_CONFD)/$(RAWHIDE_CONFD_ATTRIBUTES); \
			;; \
	esac

install-man: install-man-app install-man-fmt

install-man-app: $(RAWHIDE_APP_MANFILE)
	mkdir -p $(DESTDIR)$(APP_MANDIR)
	cp $(RAWHIDE_APP_MANFILE) $(DESTDIR)$(APP_MANDIR)
	chmod 644 $(DESTDIR)$(APP_MANDIR)/$(RAWHIDE_APP_MANFILE)
	[ "$(MAN_GZIP)" = 0 ] || rm -f $(DESTDIR)$(APP_MANDIR)/$(RAWHIDE_APP_MANFILE).gz
	[ "$(MAN_GZIP)" = 0 ] || gzip $(DESTDIR)$(APP_MANDIR)/$(RAWHIDE_APP_MANFILE)

install-man-fmt: $(RAWHIDE_FMT_MANFILE)
	mkdir -p $(DESTDIR)$(FMT_MANDIR)
	cp $(RAWHIDE_FMT_MANFILE) $(DESTDIR)$(FMT_MANDIR)
	chmod 644 $(DESTDIR)$(FMT_MANDIR)/$(RAWHIDE_FMT_MANFILE)
	[ "$(MAN_GZIP)" = 0 ] || rm -f $(DESTDIR)$(FMT_MANDIR)/$(RAWHIDE_FMT_MANFILE).gz
	[ "$(MAN_GZIP)" = 0 ] || gzip $(DESTDIR)$(FMT_MANDIR)/$(RAWHIDE_FMT_MANFILE)

uninstall: uninstall-bin uninstall-config uninstall-man

uninstall-bin:
	[ ! -f $(DESTDIR)$(APP_INSDIR)/$(RAWHIDE_PROG_NAME) ] || rm $(DESTDIR)$(APP_INSDIR)/$(RAWHIDE_PROG_NAME)

uninstall-config:
	[ ! -f $(DESTDIR)$(APP_CFGDIR)/$(RAWHIDE_CONF) ] || rm $(DESTDIR)$(APP_CFGDIR)/$(RAWHIDE_CONF)
	[ ! -f $(DESTDIR)$(APP_CFGDIR)/$(RAWHIDE_CONFD)/$(RAWHIDE_CONFD_ATTRIBUTES) ] || rm $(DESTDIR)$(APP_CFGDIR)/$(RAWHIDE_CONFD)/$(RAWHIDE_CONFD_ATTRIBUTES)
	-[ ! -d $(DESTDIR)$(APP_CFGDIR)/$(RAWHIDE_CONFD) ] || rmdir $(DESTDIR)$(APP_CFGDIR)/$(RAWHIDE_CONFD)

uninstall-man: uninstall-man-app uninstall-man-fmt

uninstall-man-app:
	[ ! -f $(DESTDIR)$(APP_MANDIR)/$(RAWHIDE_APP_MANFILE) ] || rm $(DESTDIR)$(APP_MANDIR)/$(RAWHIDE_APP_MANFILE)
	[ ! -f $(DESTDIR)$(APP_MANDIR)/$(RAWHIDE_APP_MANFILE).gz ] || rm $(DESTDIR)$(APP_MANDIR)/$(RAWHIDE_APP_MANFILE).gz

uninstall-man-fmt:
	[ ! -f $(DESTDIR)$(FMT_MANDIR)/$(RAWHIDE_FMT_MANFILE) ] || rm $(DESTDIR)$(FMT_MANDIR)/$(RAWHIDE_FMT_MANFILE)
	[ ! -f $(DESTDIR)$(FMT_MANDIR)/$(RAWHIDE_FMT_MANFILE).gz ] || rm $(DESTDIR)$(FMT_MANDIR)/$(RAWHIDE_FMT_MANFILE).gz

man: $(RAWHIDE_APP_MANFILE) $(RAWHIDE_FMT_MANFILE)

html: $(RAWHIDE_APP_MANFILE).html $(RAWHIDE_FMT_MANFILE).html

$(RAWHIDE_APP_MANFILE): $(RAWHIDE_APP_MANFILE).pod
	sed 's/C</B</g' $(RAWHIDE_APP_MANFILE).pod | pod2man --section='$(APP_MANSECT)' --center='$(APP_MANSECTNAME)' --name='$(RAWHIDE_PROG_NAME_UPPER)' --release='$(RAWHIDE_ID)' --date='$(RAWHIDE_DATE)' --quotes=none > $@

$(RAWHIDE_APP_MANFILE).html: $(RAWHIDE_APP_MANFILE).pod
	pod2html --noindex --title='$(RAWHIDE_ID) - $(RAWHIDE_PROG_NAME)($(APP_MANSECT))' < $(RAWHIDE_APP_MANFILE).pod > $@ 2>/dev/null
	rm -r pod2htm*.tmp

$(RAWHIDE_FMT_MANFILE): $(RAWHIDE_FMT_MANFILE).pod
	sed 's/C</B</g' $(RAWHIDE_FMT_MANFILE).pod | pod2man --section='$(FMT_MANSECT)' --center='$(FMT_MANSECTNAME)' --name='$(RAWHIDE_CONF_UPPER)' --release='$(RAWHIDE_ID)' --date='$(RAWHIDE_DATE)' --quotes=none > $@

$(RAWHIDE_FMT_MANFILE).html: $(RAWHIDE_FMT_MANFILE).pod
	pod2html --noindex --title='$(RAWHIDE_ID) - $(RAWHIDE_CONF)($(FMT_MANSECT))' < $(RAWHIDE_FMT_MANFILE).pod > $@ 2>/dev/null
	rm -r pod2htm*.tmp

# html2 is for internal use (requires python3 and its markdown module)

html2: README.html

README.html: README.md
	./md2html README.md $@ '$(RAWHIDE_ID) - README'

tags:     Makefile rh.h rh.c rhcmds.h rhcmds.c rhdata.h rhdata.c rhdir.h rhdir.c rhparse.h rhparse.c rherr.h rherr.c rhstr.h rhstr.c
	ctags Makefile rh.h rh.c rhcmds.h rhcmds.c rhdata.h rhdata.c rhdir.h rhdir.c rhparse.h rhparse.c rherr.h rherr.c rhstr.h rhstr.c

test: $(RAWHIDE_PROG_NAME)
	./runtests

gcov:
	gcov *.c
	./gcov_summary

check: test
tests: test

default:
	./configure --default

dist: clobber man default
	@set -e; \
	up="`pwd`/.."; \
	src=`basename \`pwd\``; \
	dst=$(RAWHIDE_ID); \
	cd ..; \
	[ "$$src" != "$$dst" -a ! -d "$$dst" ] && ln -s $$src $$dst; \
	tar chzf $$up/$(RAWHIDE_DIST) --exclude='.git*' $$dst; \
	[ -h "$$dst" ] && rm -f $$dst; \
	tar tzfv $$up/$(RAWHIDE_DIST); \
	ls -l $$up/$(RAWHIDE_DIST)

help:
	@echo "First, see: ./configure --help"
	@echo
	@echo "  ./configure         - Configure for the local system"
	@echo "  make                - Compile rh"
	@echo "  sudo make install   - Install rh"
	@echo "  sudo make uninstall - Uninstall rh"
	@echo
	@echo "To run tests (verbose, quiet, valgrind):"
	@echo
	@echo "  make test         - Run tests showing every test (thousands)"
	@echo "  make quiet=1 test - Run tests with just one line per test suite"
	@echo "  make vg=1 test    - Run tests and produce valgrind.out analysis (~40m)"
	@echo "  vim valgrind.out  - Examine the results (delete the noise, check the rest)"
	@echo 
	@echo "To run tests for test coverage analysis (97.12% on Linux):"
	@echo
	@echo "  ./configure --enable-gcov"
	@echo "  make"
	@echo "  make quiet=1 test"
	@echo "  sudo make quiet=1 RAWHIDE_TEST_MULTIBYTE_USER_GROUP=1 test"
	@echo "  make gcov"
	@echo
	@echo "But be warned that including RAWHIDE_TEST_MULTIBYTE_USER_GROUP=1 above"
	@echo "includes tests that add and remove new users and groups (Linux only)."
	@echo
	@echo "Other make targets:"
	@echo
	@echo "  help    - Show this information"
	@echo "  dist    - Create the source distribution in .."
	@echo "  clean   - Delete everything except the manual entries"
	@echo "  clobber - Delete the manual entries as well"
	@echo "  man     - Create the manual entries (needs pod2man)"
	@echo "  tags    - Create the tags file for vim (needs ctags)"
	@echo "  html    - Create manual entries as HTML (needs pod2html)"
	@echo "  html2   - Create README file as HTML (needs python3 and markdown module)"
	@echo "  check   - Same as test"
	@echo "  tests   - Same as test"
	@echo "  default - Reset Makefile to its default configuration"
	@echo
	@echo "See also:"
	@echo
	@echo "  samples/README            - Explains the optional files below"
	@echo "  samples/.rh.sh            - Shell functions/aliases to save keystrokes"
	@echo "  samples/brevity.rh        - Extra concise aliases for some built-ins"
	@echo "  samples/rawhide.conf-tiny - Small rawhide.conf for use with tiny static size"
	@echo

# vi:set ts=4 sw=4:
