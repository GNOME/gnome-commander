/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman

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
#include "gnome-cmd-includes.h"
#include "owner.h"
#include "utils.h"

GList *all_users;
GList *all_groups;


static gint compare_users (user_t *u1, user_t *u2)
{
    return strcmp (u1->name, u2->name);
}


static gint compare_groups (user_t *g1, user_t *g2)
{
    return strcmp (g1->name, g2->name);
}


static user_t *create_user (struct passwd *pw, gboolean zombie)
{
    if (pw)
    {
        user_t *user = g_new(user_t,1);

        g_assert (pw->pw_name);
        g_assert (pw->pw_passwd);
        g_assert (pw->pw_gecos);
        g_assert (pw->pw_dir);
        g_assert (pw->pw_shell);

        user->zombie =   zombie;
        user->name =     g_strdup (pw->pw_name);
        user->passwd =   g_strdup (pw->pw_passwd);
        user->uid =      pw->pw_uid;
        user->gid =      pw->pw_gid;
        user->realname = g_strdup (pw->pw_gecos);
        user->homedir =  g_strdup (pw->pw_dir);
        user->shell =    g_strdup (pw->pw_shell);
        user->groups =   NULL; /* filled in later when going through the group
                                  members*/

        return user;
    }

    return NULL;
}


static group_t *create_group (struct group *gr, gboolean zombie)
{
    if (gr)
    {
        group_t *group = g_new(group_t,1);
        char **members;

        g_assert (gr->gr_name);
        g_assert (gr->gr_passwd);

        group->zombie =  zombie;
        group->name =    g_strdup (gr->gr_name);
        group->passwd =  g_strdup (gr->gr_passwd);
        group->gid =     gr->gr_gid;
        group->members = NULL;

        members = gr->gr_mem;

        while (members && *members)
        {
            user_t *user = OWNER_get_user_by_name (*members);

            if (user)
            {
                group->members = g_list_append (group->members, user);
                user->groups = g_list_append (user->groups, group);
            }
            else
                g_printerr (_("When parsing the users and groups on this system it was found that the user %s is part of the group %s. This user can however not be found.\n"), *members, group->name);

            members++;
        }

        return group;
    }

    return NULL;
}


static void free_user (user_t *user)
{
    g_free (user->name);
    g_free (user->passwd);
    g_free (user->realname);
    g_free (user->homedir);
    g_free (user->shell);
    g_free (user);
}


static void free_group (group_t *group)
{
    g_free (group->name);
    g_free (group->passwd);
    g_list_free (group->members);
    g_free (group);
}


static void lookup_all_users ()
{
    user_t *user;
    struct passwd *pw;

    setpwent ();

    while ((pw = getpwent ()) != NULL)
    {
        user = create_user (pw, FALSE);
        all_users = g_list_append (all_users, user);
    }

    endpwent ();

    all_users = g_list_sort (all_users, (GCompareFunc)compare_users);
}


static void lookup_all_groups ()
{
    group_t *group;
    struct group *gr;

    setgrent ();

    while ((gr = getgrent ()) != NULL)
    {
        group = create_group (gr,FALSE);
        all_groups = g_list_append (all_groups, group);
    }

    endgrent ();

    all_groups = g_list_sort (all_groups, (GCompareFunc)compare_groups);
}


static void check_user_default_groups ()
{
    user_t *user;
    group_t *group;
    GList *utmp, *gtmp;


    for (utmp=all_users; utmp; utmp=utmp->next)
    {
        group_t *def_group = NULL;
        user = (user_t *) utmp->data;

        for (gtmp=user->groups; gtmp; gtmp = gtmp->next)
        {
            group = (group_t *) gtmp->data;

            if (group->gid == user->gid)
                def_group = group;
        }

        if (!def_group)
        {
            def_group = OWNER_get_group_by_gid (user->gid);
            user->groups = g_list_append (user->groups, def_group);

            def_group->members = g_list_append (def_group->members, user);
        }
    }
}


