
/* ----------------------------------------------------------------------
 * FILE: rh.c
 * VERSION: 2
 * Written by: Ken Stauffer
 * 
 * printhelp(), execute(), exam1(), exam2(), exam3(), main()
 *
 *
 * ---------------------------------------------------------------------- */

#include <sys/types.h>
#include <sys/stat.h>
#include "rh.h"

static char usage[]={
"Usage: %s [-vhlr] [ [-e expr] | [-f filename ] ] [-x command ] file ...\n"
};

/* ----------------------------------------------------------------------
 * printhelp:
 *	Print out the help screen. The string 's' is argv[0].
 *	Called when the -h option is used.
 *
 */

printhelp(s)
char *s;
{
	int i;
	struct symbol *p;

	printf(usage,s);
	printf("options:\n");
	printf("\t-h       show this message\n");
	printf("\t-l       long filename output\n");
	printf("\t-r       makes %s non-recursive\n",s);
	printf("\t-v       verbose output\n");
	printf("\t-e       get expression from the command line\n");
	printf("\t-f       get expression from file\n");
	printf("\t-x       execute a unix command for matching files\n\n");

	printf("\tvalid symbols:\n");
	for(i=1, p=symbols; p; p=p->next, i++)
		printf("%12s%s", p->name,
			((i-1)%5==4 || !p->next ) ? "\n" : " ");

	printf("\tC operators:\n");
	printf("\t! ~ - * / %% + < <= > >= == != & ^ | << >> && || ?:\n");
	printf("\tspecial operators:\n");
	printf("\t$username , \"*.c\" , [yyyy/mm/dd]\n\n");
}

/* ----------------------------------------------------------------------
 * execute:
 *	Execute the program contained in the StackProgram[]
 *	array. Each element of the StackProgram[] array contains
 *	a pointer to a function.
 *	Programs are NULL terminated.
 *	Returns the result of the expression.
 *
 */

execute()
{
	register long eval;
	register int (*efunc)();

	SP=0;
	for(PC=startPC; (efunc=StackProgram[PC].func) ; PC++) {
		eval=StackProgram[PC].value;
		(*efunc)(eval);
		if( SP >= MEM ) {
			fprintf(stderr,"stack overflow\n");
			exit(1);
		}
	}
	return( Stack[0] );
}

/* ----------------------------------------------------------------------
 * exam1: exam2: exam3:
 *	One of these functions is called for every file that 'rh' examines.
 *	exam{1,2,3}() first calls execute to see if the
 *	expression is true, it then prints the file if the expression
 *	evaluated to true (non-zero).
 *
 */

/* print file out by itself */
exam1()
{
	if( execute() ) printf("%s\n",attr.fname);
}

/* long output of file */
exam2()
{
	if( execute() ) printentry(attr.verbose,attr.buf,attr.fname);
}

/* do a system(3) call to desired command */
exam3()
{
	char command[ 2048 + 1 ];
	char *p,*q,*r,*strrchr();
	int rv;

	if( execute() ) {
		p=command;
		q=attr.command;
		while( *q ) {
			if( *q != '%' ) *p++ = *q++;
			else {
				q += 1;
				if( *q == 's' ) {
					r = attr.fname;
					while( *p++ = *r++ );
					p -= 1;
				} else if( *q == 'S' ) {
					r = strrchr(attr.fname,'/');
					r = (r) ? r+1 : attr.fname;
					while( *p++ = *r++ );
					p -= 1;
				} else *p++ = '%';
				q += 1;
			}
		}
		*p = '\0';
		rv = system(command);
		if( attr.verbose ) printf("%s exit(%d)\n",command,rv);
	}
}


/* ----------------------------------------------------------------------
 * main:
 *	parse arguments.
 *	gnu getopt() is used here.
 *	-l, -r, -h, -v options can occur as often as desired.
 *	-f,-x and -e can only occur once and MUST have an argument.
 *
 *	Read and "compile" the $HOME/.rhrc file, if it exists.
 *	Read and "compile" any -f filename, if present.
 *	Read and "compile" any -e expression, if present.
 *	If after all that no start expression is found then read from
 *	stdin for one.
 *	Perform the recursive hunt on remaining arguments.
 *
 */

main(argc,argv)
int argc;
char *argv[];
{
	extern int optind;
	extern char *optarg;
	char *dashe,*dashf,*strcat(),*getenv(),initfile[ 1024+1 ];
	int i,r;
	int dashr,dashh,dashl;
	int (*examptr)();

	/* defaults */
	dashe = NULL;		/* -e option */
	dashl = 0;		/* -l */
	dashf = NULL;		/* -f */
	dashr = 1;		/* -r */
	dashh = 0;		/* -h */
	attr.verbose = 0;	/* -v */
	attr.command = NULL;	/* -x */
	examptr = exam1;	/* default output function */

	while ((i = getopt(argc, argv, "lrhvx:e:f:")) != EOF) {
		switch( i ) {
		case 'l': examptr = exam2; dashl = 1; break;
		case 'r': dashr = 0; break;
		case 'h': dashh = 1; break;
		case 'v': attr.verbose = 1; break;
		case 'x': 
		   if( attr.command ) {
			fprintf(stderr, "%s: too many -x options\n",argv[0]);
			exit(1);
		   }
		   examptr = exam3;
		   attr.command = optarg;
		   break;
		case 'e':
		   if( dashe ) {
			fprintf(stderr, "%s: too many -e options\n",argv[0]);
			exit(1);
		   }
		   dashe = optarg;
		   break;
		case 'f':
		   if( dashf ) {
			fprintf(stderr, "%s: too many -f options\n",argv[0]);
			exit(1);
		   }
		   dashf = optarg;
		   break;
		default:
		   fprintf(stderr,"%s: use -h for help\n", argv[0],i);
		   fprintf(stderr,usage, argv[0]);
		   exit(1);
		}
		if( attr.command && dashl ) {
			fprintf(stderr,
				"%s: cannot have both -x and -l options\n",
				argv[0]);
			exit(1);
		}
	}

	PC = 0;
	startPC = -1;
	rhinit();
	if( dashh ) printhelp(argv[0]);

	expfname = getenv( HOMEENV );
	if( expfname ) {
		strcpy(initfile,expfname);
		expfname = strcat(initfile,RHRC);
		if( (expfile = fopen(expfname,"r")) != NULL ) {
			expstr = NULL;
			program();
		}
	}

	if( dashf ) {
		expstr = NULL;
		expfname = dashf;
	        if( (expfile = fopen(expfname,"r")) == NULL ) {
			fprintf(stderr,"%s: ", argv[0]);
			perror(expfname);
			exit(1);
		}
		program();
	}
	if( dashe ) {
		expfile = NULL;
		expstr = dashe;
		program();
	}
	if( startPC == -1 ) {
		expstr = NULL;
		expfname = "stdin";
		expfile = stdin;
		program();
	}

	if( startPC == -1 ) {
		fprintf(stderr,"%s: no start expression specified\n",
			argv[0] );
		exit(1);
	}
	rhfinish();

	if( optind >= argc ) {
		r=ftrw(".",examptr,(dashr)? DEPTH :1);
		if(r == -1) perror(".");
	} else
		for(; optind<argc; optind++) {
			r=ftrw(argv[optind],examptr,(dashr)? DEPTH :1);
			if( r == -1 ) perror(argv[optind]);
		}
    exit(0);
}
