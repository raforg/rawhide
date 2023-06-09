/*
* rawhide - find files using pretty C expressions
* https://raf.org/rawhide
* https://github.com/raforg/rawhide
* https://codeberg.org/raforg/rawhide
*
* Copyright (C) 1990 Ken Stauffer, 2022-2023 raf <raf@raf.org>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, see <https://www.gnu.org/licenses/>.
*
* 20230609 raf <raf@raf.org>
*/

#define _GNU_SOURCE /* For FNM_EXTMATCH and FNM_CASEFOLD in <fnmatch.h> */
#define _FILE_OFFSET_BITS 64 /* For 64-bit off_t on 32-bit systems (Not AIX) */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <fnmatch.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>

#include "rh.h"
#include "rhdata.h"
#include "rhcmds.h"
#include "rhstr.h"

#ifdef NDEBUG
#define debug(args)
#define debug_extra(args)
#else
#define debug(args) debugf args
#define debug_extra(args) debug_extraf args
#endif

#define isaleph(c) ((c) != EOF && (isalpha(c) || (c) & 0x80))

static int cpos;         /* Current byte position */
static int lineno;       /* Current line number */
static int decimal_mode; /* Only decimal, not octal */
static int last_reffile; /* Most recent reference file */
static char *saved_expstr;

static void parse_function(void);
static int parse_parameters(void);
static void parse_expression(void);
static void parse_or_expr(void);
static void parse_and_expr(void);
static void parse_bitor_expr(void);
static void parse_bitxor_expr(void);
static void parse_bitand_expr(void);
static void parse_eq_expr(void);
static void parse_rel_expr(void);
static void parse_shift_expr(void);
static void parse_add_expr(void);
static void parse_mul_expr(void);
static void parse_unary_expr(void);
static void parse_factor(void);
static void parse_arguments(int argc);
static int get_token(void);
static int get_token_decimal(void);
static llong parse_datetime(void);
static const char *show_token(void);

#ifndef NDEBUG
/*

static void debugf(const char *format, ...);

Output a parser debug message to stderr, if requested.

*/

static void debugf(const char *format, ...)
{
	va_list args;

	if (!(attr.debug_flags & DEBUG_PARSER))
		return;

	va_start(args, format);
	fprintf(stderr, "%s: ", "parser");
	vfprintf(stderr, format, args);
	fprintf(stderr, "\n");
	va_end(args);
}

/*

static void debug_extraf(const char *format, ...);

Ouptut an extra parser debug message to stderr, if requested.

*/

static void debug_extraf(const char *format, ...)
{
	va_list args;

	if (!(attr.debug_flags & DEBUG_PARSER))
		return;

	if (!(attr.debug_flags & DEBUG_EXTRA))
		return;

	va_start(args, format);
	fprintf(stderr, "%s: ", "parser");
	vfprintf(stderr, format, args);
	fprintf(stderr, "\n");
	va_end(args);
}
#endif

/*

static int getch(void);

Return the next input char from expstr or expfile.

*/

static int getch(void)
{
	int c;

	if (expstr)
		c = (*expstr) ? *expstr++ : EOF;
	else
		c = getc(expfile);

	if (c == '\n')
	{
		lineno++;
		cpos = 0;
	}

	cpos++;

	debug(("getch() = %c (%d)", (c != EOF && isprint(c)) ? c : ' ', c));

	return c;
}

/*

static void ungetch(int c);

Unget the char c so that the next getch() will return it.

*/

static void ungetch(int c)
{
	debug(("ungetch() %c (%d)", (c != EOF && isprint(c)) ? c : ' ', c));

	if (c == '\n')
	{
		lineno--;
		cpos = 1; /* Wrong but OK */
	}
	else
		cpos--;

	if (expstr)
		expstr = (c > 0) ? expstr - 1 : expstr;
	else
		ungetc(c, expfile);
}

/*

static void parser_error(const char *format, ...);

Output a syntax error message and exit.
The format parameter is a printf-like format.
Subsequent arguments must satisfy its conversions.

*/

static void parser_error(const char *format, ...)
{
	va_list args;

	if (expstr)
		fprintf(stderr, "%s: command line: -e '%s': ", prog_name, saved_expstr);
	else
		fprintf(stderr, "%s: %s: ", prog_name, expfname);

	fprintf(stderr, "line %d byte %d: ", lineno, cpos);
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");

	rawhide_finish();

	if (expfile)
		fclose(expfile);

	exit(EXIT_FAILURE);
}

/*

static void add_instruction(void (*func)(llong), llong value);

Store an instruction.

*/