/************************************************************************/


user_t *OWNER_get_user_by_uid (uid_t uid)
{
    GList *tmp;
    user_t *user;

    /* try to locate the user in the list of already found users */
    for (tmp=all_users; tmp; tmp = tmp->next)
    {
        user = tmp->data;

        if (uid == user->uid)
            return user;
    }

    // there is no such user in the system, lets create a blank user with the specified uid

    struct passwd pw;

    pw.pw_uid = uid;
    pw.pw_name = g_strdup_printf ("%d",uid);
    pw.pw_gecos = "";
    pw.pw_dir = "";
    pw.pw_shell = "";
    pw.pw_passwd = "";

    user = create_user (&pw,TRUE);
    if (user)
        all_users = g_list_append (all_users, user);

    g_free (pw.pw_name);

    return user;
}


user_t *OWNER_get_user_by_name (const char *name)
{
    GList *tmp;

    /* try to locate the user in the list of already found users */
    for (tmp = all_users; tmp; tmp = tmp->next)
    {
        user_t *user = tmp->data;

        if (strcmp (name, user->name) == 0)
            return user;
    }

    return NULL;
}


group_t *OWNER_get_group_by_gid (gid_t gid)
{
    GList *tmp;

    /* try to locate the group in the list of already found groups */
    for (tmp = all_groups; tmp; tmp = tmp->next)
    {
        group_t *group = tmp->data;

        if (gid == group->gid)
            return group;
    }

    /* there is no such group in the system, lets create a blank group with the specified gid */
    {
        struct group gr;

        gr.gr_gid = gid;
        gr.gr_name = g_strdup_printf ("%d",gid);
        gr.gr_passwd = "";
        gr.gr_mem = NULL;

        group_t *group = create_group (&gr,TRUE);
        if (group)
            all_groups = g_list_append (all_groups, group);

        g_free (gr.gr_name);

        return group;
    }
    return NULL;
}


group_t *OWNER_get_group_by_name (const char *name)
{
    GList *tmp;


    /* try to locate the group in the list of already found groups */
    for (tmp = all_groups; tmp; tmp = tmp->next)
    {
        group_t *group = tmp->data;

        if (strcmp (name, group->name) == 0)
            return group;
    }

    return NULL;
}


void OWNER_init (void)
{
    all_users = NULL;
    all_groups = NULL;

    lookup_all_users ();
    lookup_all_groups ();
    check_user_default_groups ();
}


void OWNER_free (void)
{

    GList *groups;
    GList *users;

    /* free users */
    for (users = all_users; users; users = users->next)
    {
        user_t *user = (user_t *) users->data;
        free_user (user);
    }

    /* free groups */
    for (groups = all_groups; groups; groups = groups->next)
    {
        group_t *group = (group_t *) groups->data;
        free_group (group);
    }

    g_list_free (users);
    g_list_free (groups);
}


user_t *OWNER_get_program_user (void)
{
    const char *name;
    user_t *user;

    name = g_get_user_name ();
    user = OWNER_get_user_by_name (name);

    g_assert (user);

    return user;
}


GList *OWNER_get_all_users (void)
{
    return all_users;
}


GList *OWNER_get_all_groups (void)
{
    return all_groups;
}


const gchar *OWNER_get_name_by_uid (uid_t uid)
{
    user_t *user = OWNER_get_user_by_uid (uid);
    if (!user)
        return NULL;
    return user->name;
}


const gchar *OWNER_get_name_by_gid (gid_t gid)
{
    group_t *group = OWNER_get_group_by_gid (gid);
    if (!group)
        return NULL;
    return group->name;
}


uid_t OWNER_get_uid_by_name (const gchar *name)
{
    user_t *user = OWNER_get_user_by_name (name);
    if (!user)
        return -1;
    return user->uid;
}


gid_t OWNER_get_gid_by_name (const gchar *name)
{
    group_t *group = OWNER_get_group_by_name (name);
    if (!group)
        return -1;
    return group->gid;
}


