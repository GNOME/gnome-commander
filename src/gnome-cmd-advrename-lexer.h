/** 
 * @file gnome-cmd-advrename-lexer.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2024 Uwe Scholz\n
 *
 * @copyright This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * @copyright This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * @copyright You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#include "gnome-cmd-file.h"

struct GnomeCmdAdvrenameTemplate;

extern "C" GnomeCmdAdvrenameTemplate *gnome_cmd_advrename_template_new(const char *template_string);
extern "C" void gnome_cmd_advrename_template_free(GnomeCmdAdvrenameTemplate *tmplt);

extern "C" char *gnome_cmd_advrename_gen_fname(GnomeCmdAdvrenameTemplate *tmplt,
                                               glong start,
                                               glong step,
                                               guint precision,
                                               gulong index,
                                               gulong count,
                                               GnomeCmdFile *file);
