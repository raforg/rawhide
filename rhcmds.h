/*
* rawhide - find files using pretty C expressions
* https://raf.org/rawhide
* https://github.com/raforg/rawhide
* https://codeberg.org/raforg/rawhide
*
* Copyright (C) 1990 Ken Stauffer, 2022 raf <raf@raf.org>
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
* 20221011 raf <raf@raf.org>
*/

#ifndef RAWHIDE_RHCMDS_H
#define RAWHIDE_RHCMDS_H

void c_le(llong i);
void c_lt(llong i);
void c_ge(llong i);
void c_gt(llong i);
void c_ne(llong i);
void c_eq(llong i);
void c_bitor(llong i);
void c_bitand(llong i);
void c_bitxor(llong i);
void c_lshift(llong i);
void c_rshift(llong i);
void c_plus(llong i);
void c_mul(llong i);
void c_minus(llong i);
void c_div(llong i);
void c_mod(llong i);
void c_not(llong i);
void c_bitnot(llong i);
void c_uniminus(llong i);
void c_qm(llong i);
void c_colon(llong i);
void c_param(llong i);
void c_func(llong i);
void c_return(llong i);
void c_number(llong i);
void c_dev(llong i);
void c_major(llong i);
void c_minor(llong i);
void c_ino(llong i);
void c_mode(llong i);
void c_nlink(llong i);
void c_uid(llong i);
void c_gid(llong i);
void c_rdev(llong i);
void c_rmajor(llong i);
void c_rminor(llong i);
void c_size(llong i);
void c_blksize(llong i);
void c_blocks(llong i);
void c_atime(llong i);
void c_mtime(llong i);
void c_ctime(llong i);
unsigned long get_attr(void);
unsigned long get_proj(void);
unsigned long get_gen(void);
#if HAVE_ATTR || HAVE_FLAGS
void c_attr(llong i);
#endif
#if HAVE_ATTR
void c_proj(llong i);
void c_gen(llong i);
#endif
void c_depth(llong i);
void c_prune(llong i);
void c_trim(llong i);
void c_exit(llong i);
void c_strlen(llong i);
void c_nouser(llong i);
void c_nogroup(llong i);
void c_readable(llong i);
void c_writable(llong i);
void c_executable(llong i);
void c_glob(llong i);
void c_path(llong i);
void c_link(llong i);
#ifdef FNM_CASEFOLD
void c_i(llong i);
void c_ipath(llong i);
void c_ilink(llong i);
#endif
#ifdef HAVE_PCRE2
void pcre2_cache_free(void);
void c_re(llong i);
void c_repath(llong i);
void c_relink(llong i);
void c_rei(llong i);
void c_reipath(llong i);
void c_reilink(llong i);
#endif
#ifdef HAVE_MAGIC
void c_what(llong i);
#ifdef FNM_CASEFOLD
void c_iwhat(llong i);
#endif
#ifdef HAVE_PCRE2
void c_rewhat(llong i);
void c_reiwhat(llong i);
#endif
#endif
#ifdef HAVE_MAGIC
void c_mime(llong i);
#ifdef FNM_CASEFOLD
void c_imime(llong i);
#endif
#ifdef HAVE_PCRE2
void c_remime(llong i);
void c_reimime(llong i);
#endif
#endif
#ifdef HAVE_ACL
void c_acl(llong i);
#ifdef FNM_CASEFOLD
void c_iacl(llong i);
#endif
#ifdef HAVE_PCRE2
void c_reacl(llong i);
void c_reiacl(llong i);
#endif
#endif
#ifdef HAVE_EA
void c_ea(llong i);
#ifdef FNM_CASEFOLD
void c_iea(llong i);
#endif
#ifdef HAVE_PCRE2
void c_reea(llong i);
void c_reiea(llong i);
#endif
#endif
void c_sh(llong i);

void r_exists(llong i);
void r_dev(llong i);
void r_major(llong i);
void r_minor(llong i);
void r_ino(llong i);
void r_mode(llong i);
void r_nlink(llong i);
void r_uid(llong i);
void r_gid(llong i);
void r_rdev(llong i);
void r_rmajor(llong i);
void r_rminor(llong i);
void r_size(llong i);
void r_blksize(llong i);
void r_blocks(llong i);
void r_atime(llong i);
void r_mtime(llong i);
void r_ctime(llong i);
#if HAVE_ATTR || HAVE_FLAGS
void r_attr(llong i);
#endif
#if HAVE_ATTR
void r_proj(llong i);
void r_gen(llong i);
#endif
void r_strlen(llong i);
void r_type(llong i);
void r_perm(llong i);

void t_exists(llong i);
void t_dev(llong i);
void t_major(llong i);
void t_minor(llong i);
void t_ino(llong i);
void t_mode(llong i);
void t_nlink(llong i);
void t_uid(llong i);
void t_gid(llong i);
void t_rdev(llong i);
void t_rmajor(llong i);
void t_rminor(llong i);
void t_size(llong i);
void t_blksize(llong i);
void t_blocks(llong i);
void t_atime(llong i);
void t_mtime(llong i);
void t_ctime(llong i);
void t_strlen(llong i);

char *read_symlink(void);
void prepare_target(void);
const char *get_what(void);
const char *get_mime(void);
char *get_acl(int);
char *get_ea(int);
void set_dirsize(void);
int has_real_acl(void);
int has_real_ea(void);

#ifndef NDEBUG
char *instruction_name(void (*func)(llong));
#endif

#endif
