/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2012 Piotr Eljasiak
    Copyright (C) 2013-2016 Uwe Scholz

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

#ifndef __TUPLE_H__
#define __TUPLE_H__

#include <utility>

template <typename T1, typename T2, typename T3>
struct triple: public std::pair<T1,T2>
{
    typedef T3 third_type;

    T3 third;

    triple(): third(T3()) {}
    triple(const T1 &t1, const T2 &t2, const T3 &t3): std::pair<T1,T2>(t1,t2), third(t3) {}
    template<typename U1, typename U2, typename U3> triple(const triple<U1,U2,U3> &t): std::pair<U1,U2>(t.first,t.second), third(third) {}
};

template <typename T1, typename T2, typename T3>
inline bool operator == (const triple<T1,T2,T3> &x, const triple<T1,T2,T3> &y)
{
    return x.first == y.first && x.second == y.second && x.third == y.third;
}

template <typename T1, typename T2, typename T3>
inline bool operator < (const triple<T1,T2,T3> &x, const triple<T1,T2,T3> &y)
{
    return x.first < y.first ||
            !(y.first < x.first) && (x.second < y.second || !(y.second < x.second) && x.third < y.third);
}

template <typename T1, typename T2, typename T3>
inline bool operator != (const triple<T1,T2,T3> &x, const triple<T1,T2,T3> &y)
{
    return !(x == y);
}

template <typename T1, typename T2, typename T3>
inline bool operator > (const triple<T1,T2,T3> &x, const triple<T1,T2,T3> &y)
{
    return y < x;
}

template <typename T1, typename T2, typename T3>
inline bool operator <=(const triple<T1,T2,T3> &x, const triple<T1,T2,T3> &y)
{
    return !(y < x);
}

template <typename T1, typename T2, typename T3>
inline bool operator >= (const triple<T1,T2,T3> &x, const triple<T1,T2,T3> &y)
{
    return !(x < y);
}

template <typename T1, typename T2, typename T3>
inline triple<T1,T2,T3> make_triple(const T1 &t1, const T2 &t2, const T3 &t3)
{
    return triple<T1,T2,T3>(t1,t2,t3);
}

#endif // __TUPLE_H__
