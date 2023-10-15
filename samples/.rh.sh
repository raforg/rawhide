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

# rq - rh with automatic "" around the first argument
# usage: rq pattern [options] [path...]
# e.g.:  rq '*.c' instead of rh '"*.c"'
rq() { rq_pat="$1"; shift && rh -e "\"$rq_pat\"" "$@"; }

# rql - rh -l with automatic "" around the first argument
# usage: rql pattern [options] [path...]
# e.g.:  rql '*.c' instead of rh -l '"*.c"'
rql() { rql_pat="$1"; shift && rh -le "\"$rql_pat\"" "$@"; }

# rqv - rh -v with automatic "" around the first argument
# usage: rqv pattern [options] [path...]
# e.g.:  rqv '*.c' instead of rh -v '"*.c"'
rqv() { rqv_pat="$1"; shift && rh -ve "\"$rqv_pat\"" "$@"; }


# ri - rh with automatic "".i around the first argument
# usage: ri pattern [options] [path...]
# e.g.:  ri '*.c' instead of rh '"*.c".i'
ri() { ri_pat="$1"; shift && rh -e "\"$ri_pat\".i" "$@"; }

# ril - rh -l with automatic "".i around the first argument
# usage: ril pattern [options] [path...]
# e.g.:  ril '*.c' instead of rh -l '"*.c".i'
ril() { ril_pat="$1"; shift && rh -le "\"$ril_pat\".i" "$@"; }

# riv - rh -v with automatic "".i around the first argument
# usage: riv pattern [options] [path...]
# e.g.:  riv '*.c' instead of rh -v '"*.c".i'
riv() { riv_pat="$1"; shift && rh -ve "\"$riv_pat\".i" "$@"; }


# re - rh with automatic "".re around the first argument
# usage: re pattern [options] [path...]
# e.g.:  re '\.c$' instead of rh '"\.c$".re'
re() { re_pat="$1"; shift && rh -e "\"$re_pat\".re" "$@"; }

# rel - rh -l with automatic "".re around the first argument
# usage: rel pattern [options] [path...]
# e.g.:  rel '\.c$' instead of rh -l '"\.c$".re'
rel() { rel_pat="$1"; shift && rh -le "\"$rel_pat\".re" "$@"; }

# rev - rh -v with automatic "".re around the first argument
# usage: rev pattern [options] [path...]
# e.g.:  rev '\.c$' instead of rh -v '"\.c$".re'
rev() { rev_pat="$1"; shift && rh -ve "\"$rev_pat\".re" "$@"; }


# rei - rh with automatic "".rei around the first argument
# usage: rei pattern [options] [path...]
# e.g.:  rei '\.c$' instead of rh '"\.c$".rei'
rei() { rei_pat="$1"; shift && rh -e "\"$rei_pat\".rei" "$@"; }

# reil - rh -l with automatic "".rei around the first argument
# usage: reil pattern [options] [path...]
# e.g.:  reil '\.c$' instead of rh -l '"\.c$".rei'
reil() { reil_pat="$1"; shift && rh -le "\"$reil_pat\".rei" "$@"; }

# reiv - rh -v with automatic "".rei around the first argument
# usage: reiv pattern [options] [path...]
# e.g.:  reiv '\.c$' instead of rh -v '"\.c$".rei'
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


# jqs - (helper) use jq to sort rh -j by path
# usage: jq arguments that don't conflict with -s
# e.g.: rh -j | jqs -r
jqs() { jq -s "$@" 'sort_by(.path) | .[].path'; }

# jqt - (helper) use jq to sort rh -j by mtime, most recent first
# usage: jq arguments that don't conflict with -s
# e.g.: rh -j | jqt -r
jqt() { jq -s "$@" 'sort_by(-.mtime_unix,.path) | .[].path'; }

# jqz - (helper) use jq to sort rh -j by size
# usage: jq arguments that don't conflict with -s
# e.g.: rh -j | jqz -r
jqz() { jq -s "$@" 'sort_by(.size,.path) | .[].path'; }

