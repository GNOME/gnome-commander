/*
    LibGViewer - GTK+ File Viewer library
    Copyright (C) 2006 Assaf Gordon

    Part of
        GNOME Commander - A GNOME based file manager
        Copyright (C) 2001-2006 Marcus Bjurman
        Copyright (C) 2007-2009 Piotr Eljasiak

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#ifndef __LIBGVIEWER_TYPES_H__
#define __LIBGVIEWER_TYPES_H__

/* TODO: Change these for Big-Endian machines */

#define is_displayable(c) (((c) >= 0x20) && ((c) < 0x7f))

#define GV_FIRST_BYTE(x)  ((unsigned char)(x)&0xFF)
#define GV_SECOND_BYTE(x) ((unsigned char)((x)>>8)&0xFF)
#define GV_THIRD_BYTE(x)  ((unsigned char)((x)>>16)&0xFF)
#define GV_FOURTH_BYTE(x) ((unsigned char)((x)>>24)&0xFF)

/*
 Note:
 UTF-8 encoding supports up to 6 (six) bytes per single character.
 We use only 32bits (4 bytes) to hold a UTF-8 data.
 This means some exotic UTF-8 characters are NOT supported...
*/
typedef guint32  char_type;
#define INVALID_CHAR ((char_type) -1)
/*
 Note:
 Currently on files<2GB are supported (even though this is unsigned)
*/
typedef unsigned long offset_type;
#define INVALID_OFFSET ((offset_type) -1)

#endif // __LIBGVIEWER_TYPES_H__
