# rawhide - find files using pretty C expressions
# https://raf.org/rawhide
# https://github.com/raforg/rawhide
# https://codeberg.org/raforg/rawhide
#
# Copyright (C) 1990 Ken Stauffer, 2022-2023 raf <raf@raf.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <https://www.gnu.org/licenses/>.
#
# 20231013 raf <raf@raf.org>

# .rh.sh - Some command line shell syntactic sugar to save keystrokes

# rhe - rh with automatic -e before the first argument
# usage: rhe 'expression' [options] [--] [path...]
# e.g.: rhe f instead of rh -e f
rhe() { rhe_expr="$1"; shift && rh -e "$rhe_expr" "$@"; }


# rq - rh with automatic -e and "" around the first argument
# usage: rq 'pattern' [options] [--] [path...]
# e.g.:  rq '*.c' instead of rh -e '"*.c"'
rq() { rq_pat="$1"; shift && rh -e "\"$rq_pat\"" "$@"; }

# rql - rh -l with automatic -e and "" around the first argument
# usage: rql 'pattern' [options] [--] [path...]
# e.g.:  rql '*.c' instead of rh -le '"*.c"'
rql() { rql_pat="$1"; shift && rh -le "\"$rql_pat\"" "$@"; }

# rqv - rh -v with automatic -e and "" around the first argument
# usage: rqv 'pattern' [options] [--] [path...]
# e.g.:  rqv '*.c' instead of rh -ve '"*.c"'
rqv() { rqv_pat="$1"; shift && rh -ve "\"$rqv_pat\"" "$@"; }


# ri - rh with automatic -e and "".i around the first argument
# usage: ri 'pattern' [options] [--] [path...]
# e.g.:  ri '*.c' instead of rh -e '"*.c".i'
ri() { ri_pat="$1"; shift && rh -e "\"$ri_pat\".i" "$@"; }

# ril - rh -l with automatic -e and "".i around the first argument
# usage: ril 'pattern' [options] [--] [path...]
# e.g.:  ril '*.c' instead of rh -le '"*.c".i'
ril() { ril_pat="$1"; shift && rh -le "\"$ril_pat\".i" "$@"; }

# riv - rh -v with automatic -e and "".i around the first argument
# usage: riv 'pattern' [options] [--] [path...]
# e.g.:  riv '*.c' instead of rh -ve '"*.c".i'
riv() { riv_pat="$1"; shift && rh -ve "\"$riv_pat\".i" "$@"; }


# re - rh with automatic -e and "".re around the first argument
# usage: re 'pattern' [options] [--] [path...]
# e.g.:  re '\.c$' instead of rh -e '"\.c$".re'
re() { re_pat="$1"; shift && rh -e "\"$re_pat\".re" "$@"; }

# rel - rh -l with automatic -e and "".re around the first argument
# usage: rel 'pattern' [options] [--] [path...]
# e.g.:  rel '\.c$' instead of rh -le '"\.c$".re'
rel() { rel_pat="$1"; shift && rh -le "\"$rel_pat\".re" "$@"; }

# rev - rh -v with automatic -e and "".re around the first argument
# usage: rev 'pattern' [options] [--] [path...]
# e.g.:  rev '\.c$' instead of rh -ve '"\.c$".re'
rev() { rev_pat="$1"; shift && rh -ve "\"$rev_pat\".re" "$@"; }
# Warning: This function name clashes with rev(1).


# rei - rh with automatic "".rei around the first argument
# usage: rei 'pattern' [options] [--] [path...]
# e.g.:  rei '\.c$' instead of rh -e '"\.c$".rei'
rei() { rei_pat="$1"; shift && rh -e "\"$rei_pat\".rei" "$@"; }

# reil - rh -l with automatic "".rei around the first argument
# usage: reil 'pattern' [options] [--] [path...]
# e.g.:  reil '\.c$' instead of rh -le '"\.c$".rei'
reil() { reil_pat="$1"; shift && rh -le "\"$reil_pat\".rei" "$@"; }

# reiv - rh -v with automatic "".rei around the first argument
# usage: reiv 'pattern' [options] [--] [path...]
# e.g.:  reiv '\.c$' instead of rh -ve '"\.c$".rei'
reiv() { reiv_pat="$1"; shift && rh -ve "\"$reiv_pat\".rei" "$@"; }


alias rl='rh -rl' # rh -l version of ls -lA (unsorted)
alias rlr='rh -l' # rh -l version of ls -lAR (unsorted)

alias rv='rh -rv' # rh -v version of ls -lA (unsorted)
alias rvr='rh -v' # rh -v version of ls -lAR (unsorted)

alias rj='rh -j'

alias r0='rh -0'

alias r1='rh -1'
alias r1l='rh -1l'
alias r1v='rh -1v'

alias ry='rh -y'
alias ryl='rh -yl'
alias ryv='rh -yv'

