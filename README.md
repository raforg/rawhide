# README

*rawhide* - (*rh*) find files using pretty C expressions

# INTRODUCTION

*Rawhide* (*rh*) lets you search for files on the command line using
expressions and user-defined functions in a mini-language inspired by *C*.
It's like *find(1)*, but more fun to use. Search criteria can be very
readable and self-explanatory and/or very concise and typeable, and you can
create your own lexicon of search terms. The output can include lots of
detail, like *ls(1)*.

# DESCRIPTION

*Rawhide* (*rh*) searches the filesystem, starting at each given path, for
files that make the given search criteria expression true. If no search
paths are given, the current working directory is searched.

The search criteria expression can come from the command line (with the `-e`
option), from a file (with the `-f` option), or from standard input
(*stdin*) (with `-f-`). If there is no explicit `-e` option expression, *rh*
looks for an implicit expression among any remaining command line arguments.
If no expression is specified, the default search criteria is the expression
`1`, which matches all filesystem entries.

An *rh* expression is a *C*-like expression that can call user-defined
functions. These expressions can contain all of *C*'s conditional, logical,
relational, equality, arithmetic, and bit operators.

Numeric constants can be decimal, octal, or hexadecimal integers. Decimal
constants can have scale units (e.g., `10K`).

There are built-in symbols that represent each candidate file's inode
metadata. These are the fields in the corresponding *stat(2)* structure
(e.g., `st_mode`, `st_uid`, `st_size`, `st_mtime`, ...). See *stat(2)* for
details. For convenience, the `"st_"` prefix is omitted from the symbol
names (e.g., `st_mtime` is used as `mtime`).

Other built-in symbols represent the constants defined by *C*'s
`<sys/stat.h>` header file. These are useful for interpreting the `mode` in
order to identify file types and permissions. The `"S_"` prefix is omitted
from the symbol names (e.g., `S_IFMT` is used as `IFMT`).

Other built-in symbols represent various useful values and constants,
control flow, more file information, and candidate symlink target inode
metadata.

File glob patterns and *Perl*-compatible regular expressions (regexes) can
be used to match files by their name, path, symlink target path, body, file
type description, MIME type, access control list, and extended attributes.

Search criteria can also include comparisons with the inode metadata of
arbitrary reference files, and the exit success status of arbitrary shell
commands.

Functions are a means of referring to an expression by name. They allow
complex expressions to be composed of simpler ones. They also allow you to
create your own lexicon of search terms for finding files.

There is a default standard library of functions to start with. It provides
a high-level interface to the built-in symbols mentioned above, and makes
*rh* easy to use. See *rawhide.conf(5)* for details.

