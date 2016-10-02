/** 
 * @file gnome-cmd-xml-config.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2016 Uwe Scholz\n
 *
 * @copyright This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * @copyright This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * @copyright You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

//////////////////////////////////////////////////////////////////////
//
// xmlparser.h: interface & implementation for the XMLStream class.
//
// Author: Oboltus, December 2003
//
// This code is provided "as is", with absolutely no warranty expressed
// or implied. Any use is at your own risk.
//
//////////////////////////////////////////////////////////////////////

#ifndef __GNOME_CMD_XML_CONFIG_H__
#define __GNOME_CMD_XML_CONFIG_H__

#include <stack>
#include <string>
#include <sstream>

namespace XML
{
    class xstream
    {
      public:

        enum {versionMajor=1, versionMinor=0};              // XML version constants

        struct Controller
        {
            enum what_type {whatTag, whatTagEnd, whatAttribute, whatCharData, whatComment};

            what_type what;
            std::string str;

            Controller(const Controller &c) : what(c.what), str(c.str) {}
            explicit Controller(const what_type _what) : what(_what)   {}

            // use template constructor because string field <str> may be initialized from different sources: char*, std::string etc
            template <typename T>
            Controller(const what_type _what, const T &_str) : what(_what), str(_str) {}
        };

        // xstream refers std::ostream object to perform actual output operations
        explicit xstream(std::ostream &_s) : s(_s), state(stateNone)
        {
            s << "<?xml version=\"" << versionMajor << '.' << versionMinor << "\" encoding=\"UTF-8\"?>";
        }

        // Before destroying check whether all the open tags are closed
        ~xstream()
        {
            if (state==stateTagName)
            {
                s << "/>";
                state = stateNone;
            }

            while (tags.size())
                endTag(tags.top().first);

            g_free(escaped_text);
            escaped_text = NULL;
        }

        // default behaviour - delegate object output to std::stream
        template <typename T>
        xstream &operator << (const T &value)
        {
            if (state==stateTagName)
                tagName << value;
            s << value;

            return *this;
        }

        // this is the main working horse and it's long a little
        xstream &operator << (const Controller &controller)
        {
            switch (controller.what)
            {
                case Controller::whatTag:
                    closeTagStart();
                    s << '\n' << tabs() << '<';
                    if (controller.str.empty())
                    {
                        clearTagName();
                        state = stateTagName;
                    }
                    else
                    {
                        s << controller.str;
                        tags.push(make_pair(controller.str,false));
                        state = stateTag;
                    }
                    break; // Controller::whatTag

                case Controller::whatTagEnd:
                    endTag(controller.str);
                    break; // Controller::whatTagEnd

                case Controller::whatAttribute:
                    switch (state)
                    {
                        case stateTagName:
                            tags.push(make_pair(tagName.str(),false));
                            break;

                        case stateAttribute:
                            s << '"';
                            break;

                        default:
                            break;
                    }

                    if (stateNone!=state)
                    {
                        s << ' ' << controller.str << "=\"";
                        state = stateAttribute;
                    }
                    // else throw some error - unexpected attribute (out of any tag)

                    break; // Controller::whatAttribute

                case Controller::whatCharData:
                    closeTagStart();
                    tags.top().second = true;
                    state = stateNone;
                    break; // Controller::whatCharData

                case Controller::whatComment:
                    s << "\n<!-- " << controller.str << " -->";
                    break; // Controller::whatComment
            }

            return *this;
        }

      private:

        enum state_type {stateNone, stateTag, stateAttribute, stateTagName};    // state of the stream

        typedef std::stack<std::pair<std::string, bool> > tag_stack_type;       // tag name stack

        std::ostream &s;
        state_type state;
        tag_stack_type tags;
        std::ostringstream tagName;

        static gchar *escaped_text;

        const char *tabs(unsigned offset=0) const
        {
            static std::string tabs(32,'\t');

            if (tabs.size() <= tags.size())
                tabs.append(32,'\t');

            return tabs.c_str() + tabs.size() - tags.size() + offset;
        }

        // I don't know any way easier (legal) to clear std::stringstream...
        void clearTagName()
        {
            tagName.rdbuf()->str(std::string());   //  FIXME:  tagName.str(std::string());  ??
        }

        void closeTagStart(bool self_closed=false)    // Close current tag
        {
            if (state==stateTagName)
                tags.push(make_pair(tagName.str(),false));

            switch (state)        // note: absence of 'break's is not an error
            {
                case stateAttribute:
                    s << '"';

                case stateTagName:
                case stateTag:
                    if (self_closed)
                        s << "/>";
                    else
                        s << '>';

                default:
                    break;
            }
        }

        void endTag(const std::string &tag)    // Close tag (may be with closing all of its children)
        {
            bool brk = false;

            while (tags.size() && !brk)
            {
                if (state==stateNone)
                {
                    if (!tags.top().second)
                        s << '\n' << tabs(1);
                    s << "</" << tags.top().first << '>';
                }
                else
                {
                    closeTagStart(true);
                    state = stateNone;
                }
                brk = tag.empty() || tag==tags.top().first;
                tags.pop();
            }
        }

        friend const gchar *escape(const gchar *s);
        friend const gchar *escape(const std::string &s);
    };

    inline const xstream::Controller tag()
    {
        return xstream::Controller(xstream::Controller::whatTag);
    }

    inline const xstream::Controller tag(const char *tag_name)
    {
        return xstream::Controller(xstream::Controller::whatTag, tag_name);
    }

    inline const xstream::Controller endtag()
    {
        return xstream::Controller(xstream::Controller::whatTagEnd);
    }

    inline const xstream::Controller endtag(const char *tag_name)
    {
        return xstream::Controller(xstream::Controller::whatTagEnd, tag_name);
    }

    inline const xstream::Controller attr(const char *attr_name)
    {
        return xstream::Controller(xstream::Controller::whatAttribute, attr_name);
    }

    inline const xstream::Controller chardata()
    {
        return xstream::Controller(xstream::Controller::whatCharData);
    }

    inline const xstream::Controller comment(const char *text)
    {
        return xstream::Controller(xstream::Controller::whatComment, text);
    }

    inline const gchar *escape(const gchar *s)
    {
        g_free (xstream::escaped_text);
        xstream::escaped_text = g_markup_escape_text (s, -1);
        return xstream::escaped_text;
    }

    inline const gchar *escape(const std::string &s)
    {
        return escape(s.c_str());
    }
}

struct GnomeCmdData;

gboolean gnome_cmd_xml_config_parse (const gchar *xml, gsize xml_len, GnomeCmdData &cfg);
gboolean gnome_cmd_xml_config_load (const gchar *path, GnomeCmdData &cfg);

#endif // __GNOME_CMD_XML_CONFIG_H__
