
/* ----------------------------------------------------------------------
 * FILE: rhparse.c
 * VERSION: 2
 * Written by: Ken Stauffer
 * This contains the parser for the C expressions,
 * gettoken(), getit() and ungetit() routines.
 * sectime(), datespec(), expression(), expr(), exp0(), ... , factor()
 * locatename(), push(), find_macro()
 *
 *
 * ---------------------------------------------------------------------- */

#include "rh.h"
#include <ctype.h>
#include <pwd.h>

static int cpos;		/* current character position */
static int lineno;		/* current line number */

/* ----------------------------------------------------------------------
 * getit:
 *	Return the next character, input is obtained from a file or
 *	a string.
 *	If expstr == NULL then input is from the file called 'expfname'
 *	with file pointer 'expfile'.
 *
 *	If expstr != NULL then input is from the string 'expstr'
 *
 */

getit()
{
	int c;

	if( expstr ) c = (*expstr) ? *expstr++ : EOF;
	else c = getc(expfile);

	if( c == '\n' ) { lineno++; cpos = 0; }
	cpos++;

	return(c);
}

/* ----------------------------------------------------------------------
 * ungetit:
 *	Unget a char.
 *	A character is ungotten using the same scheme as stated for
 *	getit() for determining where input is comming from.
 *
 */

ungetit(c)
int c;
{
	if( c == '\n' ) { lineno--; cpos = 1; }
	else cpos--;
	if( expstr ) expstr = (c > 0) ? expstr-1 : expstr;
	else ungetc(c,expfile);
}

/* ----------------------------------------------------------------------
 * error:
 *	Print an error message and quit.
 */
 
error(s)
char *s;
{
	if( expstr )
		fprintf(stderr,"Command line: ");
	else
		fprintf(stderr,"%s: ",expfname);

	fprintf(stderr,"line: %d, char: %d, %s.\n",lineno,cpos,s);
	exit(1);
}

/* ----------------------------------------------------------------------
 * insertname:
 *	Inserts the symbol named 's' with type 't' and value 'val'
 *	into the symbol table. Return the a pointer to the symbol
 *	table entry. The symbol is inserted into the head of the
 *	linked list. This behavior is relied upon elswhere.
 *
 */

struct symbol *insertname(s,t,val)
char *s;
int t;
long val;
{
	char *p,*malloc();
	struct symbol *sym;

	sym = (struct symbol *) malloc( sizeof(struct symbol) );
	if( sym == NULL ) error("no more memory");

	p = sym->name = malloc( strlen(s)+1 );
	if( sym->name == NULL ) error("no more memory");
	while( *p++ = *s++ );
	sym->type = t;
	sym->value = val;

	sym->next = symbols;
	symbols = sym;
	
	return( sym );
}

/* ----------------------------------------------------------------------
 * locatename:
 *	Do a linear search for 's' in the linked list symbols.
 *
 */

struct symbol *locatename(s)
char *s;
{
	struct symbol *p;

	for(p=symbols; p; p = p->next )
		if( !strcmp(s,p->name) ) return(p);

	return(NULL);
}

/* ----------------------------------------------------------------------
 * push:
 *	"assemble" the instruction into the StackProgram[] array.
 *
 */

push(func,val)
int (*func)();
long val;
{
	if( PC >= LENGTH ) error("program to big");
	StackProgram[PC].func=func;
	StackProgram[PC++].value=val;
}

/* ----------------------------------------------------------------------
 * program:
 *	Parse a program of the form:
 *		<program> ==> <function-list> <expression> EOF
 *			| <function-list> EOF
 *			| <function-list> <expression> ;
 *
 *		<function-list> ==> <function> <function-list>
 *				| empty
 */

program()
{
	cpos = 0; lineno = 1;

	token = gettoken();
	for(;;) {
		if( token != IDENTIFIER ) break;
		function();
	}

	if( token != EOF ) {
		startPC = PC;
		expression();
		push(NULL,0);
	}
	if( token != EOF && token != ';') error("EOF expected");
}

/* ----------------------------------------------------------------------
 * function:
 *	parse a function definition. Grammer for a function is:
 *	<function> ==> IDENTIFIER <id-list> { RETURN <expression> ; }
 *
 *	<id-list> ==> ( <ids> )
 *			| ( )
 *			| empty
 *
 *	<ids> ==> IDENTIFIER <idtail>
 *
 *	<idtail> ==> , <ids>
 *		| empty
 *
 */

