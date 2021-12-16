
/* ----------------------------------------------------------------------
 * FILE: rhcmds.c
 * VERSION: 2
 * Written by: Ken Stauffer
 * This file contains the functions that do the evaluation of
 * the stack program.
 * These functions are simple, and behave like RPN operators, that is
 * they use the last two values on the stack, apply an operator
 * and push the result. Similarly for unary ops.
 *
 * ---------------------------------------------------------------------- */

#include "rh.h"

#include <string.h>
#include <fnmatch.h>
#include <sys/stat.h>

#ifndef FNM_EXTMATCH
#define FNM_EXTMATCH 0
#endif

/* binary operators */

void c_or(long i)		{ Stack[SP - 2] = Stack[SP - 2] || Stack[SP - 1]; SP--; }
void c_and(long i)		{ Stack[SP - 2] = Stack[SP - 2] && Stack[SP - 1]; SP--; }
void c_le(long i)		{ Stack[SP - 2] = Stack[SP - 2] <= Stack[SP - 1]; SP--; }
void c_lt(long i)		{ Stack[SP - 2] = Stack[SP - 2] < Stack[SP - 1]; SP--;  }
void c_ge(long i)		{ Stack[SP - 2] = Stack[SP - 2] >= Stack[SP - 1]; SP--; }
void c_gt(long i)		{ Stack[SP - 2] = Stack[SP - 2] > Stack[SP - 1]; SP--;  }
void c_ne(long i)		{ Stack[SP - 2] = Stack[SP - 2] != Stack[SP - 1]; SP--; }
void c_eq(long i)		{ Stack[SP - 2] = Stack[SP - 2] == Stack[SP - 1]; SP--; }
void c_bor(long i)		{ Stack[SP - 2] = Stack[SP - 2] | Stack[SP - 1]; SP--;  }
void c_band(long i)		{ Stack[SP - 2] = Stack[SP - 2] & Stack[SP - 1]; SP--;  }
void c_bxor(long i)		{ Stack[SP - 2] = Stack[SP - 2] ^ Stack[SP - 1]; SP--;  }
void c_lshift(long i)	{ Stack[SP - 2] = Stack[SP - 2] << Stack[SP - 1]; SP--; }
void c_rshift(long i)	{ Stack[SP - 2] = Stack[SP - 2] >> Stack[SP - 1]; SP--; }
void c_plus(long i)		{ Stack[SP - 2] = Stack[SP - 2] + Stack[SP - 1]; SP--;  }
void c_mul(long i)		{ Stack[SP - 2] = Stack[SP - 2] * Stack[SP - 1]; SP--;  }
void c_minus(long i)	{ Stack[SP - 2] = Stack[SP - 2] - Stack[SP - 1]; SP--;  }
void c_div(long i)		{ Stack[SP - 2] = Stack[SP - 2] / Stack[SP - 1]; SP--;  }
void c_mod(long i)		{ Stack[SP - 2] = Stack[SP - 2] % Stack[SP - 1]; SP--;  }


/* unary operators */

void c_not(long i)		{ Stack[SP - 1] = !Stack[SP - 1]; }
void c_bnot(long i)		{ Stack[SP - 1] = ~Stack[SP - 1]; }
void c_uniminus(long i)	{ Stack[SP - 1] = -Stack[SP - 1]; }

/* ternary operator ?: */

void c_qm(long i)		{ PC = (Stack[SP - 1]) ? PC : i; SP--; }
void c_colon(long i)	{ PC = i; }

/* accessing a parameter */

void c_param(long i)
{
	Stack[SP++] = Stack[FP + i];
}

/* calling a function */

void c_func(long i)
{
	Stack[SP++] = PC;
	Stack[SP++] = FP;
	PC = i;
	FP = SP - (StackProgram[PC].value + 2);
}

/* returning from a function */

void c_return(long i)
{
	PC = Stack[SP - 3];
	FP = Stack[SP - 2];
	Stack[SP - (3 + i)] = Stack[SP - 1];
	SP -= (2 + i);
}

/* operand functions */

void c_number(long i)	{ Stack[SP++] = i;                  }
void c_atime(long i)	{ Stack[SP++] = attr.buf->st_atime; }
void c_ctime(long i)	{ Stack[SP++] = attr.buf->st_ctime; }
void c_dev(long i)		{ Stack[SP++] = attr.buf->st_dev;   }
void c_gid(long i)		{ Stack[SP++] = attr.buf->st_gid;   }
void c_ino(long i)		{ Stack[SP++] = attr.buf->st_ino;   }
void c_mode(long i)		{ Stack[SP++] = attr.buf->st_mode;  }
void c_mtime(long i)	{ Stack[SP++] = attr.buf->st_mtime; }
void c_nlink(long i)	{ Stack[SP++] = attr.buf->st_nlink; }
void c_rdev(long i)		{ Stack[SP++] = attr.buf->st_rdev;  }
void c_size(long i)		{ Stack[SP++] = attr.buf->st_size;  }
void c_uid(long i)		{ Stack[SP++] = attr.buf->st_uid;   }
void c_depth(long i)	{ Stack[SP++] = attr.depth;         }
void c_prune(long i)	{ Stack[SP++] = 0; attr.prune = 1;  }

/* calculate the filename length */

void c_baselen(long i)
{
	char *c;
	register int len;

	len = 0;

	for (c = attr.fname; *c; c++)
		len = (*c == '/') ? 0 : len + 1;

	Stack[SP++] = len;
}

/* ----------------------------------------------------------------------
 * c_str:
 *    This implements glob matching.
 *    'i' is an index into the array Strbuf[]. The
 *    string contained there is the actual '\0' terminated
 *    string that occurred in the pattern (eg "*.BAK" ), minus
 *    the quotes "".
 */

void c_str(long i)
{
	char *tail;

	tail = strrchr(attr.fname, '/');
	tail = (tail) ? tail + 1 : attr.fname;
	Stack[SP++] = fnmatch(&Strbuf[i], tail, FNM_PATHNAME | FNM_PERIOD | FNM_EXTMATCH) == 0;
}

/* vi:set ts=4 sw=4: */
