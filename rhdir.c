
/* ----------------------------------------------------------------------
 * FILE: rhdir.c
 * VERSION: 2
 * Written by: Ken Stauffer
 * This file contains the "non portable" stuff dealing with
 * directories.
 * printentry(), ftrw(), fwt1()
 *
 *
 * ---------------------------------------------------------------------- */

#include "rh.h"
#include <sys/types.h>
#include <sys/stat.h>

#define user_index(b)	((000777 & (b)) >> 6) + ((b) & S_ISUID ? 8 : 0) 
#define group_index(b)	((000077 & b) >> 3) + ((b) & S_ISGID ? 8 : 0)
#define all_index(b)	((000007 & (b)) + (((b) & S_ISVTX) ? 8 : 0))
#define ftype_index(b)	((b) >> 13)

#define isdirect(b)	(((b)&S_IFMT)==S_IFDIR)
#define isblk(b)	(((b)&S_IFMT)==S_IFBLK)
#define ischr(b)	(((b)&S_IFMT)==S_IFCHR)

/*
 * Some System do not define these macro's.
 * If these macro's are missing, then use these ones:
 *
 * #define major(b)	((b)>>8)
 * #define minor(b)	((b)&0xff)
 *
 */

#define isdot(s)	((s)[1]=='\0' && (s)[0]=='.')
#define isdotdot(s)	((s)[2]=='\0' && (s)[1]=='.' && (s)[0]=='.')

#ifdef S_IFLNK
#define islink(b)    (((b)&S_IFMT) == S_IFLNK)
#else
#define islink(b)    (0)
#define    lstat        stat
#endif

#define isproper(m)    (isdirect(m) && !islink(m) && !attr.prune)

#ifdef    POSIX_DIRECTORY_LIBRARY
#include <dirent.h>
#else
#include <sys/dir.h>
#endif

/*
 * XXX - on BSD systems, this is defined in <sys/param.h>.
 * On System V Release 3, it's defined inside "nami.c", and is,
 * unfortunately, not in any include file.
 * On systems other than those, no such simple limit exists.
 * On BSD and S5R3 systems, as distributed by Berkeley and AT&T,
 * respectively, it's 1024, so we set it to that.
 */
#define    MAXPATHLEN    1024

/* ----------------------------------------------------------------------
 * printentry:
 *	Display filename,permissions and size in a '/bin/ls' like
 *	format. If verbose is non-zero then more information is
 *	displayed.
 * uses the macros:
 *	user_index(b)
 *	group_index(b)
 *	all_index(b)
 *	ftype_index(b)
 *
 */

printentry(verbose,buf,name)
struct stat *buf;
char *name;
{
	char *t,*ctime();

	static char *ftype[]={ "p", "c" , 
			       "d" , "b" ,  
			       "-" , "l" , 
			       "s" , "t" };
 
	static char *perm[]={ "---", "--x", "-w-", "-wx" ,
			      "r--", "r-x", "rw-", "rwx" ,
			      "--S", "--s", "-wS", "-ws" ,
			      "r-S", "r-s", "rwS", "rws" };

	static char *perm2[]={ "---", "--x", "-w-", "-wx" ,
			      "r--", "r-x", "rw-", "rwx" ,
			      "--T", "--t", "-wT", "-wt" ,
			      "r-T", "r-t", "rwT", "rwt" };
 
	if( verbose ) {
		t = ctime(&buf->st_mtime);
		t[24] = '\0';
		if( ischr(buf->st_mode) || isblk(buf->st_mode) )

			printf("%s%s%s%s %4d %4d %3d,%3d %s %-s\n",
				ftype[ ftype_index(buf->st_mode) ],
				perm[ user_index(buf->st_mode) ],
				perm[ group_index(buf->st_mode) ],
				perm2[ all_index(buf->st_mode) ],
				buf->st_uid,
				buf->st_gid,
				major(buf->st_rdev),
				minor(buf->st_rdev),
				t+4,
				name );

		else

			printf("%s%s%s%s %4d %4d %6d %s %-s\n",
				ftype[ ftype_index(buf->st_mode) ],
				perm[ user_index(buf->st_mode) ],
				perm[ group_index(buf->st_mode) ],
				perm2[ all_index(buf->st_mode) ],
				buf->st_uid,
				buf->st_gid,
				buf->st_size,
				t+4,
				name );

	} else {

		if( ischr(buf->st_mode) || isblk(buf->st_mode) )

			printf("%s%s%s%s %3d,%3d %-s\n",
				ftype[ ftype_index(buf->st_mode) ],
				perm[ user_index(buf->st_mode) ],
				perm[ group_index(buf->st_mode) ],
				perm2[ all_index(buf->st_mode) ],
				major(buf->st_rdev),
				minor(buf->st_rdev),
				name );

		else

			printf("%s%s%s%s %9d %-s\n",
				ftype[ ftype_index(buf->st_mode) ],
				perm[ user_index(buf->st_mode) ],
				perm[ group_index(buf->st_mode) ],
				perm2[ all_index(buf->st_mode) ],
				buf->st_size,
				name );

	}
}

