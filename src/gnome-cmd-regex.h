/** 
 * @file gnome-cmd-regex.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2017 Uwe Scholz\n
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

#ifndef __GNOME_CMD_REGEX_H__
#define __GNOME_CMD_REGEX_H__

#include <glib.h>

#include "utils.h"

namespace GnomeCmd
{
    struct FindPattern
    {
        std::string pattern;
        gboolean match_case;

        FindPattern(): match_case(FALSE)    {}
        FindPattern(const gchar *from, gboolean case_sensitive): match_case(case_sensitive)  {  if (from)  pattern = from;  }
        FindPattern(const std::string &from, gboolean case_sensitive): pattern(from), match_case(case_sensitive)           {}
        FindPattern(const FindPattern&) = default;
    };

    struct ReplacePattern: virtual FindPattern
    {
        std::string replacement;

        ReplacePattern()    {}
        ReplacePattern(const gchar *from, const gchar *to, gboolean case_sensitive);
        ReplacePattern(const std::string &from, const std::string &to, gboolean case_sensitive): FindPattern(from,case_sensitive), replacement(to) {}
    };

    class Regex: virtual public FindPattern
    {
      protected:
        GRegex *re;
        gboolean malformed_pattern;

        void compile_pattern();

        Regex();
        Regex(const gchar *from, gboolean case_sensitive): FindPattern(from,case_sensitive)         {  compile_pattern();  }
        Regex(const std::string &from, gboolean case_sensitive): FindPattern(from,case_sensitive)   {  compile_pattern();  }
        ~Regex();

        void assign(const gchar *from, gboolean case_sensitive);
        void assign(const std::string &from, gboolean case_sensitive);

      public:

        operator gboolean () const              {  return !malformed_pattern;   }
    };

    class RegexFind: public Regex
    {
      public:

        RegexFind();
        RegexFind(const gchar *from, gboolean case_sensitive);
        RegexFind(const std::string &from, gboolean case_sensitive);

        void assign(const gchar *from, gboolean case_sensitive)           {  Regex::assign(from, case_sensitive);  }
        void assign(const std::string &from, gboolean case_sensitive)     {  Regex::assign(from, case_sensitive);  }
        gboolean match(const gchar *s);
        gboolean match(const std::string &s) {  return match(s.c_str());  }
        int start() const;
        int end() const;
        int length() const                   {  return end() - start();   }
    };

    struct RegexReplace: ReplacePattern, Regex
    {
        RegexReplace()       {}
        RegexReplace(const gchar *from, const gchar *to, gboolean case_sensitive): FindPattern(from,case_sensitive), ReplacePattern(from,to,case_sensitive), Regex(from,case_sensitive) {}
        RegexReplace(const std::string &from, const std::string &to, gboolean case_sensitive): FindPattern(from,case_sensitive), ReplacePattern(from,to,case_sensitive), Regex(from,case_sensitive) {}

        void assign(const gchar *from, const gchar *to, gboolean case_sensitive);
        void assign(const std::string &from, const std::string &to, gboolean case_sensitive)    {  replacement = to; Regex::assign(from, case_sensitive);   }
        gchar *replace(const gchar *s);
        gchar *replace(const std::string &s)    {  return replace(s.c_str());  }
    };

    inline ReplacePattern::ReplacePattern(const gchar *from, const gchar *to, gboolean case_sensitive): FindPattern(from,case_sensitive)
    {
        if (to)  replacement = to;
    }

    inline Regex::Regex(): malformed_pattern(TRUE)
    {
        re = NULL;
    }

    inline Regex::~Regex()
    {
        g_regex_unref (re);
    }

    inline void Regex::assign(const gchar *from, gboolean case_sensitive)
    {
        if (re)
            g_regex_unref (re);
        match_case = case_sensitive;

        if (from && *from)
            pattern = from;
        else
            pattern.clear();

        compile_pattern();
    }

    inline void Regex::assign(const std::string &from, gboolean case_sensitive)
    {
        if (re)
            g_regex_unref (re);
        match_case = case_sensitive;
        pattern = from;

        compile_pattern();
    }

    inline void Regex::compile_pattern()
    {
        GError *error = NULL;
        re = g_regex_new (pattern.c_str(), GRegexCompileFlags(match_case ? G_REGEX_OPTIMIZE : G_REGEX_OPTIMIZE | G_REGEX_CASELESS), G_REGEX_MATCH_NOTEMPTY, &error);
        malformed_pattern = pattern.empty() || error;
        if (error)  g_error_free (error);
    }

    inline RegexFind::RegexFind()
    {
    }

    inline RegexFind::RegexFind(const gchar *from, gboolean case_sensitive): Regex(from,case_sensitive)
    {
    }

    inline RegexFind::RegexFind(const std::string &from, gboolean case_sensitive): Regex(from,case_sensitive)
    {
    }

    inline int RegexFind::start() const
    {
        return 0;
    }

    inline int RegexFind::end() const
    {
        return 0;
    }

    inline void RegexReplace::assign(const gchar *from, const gchar *to, gboolean case_sensitive)
    {
        if (to && *to)
            replacement = to;
        else
            replacement.clear();

        return Regex::assign(from, case_sensitive);
    }

    inline gchar *RegexReplace::replace(const gchar *s)
    {
        return g_regex_replace (re, s, -1, 0, replacement.c_str(), G_REGEX_MATCH_NOTEMPTY, NULL);
    }
}

#endif // __GNOME_CMD_REGEX_H__
