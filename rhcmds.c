
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
#include <sys/types.h>
#include <sys/stat.h>

c_or(i)     long i;   { Stack[SP-2]=Stack[SP-2] || Stack[SP-1]; SP--; }
c_and(i)    long i;   { Stack[SP-2]=Stack[SP-2] && Stack[SP-1]; SP--; }
c_le(i)     long i;   { Stack[SP-2]=Stack[SP-2] <= Stack[SP-1]; SP--; }
c_lt(i)     long i;   { Stack[SP-2]=Stack[SP-2] < Stack[SP-1]; SP--;  }
c_ge(i)     long i;   { Stack[SP-2]=Stack[SP-2] >= Stack[SP-1]; SP--; }
c_gt(i)     long i;   { Stack[SP-2]=Stack[SP-2] > Stack[SP-1]; SP--;  }
c_ne(i)     long i;   { Stack[SP-2]=Stack[SP-2] != Stack[SP-1]; SP--; }
c_eq(i)     long i;   { Stack[SP-2]=Stack[SP-2] == Stack[SP-1]; SP--; }
c_bor(i)    long i;   { Stack[SP-2]=Stack[SP-2] | Stack[SP-1]; SP--;  }
c_band(i)   long i;   { Stack[SP-2]=Stack[SP-2] & Stack[SP-1]; SP--;  }
c_bxor(i)   long i;   { Stack[SP-2]=Stack[SP-2] ^ Stack[SP-1]; SP--;  }
c_lshift(i) long i;   { Stack[SP-2]=Stack[SP-2] << Stack[SP-1]; SP--; }
c_rshift(i) long i;   { Stack[SP-2]=Stack[SP-2] >> Stack[SP-1]; SP--; }
c_plus(i)   long i;   { Stack[SP-2]=Stack[SP-2] + Stack[SP-1]; SP--;  }
c_mul(i)    long i;   { Stack[SP-2]=Stack[SP-2] * Stack[SP-1]; SP--;  }
c_minus(i)  long i;   { Stack[SP-2]=Stack[SP-2] - Stack[SP-1]; SP--;  }
c_div(i)    long i;   { Stack[SP-2]=Stack[SP-2] / Stack[SP-1]; SP--;  }
c_mod(i)    long i;   { Stack[SP-2]=Stack[SP-2] % Stack[SP-1]; SP--;  }


/* unary instructions */

c_not(i)      long i; { Stack[SP-1] = ! Stack[SP-1]; }
c_bnot(i)     long i; { Stack[SP-1] = ~ Stack[SP-1]; }
c_uniminus(i) long i; { Stack[SP-1] = - Stack[SP-1]; }

/* trinary operator ?: */

c_qm(i)    long i; { PC = (Stack[SP-1]) ?  PC : i; SP--; }
c_colon(i) long i; { PC = i; }

/* accessing a parameter */

c_param(i)
long i;
{
	Stack[ SP++ ] = Stack[ FP + i ];
}

/* calling a function */

c_func(i)
long i;
{
	Stack[ SP++ ] = PC;
	Stack[ SP++] = FP;
	PC = i;
	FP = SP-(StackProgram[PC].value+2);
}

/* returning from a function */

c_return(i)
long i;
{
	PC = Stack[ SP-3 ];
	FP = Stack[ SP-2 ];
	Stack[ SP-(3+i) ] = Stack[ SP-1 ];
	SP -= (2+i);
}

/* operand functions */

c_number(i) long i; { Stack[SP++] = i;                  }
c_atime(i)  long i; { Stack[SP++] = attr.buf->st_atime; }
c_ctime(i)  long i; { Stack[SP++] = attr.buf->st_ctime; }
c_dev(i)    long i; { Stack[SP++] = attr.buf->st_dev;   }
c_gid(i)    long i; { Stack[SP++] = attr.buf->st_gid;   }
c_ino(i)    long i; { Stack[SP++] = attr.buf->st_ino;   }
c_mode(i)   long i; { Stack[SP++] = attr.buf->st_mode;  }
c_mtime(i)  long i; { Stack[SP++] = attr.buf->st_mtime; }
c_nlink(i)  long i; { Stack[SP++] = attr.buf->st_nlink; }
c_rdev(i)   long i; { Stack[SP++] = attr.buf->st_rdev;  }
c_size(i)   long i; { Stack[SP++] = attr.buf->st_size;  }
c_uid(i)    long i; { Stack[SP++] = attr.buf->st_uid;   }
c_depth(i)  long i; { Stack[SP++] = attr.depth;         }
c_prune(i)  long i; { Stack[SP++] = 0; attr.prune = 1;  }

/* calculate the filename length */

c_baselen(i)
long i;
{
	char *c; register int len;

	len = 0;
	for(c=attr.fname; *c; c++ )
		if( *c == '/' ) len = 0;
		else len += 1;
	Stack[SP++] = len;
}

/* ----------------------------------------------------------------------
 * c_str:
 *    This implements the regular expression stuff.
 *    'i' is an index into the array Strbuf[]. The
 *    string contained there is the actual '\0' terminated
 *    string that occured in the expression (eg "*.BAK" ), minus
 *    the quotes "".
 */

c_str(i)
long i;
{
	char *tail,*strrchr();

	tail = strrchr(attr.fname, '/');
	tail = (tail) ? tail+1 : attr.fname;
	Stack[SP++] = glob_match(&Strbuf[i], tail, 1);
}