function()
{
	struct symbol *s;

	s = tokensym;
	tokensym->value = PC;
	tokensym->type = FUNCTION;
	tokensym->func = c_func;

	token = gettoken();

	push(NULL, idlist() );		/* save number of args for function */

	if( token != '{' ) error("expected '{'");
	token = gettoken();

	if( token != RETURN ) error("expected keyword: return");
	token = gettoken();

	expression();

	if( token != ';' ) error("expected ';'");
	token = gettoken();

	push(c_return,StackProgram[s->value].value);

	/* free up the parameter symbols */
	while( symbols->type == PARAM ) {
		s = symbols;
		symbols = symbols->next;
		free(s->name);
		free(s);
	}

	if( token != '}' ) error("expected '}'");
	token = gettoken();
}

/* ----------------------------------------------------------------------
 * idlist:
 *	Return the maximum offset obtained in parsing the parameter list.
 *	<id-list> ==> ( <ids> )
 *		| ()
 *		| empty
 *
 *	<ids> ==> IDENTIFIER <idtail>
 *	<idtail> ==> <ids> , <idtail>
 *		| empty
 */

idlist()
{
	int offset = 0;

	if( token == '(' ) token = gettoken();
	else if( token == '{' ) return(0);
	else error("expected '(' or '{'");

	if( token == ')' ) {
		token = gettoken();
		return(0);
	}

	for(;;) {
		if( token != IDENTIFIER ) error("identifier expected");
		tokensym->type = PARAM;
		tokensym->func = c_param;
		tokensym->value = offset++;
		token = gettoken();
		if( token == ')' ) break;
		if( token != ',' ) error("expected ')'");
		token = gettoken();
	}

	token = gettoken();
	return(offset);
}

/* ----------------------------------------------------------------------
 * expression:
 *	Parse an expression. (top-level routine)
 *	OPERATOR ?:
 *
 */

expression()
{
	int qm,colon,nop;

	expr0();
	if( token == '?' ) {
		token = gettoken();
		qm = PC;
		push(c_qm,0);
		expression();
		if( token != ':' ) error("missing ':'");
		token = gettoken();
		colon = PC;
		push(c_colon,0);
		expression();

		StackProgram[qm].value = colon;
		StackProgram[colon].value = PC-1;
	}
}		

/* OPERATOR || */ 
expr0()
{
	expr1();
	for(;;)
		if( token == OR ) {
			token = gettoken();
			expr1();
			push(c_or,0);
	       } else break;
}

/* OPERATOR && */ 
expr1()
{
	expr2();
	for(;;)
		if( token == AND ) {
			token = gettoken();
			expr2();
			push(c_and,0);
		} else break;
}

/* OPERATOR | */
expr2()
{
	expr3();
	for(;;)
		if( token == '|' ) {
			token = gettoken();
			expr3();
			push(c_bor,0);
		} else break;
}

/* OPERATOR ^ */
expr3()
{
	expr4();
	for(;;)
		if( token == '^' ) {
			token = gettoken();
			expr4();
			push(c_bxor,0);
		} else break;
}

/* OPERATOR & */
expr4()
{
	expr5();
	for(;;)
		if( token == '&' ) {
			token = gettoken();
			expr5();
			push(c_band,0);
		} else break;
}

/* OPERATOR == != */
expr5()
{
	int t;
	expr6();
	for(;t=token;)
		if( t==EQ ) {
			token = gettoken();
			expr6();
			push(c_eq,0);
		} else if( t==NE ) {
			token = gettoken();
			expr6();
			push(c_ne,0);
		} else break;
}

/* OPERATOR < <= > >= */
expr6()
{
	int t;
	expr7();
	for(;t=token;)
		if( t==LE ) {
			token = gettoken();
			expr7();
			push(c_le,0);
		} else if( t==GE ) {
			token = gettoken();
			expr7();
			push(c_ge,0);
		} else if( t=='>' ) {
			token = gettoken();
			expr7();
			push(c_gt,0);
		} else if( t=='<' ) {
			token = gettoken();
			expr7();
			push(c_lt,0);
		} else break;
}

/* OPERATOR << >> */
expr7()
{
	int t;
	expr8();
	for(;t=token;)
		if( t==SHIFTL ) {
			token = gettoken();
			expr8();
			push(c_lshift,0);
		} else if( t==SHIFTR ) {
			token = gettoken();
			expr8();
			push(c_rshift,0);
		} else break;
}