static void add_instruction(void (*func)(llong), llong value)
{
	debug_extra(("instruction %s %lld", instruction_name(func), value));

	if (rawhide_instruction(func, value) == -1)
		parser_error("program too big");
}

/*

void parse_program(void);

Parse a program:

	<program> ::=
		  <function-list> <expression> ";" EOF
		| <function-list> <expression> EOF
		| <function-list> EOF

	<function-list> ::=
		  <function> <function-list>
		| EMPTY

*/

void parse_program(void)
{
	debug(("program(%s)", (expstr) ? expstr : expfname));

	cpos = 0;
	lineno = 1;
	saved_expstr = expstr;
	token = get_token();

	while (token == IDENTIFIER)
		parse_function();

	if (token != EOF)
	{
		startPC = PC;
		parse_expression();
		add_instruction(NULL, 0);
	}

	if (token != ';' && token != EOF)
	{
		if (token == '{' || token == '(')
			parser_error("expected ';' or EOF after final condition expression, found %s (possible attempt to redefine an existing function)", show_token());
		else
			parser_error("expected ';' or EOF after final condition expression, found %s", show_token());
	}
}

/*

static void parse_function(void);

Parse a function:

	<function> ::=
		IDENTIFIER <parameters> "{" <return> <expression> <eos> "}"

	<return> ::=
		  "return"
		| EMPTY

	<eos> == >
		  ";"
		| EMPTY

*/

static void parse_function(void)
{
	symbol_t *s;

	debug(("function()"));

	s = tokensym;
	tokensym->value = PC;
	tokensym->type = FUNCTION;
	tokensym->func = c_func;

	token = get_token();

	add_instruction(NULL, parse_parameters()); /* Save number of parameters for function */

	if (token != '{')
		parser_error("expected '{' to start a function body, found %s", show_token());

	token = get_token();

	if (token == RETURN)
		token = get_token();

	parse_expression();

	if (token == ';')
		token = get_token();

	add_instruction(c_return, Program[s->value].value);

	/* Free the parameter symbols */

	while (symbols->type == PARAM)
	{
		s = symbols;
		symbols = symbols->next;
		free(s->name);
		free(s);
	}

	if (token != '}')
		parser_error("expected '}' to end a function body, found %s", show_token());

	token = get_token();
}

/*

static int parse_parameters(void);

Parse a parameter list and return the maximum offset:

	<parameters> ::=
		  "(" <id-list> ")"
		| "(" ")"
		| EMPTY

	<id-list> ::=
		IDENTIFIER <id-tail>

	<id-tail> ::=
		  "," <id-list>
		| EMPTY

*/

static int parse_parameters(void)
{
	int offset = 0;

	debug_extra(("parameters()"));

	if (token == '(')
		token = get_token();
	else if (token == '{')
		return 0;
	else
		parser_error("expected '(' or '{', found %s (possible attempt to call an undefined function)", show_token());

	if (token == ')')
	{
		token = get_token();
		return 0;
	}

	for (;;)
	{
		if (token != IDENTIFIER)
			parser_error("expected identifier (parameter name), found %s", show_token());

		tokensym->type = PARAM;
		tokensym->func = c_param;
		tokensym->value = offset++;

		token = get_token();

		if (token == ')')
			break;

		if (token != ',')
			parser_error("expected ')' (end of parameters) or ',' (before next parameter), found %s", show_token());

		token = get_token();
	}

	token = get_token();

	return offset;
}

/*

static void parse_expression(void);

Parse an expression. The conditional operator:

	<expression> ::= <or> "?" <expression> ":" <expression> | <or>

*/

static void parse_expression(void)
{
	debug(("expression()"));

	parse_or_expr();

	if (token == '?')
	{
		int qm, colon;

		qm = PC;
		add_instruction(c_qm, 0);

		token = get_token();
		parse_expression();

		if (token != ':')
			parser_error("expected ':' after '?' (ternary operator), found %s", show_token());

		colon = PC;
		add_instruction(c_colon, 0);

		token = get_token();
		parse_expression();

		Program[qm].value = colon;
		Program[colon].value = PC - 1;
	}
}

/*

static void parse_or_expr(void);

Parse an or expression:

	<or> ::= <and> "||" <or> | <and>

*/

