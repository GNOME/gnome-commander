/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2011 Piotr Eljasiak
    Copyleft      2010-2011 Guillaume Wardavoir

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

#ifndef __GCMDGTKFOLDVIEW_LOGGER_H__
#define __GCMDGTKFOLDVIEW_LOGGER_H__

#include    "gnome-cmd-foldview-utils.h"

#include    <glib.h>

//  ***************************************************************************
//								Logging
//  ***************************************************************************

//  ===========================================================================
//	Logger
//  ===========================================================================
struct Logger
{
    //=========================================================================
    //	Types
    //=========================================================================
    public:
    typedef void (*LogFunction)(const char* _header, const char* _format, ...);
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    public:
    enum eLogFlavour
    {
        eLogInf         = 0,
        eLogWng         = 1,
        eLogErr         = 2,

        eLogTki         = 3,
        eLogTkw         = 4,
        eLogTke         = 5
    };
    //=========================================================================
    //	Structs
    //=========================================================================

    private:
    struct Channel
    {
        private:
        gint            a_index;
        LogFunction     a_log_function[6];
        gchar   *       d_header;

        public:
        Channel()
        {
            a_index                 = -1;
            a_log_function[eLogInf] = &NoLog;
            a_log_function[eLogWng] = &NoLog;
            a_log_function[eLogErr] = &NoLog;
            a_log_function[eLogTki] = &NoLog;
            a_log_function[eLogTkw] = &NoLog;
            a_log_function[eLogTke] = &NoLog;
            d_header                = NULL;
        }
        ~Channel()
        {
            if ( d_header )
                g_free(d_header);
        }

        inline  void            set_header(const gchar* _header)
                                {
                                    d_header = g_strdup(_header);
                                }
        inline  const gchar*    get_header()
                                {
                                    return d_header;
                                }
        inline  LogFunction     log_function(eLogFlavour _flavour)
                                {
                                    return a_log_function[_flavour];
                                }
        inline  void            unmute(eLogFlavour _flavour)
                                {
                                    a_log_function[_flavour] = Log_function[_flavour];
                                }
        inline  void            mute(eLogFlavour _flavour)
                                {
                                    a_log_function[_flavour] = &NoLog;
                                }
    };
    //=========================================================================
    //	New, ...
    //=========================================================================
    public:
    Logger(gint _channel_card)
    {
        a_channel_card  = _channel_card;
        d_channel       = (Channel**)g_try_malloc0( sizeof(Channel*) * _channel_card );
    }
    ~Logger()
    {
        gint i = 0;
        //.....................................................................
        for( i = 0 ; i != channel_card() ; i++ )
            if ( channel(i) )
                delete (GcmdStruct<Channel>*)channel(i);

        g_free(d_channel);
    }

    //=========================================================================
    //	Members
    //=========================================================================
    private:
    static  LogFunction     Log_function[6];

            gint            a_channel_card;
            Channel     **  d_channel;
    //=========================================================================
    //	Methods
    //=========================================================================
    private:
    static          void Inf    (const char* _header, const char* _format, ...);
    static          void Wng    (const char* _header, const char* _format, ...);
    static          void Err    (const char* _header, const char* _format, ...);
    static          void Tki    (const char* _header, const char* _format, ...);
    static          void Tkw    (const char* _header, const char* _format, ...);
    static          void Tke    (const char* _header, const char* _format, ...);
    static  inline  void NoLog  (const char* _header, const char* _format, ...)   {}

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    public:

    inline  LogFunction     log_function(gint _channel, eLogFlavour _flavour)
    {
        return channel(_channel)->log_function(_flavour);
    }

    inline  gint            channel_card()
    {
        return a_channel_card;
    }

    inline  Channel *       channel(gint _channel)
    {
        return d_channel[_channel];
    }

    inline  const gchar *   header(gint _channel)
    {
        return channel(_channel)->get_header();
    }

    inline  void            channel_mute    (gint _channel, eLogFlavour _flavour)   { channel(_channel)->mute(_flavour);   }
    inline  void            channel_unmute  (gint _channel, eLogFlavour _flavour)   { channel(_channel)->unmute(_flavour); }

    inline  gboolean        channel_create(gint _channel, const gchar* _header, guint8 _b8)
    {
        g_return_val_if_fail( _channel >=  0,               FALSE );
        g_return_val_if_fail( _channel <= channel_card(),   FALSE );
        g_return_val_if_fail( ! channel(_channel),          FALSE );

        d_channel[_channel] = GCMD_STRUCT_NEW(Channel);
        d_channel[_channel]->set_header(_header);

        if ( _b8 & 0x20 )   d_channel[_channel]->unmute(eLogInf);
        if ( _b8 & 0x10 )   d_channel[_channel]->unmute(eLogWng);
        if ( _b8 & 0x08 )   d_channel[_channel]->unmute(eLogErr);
        if ( _b8 & 0x04 )   d_channel[_channel]->unmute(eLogTki);
        if ( _b8 & 0x02 )   d_channel[_channel]->unmute(eLogTkw);
        if ( _b8 & 0x01 )   d_channel[_channel]->unmute(eLogTke);

        return TRUE;
    }

};



#endif //__GCMDGTKFOLDVIEW_LOGGER_H__