alias rY='rh -Y'
alias rYl='rh -Yl'
alias rYv='rh -Yv'


# bjq - (helper) protect jq from any non-utf8 input (messes up utf8)
# usage: Same as jq
latin1_to_utf8() { iconv -f LATIN1 -t UTF-8; }
utf8_to_latin1() { iconv -f UTF-8 -t LATIN1; }
bjq() { latin1_to_utf8 | jq "$@" | utf8_to_latin1; }

# jqs - (helper) use jq to sort rh -j by path
# jqss - Same as jqs, but shell-quoted
# jqsz - Same as jqs, but nul-terminated, for xargs
# usage: jq arguments that don't conflict with -s
# e.g.: rh -j | jqs
jqs()  { bjq -sr "$@" 'sort_by(.path)[].path'; }
jqss() { bjq -sr "$@" 'sort_by(.path)[].path|@sh'; }
jqsz() { bjq -sj "$@" 'sort_by(.path)[].path+"\u0000"'; }

# jqt - (helper) use jq to sort rh -j by mtime, most recent first
# jqts - Same as jqt, but shell-quoted
# jqtz - Same as jqt, but nul-terminated, for xargs
# usage: jq arguments that don't conflict with -s
# e.g.: rh -j | jqt
jqt()  { bjq -sr "$@" 'sort_by(-.mtime_unix,.path)[].path'; }
jqts() { bjq -sr "$@" 'sort_by(-.mtime_unix,.path)[].path|@sh'; }
jqtz() { bjq -sj "$@" 'sort_by(-.mtime_unix,.path)[].path+"\u0000"'; }

# jqz - (helper) use jq to sort rh -j by size
# jqzs - Same as jqz, but shell-quoted
# jqzz - Same as jqz, but nul-terminated
# usage: jq arguments that don't conflict with -s
# e.g.: rh -j | jqz
jqz()  { bjq -sr "$@" 'sort_by(.size,.path)|.[].path'; }
jqzs() { bjq -sr "$@" 'sort_by(.size,.path)|.[].path|@sh'; }
jqzz() { bjq -sj "$@" 'sort_by(.size,.path)|.[].path+"\u0000"'; }

# rhxargs - (helper) use xargs to invoke rh -M0
# usage: rhxargs [options]
# e.g.: rhxargs -l or rhxargs -v
rhxargs() { xargs -r0 rh -M0 -e1 "$@" --; }


# rhs - plain rh sorted by path (like ls -1AR)
# usage: rh arguments that don't conflict with -j
# usage: rhs [options] [--] [path...]
# e.g.: rhs -e f
rhs() { rh -j "$@" | jqs; }

# rht - plain rh sorted by mtime, most recent first (like ls -1ARt)
# usage: rh arguments that don't conflict with -j
# e.g.: rht -e f
rht() { rh -j "$@" | jqt; }

# rhz - plain rh sorted by size (like ls -1AR but sorted by size)
# usage: rh arguments that don't conflict with -j
# e.g.: rhz -e 'size > 1M'
rhz() { rh -j "$@" | jqz; }


# rls - rh -rl sorted by path (like ls -lA)
# usage: rh arguments that don't conflict with -r or -j
# e.g.: rls -e f
rls() { rh -rj "$@" | jqsz | rhxargs -l; }

# rlt - rh -rl sorted by mtime, most recent first (like ls -lAt)
# usage: rh arguments that don't conflict with -r or -j
# e.g.: rlt -e f
rlt() { rh -rj "$@" | jqtz | rhxargs -l; }

# rlz - rh -rl sorted by size (like ls -lA but sorted by size)
# usage: rh arguments that don't conflict with -r or -j
# e.g.: rlz -e 'size > 1M'
rlz() { rh -rj "$@" | jqzz | rhxargs -l; }


# rlrs - rh -l sorted by path (like ls -lAR)
# usage: rh arguments that don't conflict with -j
# e.g.: rlrs -e f
rlrs() { rh -j "$@" | jqsz | rhxargs -l; }

# rlrt - rh -l sorted by mtime, most recent first (like ls -lARt)
# usage: rh arguments that don't conflict with -r or -j
# e.g.: rlrt -e f
rlrt() { rh -j "$@" | jqtz | rhxargs -l; }

# rlrz - rh -l sorted by size (like ls -lAR but sorted by size)
# usage: rh arguments that don't conflict with -r or -j
# e.g.: rlrz -e 'size > 1M'
rlrz() { rh -j "$@" | jqzz | rhxargs -l; }


# rvs - rh -rv sorted by path (like ls -lA)
# usage: rh arguments that don't conflict with -r or -j
# e.g.: rvs -e f
rvs() { rh -rj "$@" | jqsz | rhxargs -v; }

