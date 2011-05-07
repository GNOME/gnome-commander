/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2010 Piotr Eljasiak
    Copyleft      2010-2010 Guillaume Wardavoir

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

#ifndef __GCMDGTKFOLDVIEW_UTILS_H__
#define __GCMDGTKFOLDVIEW_UTILS_H__

#include    <glib.h>

//  ***************************************************************************
//
//  Some macros
//
//  ***************************************************************************
#define GCMD_STRINGIZE(a)   #a
#define GCMD_LABEL          __attribute__ ((unused)) asm("nop");

//  ***************************************************************************
//
//  Macros for inserting binary numbers in source code ( up to 32 bits )
//
//  endianness : b1 is lsb :
//
//	  MB32(b4,b3,b2,b1) = b1 + 2^8 * b2 + ... + 2^24 * b4
//
//  I know there are too many parenthesis, but gcc complains
//
//  ***************************************************************************

//
//  helper macros
//

//
//  GCMD_HEX_(token)
//
//  Convert a 8-binary-token to a 32 bits integer
//
//  b8_max													=
//	  11111111												->
//	  0x11111111LU											->
//	  binary 0001 0001 0001 0001 0001 0001 0001 0001
//
//	So b8_max fits in 32 bits
//
#define GCMD_HEX_(token)													\
	(guint32)(0x##token##LU)
//
// 8-bit conversion macro, takes a the result of GCMD_HEX_() as input
//
#define GCMD_B8_(x)															\
(																			\
	  ( (x & 0x00000001)   ? 0x01	: 0	)									\
	+ ( (x & 0x00000010)   ? 0x02	: 0	)									\
	+ ( (x & 0x00000100)   ? 0x04	: 0	)									\
	+ ( (x & 0x00001000)   ? 0x08	: 0	)									\
	+ ( (x & 0x00010000)   ? 0x10	: 0	)									\
	+ ( (x & 0x00100000)   ? 0x20	: 0	)									\
	+ ( (x & 0x01000000)   ? 0x40	: 0	)									\
	+ ( (x & 0x10000000)   ? 0x80	: 0	)									\
)
//
// The two above macros in one
//
#define GCMD_B8(b)															\
	( GCMD_B8_( GCMD_HEX_(b) ) )


//
//  32 bits
//
#define GCMD_B32(b4,b3,b2,b1)												\
(																			\
	(guint32)																\
		(																	\
				( ((guint32)GCMD_B8(b1))			)						\
			+   ( ((guint32)GCMD_B8(b2)) << 8		)						\
			+   ( ((guint32)GCMD_B8(b3)) << 16		)						\
			+   ( ((guint32)GCMD_B8(b4)) << 24		)						\
		)																	\
)


//  ***************************************************************************
//
//							GStruct
//
//  ***************************************************************************
//
//  I like & trust glib memory management.
//  This template is for avoiding overloading new / delete in all structs.
//
//  There are many GStruct<> template-constructors, for handling
//  the number of constructor parameters of any "T" ( I tried va_args but
//  didnt succeed ).
//
//  Exercice : add a template parameter N, representing the number of arguments
//  of T constructor, and autogenerate the GStruct<T,N> constructors
//  with recursive templates.
//
template <typename T> struct GcmdStruct : public T
{
    public:

   void*		operator new(size_t size)
                {
                    GcmdStruct<T>* g = g_try_new0(GcmdStruct<T>, 1);

                    if ( !g )
                        g_critical("GStruct<%s>::new():g_try_new0 failed", GCMD_STRINGIZE(T));

                    return g;
                }

    void		operator delete (void *p)
                {
                    g_free(p);
                }

    GcmdStruct() : T() {}
    template <typename U1> GcmdStruct(U1 u1) : T(u1) {}
    template <typename U1, typename U2> GcmdStruct(U1 u1, U2 u2) : T(u1, u2) {}
    template <typename U1, typename U2, typename U3> GcmdStruct(U1 u1, U2 u2, U3 u3) : T(u1, u2, u3) {}
    template <typename U1, typename U2, typename U3, typename U4> GcmdStruct(U1 u1, U2 u2, U3 u3, U4 u4) : T(u1, u2, u3, u4) {}
    template <typename U1, typename U2, typename U3, typename U4, typename U5> GcmdStruct(U1 u1, U2 u2, U3 u3, U4 u4, U5 u5) : T(u1, u2, u3, u4, u5) {}
    template <typename U1, typename U2, typename U3, typename U4, typename U5, typename U6> GcmdStruct(U1 u1, U2 u2, U3 u3, U4 u4, U5 u5, U6 u6) : T(u1, u2, u3, u4, u5, u6) {}
    template <typename U1, typename U2, typename U3, typename U4, typename U5, typename U6, typename U7> GcmdStruct(U1 u1, U2 u2, U3 u3, U4 u4, U5 u5, U6 u6, U7 u7) : T(u1, u2, u3, u4, u5, u6, u7) {}

    virtual     ~GcmdStruct<T>() {}
};

//
//  The GStruct<> struct comes with a convenience macro for
//  creating new instances of GStruct<> - and its free of charge.
//
#define GCMD_STRUCT_NEW_CAST( type, ... )   (type*) ( new GcmdStruct<type>(__VA_ARGS__) )
#define GCMD_STRUCT_NEW( type, ... )                ( new GcmdStruct<type>(__VA_ARGS__) )

//  ***************************************************************************
//
//								Logging
//
//  ***************************************************************************
enum eLogChannel
{
    // common
    eLogGcmd        =  0,
    eLogLeaks       =  1,

    // control
    eLogFifo        =  5,
    eLogMsg         =  6,

    // model
    eLogFiles       = 10,
    eLogRefresh     = 11,
    eLogSort        = 12,
    eLogEnumerate   = 13,
    eLogCheck       = 14,
    eLogExpand      = 15,
    eLogMonitor     = 16,

    eLogTreeNode    = 20,
    eLogTreeBlock   = 21,
    eLogTreeStore   = 22
};

#define GCMD_LOG_HELPER(_c, _f, ...) (sLogger.log_function(_c, _f))(sLogger.header(_c), __VA_ARGS__);
//.....................................................................
#define GCMD_INF(...)       GCMD_LOG_HELPER(eLogGcmd,       Logger::eLogInf,  __VA_ARGS__)
#define GCMD_WNG(...)       GCMD_LOG_HELPER(eLogGcmd,       Logger::eLogWng,  __VA_ARGS__)
#define GCMD_ERR(...)       GCMD_LOG_HELPER(eLogGcmd,       Logger::eLogErr,  __VA_ARGS__)
//  ---------------------------------------------------------------------------
#define LEAKS_INF(...)      GCMD_LOG_HELPER(eLogLeaks,      Logger::eLogInf,  __VA_ARGS__)
//  ---------------------------------------------------------------------------
#define FIFO_INF(...)       GCMD_LOG_HELPER(eLogFifo,       Logger::eLogInf,  __VA_ARGS__)
#define FIFO_WNG(...)       GCMD_LOG_HELPER(eLogFifo,       Logger::eLogWng,  __VA_ARGS__)
#define FIFO_ERR(...)       GCMD_LOG_HELPER(eLogFifo,       Logger::eLogErr,  __VA_ARGS__)
//  ---------------------------------------------------------------------------
#define MSG_INF(...)        GCMD_LOG_HELPER(eLogMsg,        Logger::eLogInf,  __VA_ARGS__)
#define MSG_WNG(...)        GCMD_LOG_HELPER(eLogMsg,        Logger::eLogWng,  __VA_ARGS__)
#define MSG_ERR(...)        GCMD_LOG_HELPER(eLogMsg,        Logger::eLogErr,  __VA_ARGS__)
//  ---------------------------------------------------------------------------
#define FILES_INF(...)      GCMD_LOG_HELPER(eLogFiles,      Logger::eLogInf,  __VA_ARGS__)
#define FILES_WNG(...)      GCMD_LOG_HELPER(eLogFiles,      Logger::eLogWng,  __VA_ARGS__)
#define FILES_ERR(...)      GCMD_LOG_HELPER(eLogFiles,      Logger::eLogErr,  __VA_ARGS__)
//  ---------------------------------------------------------------------------
#define REFRESH_INF(...)    GCMD_LOG_HELPER(eLogRefresh,    Logger::eLogInf,  __VA_ARGS__)
#define REFRESH_WNG(...)    GCMD_LOG_HELPER(eLogRefresh,    Logger::eLogWng,  __VA_ARGS__)
#define REFRESH_ERR(...)    GCMD_LOG_HELPER(eLogRefresh,    Logger::eLogErr,  __VA_ARGS__)
#define REFRESH_TKI(...)    GCMD_LOG_HELPER(eLogRefresh,    Logger::eLogTki,  __VA_ARGS__)
#define REFRESH_TKW(...)    GCMD_LOG_HELPER(eLogRefresh,    Logger::eLogTkw,  __VA_ARGS__)
#define REFRESH_TKE(...)    GCMD_LOG_HELPER(eLogRefresh,    Logger::eLogTke,  __VA_ARGS__)
//  ---------------------------------------------------------------------------
#define SORT_INF(...)       GCMD_LOG_HELPER(eLogSort,       Logger::eLogInf,  __VA_ARGS__)
#define SORT_WNG(...)       GCMD_LOG_HELPER(eLogSort,       Logger::eLogWng,  __VA_ARGS__)
#define SORT_ERR(...)       GCMD_LOG_HELPER(eLogSort,       Logger::eLogErr,  __VA_ARGS__)
#define SORT_TKI(...)       GCMD_LOG_HELPER(eLogSort,       Logger::eLogTki,  __VA_ARGS__)
#define SORT_TKW(...)       GCMD_LOG_HELPER(eLogSort,       Logger::eLogTkw,  __VA_ARGS__)
#define SORT_TKE(...)       GCMD_LOG_HELPER(eLogSort,       Logger::eLogTke,  __VA_ARGS__)
//  ---------------------------------------------------------------------------
#define ENUMERATE_INF(...)  GCMD_LOG_HELPER(eLogEnumerate,  Logger::eLogInf,  __VA_ARGS__)
#define ENUMERATE_WNG(...)  GCMD_LOG_HELPER(eLogEnumerate,  Logger::eLogWng,  __VA_ARGS__)
#define ENUMERATE_ERR(...)  GCMD_LOG_HELPER(eLogEnumerate,  Logger::eLogErr,  __VA_ARGS__)
//  ---------------------------------------------------------------------------
#define CHECK_INF(...)      GCMD_LOG_HELPER(eLogCheck,      Logger::eLogInf,  __VA_ARGS__)
#define CHECK_WNG(...)      GCMD_LOG_HELPER(eLogCheck,      Logger::eLogWng,  __VA_ARGS__)
#define CHECK_ERR(...)      GCMD_LOG_HELPER(eLogCheck,      Logger::eLogErr,  __VA_ARGS__)
//  ---------------------------------------------------------------------------
#define EXPAND_INF(...)     GCMD_LOG_HELPER(eLogExpand,     Logger::eLogInf,  __VA_ARGS__)
#define EXPAND_WNG(...)     GCMD_LOG_HELPER(eLogExpand,     Logger::eLogWng,  __VA_ARGS__)
#define EXPAND_ERR(...)     GCMD_LOG_HELPER(eLogExpand,     Logger::eLogErr,  __VA_ARGS__)
//  ---------------------------------------------------------------------------
#define MONITOR_INF(...)    GCMD_LOG_HELPER(eLogMonitor,    Logger::eLogInf,  __VA_ARGS__)
#define MONITOR_WNG(...)    GCMD_LOG_HELPER(eLogMonitor,    Logger::eLogWng,  __VA_ARGS__)
#define MONITOR_ERR(...)    GCMD_LOG_HELPER(eLogMonitor,    Logger::eLogErr,  __VA_ARGS__)
//  ---------------------------------------------------------------------------
#define NODE_INF(...)       GCMD_LOG_HELPER(eLogTreeNode,   Logger::eLogInf,  __VA_ARGS__)
#define NODE_WNG(...)       GCMD_LOG_HELPER(eLogTreeNode,   Logger::eLogWng,  __VA_ARGS__)
#define NODE_ERR(...)       GCMD_LOG_HELPER(eLogTreeNode,   Logger::eLogErr,  __VA_ARGS__)
#define NODE_TKI(...)       GCMD_LOG_HELPER(eLogTreeNode,   Logger::eLogTki,  __VA_ARGS__)
#define NODE_TKW(...)       GCMD_LOG_HELPER(eLogTreeNode,   Logger::eLogTkw,  __VA_ARGS__)
#define NODE_TKE(...)       GCMD_LOG_HELPER(eLogTreeNode,   Logger::eLogTke,  __VA_ARGS__)
//  ---------------------------------------------------------------------------
#define BLOCK_INF(...)      GCMD_LOG_HELPER(eLogTreeBlock,  Logger::eLogInf,  __VA_ARGS__)
#define BLOCK_WNG(...)      GCMD_LOG_HELPER(eLogTreeBlock,  Logger::eLogWng,  __VA_ARGS__)
#define BLOCK_ERR(...)      GCMD_LOG_HELPER(eLogTreeBlock,  Logger::eLogErr,  __VA_ARGS__)
//  ---------------------------------------------------------------------------
#define STORE_INF(...)      GCMD_LOG_HELPER(eLogTreeStore,  Logger::eLogInf,  __VA_ARGS__)
#define STORE_WNG(...)      GCMD_LOG_HELPER(eLogTreeStore,  Logger::eLogWng,  __VA_ARGS__)
#define STORE_ERR(...)      GCMD_LOG_HELPER(eLogTreeStore,  Logger::eLogErr,  __VA_ARGS__)

//*****************************************************************************
//
//	  GtkTreeIter macros
//
//*****************************************************************************

#define GCMD_ITER_COPY(dest, src)											\
	(dest)->stamp			= (src)->stamp;									\
	(dest)->user_data		= (src)->user_data;								\
	(dest)->user_data2		= (src)->user_data2;							\
	(dest)->user_data3		= (src)->user_data3;


#endif //__GCMDGTKFOLDVIEW_UTILS_H__