static void parse_or_expr(void)
{
	/* (a || b) is ((a) ? 1 : (b) ? 1 : 0) */

	debug_extra(("or_expr()"));

	parse_and_expr();

	for (;;)
	{
		if (token == OR)
		{
			int qm, colon;

			qm = PC;
			add_instruction(c_qm, 0);
			add_instruction(c_number, 1);

			colon = PC;
			add_instruction(c_colon, 0);

			token = get_token();
			parse_and_expr();

			Program[qm].value = colon;
			Program[colon].value = PC - 1;

			qm = PC;
			add_instruction(c_qm, 0);
			add_instruction(c_number, 1);

			colon = PC;
			add_instruction(c_colon, 0);
			add_instruction(c_number, 0);

			Program[qm].value = colon;
			Program[colon].value = PC - 1;
		}
		else
			break;
	}
}

/*

static void parse_and_expr(void);

Parse an and expression:

	<and> ::= <bitor> "&&" <and> | <bitor>

*/

static void parse_and_expr(void)
{
	/* (a && b) is ((a) ? (b) ? 1 : 0 : 0) */

	debug_extra(("and_expr()"));

	parse_bitor_expr();

	for (;;)
	{
		if (token == AND)
		{
			int qm1, colon1;

			qm1 = PC;
			add_instruction(c_qm, 0);

			token = get_token();
			parse_bitor_expr();

			{
				int qm2, colon2;

				qm2 = PC;
				add_instruction(c_qm, 0);

				add_instruction(c_number, 1);

				colon2 = PC;
				add_instruction(c_colon, 0);

				add_instruction(c_number, 0);

				Program[qm2].value = colon2;
				Program[colon2].value = PC - 1;
			}

			colon1 = PC;
			add_instruction(c_colon, 0);

			add_instruction(c_number, 0);

			Program[qm1].value = colon1;
			Program[colon1].value = PC - 1;
		}
		else
			break;
	}
}

/*

static void parse_bitor_expr(void);

Parse a bitor expression:

	<bitor> ::= <bitxor> "|" <bitor> | <bitxor>

*/

static void parse_bitor_expr(void)
{
	debug_extra(("bitor_expr()"));

	parse_bitxor_expr();

	for (;;)
	{
		if (token == '|')
		{
			token = get_token();
			parse_bitxor_expr();
			add_instruction(c_bitor, 0);
		}
		else
			break;
	}
}

/*

static void parse_bitxor_expr(void);

Parse a bitxor expression:

	<bitxor> ::= <bitand> "^" <bitxor> | <bitand>

*/

static void parse_bitxor_expr(void)
{
	debug_extra(("bitxor_expr()"));

	parse_bitand_expr();

	for (;;)
	{
		if (token == '^')
		{
			token = get_token();
			parse_bitand_expr();
			add_instruction(c_bitxor, 0);
		}
		else
			break;
	}
}

/*

static void parse_bitand_expr(void);

Parse a bitand expression:

	<bitand> ::= <eq> "&" <bitand> | <eq>

*/

static void parse_bitand_expr(void)
{
	debug_extra(("bitand_expr()"));

	parse_eq_expr();

	for (;;)
	{
		if (token == '&')
		{
			token = get_token();
			parse_eq_expr();
			add_instruction(c_bitand, 0);
		}
		else
			break;
	}
}

/*

static void parse_eq_expr(void);

Parse an equality expression:

	<eq> ::= <rel> <eq-op> <rel> | <rel>
	<eq-op> ::= "==" | "!="

*/

static void parse_eq_expr(void)
{
	debug_extra(("eq_expr()"));

	parse_rel_expr();

	if (token == EQ)
	{
		token = get_token();
		parse_rel_expr();
		add_instruction(c_eq, 0);
	}
	else if (token == NE)
	{
		token = get_token();
		parse_rel_expr();
		add_instruction(c_ne, 0);
	}
}

/*

static void parse_rel_expr(void);

Parse a relative comparison expression:

	<rel> ::= <shift> <rel-op> <shift> | <shift>
	<rel-op> ::= "<" | ">" | "<=" | ">="

*/

static void parse_rel_expr(void)
{
	debug_extra(("rel_expr()"));

	parse_shift_expr();

	if (token == LE)
	{
		token = get_token();
		parse_shift_expr();
		add_instruction(c_le, 0);
	}
	else if (token == GE)
	{
		token = get_token();
		parse_shift_expr();
		add_instruction(c_ge, 0);
	}
	else if (token == '>')
	{
		token = get_token();
		parse_shift_expr();
		add_instruction(c_gt, 0);
	}
	else if (token == '<')
	{
		token = get_token();
		parse_shift_expr();
		add_instruction(c_lt, 0);
	}
}

/*

static void parse_shift_expr(void);

Parse a shift expression:

	<shift> ::= <add> <shift-op> <shift> | <add>
	<shift-op> ::= "<<" | ">>"

*/