# rvt - rh -rv sorted by mtime, most recent first (like ls -lAt)
# usage: rh arguments that don't conflict with -r or -j
# e.g.: rvt -e f
rvt() { rh -rj "$@" | jqtz | rhxargs -v; }

# rvz - rh -rv sorted by size (like ls -lA but sorted by size)
# usage: rh arguments that don't conflict with -r or -j
# e.g.: rvz -e 'size > 1M'
rvz() { rh -rj "$@" | jqzz | rhxargs -v; }


# rvrs - rh -v sorted by path (like ls -lAR)
# usage: rh arguments that don't conflict with -j
# e.g.: rvrs -e f
rvrs() { rh -j "$@" | jqsz | rhxargs -v; }

# rvrt - rh -v sorted by mtime, most recent first (like ls -lARt)
# usage: rh arguments that don't conflict with -r or -j
# e.g.: rvrt -e f
rvrt() { rh -j "$@" | jqtz | rhxargs -v; }

# rvrz - rh -v sorted by size (like ls -lAR but sorted by size)
# usage: rh arguments that don't conflict with -r or -j
# e.g.: rvrz -e 'size > 1M'
rvrz() { rh -j "$@" | jqzz | rhxargs -v; }


# Alternate implementations if xargs -r isn't supported.
# This applies to macOS and Solaris.
xargs -r < /dev/null > /dev/null 2>&1
if [ $? != 0 ]
then
	rls()  { eval rh -lM0 -e1 -- $(rh -rj "$@" | jqss); }
	rlt()  { eval rh -lM0 -e1 -- $(rh -rj "$@" | jqts); }
	rlz()  { eval rh -lM0 -e1 -- $(rh -rj "$@" | jqzs); }
	rlrs() { eval rh -lM0 -e1 -- $(rh -j  "$@" | jqss); }
	rlrt() { eval rh -lM0 -e1 -- $(rh -j  "$@" | jqts); }
	rlrz() { eval rh -lM0 -e1 -- $(rh -j  "$@" | jqzs); }
	rvs()  { eval rh -vM0 -e1 -- $(rh -rj "$@" | jqss); }
	rvt()  { eval rh -vM0 -e1 -- $(rh -rj "$@" | jqts); }
	rvz()  { eval rh -vM0 -e1 -- $(rh -rj "$@" | jqzs); }
	rvrs() { eval rh -vM0 -e1 -- $(rh -j  "$@" | jqss); }
	rvrt() { eval rh -vM0 -e1 -- $(rh -j  "$@" | jqts); }
	rvrz() { eval rh -vM0 -e1 -- $(rh -j  "$@" | jqzs); }
fi


# export RAWHIDE_CONFIG=/etc/rawhide.conf
# export RAWHIDE_RC="$HOME"/.rhrc
# export RAWHIDE_COLUMN_WIDTH_DEV_MAJOR=1
# export RAWHIDE_COLUMN_WIDTH_DEV_MINOR=1
# export RAWHIDE_COLUMN_WIDTH_INODE=6
# export RAWHIDE_COLUMN_WIDTH_BLKSIZE=1
# export RAWHIDE_COLUMN_WIDTH_BLOCKS=2
# export RAWHIDE_COLUMN_WIDTH_SPACE=6
# export RAWHIDE_COLUMN_WIDTH_SPACE_UNITS=4
# export RAWHIDE_COLUMN_WIDTH_NLINK=1
# export RAWHIDE_COLUMN_WIDTH_USER=3
# export RAWHIDE_COLUMN_WIDTH_GROUP=3
# export RAWHIDE_COLUMN_WIDTH_SIZE=6
# export RAWHIDE_COLUMN_WIDTH_SIZE_UNITS=4
# export RAWHIDE_COLUMN_WIDTH_RDEV_MAJOR=2
# export RAWHIDE_COLUMN_WIDTH_RDEV_MINOR=3

# #export RAWHIDE_REPORT_BROKEN_SYMLINKS=1
# #export RAWHIDE_DONT_REPORT_CYCLES=1
# #export RAWHIDE_PCRE2_NOT_UTF8_DEFAULT=1
# #export RAWHIDE_PCRE2_UTF8_DEFAULT=1
# #export RAWHIDE_PCRE2_DOTALL_ALWAYS=1
# #export RAWHIDE_PCRE2_MULTILINE_ALWAYS=1
# #export RAWHIDE_SOLARIS_ACL_NO_TRIVIAL=1
# #export RAWHIDE_SOLARIS_EA_NO_SUNWATTR=1
# #export RAWHIDE_SOLARIS_EA_NO_STATINFO=1
# #export RAWHIDE_EA_SIZE=4096 # On Solaris: 65536
# #export RAWHIDE_ALLOW_IMPLICIT_EXPR_HEURISTIC=1
# #export RAWHIDE_USER_SHELL=
# #export RAWHIDE_USER_SHELL_LIKE_CSH=1