/* OPERATOR + - */
expr8()
{
	int t;
	expr9();
	for(;t=token;)
		if( t=='+' ) {
			token = gettoken();
			expr9();
			push(c_plus,0);
		} else if( t=='-' ) {
			token = gettoken();
			expr9();
			push(c_minus,0);
		} else break;
}

/* OPERATOR * / % */
expr9()
{
	int t;
	expr10();
	for(;t=token;)
		if( t== '*' ) {
			token = gettoken();
			expr10();
			push(c_mul,0);
		} else if( t== '/' ) {
			token = gettoken();
			expr10();
			push(c_div,0);
		} else if( t== '%' ) {
			token = gettoken();
			expr10();
			push(c_mod,0);
		} else break;
}

/* OPERATOR ~ ! - */ 
expr10()
{
	int t;
	t = token;	
	if( t=='!' ){
		token = gettoken();
		expr10();
		push(c_not,0);
	} else if( t== '~' ) {
		token = gettoken();
		expr10();
		push(c_bnot,0);
	} else if( t== '-' ) {
		token = gettoken();
		expr10();
		push(c_uniminus,0);
	} else factor();
}

/* ----------------------------------------------------------------------
 * explist:
 *	argc is the number of arguments expected.
 *	Parse an expression list of the form:
 *		<explist> ==> ( <exps> )
 *			| ( )
 *			| empty
 *
 *		<exps> ==> <exps> , <expression>
 *			| <expression>
 *
 */

explist( argc )
{
	if( token != '(' && !argc ) return;

	if( token != '(' ) error("missing '('");
	token = gettoken();

	if( !argc && token == ')' ) {
		token = gettoken();
		return;
	}

	for(;;) {
		expression();
		argc--;
		if( token == ')' ) break;
		if( token != ',' ) error("missing ','");
		token = gettoken();
	}

	token = gettoken();
	if( argc ) error("wrong number of arguments");
}	

/* ----------------------------------------------------------------------
 * factor:
 *	Parse a factor. Could be a number, variable, date, function call or
 *	regular expression string.
 */
 
factor()
{
	long l,datespec();
	int pc;

	switch(token) {
		case '(':
			token = gettoken();
			expression();
			if( token != ')' )
				error("missing ')'");
			token = gettoken();
			break;
		case NUMBER:
			push(c_number,tokenval);
			token = gettoken();
			break;
		case FUNCTION:
			pc = tokensym->value;
			token = gettoken();
			explist( StackProgram[ pc ].value );
			push(c_func,pc);
			break;
		case PARAM:
			push(c_param,tokensym->value);
			token = gettoken();
			break;
		case FIELD:
			push(tokensym->func,tokenval);
			token = gettoken();
			break;
		case '[':
			token = gettoken();
			l=datespec();
			if( token != ']' )
				error("missing ']'");
			token = gettoken();
			push(c_number,l);
			break;
		case STR:
			push(c_str,tokenval);
			token = gettoken();
			break;
		case IDENTIFIER:
			error("undefined identifier");
		default:
			error("syntax error");
	}
}

/* ----------------------------------------------------------------------
 * sectime:
 *	calculate the number of seconds between January 1, 1970
 *	and year/month/day. Return that value.
 *
 */

#define leap(d)	(((d % 4 == 0) && (d % 100 != 0)) || (d % 400 == 0))
#define DAYSEC	(3600*24)
#define YERSEC	(3600*24*365)
#define TIME0	1970

long sectime(year,month,day)
int year,month,day;
{

        static int months[13]={0,31,28,31,30,31,30,31,31,30,31,30,31};
	int yeardays,leapers,x;
	long seconds;

	if(month>12 || month<1 || year<TIME0 || day<1 || day>months[month]+
		(month==2 && leap(year)) )
			return(-1);

	yeardays = leapers = 0;

	for(x=1;x<month;x++)
		yeardays += months[x];
	if ((month > 2) && leap(year)) yeardays++;

	for(x=TIME0; x<year; x++)
		if(leap(x)) leapers++;
	
	seconds = yeardays*DAYSEC+(year-TIME0)*YERSEC+7*3600+
			leapers*DAYSEC + day*DAYSEC;

	return(seconds);

}

