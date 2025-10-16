/*
rawhide - find files using pretty C expressions
https://raf.org/rawhide
https://github.com/raforg/rawhide
https://codeberg.org/raforg/rawhide

Copyright (C) 1990 Ken Stauffer, 2022-2023 raf <raf@raf.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <https://www.gnu.org/licenses/>.

20231013 raf <raf@raf.org>
*/

#ifndef	RAWHIDE_RHFNMATCH_H
#define	RAWHIDE_RHFNMATCH_H

int rhfnmatch(const char *pattern, const char *string, int flags);

#endif
