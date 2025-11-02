# Contributing to *rawhide* (*rh*)

Since *rawhide* is published on *github*, I accept that I have to work with
pull requests, and have happily done so, but to be honest, I'm happy to do
all the work myself if I can. So talk to me. I prefer communicating via
email (`raf@raf.org`).

If you want to make changes yourself, please follow my coding/documentation
style precisely. Any changes to functionality probably need to include
corresponding changes to documentation and tests. Everything that *rh* does
needs to be documented, and everything that is documented needs to be
tested.

When changing code and tests, please run all of the tests as both a
non-`root` user, and as the `root` user, and consider running all tests on
*Linux*, *FreeBSD*, *OpenBSD*, *NetBSD*, *Solaris*, *macOS*, and *Cygwin*
(if possible). And on *Linux*, perform the undefined behaviour sanitizer
tests (*ubsan*), and the address sanitizer tests (*asan*), and the memory
sanitizer tests (msan) that are described in the output of `make help`. Also
consider running tests under *valgrind* (but it's slow and more of a hassle
to analyze). Also perform test coverage analysis (as described in the output
of `make help`) to make sure that all new code is covered (within reason).

An important thing to consider when changing documentation is to minimize
the cognitive load on the reader. Always answer questions before they will
have even occurred to the reader. And always consider different types of
reader. Some people read the whole document. Some people read just the
paragraph of interest to them at the moment. Support both. The documentation
includes many instances of repetition of small pieces of text, so as to
support readers who only search for the section that interests them right
now. Limit such repetition to a single paragraph so as not to overburden
other readers who read the entire document. When too much would need to be
duplicated, explicitly refer to another section instead. Unless it makes
more sense not to.

When changing documentation, generate the manual entries and proofread the
changes in an 80-column wide terminal window (avoiding wrapped example
lines), and generate the HTML versions and proofread the changes in a web
browser. Use C<>, B<>, I<>, and S<>, wisely.

One odd thing that won't be obvious is that, since the manual entries are
written in the POD format, I prefer it that no line of text, other than the
last line of a paragraph, end with a dot ("."). If one does, it will result
in the sentence being followed by two spaces after the dot (in manual
entries). I prefer a single space there. So, whenever your documentation
changes result in a mid-paragraph line that ends with a dot, please move the
last word and dot to become the start of the next line. Sorry if this is
petty, but if you don't do it, I'll have to. :-)

**Note:** If you contribute code, comments, documentation, and/or tests to
*rawhide*, I will interpret that as you assigning copyright to me, unless
otherwise arranged. If you need to retain copyright, and need to have a new
copyright notice added, just let me know.

--------------------------------------------------------------------------------

    URL: https://raf.org/rawhide
    GIT: https://github.com/raforg/rawhide
    GIT: https://codeberg.org/raforg/rawhide
    Date: 20231013
    Authors: 1990 Ken Stauffer, 2022-2023 raf <raf@raf.org>