/* ----------------------------------------------------------------------
 * ftrw:
 *	Entry point to do the search, ftrw is a front end
 *	to the recursive fwt1.
 *	ftrw() initializes some global variables and
 *	builds the initial filename string which is passed to
 *	ftw1().
 */

ftrw(f,fn,depth)
char *f;
int (*fn)();
int depth;
{
	char *p,filebuf[ MAXPATHLEN+1 ];
	struct stat statbuf;
	int last;

	attr.prune = 0;
	attr.depth = 0;
	attr.func=fn;
	attr.fname = filebuf;
	attr.buf = &statbuf;
	strcpy(attr.fname,f);

	last = 0;
	for(p=attr.fname; *p; p++)
		if( *p == '/' ) last = 1;
		else last = 0;

	if( !last ) { *p++ = '/'; *p = '\0'; }

	if( lstat(attr.fname,attr.buf) < 0 ) return(-1);

	(*(attr.func))();

	if( isproper( attr.buf->st_mode ) ) fwt1(depth,p);

	return(0);
}

/* ----------------------------------------------------------------------
 * fwt1:
 *	'p' points to the end of the string in attr.fname
 *
 *	2 versions of this routine currently live here:
 *	"new-style", for systems with a BSD or POSIX-style
 *	directory library, and systems without such a
 *	directory library. They both differ in
 *	the manner in which they access directories.
 *	Any chnages needed to work on another system
 *	should only have to made for this routine.
 *
 *	Below is the "directory library" version of fwt1()
 *
 */

#if defined(POSIX_DIRECTORY_LIBRARY) || defined(BSD)

static fwt1(depth,p)
int depth;
char *p;
{
	char *q,*s;
	DIR *dirp;
#ifdef POSIX_DIRECTORY_LIBRARY
	struct dirent *dp;
#else
	struct direct *dp;
#endif
	if( !depth ) return;
	attr.depth += 1;

	dirp=opendir(attr.fname);
	if( dirp == NULL ) return;
	for(dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
		if( isdot(dp->d_name) || isdotdot(dp->d_name) ) continue;
		s = p;
		q = dp->d_name;
		while( *s++ = *q++ );
		s -= 1;
		if( lstat(attr.fname,attr.buf) < 0 ) continue;
		(*(attr.func))();
		if( isproper( attr.buf->st_mode ) ) {
			*s++ = '/';
			*s = '\0';
			fwt1(depth-1,s);
		}
	}
	closedir(dirp);
	attr.depth -= 1;
	attr.prune = 0;
	*p = '\0';
}
#else

/* ----------------------------------------------------------------------
 * fwt1:
 *	This function does the same thing as fwt1() above, but is
 *	meant for systems without a directory library, that does
 *	directory reading "by hand".
 *
 *    Below is the "no directory library" version of fwt1()
 *
 */

static fwt1(depth,p)
int depth;
char *p;
{
	char *q,*s;
	FILE *dirp;
	struct direct dp;
	int count;

	if( !depth ) return;
	attr.depth += 1;

	dirp=fopen(attr.fname,"r");
	if( dirp == NULL ) return;
	for(count = fread(&dp,sizeof(struct direct),1,dirp); count;
		count = fread(&dp,sizeof(struct direct),1,dirp) ) {

		if( isdot(dp.d_name) || isdotdot(dp.d_name) || dp.d_ino==0 )
			continue;
		s = p;
		q = dp.d_name;
		while( *s++ = *q++ ); 
		s -= 1;

		if( lstat(attr.fname,attr.buf) < 0 ) continue;
		(*(attr.func))();
		if( isproper( attr.buf->st_mode ) ) {
			*s++ = '/';
			*s = '\0';
			fwt1(depth-1,s);
		}
	}
	fclose(dirp);
	attr.depth -= 1;
	attr.prune = 0;
	*p = '\0';
}

#endif
