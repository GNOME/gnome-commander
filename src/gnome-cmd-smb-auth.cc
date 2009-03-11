/*
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

#include <config.h>
#include <libgnomevfs/gnome-vfs-module-callback.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-smb-auth.h"

using namespace std;


static void // GnomeVFSModuleCallback
vfs_full_authentication_callback (const GnomeVFSModuleCallbackFullAuthenticationIn *in_args, size_t in_size,
                                  GnomeVFSModuleCallbackFullAuthenticationOut *out_args, size_t out_size,
                                  gpointer user_data)
{
    GnomePasswordDialog *gpwd =  GNOME_PASSWORD_DIALOG (gnome_password_dialog_new (_("Enter password"),
                                                                                   _("Problem: access not permitted\n\n"
                                                                                     "please supply user credentials\n\n"
                                                                                     "Remember: wrong credentials may lead to account locking"),
                                                                                     "", "", FALSE));

    gnome_password_dialog_set_show_domain (gpwd, in_args->flags & GNOME_VFS_MODULE_CALLBACK_FULL_AUTHENTICATION_NEED_DOMAIN);
    gnome_password_dialog_set_domain (gpwd, in_args->domain);
    gnome_password_dialog_set_show_remember (gpwd, FALSE);
    gnome_password_dialog_set_show_username (gpwd, in_args->flags & GNOME_VFS_MODULE_CALLBACK_FULL_AUTHENTICATION_NEED_USERNAME);
    gnome_password_dialog_set_username (gpwd, in_args->username);
    gnome_password_dialog_set_show_userpass_buttons (gpwd, in_args->flags & GNOME_VFS_MODULE_CALLBACK_FULL_AUTHENTICATION_ANON_SUPPORTED);
    gnome_password_dialog_set_show_password (gpwd, in_args->flags & GNOME_VFS_MODULE_CALLBACK_FULL_AUTHENTICATION_NEED_PASSWORD);

    out_args->abort_auth = gnome_password_dialog_run_and_block (GNOME_PASSWORD_DIALOG (gpwd)) == FALSE;

    if (out_args->abort_auth)
        return;

    out_args->username = g_strdup (gnome_password_dialog_get_username (gpwd));
    out_args->password = g_strdup (gnome_password_dialog_get_password (gpwd));
    out_args->domain = g_strdup (gnome_password_dialog_get_domain (gpwd));
}


void gnome_cmd_smb_auth_init ()
{
    gnome_vfs_module_callback_set_default (GNOME_VFS_MODULE_CALLBACK_FULL_AUTHENTICATION,
                                           (GnomeVFSModuleCallback) vfs_full_authentication_callback,
                                           GINT_TO_POINTER (0), NULL);
}
