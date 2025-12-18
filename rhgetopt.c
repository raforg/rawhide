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
* 20231013 raf <raf@raf.org>
*/

/*

Copyright (C) 1987-1999 Free Software Foundation, Inc.

The GNU C Library is free software; you can redistribute it and/or modify
it under the terms of the GNU Library General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If not,
write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.

*/

/* Minimal GNU (permuting) getopt() with just enough for rawhide */

#include <string.h>

#include "rhgetopt.h"

char *optarg;     /* The current option's argument, if any */
int optind = 1;   /* The index of the first non-option arg after scanning */
int optopt = '?'; /* Invalid option character */

static char *nextchar = NULL; /* Next char to scan in the current argv item */
static int first_nonopt = 1;  /* Index of the first non-opt argv item skipped */
static int last_nonopt = 1;   /* Index of the last non-opt argv item skipped */

/* Exchange two adjacent subsequences of ARGV.
   One subsequence is elements [first_nonopt,last_nonopt)
   which contains all the non-options that have been skipped so far.
   The other is elements [last_nonopt,optind), which contains all
   the options processed since those non-options were skipped.

   `first_nonopt' and `last_nonopt' are relocated so that they describe
   the new indices of the non-options in ARGV after they are moved.  */

static void exchange(char **argv)
{
  int bottom = first_nonopt;
  int middle = last_nonopt;
  int top = optind;
  int i;

  /* Exchange the shorter segment with the far end of the longer segment.
     That puts the shorter segment into the right place.
     It leaves the longer segment in the right place overall,
     but it consists of two parts that need to be swapped next.  */

  while (top > middle && middle > bottom)
    {
      #if 0 /* Can't happen (exchange is always after a single option or --) */
      if (top - middle > middle - bottom)
	{
	  /* Bottom segment is the short one.  */
	  int len = middle - bottom;

	  /* Swap it with the top part of the top segment.  */
	  for (i = 0; i < len; i++)
	    {
	      char *temp = argv[bottom + i];
	      argv[bottom + i] = argv[top - (middle - bottom) + i];
	      argv[top - (middle - bottom) + i] = temp;
	    }
	  /* Exclude the moved bottom segment from further swapping.  */
	  top -= len;
	}
      else
      #endif
	{
	  /* Top segment is the short one.  */
	  int len = top - middle; /* Always 1, loop is unnecessary */

	  /* Swap it with the bottom part of the bottom segment.  */
	  for (i = 0; i < len; i++)
	    {
	      char *temp = argv[bottom + i];
	      argv[bottom + i] = argv[middle + i];
	      argv[middle + i] = temp;
	    }
	  /* Exclude the moved top segment from further swapping.  */
	  bottom += len;
	}
    }

  /* Update records for the slots the non-options now occupy.  */

  first_nonopt += (optind - last_nonopt);
  last_nonopt = optind;
}

/* Scan elements of ARGV (whose length is ARGC) for option characters
   given in OPTSTRING.

   If an element of ARGV starts with '-', and is not exactly "-" or "--",
   then it is an option element.  The characters of this element
   (aside from the initial '-') are option characters.  If `getopt'
   is called repeatedly, it returns successively each of the option characters
   from each of the option elements.

   If `getopt' finds another option character, it returns that character,
   updating `optind' and `nextchar' so that the next call to `getopt' can
   resume the scan with the following option character or ARGV-element.

   If there are no more option characters, `getopt' returns -1.
   Then `optind' is the index in ARGV of the first ARGV-element
   that is not an option.  (The ARGV-elements have been permuted
   so that those that are not options now come last.)

   OPTSTRING is a string containing the legitimate option characters.
   If an option character is seen that is not listed in OPTSTRING,
   return '?'.

   If a char in OPTSTRING is followed by a colon, that means it wants an arg,
   so the following text in the same ARGV-element, or the text of the following
   ARGV-element, is returned in `optarg'.  */

int getopt(int argc, char *const *argv, const char *optstring)
{
  optarg = NULL;

  /* Test whether ARGV[optind] points to a non-option argument.
     Either it does not have option syntax, or there is an environment flag
     from the shell indicating it is not an option.  The later information
     is only used when the used in the GNU libc.  */

  if (nextchar == NULL || *nextchar == '\0')
    {
      /* Advance to the next ARGV-element.  */

      /* If we have just processed an option following some non-options,
	 exchange them so that the option comes first.  */

      if (first_nonopt != last_nonopt && last_nonopt != optind)
	exchange((char **)argv);
      else if (last_nonopt != optind)
	first_nonopt = optind;

      /* Skip any additional non-options
	 and extend the range of non-options previously skipped.  */

      while (optind < argc && (argv[optind][0] != '-' || argv[optind][1] == '\0'))
	optind++;
      last_nonopt = optind;

      /* The special ARGV-element `--' means premature end of options.
	 Skip it like a NULL option,
	 then exchange with previous non-options as if it were an option,
	 then skip everything else like a non-option.  */

      if (optind != argc && !strcmp(argv[optind], "--"))
	{
	  optind++;

	  if (first_nonopt != last_nonopt && last_nonopt != optind)
	    exchange((char **)argv);
	  else if (first_nonopt == last_nonopt)
	    first_nonopt = optind;
	  last_nonopt = argc;

	  optind = argc;
	}

      /* If we have done all the ARGV-elements, stop the scan
	 and back over any non-options that we skipped and permuted.  */

      if (optind == argc)
	{
	  /* Set the next-arg-index to point at the non-options
	     that we previously skipped, so the caller will digest them.  */
	  if (first_nonopt != last_nonopt)
	    optind = first_nonopt;
	  return -1;
	}

      /* We have found another option-ARGV-element.
	 Skip the initial punctuation.  */

      nextchar = argv[optind] + 1;
    }

  /* Decode the current option-ARGV-element.  */

  char c = *nextchar++;
  char *temp = strchr(optstring, c);

  /* Increment `optind' when we start to process its last character.  */
  if (*nextchar == '\0')
    ++optind;

  if (temp == NULL || c == ':')
    {
      optopt = c;
      return '?';
    }
  if (temp[1] == ':')
    {
      /* This is an option that requires an argument.  */
      if (*nextchar != '\0')
	{
	  optarg = nextchar;
	  /* If we end this ARGV-element by taking the rest as an arg,
	     we must advance to the next element now.  */
	  optind++;
	}
      else if (optind == argc)
	{
	  optopt = c;
	  c = ':';
	}
      else
	/* We already incremented `optind' once;
	   increment it again when taking next ARGV-elt as argument.  */
	optarg = argv[optind++];
      nextchar = NULL;
    }
  return c;
}

/* vi:set ts=8 sw=2 sts=2: */
