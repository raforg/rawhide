
/* ----------------------------------------------------------------------
 * FILE: rh.h
 * VERSION: 2
 * Written by: Ken Stauffer
 * This header contains the #define's for the tokens.
 * It also contains structure definitions.
 * 
 * ---------------------------------------------------------------------- */

#include <stdio.h>

#define RHRC		"/.rhrc"	/* start up file */
#define RHENV		"RH"		/* rh environment variable */
#define HOMEENV		"HOME"		/* path to your home directory */

/* Definition of returned tokens from the lexical analyzer */

#define OR		256	/* || */
#define AND		257	/* && */
#define LE		258	/* <= */
#define GE		259	/* >= */
#define NE		260	/* != */
#define EQ		261	/* == */
#define SHIFTL		262	/* << */
#define SHIFTR		263	/* >> */
#define NOP		264	/* no-operation */
#define NUMBER		265	/* literal numbers and symbolic constants */
#define STR		266	/* regular expression strings */
#define FIELD		267	/* file fields (size,mode,nlinks) */
#define FUNCTION	268	/* user defined function */
#define RETURN		269	/* return keyword */
#define PARAM		270	/* parameter to functions */
#define IDENTIFIER	271

#define LENGTH		2000	/* size of stack program */
#define MEM		1000	/* size of stack */
#define IDLENGTH	20	/* length of an identifier */
#define STRLEN		200	/* total chars available for strings */

#if BSD
#define DEPTH		getdtablesize()
#define strrchr		rindex
#ifdef SUNOS_4
#define    POSIX_DIRECTORY_LIBRARY
#endif /* SUNOS_4 */
#endif /* BSD */

#ifdef SYSV
#ifdef SYSVR3
#define    DEPTH        ulimit(4, 0L)
#define    POSIX_DIRECTORY_LIBRARY
#else /* SYSVR3 */
/* This value was arbitrarily chosen */
#define DEPTH        20
#endif /* SYSVR3 */
#endif /* SYSV */

/*
 * Structure of a "rh-assembly" instruction.
 *
 */

struct instr {
	int		(*func)();
	long		value;
};

/*
 * Structure of a symbol.
 *
 */

struct symbol {
	char		*name;
	int		type;
	long		value;
	int		(*func)();
	struct symbol	*next;
};

/*
 * Structure defining the rh runtime environment.
 *
 */

struct runtime {
	struct stat	*buf;		/* stat info of current file */
	char		*fname;		/* file name of current file */
	int		depth;		/* relative depth of current file */
	int		(*func)();	/* examination function */
	int		prune;		/* flag to indicate prunning */
	int		verbose;	/* used by the (*func)() routine */
	char		*command;	/* command to exec for current file */
};

#ifndef DATA
	extern struct symbol	*symbols;

	extern struct symbol	*tokensym;
	extern long		tokenval;
	extern long		token;

	extern struct instr	StackProgram[];
	extern int		PC;
	extern int		startPC;
	extern long		Stack[];
	extern int		SP;
	extern int		FP;	/* frame pointer */

	extern struct runtime	attr;

	extern char		Strbuf[];
	extern int		strfree;

	extern char		*expstr;
	extern char		*expfname;
	extern FILE		*expfile;

#endif

extern	c_or(),      c_and(),      c_le(),      c_lt(),      c_ge(),
	c_gt(),      c_ne(),       c_eq(),      c_bor(),     c_band(),
	c_bxor(),    c_not(),      c_plus(),    c_mul(),     c_minus(),
	c_div(),     c_mod(),      c_number(),  c_atime(),   c_ctime(),
	c_dev(),     c_gid(),      c_ino(),     c_mode(),    c_mtime(),
	c_nlink(),   c_rdev(),     c_size(),    c_uid(),     c_str(),
	c_bnot(),    c_uniminus(), c_lshift(),  c_rshift(),  c_qm(),
	c_colon(),   c_return(),  c_func(),     c_param(),   c_depth(),
	c_baselen(), c_pathlen(), c_prune();

