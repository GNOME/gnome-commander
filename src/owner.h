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

#ifndef __OWNER_H__
#define __OWNER_H__

#include <grp.h>
#include <pwd.h>

struct group_t
{
    char *name;
    gid_t gid;
    GList *members;     // stores the members as char *strings
    gboolean zombie;    // The gid of this group doesn't match any group in the system
};

struct user_t
{
    char *name;
    uid_t uid;
    gid_t gid;
    group_t *group;
    char *realname;
    GList *groups;
    gboolean zombie;    // The uid of this user doesn't match any user in the system
};

void OWNER_init ();
void OWNER_free ();
user_t *OWNER_get_program_user ();
user_t *OWNER_get_user_by_uid (uid_t uid);
group_t *OWNER_get_group_by_gid (gid_t gid);
user_t *OWNER_get_user_by_name (const char *name);
group_t *OWNER_get_group_by_name (const char *name);
GList *OWNER_get_all_users ();
GList *OWNER_get_all_groups ();

inline const gchar *OWNER_get_name_by_uid (uid_t uid)
{
    user_t *user = OWNER_get_user_by_uid (uid);

    return user ? user->name : NULL;
}


inline const gchar *OWNER_get_name_by_gid (gid_t gid)
{
    group_t *group = OWNER_get_group_by_gid (gid);

    return group ? group->name : NULL;
}


inline uid_t OWNER_get_uid_by_name (const gchar *name)
{
    user_t *user = OWNER_get_user_by_name (name);

    return user ? user->uid : -1;
}


inline gid_t OWNER_get_gid_by_name (const gchar *name)
{
    group_t *group = OWNER_get_group_by_name (name);

    return group ? group->gid : -1;
}

#endif // __OWNER_H__
