/*
    GNOME Commander - A GNOME based file manager 
    Copyright (C) 2001-2003 Marcus Bjurman

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/ 
#ifndef __GNOME_CMD_SMB_PATH_H__
#define __GNOME_CMD_SMB_PATH_H__

#include "gnome-cmd-path.h"


#define GNOME_CMD_SMB_PATH(obj) \
	GTK_CHECK_CAST (obj, gnome_cmd_smb_path_get_type (), GnomeCmdSmbPath)
#define GNOME_CMD_SMB_PATH_CLASS(klass) \
	GTK_CHECK_CLASS_CAST (klass, gnome_cmd_smb_path_get_type (), GnomeCmdSmbPathClass)
#define GNOME_CMD_IS_SMB_PATH(obj) \
    GTK_CHECK_TYPE (obj, gnome_cmd_smb_path_get_type ())


typedef struct _GnomeCmdSmbPath GnomeCmdSmbPath;
typedef struct _GnomeCmdSmbPathClass GnomeCmdSmbPathClass;
typedef struct _GnomeCmdSmbPathPrivate GnomeCmdSmbPathPrivate;


struct _GnomeCmdSmbPath
{
	GnomeCmdPath parent;

	GnomeCmdSmbPathPrivate *priv;
};

struct _GnomeCmdSmbPathClass
{
	GnomeCmdPathClass parent_class;

	void (* update_text)  (GtkEditable    *editable,
						   gint            start_pos,
						   gint            end_pos);
};


GtkType
gnome_cmd_smb_path_get_type (void);

GnomeCmdPath *
gnome_cmd_smb_path_new (const gchar *workgroup,
						const gchar *resource,
						const gchar *path);

GnomeCmdPath *
gnome_cmd_smb_path_new_from_str (const gchar *path_str);


#endif //__GNOME_CMD_SMB_PATH_H__
