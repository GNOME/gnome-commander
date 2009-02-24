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

class GnomeCmdOwner
{
    GThread *thread;
    gboolean stop_thread;
    char *buff;
    size_t buffsize;

    uid_t user_id;
    gid_t group_id;

    GList *group_names;

  public:

    template <typename T, typename ID>
    class HashTable
    {
        GHashTable *id_table;
        GHashTable *name_table;

        GList *entries;

        struct Entry
        {
            ID id;
            char *name;
            T  data;
        };

        Entry *lookup(ID id)                    {  return (Entry *) g_hash_table_lookup (id_table, &id);      }
        Entry *lookup(const gchar *name)        {  return (Entry *) g_hash_table_lookup (name_table, name);   }
        Entry *add(ID id, const gchar *name);

      public:

        HashTable();
        ~HashTable();

        guint size()                            {  return g_hash_table_size (id_table);  }
        gboolean empty()                        {  return size()==0;                     }

        const gchar *operator [] (ID id);
        ID operator [] (const gchar *name);
        GList *get_names();

        friend class GnomeCmdOwner;
    };

    struct user_t
    {
        char *real_name;
        gid_t gid;
        gboolean zombie;    // the uid of this user doesn't match any user in the system
    };

    struct group_t
    {
        gboolean zombie;    // the gid of this group doesn't match any group in the system
    };

    typedef HashTable<user_t,uid_t> GnomeCmdUsers;
    typedef HashTable<group_t,gid_t> GnomeCmdGroups;

  private:

    GnomeCmdUsers::Entry *new_entry(const struct passwd *pw);
    GnomeCmdGroups::Entry *new_entry(const struct group *grp);

    static gpointer perform_load_operation (GnomeCmdOwner *self);

  public:

    GnomeCmdUsers users;
    GnomeCmdGroups groups;

    GnomeCmdOwner();
    ~GnomeCmdOwner();

    uid_t uid() const           {  return user_id;   }
    gid_t gid() const           {  return group_id;  }
    gboolean is_root()          {  return uid()==0;  }

    GList *get_group_names()    {  return group_names;  }

    const char *get_name_by_uid(uid_t uid);
    const char *get_name_by_gid(gid_t gid);

   void load_async();
};

template <typename T, typename ID>
inline GnomeCmdOwner::HashTable<T,ID>::HashTable()
{
    entries = NULL;
    id_table = g_hash_table_new (g_int_hash, g_int_equal);
    name_table = g_hash_table_new (g_str_hash, g_str_equal);
}

template <typename T, typename ID>
inline GnomeCmdOwner::HashTable<T,ID>::~HashTable()
{
    g_hash_table_destroy (id_table);
    g_hash_table_destroy (name_table);
    if (entries)
    {
        g_list_foreach (entries, (GFunc) g_free, NULL);
        g_list_free (entries);
    }
}

template <typename T, typename ID>
inline typename GnomeCmdOwner::HashTable<T,ID>::Entry *GnomeCmdOwner::HashTable<T,ID>::add(ID id, const gchar *name)
{
    Entry *e = g_new0 (Entry, 1);

    e->id = id;
    e->name = g_strdup (name);

    entries = g_list_prepend (entries, e);

    g_hash_table_insert (id_table, &e->id, e);
    g_hash_table_insert (name_table, e->name, e);

    return e;
}

template <typename T, typename ID>
inline const gchar *GnomeCmdOwner::HashTable<T,ID>::operator [] (ID id)
{
    Entry *entry = lookup(id);

    g_assert (entry != NULL);

    return entry ? entry->name : NULL;
}

template <typename T, typename ID>
inline ID GnomeCmdOwner::HashTable<T,ID>::operator [] (const gchar *name)
{
    Entry *entry = lookup(name);

    g_assert (entry != NULL);

    return entry ? entry->id : -1;
}

#if GLIB_CHECK_VERSION (2, 14, 0)
template <typename T, typename ID>
inline GList *GnomeCmdOwner::HashTable<T,ID>::get_names()
{
    return g_hash_table_get_keys (name_table);  //  FIXME:  sort ?
}
#endif

inline GnomeCmdOwner::GnomeCmdUsers::Entry *GnomeCmdOwner::new_entry(const struct passwd *pw)
{
    g_return_val_if_fail (pw!=NULL, NULL);

    GnomeCmdUsers::Entry *entry = users.add(pw->pw_uid, pw->pw_name);

    entry->data.gid = pw->pw_gid;
    // entry->data.real_name = g_strdup (pw->pw_gecos);     //  not used at the moment
    // entry->data.zombie = FALSE;

    return entry;
}

inline GnomeCmdOwner::GnomeCmdGroups::Entry *GnomeCmdOwner::new_entry(const struct group *grp)
{
    g_return_val_if_fail (grp!=NULL, NULL);

    GnomeCmdGroups::Entry *entry = groups.add(grp->gr_gid, grp->gr_name);

    // entry->data.zombie = FALSE;

    return entry;
}

inline GnomeCmdOwner::~GnomeCmdOwner()
{
    stop_thread = TRUE;
    g_thread_join (thread);
    g_free (buff);
    g_list_free (group_names);
}

inline const char *GnomeCmdOwner::get_name_by_uid(uid_t id)
{
    GnomeCmdUsers::Entry *entry = users.lookup(id);

    if (entry)
        return entry->name;

    struct passwd pwd, *result=NULL;

    getpwuid_r(id, &pwd, buff, buffsize, &result);

    if (!result)        //  zombie
    {
        char s[32];

        snprintf (s, sizeof(s), "%u", id);
        entry = users.add(id, s);
        entry->data.zombie = TRUE;

        return entry->name;
    }

    entry = new_entry(result);

    if (!groups.lookup(entry->data.gid))
        get_name_by_gid(entry->data.gid);

    return entry->name;
}

inline const char *GnomeCmdOwner::get_name_by_gid(gid_t id)
{
    GnomeCmdGroups::Entry *entry = groups.lookup(id);

    if (entry)
        return entry->name;

    struct group grp, *result=NULL;

    getgrgid_r(id, &grp, buff, buffsize, &result);

    if (!result)        //  zombie
    {
        char s[32];

        snprintf (s, sizeof(s), "%u", id);
        entry = groups.add(id, s);
        entry->data.zombie = TRUE;

        return entry->name;
    }

    entry = new_entry(result);

    return entry->name;
}

extern GnomeCmdOwner gcmd_owner;

#endif // __OWNER_H__