# rhs - plain rh sorted by path (like ls -1AR)
# usage: rh arguments that don't conflict with -j
# e.g.: rhs f
rhs() { rh -j "$@" | jqs -r; }

# rht - plain rh sorted by mtime, most recent first (like ls -1ARt)
# usage: rh arguments that don't conflict with -j
# e.g.: rht f
rht() { rh -j "$@" | jqt -r; }

# rhz - plain rh sorted by size (like ls -1AR but sorted by size)
# usage: rh arguments that don't conflict with -j
# e.g.: rhz 'size > 1M'
rhz() { rh -j "$@" | jqz -r; }

# rls - rh -rl sorted by path (like ls -lA)
# usage: rh arguments that don't conflict with -r or -j
# e.g.: rls f
rls() { eval rh -lM0 `rh -rj "$@" | jqs`; }

# rlt - rh -rl sorted by mtime, most recent first (like ls -lAt)
# usage: rh arguments that don't conflict with -r or -j
# e.g.: rlt f
rlt() { eval rh -lM0 `rh -rj "$@" | jqt`; }

# rlz - rh -rl sorted by size (like ls -lA but sorted by size)
# usage: rh arguments that don't conflict with -r or -j
# e.g.: rlz 'size > 1M'
rlz() { eval rh -lM0 `rh -rj "$@" | jqz`; }

# rlrs - rh -l sorted by path (like ls -lAR)
# usage: rh arguments that don't conflict with -j
# e.g.: rlrs f
rlrs() { eval rh -lM0 `rh -j "$@" | jqs`; }

# rlrt - rh -l sorted by mtime, most recent first (like ls -lARt)
# usage: rh arguments that don't conflict with -r or -j
# e.g.: rlrt f
rlrt() { eval rh -lM0 `rh -j "$@" | jqt`; }

# rlrz - rh -l sorted by size (like ls -lAR but sorted by size)
# usage: rh arguments that don't conflict with -r or -j
# e.g.: rlrz 'size > 1M'
rlrz() { eval rh -lM0 `rh -j "$@" | jqz`; }

# rvs - rh -rv sorted by path (like ls -lA)
# usage: rh arguments that don't conflict with -r or -j
# e.g.: rvs f
rvs() { eval rh -vM0 `rh -rj "$@" | jqs`; }

# rvt - rh -rv sorted by mtime, most recent first (like ls -lAt)
# usage: rh arguments that don't conflict with -r or -j
# e.g.: rvt f
rvt() { eval rh -vM0 `rh -rj "$@" | jqt`; }

# rvz - rh -rv sorted by size (like ls -lA but sorted by size)
# usage: rh arguments that don't conflict with -r or -j
# e.g.: rvz 'size > 1M'
rvz() { eval rh -vM0 `rh -rj "$@" | jqz`; }

# rvrs - rh -v sorted by path (like ls -lAR)
# usage: rh arguments that don't conflict with -j
# e.g.: rvrs f
rvrs() { eval rh -vM0 `rh -j "$@" | jqs`; }

# rvrt - rh -v sorted by mtime, most recent first (like ls -lARt)
# usage: rh arguments that don't conflict with -r or -j
# e.g.: rvrt f
rvrt() { eval rh -vM0 `rh -j "$@" | jqt`; }

# rvrz - rh -v sorted by size (like ls -lAR but sorted by size)
# usage: rh arguments that don't conflict with -r or -j
# e.g.: rvrz 'size > 1M'
rvrz() { eval rh -vM0 `rh -j "$@" | jqz`; }


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
# #export RAWHIDE_SOLARIS_ACL_NO_TRIVIAL=1
# #export RAWHIDE_SOLARIS_EA_NO_SUNWATTR=1
# #export RAWHIDE_SOLARIS_EA_NO_STATINFO=1
# #export RAWHIDE_EA_SIZE=4096 # On Solaris: 65536


