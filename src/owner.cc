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
#include <unistd.h>

#include <map>
#include <set>

#include "gnome-cmd-includes.h"
#include "owner.h"
#include "utils.h"

using namespace std;


GnomeCmdOwner gcmd_owner;


static gint compare_names (const gchar *name1, const gchar *name2)
{
    return strcmp(name1, name2);
}


#if !GLIB_CHECK_VERSION (2, 14, 0)
template <typename T, typename ID>
GList *GnomeCmdOwner::HashTable<T,ID>::get_names()
{
    GList *retval = NULL;

    for (GList *l=users; l; l=l->next)
        retval = g_list_prepend (retval, static_cast<user_t *>(l->data)->name);

    return g_list_sort (retval, (GCompareFunc) compare_users);
}
#endif


GnomeCmdOwner::GnomeCmdOwner()
{
    thread = NULL;
    stop_thread = FALSE;
    group_names = NULL;

    if (!buff)
    {
        long int pw_size = sysconf(_SC_GETPW_R_SIZE_MAX);
        long int gr_size = sysconf(_SC_GETGR_R_SIZE_MAX);

        if (pw_size==-1)    pw_size = 4096;     // `sysconf' does not support _SC_GETPW_R_SIZE_MAX. Try a moderate value.
        if (gr_size==-1)    gr_size = 4096;     // `sysconf' does not support _SC_GETGR_R_SIZE_MAX. Try a moderate value.

        buffsize = max(pw_size, gr_size);
        buff = g_new0 (char, buffsize);
    }

    user_id = geteuid();
    get_name_by_uid(user_id);
    group_id = users.lookup(user_id)->data.gid;
}


gpointer GnomeCmdOwner::perform_load_operation (GnomeCmdOwner *self)
{
    map <uid_t, set<gid_t> > user_groups;
    map <gid_t, set<uid_t> > group_members;

    setpwent();

    for (struct passwd *pwd=getpwent(); !self->stop_thread && pwd; pwd=getpwent())
    {
        GnomeCmdUsers::Entry *e = self->users.lookup(pwd->pw_uid);

        if (!e)
            e = self->new_entry(pwd);

        user_groups[e->id].insert(e->data.gid);
        group_members[e->data.gid].insert(e->id);
    }

    endpwent();

    setgrent();

    for (struct group *grp=getgrent(); !self->stop_thread && grp; grp=getgrent())
    {
        if (!self->groups.lookup(grp->gr_gid))
            self->new_entry(grp);

        for (char **mem=grp->gr_mem; *mem; ++mem)
        {
            GnomeCmdUsers::Entry *e = self->users.lookup(*mem);

            if (e)
            {
                user_groups[e->id].insert(grp->gr_gid);
                group_members[grp->gr_gid].insert(e->id);
            }
        }
    }

    endgrent();

    if (!self->is_root())
    {
        map <uid_t, set<gid_t> >::iterator x = user_groups.find(self->uid());

        if (x!=user_groups.end())
            for (set<gid_t>::const_iterator i=x->second.begin(); i!=x->second.end(); ++i)
                self->group_names = g_list_prepend (self->group_names, (gpointer) self->groups[*i]);
    }
    else
        for (map <gid_t, set<uid_t> >::const_iterator i=group_members.begin(); i!=group_members.end(); ++i)
            self->group_names = g_list_prepend (self->group_names, (gpointer) self->groups[i->first]);

    self->group_names = g_list_sort (self->group_names, (GCompareFunc) compare_names); ;

    return NULL;
}


void GnomeCmdOwner::load_async()
{
    thread = g_thread_create ((GThreadFunc) perform_load_operation, this, TRUE, NULL);
}