static void parse_shift_expr(void)
{
	debug_extra(("shift_expr()"));

	parse_add_expr();

	for (;;)
	{
		if (token == SHIFTL)
		{
			token = get_token();
			parse_add_expr();
			add_instruction(c_lshift, 0);
		}
		else if (token == SHIFTR)
		{
			token = get_token();
			parse_add_expr();
			add_instruction(c_rshift, 0);
		}
		else
			break;
	}
}

/*

static void parse_add_expr(void);

Parse an additive expression:

	<add> ::= <mul> <add-op> <add> | <mul>
	<add-op> ::= "+" | "-"

*/

static void parse_add_expr(void)
{
	debug_extra(("add_expr()"));

	parse_mul_expr();

	for (;;)
	{
		if (token == '+')
		{
			token = get_token();
			parse_mul_expr();
			add_instruction(c_plus, 0);
		}
		else if (token == '-')
		{
			token = get_token();
			parse_mul_expr();
			add_instruction(c_minus, 0);
		}
		else
			break;
	}
}

/*

static void parse_mul_expr(void);

Parse a multiplicative expression:

	<mul> ::= <unary> <mul-op> <mul> | <unary>
	<mul-op> ::= "*" | "/" | "%"

*/

static void parse_mul_expr(void)
{
	debug_extra(("mul_expr()"));

	parse_unary_expr();

	for (;;)
	{
		if (token == '*')
		{
			token = get_token();
			parse_unary_expr();
			add_instruction(c_mul, 0);
		}
		else if (token == '/')
		{
			token = get_token();
			parse_unary_expr();
			add_instruction(c_div, 0);
		}
		else if (token == '%')
		{
			token = get_token();
			parse_unary_expr();
			add_instruction(c_mod, 0);
		}
		else
			break;
	}
}

/*

static void parse_unary_expr(void);

Parse a unary operator expression:

	<unary> ::= <unary-op> <unary> | <factor>
	<unary-op> ::= "-" | "~" | "!"

*/

static void parse_unary_expr(void)
{
	debug_extra(("unary_expr()"));

	if (token == '!')
	{
		token = get_token();
		parse_unary_expr();
		add_instruction(c_not, 0);
	}
	else if (token == '-')
	{
		token = get_token();
		parse_unary_expr();
		add_instruction(c_uniminus, 0);
	}
	else if (token == '~')
	{
		token = get_token();
		parse_unary_expr();
		add_instruction(c_bitnot, 0);
	}
	else
		parse_factor();
}

/*

static void parse_factor(void);

Parse a factor:

	<factor> ::=
		  <number>
		| <user-id>
		| <group-id>
		| "[" <date-time> "]"
		| <pattern>
		| <reference-file>
		| <built-in>
		| <function-call>
		| <parameter>
		| "(" <expression> ")"

*/

static void parse_factor(void)
{
	debug_extra(("factor()"));

	switch (token)
	{
		case '(':
		{
			token = get_token();
			parse_expression();

			if (token != ')')
				parser_error("expected ')', found %s", show_token());

			token = get_token();

			break;
		}

		case NUMBER:
		{
			add_instruction(c_number, tokenval);
			token = get_token();

			break;
		}

		case FUNCTION:
		{
			int pc = tokensym->value;

			token = get_token();
			parse_arguments(Program[pc].value);
			add_instruction(c_func, pc);

			break;
		}

		case PARAM:
		{
			add_instruction(c_param, tokensym->value);
			token = get_token();

			break;
		}

		case '[':
		{
			llong l;

			token = get_token_decimal();
			l = parse_datetime();

			if (token != ']')
				parser_error("expected ']', found %s", show_token());

			token = get_token();
			add_instruction(c_number, l);

			break;
		}

		case STRING:
		{
			add_instruction(c_glob, tokenval);
			token = get_token();

			break;
		}

		case FIELD:
		case PATMOD:
		case REFFILE:
		{
			add_instruction(tokensym->func, tokenval);
			token = get_token();

			break;
		}

		case IDENTIFIER:
		{
			parser_error("undefined identifier: %s", show_token());
		}

		default:
		{
			parser_error("syntax error: %s", show_token());
		}
	}
}

/*

static void parse_arguments(int argc);

Parse function arguments:

	<arguments> ::=
		  "(" <expr-list> ")"
		| "(" ")"
		| EMPTY

	<expr-list> ::=
		<expression> <expr-tail>

	<expr-tail> ::=
		  "," <expr-list>
		| EMPTY

The argc parameter is the number of arguments expected.

*/

