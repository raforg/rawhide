#
# rh:
# >>>> VERSION: 2 <<<<
# Written by: Ken Stauffer
#
# Place one of the following -D's onto the end of
# the definition of CFLAGS.
#
# -DSYSV
#	- System V/III like OS (including Xenix)
#
# -DBSD
#	- BSD like OS (including SunOS)
#
# Also, place one of the following -D's onto the end of
# the definition of CFLAGS if you have an operating system
# in the specified subclass of the classes listed above.
#
# -DSUNOS_4
#    - SunOS 4.x (subclass of BSD)
#
# -DSYSVR3
#    - System V Release 3.x (subclass of SYSV)
#
# getopt.c uses void, so some systems may also need
# -Dvoid=int
#

CFLAGS= -DBSD -DSUNOS_4 -O

OBJS= rhcmds.o rh.o rhparse.o rhdir.o rhdata.o getopt.o glob_match.o

rh: $(OBJS)
	cc $(CFLAGS) -o rh $(OBJS)

rhdir.o: rhdir.c rh.h
	cc $(CFLAGS) -c rhdir.c

rh.o: rh.c rh.h
	cc $(CFLAGS) -c rh.c

rhcmds.o: rhcmds.c rh.h
	cc $(CFLAGS) -c rhcmds.c

rhparse.o: rhparse.c rh.h
	cc $(CFLAGS) -c rhparse.c

rhdata.o: rhdata.c rh.h
	cc $(CFLAGS) -c rhdata.c

getopt.o: getopt.c
	cc $(CFLAGS) -Dvoid=int -c getopt.c

glob_match.o: glob_match.c
	cc $(CFLAGS) -Dvoid=int -c glob_match.c

clean:
	rm -f rh core $(OBJS)
