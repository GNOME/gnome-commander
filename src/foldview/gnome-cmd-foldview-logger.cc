/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2010 Piotr Eljasiak
    Copyleft      2010-2010 Guillaume Wardavoir

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

#include	"gnome-cmd-foldview-logger.h"

#include	<stdio.h>

//  ****************************************************************************
//
//                              Static
//
//  ****************************************************************************
Logger::LogFunction     Logger::Log_function[6] =
                        {
                            &Logger::Inf,
                            &Logger::Wng,
                            &Logger::Err,
                            &Logger::Tki,
                            &Logger::Tkw,
                            &Logger::Tke
                        };

static char			s1  [1024];
static char			s2  [1024];


/*
	Prompt color format : <ESC>[{attr};{fg};{bg}m

    {attr} needs to be one of the following :

	0 Reset All Attributes (return to normal mode)
	1 Bright (usually turns on BOLD)
	2 Dim
	3 Underline
	5 Blink
	7 Reverse
	8 Hidden

    {fg} needs to be one of the following :

      30 Black
      31 Red
      32 Green
      33 Yellow
      34 Blue
      35 Magenta
      36 Cyan
      37 White

    {bg} needs to be one of the following:

      40 Black
      41 Red
      42 Green
      43 Yellow
      44 Blue
      45 Magenta
      46 Cyan
      47 White

*/

void Logger::Inf(const char* _header, const char* _format, ...)
{
            va_list     val;
    //.........................................................................
    va_start(val, _format); vsprintf(s1, _format, val); va_end(val);
    sprintf(s2, "\033[0;32mINF:\033[0m%s:%s", _header, s1);
    printf("%s\n", s2);
};
void Logger::Wng(const char* _header, const char* _format, ...)
{
            va_list     val;
    //.........................................................................
    va_start(val, _format); vsprintf(s1, _format, val); va_end(val);
    sprintf(s2, "\033[0;35mWNG:\033[0m%s:%s", _header, s1);
    printf("%s\n", s2);
};
void Logger::Err(const char* _header, const char* _format, ...)
{
    va_list     val;
    //.........................................................................
    va_start(val, _format); vsprintf(s1, _format, val); va_end(val);
    sprintf(s2, "\033[0;31mERR:\033[0m%s:%s", _header, s1);
    printf("%s\n", s2);
};

void Logger::Tki(const char* _header, const char* _format, ...)
{
    va_list     val;
    //.........................................................................
    va_start(val, _format); vsprintf(s1, _format, val); va_end(val);
    sprintf(s2, "\033[0;33mTKI:\033[0m%s:%s", _header, s1);
    printf("%s\n", s2);
};
void Logger::Tkw(const char* _header, const char* _format, ...)
{
    va_list     val;
    //.........................................................................
    va_start(val, _format); vsprintf(s1, _format, val); va_end(val);
    sprintf(s2, "\033[1;33mTKW:\033[0m%s:%s", _header, s1);
    printf("%s\n", s2);
};
void Logger::Tke(const char* _header, const char* _format, ...)
{
    va_list     val;
    //.........................................................................
    va_start(val, _format); vsprintf(s1, _format, val); va_end(val);
    sprintf(s2, "\033[7;33mTKE:\033[0m%s:%s", _header, s1);
    printf("%s\n", s2);
};





//  ****************************************************************************
//
//                              All the rest
//
//  ****************************************************************************




