static void parse_arguments(int argc)
{
	int orig_argc = argc;

	debug_extra(("arguments()"));

	if (token != '(' && !argc)
		return;

	if (token != '(')
		parser_error("expected '(' in function call (%d argument%s %s required), found %s", argc, (argc == 1) ? "" : "s", (argc == 1) ? "is" : "are", show_token());

	token = get_token();

	if (!argc && token == ')')
	{
		token = get_token();
		return;
	}

	for (;;)
	{
		parse_expression();
		argc--;

		if (token == ')')
			break;

		if (token != ',')
			parser_error("expected ')' or ',' in function argument list, found %s", show_token());

		token = get_token();
	}

	token = get_token();

	if (argc)
		parser_error("wrong number of function arguments: expected %d, found %d", orig_argc, orig_argc - argc);
}

/*

static llong parse_datetime(void);

Parse a date/time:

	<date-time> ::=
		  NUMBER "/" NUMBER "/" NUMBER
		| NUMBER "/" NUMBER "/" NUMBER NUMBER
		| NUMBER "/" NUMBER "/" NUMBER NUMBER ":" NUMBER
		| NUMBER "/" NUMBER "/" NUMBER NUMBER ":" NUMBER ":" NUMBER

If the year is below 100 it is interpreted as being in the current century.
Return the seconds from the UNIX epoch until the specified date/time.

*/

static llong parse_datetime(void)
{
	int year, month, day, hour = 0, minute = 0, second = 0;
	llong seconds;
	struct tm tm[1];

	debug_extra(("datetime()"));

	/* Year */

	if (token != NUMBER)
		parser_error("expected year number, found %s", show_token());

	year = tokenval;

	if (year < 100)
	{
		time_t now = time(NULL);
		struct tm *lc = localtime(&now);

		year += ((lc->tm_year + 1900) / 100) * 100;
	}

	/* Month */

	if ((token = get_token()) != '/')
		parser_error("expected '/' after year, found %s", show_token());

	if ((token = get_token_decimal()) != NUMBER)
		parser_error("expected month number, found %s", show_token());

	month = tokenval;

	/* Day */

	if ((token = get_token()) != '/')
		parser_error("expected '/' after month, found %s", show_token());

	if ((token = get_token_decimal()) != NUMBER)
		parser_error("expected day number, found %s", show_token());

	day = tokenval;

	/* Hour (optional) */

	if ((token = get_token_decimal()) == NUMBER)
	{
		hour = tokenval;

		/* Minutes (optional) */

		if ((token = get_token()) == ':')
		{
			if ((token = get_token_decimal()) != NUMBER)
				parser_error("expected minute number, found %s", show_token());

			minute = tokenval;

			/* Seconds (optional) */

			if ((token = get_token()) == ':')
			{
				if ((token = get_token_decimal()) != NUMBER)
					parser_error("expected second number, found %s", show_token());

				second = tokenval;
				token = get_token();
			}
		}
	}

	/* Convert to seconds since the epoch */

	tm->tm_sec = second;
	tm->tm_min = minute;
	tm->tm_hour = hour;
	tm->tm_mday = day;
	tm->tm_mon = month - 1;
	tm->tm_year = year - 1900;
	tm->tm_wday = 0;
	tm->tm_yday = 0;
	tm->tm_isdst = -1;

	if ((seconds = (llong)mktime(tm)) == -1 || (year == 1900 && attr.test_invalid_date))
	{
		if (hour == 0 && minute == 0 && second == 0)
			parser_error("invalid date: [%04d/%d/%d]", year, month, day);
		else if (minute == 0 && second == 0)
			parser_error("invalid time: [%04d/%d/%d %d]", year, month, day, hour);
		else if (second == 0)
			parser_error("invalid time: [%04d/%d/%d %d:%02d]", year, month, day, hour, minute);
		else
			parser_error("invalid time: [%04d/%d/%d %d:%02d:%02d]", year, month, day, hour, minute, second);
	}

	return seconds;
}

/*

static int get_token(void);

Scan the next token and return the token type.
The global variable, tokenval, will contain any extra
attribute associated with the returned token type, i.e.,
the value of a NUMBER token, the index of the string etc.
The global variable, tokensym, will be a pointer to the
symbol table entry for any symbol encountered.

Tokens look like:

    <number> ::=
          DECIMAL
        | DECIMAL NOSPACE <scale>
        | "0" NOSPACE OCTAL
        | "0x" NOSPACE HEXADECIMAL

    <scale> ::=
          "K" | "M" | "G" | "T" | "P" | "E"
        | "k" | "m" | "g" | "t" | "p" | "e"

    <user-id> ::=
          "$" NOSPACE USERNAME
        | "$$"

    <group-id> ::=
          "@" NOSPACE GROUPNAME
        | "@@"

    <pattern> ::=
          STRING
        | STRING NOSPACE "." NOSPACE <pattern-modifier>

    <pattern-modifier> ::= PATMOD symbol tale entries

    <reference-file> ::=
        STRING NOSPACE "." NOSPACE <reference-file-field>

    <reference-file-field> ::= REFFILE symbol table entries

    <built-in> ::= FIELD/NUMBER symbol table entries

    <parameter> ::= PARAM symbol table entries

And single-letter operators.

*/