/* ----------------------------------------------------------------------
 * datespec:
 *	parse a date. Return the number of seconds from
 *	some date in 1970, until the specified date.
 */

long datespec()
{
	int year,month,day,seconds;

	if( token != NUMBER ) error("number expected");
	year = tokenval;
	token = gettoken();
	if( token != '/' ) error("missing '/'");
	token = gettoken();
	if( token != NUMBER ) error("number expected");
	month = tokenval;
	token = gettoken();
	if( token != '/' ) error("missing '/'");
	token = gettoken();
	if( token != NUMBER ) error("number expected");
	day = tokenval;
	token = gettoken();

	if( (seconds = sectime(year,month,day)) < 0 ) 
		error("invalid date");

	return(seconds);
}


/* ----------------------------------------------------------------------
 * gettoken:
 *	Return the next token.
 *	global variable: tokenval will contain any extra
 *	attribute associated with the returned token, ie
 *	the VALUE of a number, the index of the string etc...
 *	tokensym will be a pointer to the symbol table entry for
 *	any symbol encountered.
 *
 */
 
gettoken()
{
	char buf[IDLENGTH+1],*bufp=buf;
	int c,incomment;

	incomment = 0;
	c = getit();
	while( c == ' ' || c == '\t' || c == '\n' || c == '/' || incomment) {
	   if( c == '/' && !incomment) {
		c = getit();
		if( c != '*' ) {
			ungetit(c);
			c = '/';
			break;
		}
		incomment = 1;
	   } else if( c == '*' ) {
		c = getit();
		if( c == '/' ) incomment = 0;
	   }
	   c = getit();
	}

	if(c=='0') {
		tokenval=0;
		while( ( c=getit() ) >= '0' && c <= '7' ) {
			tokenval <<= 3;
			tokenval += c-'0';
		}
		if( isdigit(c) ) error("bad octal constant");
		ungetit(c);
		return(NUMBER);
	}
 
	if(isdigit(c)) {
		tokenval=c-'0';
		while(isdigit( (c=getit()) )) {
			tokenval *=10;
			tokenval += c-'0';
		}
		ungetit(c);
		return(NUMBER);
	}
	
	if(isalpha(c)) {
	   int count=0;
	   do {
		if(count++ < IDLENGTH) *bufp++ = c;
		c=getit();
	   } while( isalnum(c) );
	   ungetit(c);
	   *bufp='\0';
	   if( (tokensym=locatename(buf)) == NULL ) {
			tokensym = insertname(buf,IDENTIFIER,0);
	   }
	   tokenval = tokensym->value;
	   return( tokensym->type );
	}

	if( c == '"' ) {
		tokenval=strfree;
		while( (c=getit()) != '"' ) {
			if( strfree > STRLEN )
				error("no more string space");
			Strbuf[strfree++]= c;
		}
		Strbuf[strfree++]='\0';
		return(STR);
	}

	if( c == '=' ) {
		c=getit();
		if(c== '=') return(EQ);
		else {
			ungetit(c);
			return('=');
		}
	}

	if( c== '$' ) {
	   int count=0;
	   struct passwd *info,*getpwnam();
	   c=getit();
	   if( c=='$' ) {
		tokenval = getuid();
		return( NUMBER );
	   }
	   do {
		if (count++ < IDLENGTH) *bufp++ = c;
		c=getit();
	   } while( isalnum(c) );
	   ungetit(c);
	   *bufp='\0';
	   if( (info=getpwnam(buf)) == NULL ) 
		error("no such user");
	   tokenval = info->pw_uid;
	   return( NUMBER );
	}
	
	if( c == '!' ) {
		c=getit();
		if( c == '=' ) return(NE);
		ungetit(c);
		return('!');
	}
 
	if( c == '>' ) {
		c=getit();
		if( c == '=' ) return(GE);
		if( c == '>' ) return(SHIFTR);
		ungetit(c);
		return('>');
	}

	if( c == '<' ) {
		c=getit();
		if( c == '=' ) return(LE);
		if( c == '<' ) return(SHIFTL);
		ungetit(c);
		return('<');
	}

	if( c == '&' ) {
		c=getit();
		if( c == '&' ) return(AND);
		ungetit(c);
		return('&');
	}

	if( c == '|' ) {
		c=getit();
		if( c == '|' ) return(OR);
		ungetit(c);
		return('|');
	}

	return(c);
}

