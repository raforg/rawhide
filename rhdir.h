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

#ifndef RAWHIDE_RHDIR_H
#define RAWHIDE_RHDIR_H

llong env_int(char *envname, llong min_value, llong max_value, llong default_value);
int rawhide_search(char *fpath);
int following_symlinks(void);
const char *modestr(struct stat *statbuf);
void visitf_default(void);
void visitf_long(void);
void visitf_execute(void);
void visitf_execute_local(void);
void visitf_unlink(void);
void visitf_format(void);
int syscmd(const char *cmd);
int remove_danger_from_path(void);
int interpolate_command(const char *srccmd, char *command, int cmdbufsize);
int chdir_local(int do_debug);

#define CMDBUFSIZE 8192

#endif