static int _get_token(void);
static int get_token(void)
{
	int saved_token, new_token;

	new_token = _get_token();
	saved_token = token;
	token = new_token;
	debug(("get_token() = %s", show_token()));
	token = saved_token;

	return new_token;
}

static int _get_token(void)
{
	static char *scales = "KMGTPEkmgtpe";
	static llong factor[] =
	{
		1LL << 10, 1LL << 20, 1LL << 30, 1LL << 40, 1LL << 50, 1LL << 60,
		1000LL, 1000000LL, 1000000000LL, 1000000000000LL, 1000000000000000LL, 1000000000000000000LL
	};

	char buf[MAX_IDENT_LENGTH + 1], *bufp = buf;
	int incomment = 0, c;
	char *scale, *start;

	tokensym = NULL;
	c = getch();

	/* Comment */

	while (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '/' || c == '#' || incomment)
	{
		if (c == EOF)
			parser_error("unterminated comment");

		if (c == '/' && !incomment)
		{
			c = getch();

			if (c == '/')
			{
				while ((c = getch()) != '\n' && c != EOF)
					;
			}
			else if (c == '*')
			{
				incomment = 1;
			}
			else
			{
				ungetch(c);
				c = '/';

				break;
			}
		}
		else if (c == '*')
		{
			c = getch();

			if (c == '/')
				incomment = 0;
		}
		else if (c == '#' && !incomment)
		{
			while ((c = getch()) != '\n' && c != EOF)
				;
		}

		c = getch();
	}

	/* Octal / Hexadecimal */

	if (c == '0' && !decimal_mode)
	{
		tokenval = 0;

		c = getch();

		if (c == 'x')
		{
			int num_digits = 0;

			while (isxdigit(c = getch()))
			{
				tokenval <<= 4;
				tokenval += (isdigit(c)) ? c - '0' : 10 + c - ((isupper(c)) ? 'A' : 'a');
				++num_digits;
			}

			if (num_digits == 0)
				parser_error("invalid hexadecimal digit: %c", c);

			ungetch(c);
		}
		else
		{
			ungetch(c);

			while ((c = getch()) >= '0' && c <= '7')
			{
				tokenval <<= 3;
				tokenval += c - '0';
			}

			if (isdigit(c))
				parser_error("invalid octal digit: %c", c);

			ungetch(c);
		}

		return NUMBER;
	}

	/* Decimal */

	if (isdigit(c))
	{
		tokenval = c - '0';

		while (isdigit((c = getch())))
		{
			tokenval *= 10;
			tokenval += c - '0';
		}

		if ((scale = strchr(scales, c)))
			tokenval *= factor[scale - scales];
		else
			ungetch(c);

		return NUMBER;
	}

	/* Identifier / Keyword */

	if (isaleph(c) || c == '_')
	{
		do
		{
			/* Identifiers are silently truncated */

			if (bufp - buf < MAX_IDENT_LENGTH)
				*bufp++ = c;

			c = getch();
		}
		while (isaleph(c) || isdigit(c) || c == '_');

		ungetch(c);
		*bufp = '\0';

		if (!strcmp(buf, "return"))
			return RETURN;

		if (!(tokensym = locate_symbol(buf)))
			if (!(tokensym = insert_symbol(buf, IDENTIFIER, 0)))
				parser_error("no more memory");

		tokenval = tokensym->value;

		return tokensym->type;
	}

	/* Glob/regex pattern / Reference file / Shell command */

	if (c == '"')
	{
		llong reffield, refstrfree, i, stripped = 0;

		tokenval = strfree;

		while ((c = getch()) != '"' && c != EOF)
		{
			if (strfree >= MAX_DATA_SIZE - 1)
				parser_error("no more string space");

			if (c == '\\')
			{
				if ((c = getch()) == EOF)
					parser_error("invalid character quoting in string");

				/* Backslash is only special before \ or " */

				if (!(c == '\\' || c == '"'))
				{
					if (strfree >= MAX_DATA_SIZE - 2)
						parser_error("no more string space");

					Strbuf[strfree++] = '\\';
				}
			}

			Strbuf[strfree++] = c;
		}

		if (c != '"')
			parser_error("invalid string literal (missing closing double quote)");

		Strbuf[strfree++] = '\0';

		/* Look for a suffix */

		if ((c = getch()) != '.')
		{
			ungetch(c);

			/* Assume "".path when / is present */

			if (!strchr(Strbuf + tokenval, '/'))
				return STRING;

			tokensym = locate_symbol(".path");

			return PATMOD;
		}

		/* Get the pattern modifier or reference file field name */

		reffield = refstrfree = strfree;

		if (refstrfree >= MAX_DATA_SIZE - 1)
			parser_error("no more reference file field name space");

		Strbuf[refstrfree++] = c;

		while ((c = getch()) != EOF && isalpha(c))
		{
			if (refstrfree >= MAX_DATA_SIZE - 1)
				parser_error("no more reference file field name space");

			Strbuf[refstrfree++] = c;
		}

		ungetch(c);
		Strbuf[refstrfree] = '\0';

		/* Look for the symbol (it can be a unique prefix of a pattern modifier) */

		if (!(tokensym = locate_symbol(Strbuf + reffield)))
			tokensym = locate_patmod_prefix(Strbuf + reffield);

		if (!tokensym || (tokensym->type != REFFILE && tokensym->type != PATMOD))
			parser_error("invalid string suffix: %s (expected pattern modifier or reference file field)", ok(Strbuf + reffield));

		/* When / is present, replace i with ipath, re with repath, and rei with reipath */

		if (tokensym->type == PATMOD)
		{
			if (strchr(Strbuf + tokenval, '/'))
			{
				#ifdef FNM_CASEFOLD
				if (!strcmp(tokensym->name, ".i"))
					tokensym = locate_symbol(".ipath");
				#endif
				#ifdef HAVE_PCRE2
				if (!strcmp(tokensym->name, ".re"))
					tokensym = locate_symbol(".repath");
				else if (!strcmp(tokensym->name, ".rei"))
					tokensym = locate_symbol(".reipath");
				#endif
			}

			return PATMOD;
		}

		/* Strip trailing / */

		for (refstrfree = reffield - 1; refstrfree - 1 > tokenval && Strbuf[refstrfree - 1] == '/'; )
			Strbuf[--refstrfree] = '\0', ++stripped;

		/* If the reference path is empty, it refers to the most recent reference path */

		if (Strbuf[tokenval] == '\0')
		{
			if (!reffree)
				parser_error("invalid implicit reference file path: \"\"%s (no preceding reference file path to refer to)", ok(Strbuf + reffield));

			strfree = tokenval;      /* Discard this empty reference file path */
			tokenval = last_reffile; /* Index into RefFile of the most recent reference file */

			return REFFILE;
		}

		/* Look for an earlier reference to the same path, or record this one */

		for (i = 0; i < reffree; ++i)
			if (Strbuf[tokenval] && !strcmp(Strbuf + RefFile[i].fpathi, Strbuf + tokenval))
				break;

		if (i < reffree)
		{
			strfree = tokenval;          /* Discard duplicates of the reference file path */
			last_reffile = tokenval = i; /* Index into RefFile of the first occurrence */
		}
		else
		{
			RefFile[reffree].fpathi = tokenval;
			start = (start = strrchr(Strbuf + tokenval, '/')) ? start + 1 : Strbuf + tokenval;
			RefFile[reffree].baselen = reffield - 1 - (start - Strbuf) - stripped;
			RefFile[reffree].exists = ((attr.follow_symlinks ? stat : lstat)(Strbuf + RefFile[reffree].fpathi, RefFile[reffree].statbuf) != -1);

			last_reffile = tokenval = reffree++;
		}

		debug(("reffile %s: type %o perm %o uid %d gid %d size %d", Strbuf + RefFile[tokenval].fpathi, RefFile[tokenval].statbuf->st_mode & S_IFMT, RefFile[tokenval].statbuf->st_mode & ~S_IFMT, RefFile[tokenval].statbuf->st_uid, RefFile[tokenval].statbuf->st_gid, RefFile[tokenval].statbuf->st_size));

		return REFFILE;
	}

	/* User ID */

	if (c == '$')
	{
		struct passwd *pwd;

		if ((c = getch()) == '$')
		{
			tokenval = getuid();

			return NUMBER;
		}

		while (isaleph(c) || isdigit(c) || c == '.' || c == '-' || c == '_')
		{
			if (bufp - buf < MAX_IDENT_LENGTH)
				*bufp++ = c;

			c = getch();
		}

		ungetch(c);
		*bufp = '\0';

		if (!(pwd = getpwnam(buf)))
			parser_error("no such user: %s", ok(buf));

		tokenval = pwd->pw_uid;

		return NUMBER;
	}

	/* Group ID */

	if (c == '@')
	{
		struct group *grp;

		if ((c = getch()) == '@')
		{
			tokenval = getgid();

			return NUMBER;
		}

		while (isaleph(c) || isdigit(c) || c == '.' || c == '-' || c == '_')
		{
			if (bufp - buf < MAX_IDENT_LENGTH)
				*bufp++ = c;

			c = getch();
		}

		ungetch(c);
		*bufp = '\0';

		if (!(grp = getgrnam(buf)))
			parser_error("no such group: %s", ok(buf));

		tokenval = grp->gr_gid;

		return NUMBER;
	}

	/* Operator == (or char '=') */

	if (c == '=')
	{
		c = getch();

		if (c == '=')
			return EQ;

		ungetch(c);

		return '=';
	}

	/* Operator != or ! */

	if (c == '!')
	{
		c = getch();

		if (c == '=')
			return NE;

		ungetch(c);

		return '!';
	}

	/* Operator >= or >> or > */

	if (c == '>')
	{
		c = getch();

		if (c == '=')
			return GE;

		if (c == '>')
			return SHIFTR;

		ungetch(c);

		return '>';
	}

	/* Operator <= or << or < */

	if (c == '<')
	{
		c = getch();

		if (c == '=')
			return LE;

		if (c == '<')
			return SHIFTL;

		ungetch(c);

		return '<';
	}

	/* Operator && or & */

	if (c == '&')
	{
		c = getch();

		if (c == '&')
			return AND;

		ungetch(c);

		return '&';
	}

	/* Operator || or | */

	if (c == '|')
	{
		c = getch();

		if (c == '|')
			return OR;

		ungetch(c);

		return '|';
	}

	/* Any other character or EOF */

	return c;
}

