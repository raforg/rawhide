/* File-name wildcard pattern matching for GNU.
   Copyright (C) 1985, 1988 Free Software Foundation, Inc.

               NO WARRANTY

  BECAUSE THIS PROGRAM IS LICENSED FREE OF CHARGE, WE PROVIDE ABSOLUTELY
NO WARRANTY, TO THE EXTENT PERMITTED BY APPLICABLE STATE LAW.  EXCEPT
WHEN OTHERWISE STATED IN WRITING, FREE SOFTWARE FOUNDATION, INC,
RICHARD M. STALLMAN AND/OR OTHER PARTIES PROVIDE THIS PROGRAM "AS IS"
WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE.  THE ENTIRE RISK AS TO THE QUALITY
AND PERFORMANCE OF THE PROGRAM IS WITH YOU.  SHOULD THE PROGRAM PROVE
DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY SERVICING, REPAIR OR
CORRECTION.

 IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW WILL RICHARD M.
STALLMAN, THE FREE SOFTWARE FOUNDATION, INC., AND/OR ANY OTHER PARTY
WHO MAY MODIFY AND REDISTRIBUTE THIS PROGRAM AS PERMITTED BELOW, BE
LIABLE TO YOU FOR DAMAGES, INCLUDING ANY LOST PROFITS, LOST MONIES, OR
OTHER SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE
USE OR INABILITY TO USE (INCLUDING BUT NOT LIMITED TO LOSS OF DATA OR
DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY THIRD PARTIES OR
A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER PROGRAMS) THIS
PROGRAM, EVEN IF YOU HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH
DAMAGES, OR FOR ANY CLAIM BY ANY OTHER PARTY.

        GENERAL PUBLIC LICENSE TO COPY

  1. You may copy and distribute verbatim copies of this source file
as you receive it, in any medium, provided that you conspicuously and
appropriately publish on each copy a valid copyright notice "Copyright
(C) 1988 Free Software Foundation, Inc."; and include following the
copyright notice a verbatim copy of the above disclaimer of warranty
and of this License.

  2. You may modify your copy or copies of this source file or
any portion of it, and copy and distribute such modifications under
the terms of Paragraph 1 above, provided that you also do the following:

    a) cause the modified files to carry prominent notices stating
    that you changed the files and the date of any change; and

    b) cause the whole of any work that you distribute or publish,
    that in whole or in part contains or is a derivative of this
    program or any part thereof, to be licensed at no charge to all
    third parties on terms identical to those contained in this
    License Agreement (except that you may choose to grant more extensive
    warranty protection to some or all third parties, at your option).

    c) You may charge a distribution fee for the physical act of
    transferring a copy, and you may at your option offer warranty
    protection in exchange for a fee.

Mere aggregation of another unrelated program with this program (or its
derivative) on a volume of a storage or distribution medium does not bring
the other program under the scope of these terms.

  3. You may copy and distribute this program (or a portion or derivative
of it, under Paragraph 2) in object code or executable form under the terms
of Paragraphs 1 and 2 above provided that you also do one of the following:

    a) accompany it with the complete corresponding machine-readable
    source code, which must be distributed under the terms of
    Paragraphs 1 and 2 above; or,

    b) accompany it with a written offer, valid for at least three
    years, to give any third party free (except for a nominal
    shipping charge) a complete machine-readable copy of the
    corresponding source code, to be distributed under the terms of
    Paragraphs 1 and 2 above; or,

    c) accompany it with the information you received as to where the
    corresponding source code may be obtained.  (This alternative is
    allowed only for noncommercial distribution and only if you
    received the program in object code or executable form alone.)

For an executable file, complete source code means all the source code for
all modules it contains; but, as a special exception, it need not include
source code for modules which are standard libraries that accompany the
operating system on which the executable file runs.

  4. You may not copy, sublicense, distribute or transfer this program
except as expressly provided under this License Agreement.  Any attempt
otherwise to copy, sublicense, distribute or transfer this program is void and
your rights to use the program under this License agreement shall be
automatically terminated.  However, parties who have received computer
software programs from you with this License Agreement will not have
their licenses terminated so long as such parties remain in full compliance.


In other words, you are welcome to use, share and improve this program.
You are forbidden to forbid anyone else to use, share and improve
what you give them.   Help stamp out software-hoarding!  */

/* Match the pattern PATTERN against the string TEXT;
   return 1 if it matches, 0 otherwise.

   A match means the entire string TEXT is used up in matching.

   In the pattern string, `*' matches any sequence of characters,
   `?' matches any character, [SET] matches any character in the specified set,
   [^SET] matches any character not in the specified set.

   A set is composed of characters or ranges; a range looks like
   character hyphen character (as in 0-9 or A-Z).
   [0-9a-zA-Z_] is the set of characters allowed in C identifiers.
   Any other character in the pattern must be matched exactly.

   To suppress the special syntactic significance of any of `[]*?^-\',
   and match the character exactly, precede it with a `\'.

   If DOT_SPECIAL is nonzero,
   `*' and `?' do not match `.' at the beginning of TEXT.  */

int
glob_match (pattern, text, dot_special)
     char *pattern, *text;
     int dot_special;
{
  register char *p = pattern, *t = text;
  register char c;

  while ((c = *p++))
    {
      switch (c)
    {
    case '?':
      if (*t == 0 || (dot_special && t == text && *t == '.')) return 0;
      else ++t;
      break;

    case '\\':
      if (*p++ != *t++) return 0;
      break;

    case '*':
      if (dot_special && t == text && *t == '.')
        return 0;
      return star_glob_match (p, t);

    case '[':
      {
        register char c1 = *t++;
        register int invert = (*p == '^');

        if (invert) p++;

        c = *p++;
        while (1)
          {
        register char cstart = c, cend = c;

        if (c == '\\')
          {
            cstart = *p++; cend = cstart;
          }
        c = *p++;
        if (c == '-')
          { cend = *p++; if (cend == '\\') cend = *p++; c = *p++; }
        if (c1 >= cstart && c1 <= cend) goto match;
        if (c == ']')
          break;
          }
        if (!invert) return 0;
        break;

      match:
        /* Skip the rest of the [...] construct that already matched.  */
        while (c != ']')
          {
            c = *p++;
        if (c == '\\') p++;
          }
        if (invert) return 0;
        break;
      }

    default:
      if (c != *t++) return 0;
    }
    }

  if (*t) return 0;
  return 1;
}

/* Like glob_match, but match PATTERN against any final segment of TEXT.  */

static int
star_glob_match (pattern, text)
     char *pattern, *text;
{
  register char *p = pattern, *t = text;
  register char c, c1;

  while ((c = *p++) == '?' || c == '*')
    {
      if (c == '?' && *t++ == 0)
    return 0;
    }

  if (c == 0)
    return 1;

  if (c == '\\') c1 = *p;
  else c1 = c;

  for (;;)
    {
      if ((c == '[' || *t == c1)
          && glob_match (p - 1, t, 0))
    return 1;
      if (*t++ == 0) return 0;
    }
}
