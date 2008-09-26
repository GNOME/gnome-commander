/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2008 Piotr Eljasiak

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

#ifndef __OWNER_H__
#define __OWNER_H__

#include <grp.h>
#include <pwd.h>

typedef struct
{
    gboolean zombie;    /* The gid of this group doesnt match any group in the system. */
    char *name;
    char *passwd;
    gid_t gid;
    GList *members;     /* stores the  members as char *strings */
} group_t;


typedef struct
{
    gboolean zombie;    /* The uid of this user doesnt match any user in the system. */
    char *name;
    char *passwd;
    uid_t uid;
    gid_t gid;
    group_t *group;
    char *realname;
    char *homedir;
    char *shell;
    GList *groups;
} user_t;

void OWNER_init (void);
void OWNER_free (void);
user_t *OWNER_get_program_user (void);
user_t *OWNER_get_user_by_uid (uid_t uid);
group_t *OWNER_get_group_by_gid (gid_t gid);
user_t *OWNER_get_user_by_name (const char *name);
group_t *OWNER_get_group_by_name (const char *name);
GList *OWNER_get_all_users (void);
GList *OWNER_get_all_groups (void);
const gchar *OWNER_get_name_by_uid (uid_t uid);
const gchar *OWNER_get_name_by_gid (gid_t gid);
uid_t OWNER_get_uid_by_name (const gchar *name);
gid_t OWNER_get_gid_by_name (const gchar *name);

#endif // __OWNER_H__