/*

static int get_token_decimal(void);

Like get_token() with decimal_mode set first and cleared afterwards.
This is for parsing datetimes to allow leading zeroes for decimals.

*/

static int get_token_decimal(void)
{
	int t;

	decimal_mode = 1;
	t = get_token();
	decimal_mode = 0;

	return t;
}

/*

static const char *show_token(void);

Return text describing the current token for error messages.

*/

static const char *show_token(void)
{
	#define BUFSIZE 4096
	static char buf[BUFSIZE];

	switch (token)
	{
		case EOF: snprintf(buf, BUFSIZE, "eof"); break;
		case RETURN: snprintf(buf, BUFSIZE, "return"); break;
		case IDENTIFIER: snprintf(buf, BUFSIZE, "identifier %s", tokensym->name); break;
		case FUNCTION: snprintf(buf, BUFSIZE, "function %s", tokensym->name); break;
		case PARAM: snprintf(buf, BUFSIZE, "param %s", tokensym->name); break;
		case NUMBER: snprintf(buf, BUFSIZE, "number %lld%s%s", tokenval, (tokensym) ? " " : "", (tokensym) ? tokensym->name : ""); break;
		case FIELD: snprintf(buf, BUFSIZE, "field %s", tokensym->name); break;
		case STRING: snprintf(buf, BUFSIZE, "string \"%s\"", Strbuf + tokenval); break;
		case PATMOD: snprintf(buf, BUFSIZE, "patmod \"%s\"%s", Strbuf + tokenval, tokensym->name); break;
		case REFFILE: snprintf(buf, BUFSIZE, "reffile \"%s\"%s", Strbuf + RefFile[tokenval].fpathi, tokensym->name); break;
		case EQ: snprintf(buf, BUFSIZE, "eq =="); break;
		case NE: snprintf(buf, BUFSIZE, "ne !="); break;
		case GE: snprintf(buf, BUFSIZE, "ge >="); break;
		case LE: snprintf(buf, BUFSIZE, "le <="); break;
		case SHIFTL: snprintf(buf, BUFSIZE, "shiftl <<"); break;
		case SHIFTR: snprintf(buf, BUFSIZE, "shiftr >>"); break;
		case AND: snprintf(buf, BUFSIZE, "and &&"); break;
		case OR: snprintf(buf, BUFSIZE, "or ||"); break;
		default: snprintf(buf, BUFSIZE, (isquotable(token)) ? "char 0x%02x" : "'%c'", (char)token); break;
	}

	return buf;
}

/* vi:set ts=4 sw=4: */
