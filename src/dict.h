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

#ifndef __DICT_H
#define __DICT_H

#include <utility>
#include <map>
#include <string>


template <typename KEY, typename VAL=std::string>
class DICT
{
    typedef std::map<KEY,const VAL *> KEY_COLL;
    typedef std::map<VAL,const KEY *> VAL_COLL;

    KEY_COLL k_coll;
    VAL_COLL v_coll;

    const KEY NO_KEY;
    const VAL NO_VALUE;

  public:

    DICT(const KEY no_key=KEY(), const VAL no_val=VAL()): NO_KEY(no_key), NO_VALUE(no_val)   {}

    void add(const KEY k, const VAL &v);
    void add(const VAL v, const KEY &k)             {  add(k,v);  }

    void clear()                                    {  k_coll.clear(); v_coll.clear();  }

    const VAL &operator [] (const KEY k) const;
    const KEY &operator [] (const VAL v) const;
};


template <typename KEY, typename VAL>
inline void DICT<KEY,VAL>::add(const KEY k, const VAL &v)
{
    std::pair<typename KEY_COLL::iterator,bool> k_pos = k_coll.insert(make_pair(k,(const VAL *) NULL));
    std::pair<typename VAL_COLL::iterator,bool> v_pos = v_coll.insert(make_pair(v,(const KEY *) NULL));

    if (k_pos.second)
        k_pos.first->second = &v_pos.first->first;

    if (v_pos.second)
        v_pos.first->second = &k_pos.first->first;
}


template <typename KEY, typename VAL>
inline const VAL &DICT<KEY,VAL>::operator [] (const KEY k) const
{
    typename KEY_COLL::const_iterator pos = k_coll.find(k);

    if (pos==k_coll.end())
        return NO_VALUE;

    return pos->second ? *pos->second : NO_VALUE;
}


template <typename KEY, typename VAL>
inline const KEY &DICT<KEY,VAL>::operator [] (const VAL v) const
{
    typename VAL_COLL::const_iterator pos = v_coll.find(v);

    if (pos==v_coll.end())
        return NO_KEY;

    return pos->second ? *pos->second : NO_KEY;
}


template <typename T>
class DICT<T,T>
{
    typedef std::map<T,const T *> T_COLL;

    T_COLL t_coll;

    const T NO_VALUE;

  public:

    DICT(const T no_val=T()): NO_VALUE(no_val)   {}

    void add(const T k, const T &v);

    void clear()                                    {  t_coll.clear();  }

    const T &operator [] (const T k) const;
};


template <typename T>
inline void DICT<T,T>::add(const T k, const T &v)
{
    std::pair<typename T_COLL::iterator,bool> k_pos = t_coll.insert(make_pair(k,(T *) NULL));
    std::pair<typename T_COLL::iterator,bool> v_pos = t_coll.insert(make_pair(v,(T *) NULL));

    if (k_pos.second)
        k_pos.first->second = &v_pos.first->first;

    if (v_pos.second)
        v_pos.first->second = &k_pos.first->first;
}


template <typename T>
inline const T &DICT<T,T>::operator [] (const T k) const
{
    typename T_COLL::const_iterator pos = t_coll.find(k);

    if (pos==t_coll.end())
        return NO_VALUE;

    return pos->second ? *pos->second : NO_VALUE;
}


template <typename KEY, typename VAL>
inline void load_data(DICT<KEY,VAL> &dict, void *a, unsigned n)
{
    if (!a)
        return;

    typedef struct
    {
        KEY key;
        VAL value;
    } TUPLE;

    TUPLE *t = static_cast<TUPLE *>(a);

    for (unsigned i=0; i<n; ++i, ++t)
         dict.add(t->key,t->value);
}


template <typename KEY>
inline void load_data(DICT<KEY,std::string> &dict, void *a, unsigned n)
{
    if (!a)
        return;

    typedef struct
    {
        KEY  key;
        char *value;
    } TUPLE;

    TUPLE *t = static_cast<TUPLE *>(a);

    for (unsigned i=0; i<n; ++i, ++t)
         dict.add(t->key,t->value);
}


template <typename VAL>
inline void load_data(DICT<std::string,VAL> &dict, void *a, unsigned n)
{
    if (!a)
        return;

    typedef struct
    {
        char *key;
        VAL  value;
    } TUPLE;

    TUPLE *t = static_cast<TUPLE *>(a);

    for (unsigned i=0; i<n; ++i, ++t)
         dict.add(t->key,t->value);
}


inline void load_data(DICT<std::string,std::string> &dict, void *a, unsigned n)
{
    if (!a)
        return;

    typedef struct
    {
        char *key;
        char *value;
    } TUPLE;

    TUPLE *t = static_cast<TUPLE *>(a);

    for (unsigned i=0; i<n; ++i, ++t)
         dict.add(t->key,t->value);
}

#endif