# SYNOPSIS

     usage: rh [options] [path...]
     options:
       -h --help    - Show this help message, then exit
       -V --version - Show the version message, then exit
       -N           - Don't read system-wide config (/etc/rawhide.conf)
       -n           - Don't read user-specific config (~/.rhrc)
       -f-          - Read functions and/or expression from stdin
       -f fname     - Read functions and/or expression from a file
      [-e] 'expr'   - Read functions and/or expression from the cmdline

     traversal options:
       -r           - Only search one level down (same as -m1 -M1)
       -m #         - Override the default minimum depth (0)
       -M #         - Override the default maximum depth (system limit)
       -D           - Depth-first searching (contents before directory)
       -1           - Single filesystem (don't cross filesystem boundaries)
       -y           - Follow symlinks on the cmdline and in reference files
       -Y           - Follow symlinks encountered while searching as well

     alternative action options:
       -x 'cmd %s'  - Execute a shell command for each match (racy)
       -X 'cmd %S'  - Like -x but run from each match's directory (safer)
       -U -U -U     - Unlink matches (but tell me three times), implies -D

     output action options:
       -l           - Output matching entries like ls -l (but unsorted)
       -d           - Include device column, implies -l
       -i           - Include inode column, implies -l
       -B           - Include block size column, implies -l
       -s           - Include blocks column, implies -l
       -S           - Include space column, implies -l
       -g           - Exclude user/owner column, implies -l
       -o           - Exclude group column, implies -l
       -a           - Include atime rather than mtime column, implies -l
       -u           - Same as -a (like ls(1))
       -c           - Include ctime rather than mtime column, implies -l
       -v           - Verbose: All columns, implies -ldiBsSac unless -xXU0Lj
       -0           - Output null chars instead of newlines (for xargs -0)
       -L format    - Output matching entries in a user-supplied format
       -j           - Output matching entries as JSON (same as -L "%j\n")

     path format options:
       -Q           - Enclose paths in double quotes
       -E           - Output C-style escapes for control characters
       -b           - Same as -E (like ls(1))
       -q           - Output ? for control characters (default if tty)
       -p           - Append / indicator to directories
       -t           - Append most type indicators (one of / @ = | >)
       -F           - Append all type indicators (one of * / @ = | >)

                        * executable
                        / directory
                        @ symlink
                        = socket
                        | fifo
                        > door (Solaris only)

     other column format options:
       -H or -HH    - Output sizes like 1.2K 34M 5.6G etc., implies -l
       -I or -II    - Like -H but with units of 1000, not 1024, implies -l
       -T           - Output mtime/atime/ctime in ISO format, implies -l
       -#           - Output numeric user/group IDs (not names), implies -l

     debug option:
       -? spec      - Output debug messages: spec can include any of:
                        cmdline, parser, traversal, exec, all, extra

     rh (rawhide) finds files using pretty C expressions.
     See the rh(1) and rawhide.conf(5) manual entries for more information.

     C operators:
       ?:  ||  &&  |  ^  &  == !=  < > <= >=  << >>  + -  * / %  - ~ !

     Rawhide tokens:
       "pattern"  "pattern".modifier  "/path".field  "cmd".sh  {cmd}.sh
       123 0777 0xffff  1K 2M 3G  1k 2m 3g  $user @group  $$ @@
       [yyyy/mm/dd] [yyyy/mm/dd hh:mm:ss]

     Glob pattern notation:
       ? * [abc] [!abc] [a-c] [!a-c]
       ?(a|b|c) *(a|b|c) +(a|b|c) @(a|b|c) !(a|b|c)
       Ksh extended glob patterns are available here (see fnmatch(3))

     Pattern modifiers:
                     .i            .re           .rei
       .path         .ipath        .repath       .reipath
       .link         .ilink        .relink       .reilink
       .body         .ibody        .rebody       .reibody
       .what         .iwhat        .rewhat       .reiwhat
       .mime         .imime        .remime       .reimime
       .acl          .iacl         .reacl        .reiacl
       .ea           .iea          .reea         .reiea
       .sh
       Case-insensitive glob matching is available here (i)
       Perl-compatible regular expressions are available here (re)
       File types and MIME types are available here (what, mime)
       Access control lists are available here (acl)
       Extended attributes are available here (ea)

     Built-in symbols:
       dev           major         minor         ino           mode
       nlink         uid           gid           rdev          rmajor
       rminor        size          blksize       blocks        atime
       mtime         ctime         attr          proj          gen
       nouser        nogroup       readable      writable      executable
       strlen        depth         prune         trim          exit
       now           today         second        minute        hour
       day           week          month         year          IFREG
       IFDIR         IFLNK         IFCHR         IFBLK         IFSOCK
       IFIFO         IFDOOR        IFMT          ISUID         ISGID
       ISVTX         IRWXU         IRUSR         IWUSR         IXUSR
       IRWXG         IRGRP         IWGRP         IXGRP         IRWXO
       IROTH         IWOTH         IXOTH         texists       tdev
       tmajor        tminor        tino          tmode         tnlink
       tuid          tgid          trdev         trmajor       trminor
       tsize         tblksize      tblocks       tatime        tmtime
       tctime        tstrlen

     Reference file fields:
       .exists       .dev          .major        .minor        .ino
       .mode         .type         .perm         .nlink        .uid
       .gid          .rdev         .rmajor       .rminor       .size
       .attr         .proj         .gen          .strlen       .inode
       .nlinks       .user         .group        .sz           .accessed
       .modified     .changed      .attribute    .project      .generation
       .len

     System-wide and user-specific functions can be defined here:
       /etc/rawhide.conf          ~/.rhrc
       /etc/rawhide.conf.d/*      ~/.rhrc.d/*

# EXAMPLES

The following are examples of *rh* expressions. Where multiple versions are
given, the first one only uses built-in symbols, and the rest usually make
use of the standard library in `/etc/rawhide.conf` (or similar) as well. See
*rawhide.conf(5)* for details.

Find files that are owned by the user `drew`, and are writable by other
people:

        (uid == $drew) && (mode & 022) # uid and mode are built-in
        (uid == $drew) && (gw | ow)    # gw and ow are in /etc/rawhide.conf

Find files that are owned by `root`, have the setuid bit set, and are
world-writable:

        !uid && (mode & ISUID) && (mode & 02) # uid, mode, ISUID: built-in
        roots && setuid && other_writable     # the rest: /etc/rawhide.conf
        roots && setuid && world_writable
        roots && suid && ow
        roots && suid && ww

Find executable files that are larger than 10KiB, and have not been executed
in the last 24 hours:

        (mode & 0111) && (size > 10 * 1024) && (atime < now - 24 * hour)
        any(0111) && (size > 10 * KiB) && accessed < ago(24 * hours)
        anyx && sz > 10K && atime < ago(day)

Find *C* source files that are smaller than 4KiB, and other files that are
smaller than 32KiB:

        size < ("*.c" ? 4K : 32K)     # size: built-in
        size < ("*.c" ? 4 : 32) * KiB # KiB: /etc/rawhide.conf

Find files that are an exact multiple of 1KiB in size:

        (size % 1024) == 0
        !(sz % 1K)

Find files that were last modified during March, 1982:

        mtime >= [1982/3/1] && mtime < [1982/4/1]
        modified >= [1982/3/1] && modified < [1982/4/1]

Find files that have been read since they were last written:

        atime > mtime
        accessed > modified

Find files whose names are between 4 and 10 bytes in length:

        strlen >= 4 && strlen <= 10
        len >= 4 && len <= 10

Find files that are at a relative depth of 3 or more below the starting
search directory:

        depth >= 3

This expression finds `*.c` files. However, it will not search in any
directories named `bin` or `tmp`. If these file names are encountered,
the `prune` built-in is evaluated, preventing the current path from
matching, and preventing further searching below the current path.

        ("tmp" || "bin") ? prune : "*.c"
        ("tmp" || "bin") && prune || "*.c"

Find files that were modified after another file was last modified:

        mtime > "/otherfile".mtime
        modified > "/otherfile".modified

Find files that are larger than one file and smaller than another file:

        size > "/somefile".size && size < "/otherfile".size
        sz > "/somefile".sz && sz < "/otherfile".sz

Find files with holes (for filesystems without transparent compression):

        (mode & IFMT) == IFREG && size && blocks && (blocks * 512) < size
        file && size && blocks && space < size

Find regular files with multiple hard links:

        (mode & IFMT) == IFREG && nlink > 1
        file && nlinks > 1
        f && nlink > 1

Find all hard links to a particular file:

        (dev == "/path".dev) && (ino == "/path".ino)
        (dev == "/path".dev) && (ino == "".ino) # Implicit 2nd reference

Find devices with the same device driver as `/dev/tty`:

        rmajor == "/dev/tty".rmajor

Find symlinks whose target paths are relative:

        "[!/]*".link

Find symlinks whose ultimate targets are on a different filesystem:

        (mode & IFMT) == IFLNK && texists && tdev != dev
        symlink && target_exists && target_dev != dev
        l && texists && tdev != dev
        texists && tdev != dev

Find symlinks whose ultimate targets don't exist:

        (mode & IFMT) == IFLNK && !texists
        symlink && !target_exists
        link && !texists
        l && !texists
        dangling
        broken

Find mountpoints under the current directory:

        $ rh -1 'dev != ".".dev'

Find directories with no sub-directories (fast, for most filesystems, but
not *btrfs*):

        $ rh 'd && nlink == 2'

The same, but works for *btrfs* (slow-ish, but demonstrates shell commands):

        $ rh 'd && "[ $(rh -red -- %S | wc -l) = 0 ]".sh'
        $ rh 'd && "[ -z \"$(rh -red -- %S)\" ]".sh'
        $ rh 'd && { [ -z "$(rh -red -- %S)" ] }.sh'

Find empty (readable) directories (fast-ish, and works for *btrfs*):

        $ rh 'd && empty'

Find symlinks whose immediate targets are also symlinks:

        $ rh -l 'l && "[ -L \"$(rh -L%%l -- %S)\" ]".sh'
        $ rh -l 'l && "[ -L \"$(readlink -- %S)\" ]".sh'
        $ rh -l 'l && { [ -L "$(readlink -- %S)" ] }.sh'

Find all hard links to all regular files that have multiple hard links (very
slow):

        # rh -e 'f && nlink > 1' \
             -X 'rh / "(dev == \"%S\".dev) && (ino == \"\".ino)"; echo' \
             /

The same, but for a single filesystem only (shorter, less slow, but still
very slow):

        # rh -1 -e 'f && nlink > 1' -X 'rh -1 / "ino == \"%S\".ino"; echo' /

Find 32-bit ELF executables:

        $ rh 'f && anyx && sz > 10k && "ELF 32-bit*executable*".what'

Find text files with ISO-8859 encoding:

        $ rh 'f && "*ISO-8859 text".what'
        $ rh 'f && "text/*; charset=iso-8859*".mime'

Find files that contain `TODO`:

        $ rh '"*TODO*".body'
        $ rh '"TODO".rebody'

Find files using a *Perl*-compatible regular expression (regex):

        $ rh '"^[a-zA-Z0-9_]+[0-9][0-9][0-9]?\..*[a-cz]$".re'
        $ rh '"^\w+\d{2,3}\..*[a-cz]$".re'

See *perlre(1)*, *pcre2pattern(3)*, and *pcre2syntax(3)* for details.

The same, but with documentation:

        $ rh '"
          ^         # Anchor the match to the start of the base name 
          \w+       # Starts with at least one word character
          \d{2,3}   # Followed by two or three digits
          \.        # Followed by a literal dot
          .*        # Followed by anything (or nothing)
          [ a-c z ] # Ends with a, b, c, or z
          $         # Anchor the match to the end of the base name
        ".re'

Case-insensitive search (anything with `abc` in the name):

        $ rh '"*ABC*".i' # Case-insensitive glob of base name
        $ rh '"ABC".rei' # Case-insensitive regex of base name

Find files by their full path starting from the search directory (anything
under an `abc` directory):

        $ rh '"*/abc/*".path'  # Glob of full path
        $ rh '"/abc/".repath'  # Regex of full path
        $ rh '"*/ABC/*".ipath' # Case-insensitive glob of full path
        $ rh '"/ABC/".reipath' # Case-insensitive regex of full path

Find symlinks by their target path (symlinks to anything under an `abc`
directory):

        $ rh -l '"*/abc/*".link'  # Glob of symlink target path
        $ rh -l '"/abc/".relink'  # Regex of symlink target path
        $ rh -l '"*/ABC/*".ilink' # Case-insensitive glob of symlink target
        $ rh -l '"/ABC/".reilink' # Case-insensitive regex of symlink target

Find files with *"POSIX"* ACLs (*Linux* and *Cygwin*) that grant write
access to the user `drew`:

        $ rh '(uid == $drew) ? "*user::?w?*".acl   : "*user:drew:?w?*".acl'
        $ rh '(uid == $drew) ? "^user::.w.$".reacl : "^user:drew:.w.$".reacl'

Find files with *NFSv4* ACLs (*FreeBSD* and *Solaris*) that grant write
access to the user `drew`:

        $ rh '(uid == $drew)
            ?    "*owner@:?w????????????:???????:allow*".acl
            : "*user:drew:?w????????????:???????:allow*".acl
        '

        $ rh '(uid == $drew)
            ?    "owner@:.w.{12}:.{7}:allow".reacl
            : "user:drew:.w.{12}:.{7}:allow".reacl
        '

        $ rh '(uid == $drew)
            ?    "owner@:[^:]+/write_data/[^:]+(:[^:]*)?:allow".reacl
            : "user:drew:[^:]+/write_data/[^:]+(:[^:]*)?:allow".reacl
        '

Note that, with *NFSv4* ACLs, you can search for ACLs using either the
compact form, or the non-compact form. But be warned that the permission
names in the non-compact form do not always appear in the same order (at
least on *Solaris*).

Find files on *macOS* with ACLs that grant write access to the user `drew`:

        $ rh '(uid == $drew) ? uw : "user:[^:]+:drew:\d+:allow:write".reacl'

Find files with non-trivial access control lists (ACL):

        $ rh '"*mask::*".acl'        # "POSIX" ACLs (Linux, Cygwin)
        $ rh '"(user|group):".reacl' # NFSv4 ACLs (FreeBSD, Solaris)
        $ rh '"?*".acl'              # macOS ACLs

Find files with extended attributes (EA):

        $ rh '"?*".ea'
        $ rh '".".reea'

Find files on *Linux* by their *selinux(8)* context (any):

        $ rh '"*security.selinux: *_u:*_r:*_t:s[0-3]*".ea'
        $ rh '"^security\.selinux:\ .*_u:.*_r:.*_t:s[0-3]".reea'

Find files on *Linux*, *FreeBSD*, *OpenBSD*, *NetBSD*, or *macOS*, that are
immutable or append-only:

        $ rh / 'immutable || append'

Find files on *Solaris* with setuid executable extended attributes (silly):

        $ rh / '"*/stat: -rws*".ea'
        $ rh / '"/stat:\ -rws".reea'

# DOCUMENTATION

*Rawhide*'s documentation can be read here:

- <https://raf.org/rawhide/manual/rh.1.html>
- <https://raf.org/rawhide/manual/rawhide.conf.5.html>

# DOWNLOAD

*Rawhide*'s source distribution can be downloaded from these locations:

- <https://raf.org/rawhide/download/rawhide-3.3.tar.gz> (or [archive.org](https://web.archive.org/web/20230000000000*/https://raf.org/rawhide/download/rawhide-3.3.tar.gz))
- <https://github.com/raforg/rawhide/releases/download/v3.3/rawhide-3.3.tar.gz>

This is free software released under the terms of the GNU General Public
Licence version 3 or later (*GPLv3+*).

# INSTALL

To install *rawhide*:

        tar xzf rawhide-3.3.tar.gz
        cd rawhide-3.3
        ./configure
        make
        make test # optional (lots of output, or set quiet=1)
        sudo make install

This will install (approximately, depending on the operating system):

        /etc/rawhide.conf
        /etc/rawhide.conf.d/
        /usr/local/bin/rh
        /usr/local/share/man/man1/rh.1
        /usr/local/share/man/man5/rawhide.conf.5

To uninstall *rawhide*:

        sudo make uninstall

To install/uninstall under `/usr` instead of `/usr/local`:

        sudo make PREFIX=/usr install
        sudo make PREFIX=/usr uninstall

To check out the `configure` script which can override paths and features:

        ./configure --help

To see what other things the `Makefile` can do:

        make help

# PACKAGING

To build a package on rpm-based distributions:
        rpmbuild -ta rawhide-3.3.tar.gz

# REQUIREMENTS

*Rawhide* should compile and work on any recent *POSIX* system (post-2008)
with a *C* compiler and *make*. It has been thoroughly tested on
*Debian*/*Ubuntu*/*Fedora* *Linux*, *FreeBSD*, *OpenBSD*, *NetBSD*, *macOS*,
*Solaris*, and *Cygwin*.

The *configure* script only knows about these systems as far as installation
locations are concerned, and whether or not manual entries should be
gzipped. For other systems, you can accept the defaults, or override them
with the *configure* script's options, or manually adjust the *Makefile* to
suit your needs.

The (non-*POSIX*) *major()* and *minor()* macros are expected to be in
`<sys/types.h>`, `<sys/sysmacros.h>`, or `<sys/mkdev.h>`. If they are
anywhere else, changes will be needed.

While optional, it is very highly recommended that *libpcre2-8* be
installed. It adds so much more fun. On *macOS*, *libpcre2-8* can be
installed via *macports* or *homebrew*.

It is recommended that *libmagic* be installed. It lets search criteria
include matching file type descriptions and MIME types. And *pkg-config* is
needed (to find and use it).

On *Linux*, *libacl* (for access control lists) and *libe2p* (for
*ext2*-style file attributes) are also recommended. And *pkg-config* is
needed (to find and use them).

To build and install from the *git* repository, *pod2man* (which comes with
*perl*) is needed to produce the manual entries. The source distribution
includes the installable manual entries (so *pod2man* isn't needed to build
and install from the distribution).

*Rawhide* only works completely in locales that use *UTF-8*. It mostly works
in locales that use an *ASCII*-compatible single-byte character encoding
such as *ISO-8859-\** (except for *JSON* output). But other multi-byte
character encodings are not supported.

--------------------------------------------------------------------------------

    URL: https://raf.org/rawhide
    GIT: https://github.com/raforg/rawhide
    GIT: https://codeberg.org/raforg/rawhide
    Date: 20231013
    Authors: 1990 Ken Stauffer, 2022-2023 raf <raf@raf.org>

