#
# rh:
# Written by: Ken Stauffer
# Restored by: raf
#

CC = cc
CFLAGS = -Wall -pedantic -O2 -g

BINDIR = /usr/local/bin
MANDIR = /usr/local/share/man/man1

OBJS = rhcmds.o rh.o rhparse.o rhdir.o rhdata.o

rh: $(OBJS)
	$(CC) $(CFLAGS) -o rh $(OBJS)

rhdir.o: rhdir.c rh.h
	$(CC) $(CFLAGS) -c rhdir.c

rh.o: rh.c rh.h rhdata.h rhdir.h rhparse.h
	$(CC) $(CFLAGS) -c rh.c

rhcmds.o: rhcmds.c rh.h
	$(CC) $(CFLAGS) -c rhcmds.c

rhparse.o: rhparse.c rh.h rhcmds.h
	$(CC) $(CFLAGS) -c rhparse.c

rhdata.o: rhdata.c rh.h rhcmds.h rhparse.h
	$(CC) $(CFLAGS) -c rhdata.c

clean:
	rm -f rh core $(OBJS) rh.1 tags

rh.1: rh.man
	nroff -man rh.man > rh.1

tags: rh.c rh.h rhcmds.c rhcmds.h rhdata.c rhdata.h rhdir.c rhdir.h rhparse.c rhparse.h
	ctags rh.c rh.h rhcmds.c rhcmds.h rhdata.c rhdata.h rhdir.c rhdir.h rhparse.c rhparse.h

install: rh rh.1
	install -m 755 rh $(BINDIR)
	install -m 644 rh.1 $(MANDIR)

