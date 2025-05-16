/**
 * @file searcher.cc
 * @brief Part of GNOME Commander - A GNOME based file manager
 *
 * @copyright (C) 2006 Assaf Gordon\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2024 Uwe Scholz\n
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
 *
 */

#include <config.h>
#include <string.h>
#include "libgviewer.h"
#include "bm_chartype.h"
#include "bm_byte.h"

using namespace std;


extern "C" void g_viewer_searcher_init (GViewerSearcher *);
extern "C" void g_viewer_searcher_finalize (GViewerSearcher *);

enum SearchMode
{
    TEXT,
    HEX
};

struct GViewerSearcherPrivate
{
    // gint-s for the indicator's atomic operations
    gint abort_indicator;

    GVInputModesData *imd;
    offset_type start_offset;
    offset_type max_offset;
    guint update_interval;

    offset_type search_result;
    gboolean search_forward;
    GViewerBMChartypeData *ct_data;
    GViewerBMChartypeData *ct_reverse_data;

    GViewerBMByteData *b_data;
    GViewerBMByteData *b_reverse_data;

    enum SearchMode searchmode;

    GnomeCmdCallback<guint> progress;
    gpointer progress_user_data;
};


static GViewerSearcherPrivate *searcher_priv (GViewerSearcher *obj)
{
    return (GViewerSearcherPrivate *) g_object_get_data (G_OBJECT (obj), "priv");
}


void g_viewer_searcher_init(GViewerSearcher *obj)
{
    auto priv = g_new0 (GViewerSearcherPrivate, 1);
    g_object_set_data_full (G_OBJECT (obj), "priv", priv, g_free);
}


void g_viewer_searcher_finalize (GViewerSearcher *cobj)
{
    auto priv = searcher_priv (cobj);

    g_clear_pointer (&priv->ct_data, free_bm_chartype_data);
    g_clear_pointer (&priv->ct_reverse_data, free_bm_chartype_data);
    g_clear_pointer (&priv->b_data, free_bm_byte_data);
    g_clear_pointer (&priv->b_reverse_data, free_bm_byte_data);
}


GViewerSearcher *g_viewer_searcher_new()
{
    GViewerSearcher *obj = G_VIEWERSEARCHER (g_object_new (G_TYPE_VIEWERSEARCHER, nullptr));

    return obj;
}


void g_viewer_searcher_abort(GViewerSearcher *src)
{
    g_return_if_fail (src != nullptr);
    auto priv = searcher_priv (src);

    g_atomic_int_set (&priv->abort_indicator, 1);
}


offset_type g_viewer_searcher_get_search_result(GViewerSearcher *src)
{
    g_return_val_if_fail (src != nullptr, 0);
    auto priv = searcher_priv (src);

    return priv->search_result;
}


void g_viewer_searcher_setup_new_text_search(GViewerSearcher *srchr,
                                             GVInputModesData *imd,
                                             offset_type start_offset,
                                             offset_type max_offset,
                                             const gchar *text,
                                             gboolean case_sensitive)
{
    g_return_if_fail (srchr != nullptr);
    auto priv = searcher_priv (srchr);

    g_return_if_fail (imd != nullptr);
    g_return_if_fail (start_offset<=max_offset);

    g_return_if_fail (text != nullptr);
    g_return_if_fail (strlen(text)>0);

    priv->imd = imd;
    priv->start_offset = start_offset;
    priv->max_offset = max_offset;

    priv->update_interval = priv->max_offset>1000 ? priv->max_offset/1000 : 10;

    priv->ct_data = create_bm_chartype_data(text, case_sensitive);
    g_return_if_fail (priv->ct_data != nullptr);

    gchar *rev_text = g_utf8_strreverse(text, -1);
    priv->ct_reverse_data = create_bm_chartype_data(rev_text, case_sensitive);
    g_free (rev_text);
    g_return_if_fail (priv->ct_reverse_data != nullptr);

    priv->searchmode = TEXT;
}


void g_viewer_searcher_setup_new_hex_search(GViewerSearcher *srchr,
                                            GVInputModesData *imd,
                                            offset_type start_offset,
                                            offset_type max_offset,
                                            const guint8 *buffer, guint buflen)
{
    g_return_if_fail (srchr != nullptr);
    auto priv = searcher_priv (srchr);

    g_return_if_fail (imd != nullptr);
    g_return_if_fail (start_offset<=max_offset);

    g_return_if_fail (buffer != nullptr);
    g_return_if_fail (buflen>0);

    priv->imd = imd;
    priv->start_offset = start_offset;
    priv->max_offset = max_offset;

    if (priv->max_offset>1000)
        priv->update_interval = priv->max_offset/1000;
    else
        priv->update_interval = 10;

    priv->b_data = create_bm_byte_data(buffer, buflen);
    g_return_if_fail (priv->b_data != nullptr);

    guint8 *rev_buffer = mem_reverse(buffer, buflen);
    priv->b_reverse_data = create_bm_byte_data(rev_buffer, buflen);
    g_free (rev_buffer);
    g_return_if_fail (priv->b_reverse_data != nullptr);

    priv->searchmode = HEX;
}


static void update_progress_indicator (GViewerSearcher *src, offset_type pos)
{
    auto priv = searcher_priv (src);

    gdouble d = (pos*1000.0) / priv->max_offset;

    priv->progress ((gint) d, priv->progress_user_data);
}


static gboolean check_abort_request (GViewerSearcher *src)
{
    auto priv = searcher_priv (src);
    return g_atomic_int_get (&priv->abort_indicator)!=0;
}


static gboolean search_hex_forward (GViewerSearcher *src)
{
    auto priv = searcher_priv (src);

    offset_type m, n, j;
    int i;
    gboolean found = FALSE;
    guint8 value;
    GViewerBMByteData *data = priv->b_data;

    m = data->pattern_len;
    n = priv->max_offset;
    j = priv->start_offset;
    int update_counter = priv->update_interval;

    while (j <= n - m)
    {
        for (i = m - 1; i >= 0; --i)
        {
            value = (guint8) gv_input_mode_get_raw_byte(priv->imd, i+j);
            if (data->pattern[i] != value)
                break;
        }

        if (i < 0)
        {
            priv->search_result = j;
            j ++;
            found = TRUE;
            break;
        }

        j += MAX((offset_type)data->good[i], data->bad[value] - m + 1 + i);

        if (--update_counter==0)
        {
            update_progress_indicator(src, j);
            update_counter = priv->update_interval;
        }

        if (check_abort_request(src))
            break;
    }

    // Store the current offset, we'll use it if the user chooses "find next"
    if (found)
        priv->start_offset = j;

    return found;
}


static gboolean search_hex_backward (GViewerSearcher *src)
{
    auto priv = searcher_priv (src);

    offset_type m, j;
    int i;
    gboolean found = FALSE;
    int update_counter;
    guint8 value;
    GViewerBMByteData *data;

    data = priv->b_reverse_data;

    m = data->pattern_len;
    j = priv->start_offset;
    update_counter = priv->update_interval;

    if (j>0)
        j--;
    while (j >= m)
    {
        for (i = m - 1; i >= 0; --i)
        {
            value = (guint8) gv_input_mode_get_raw_byte(priv->imd, j-i);
            if (data->pattern[i] != value)
                break;
        }

        if (i < 0)
        {
            priv->search_result = j;
            found = TRUE;
            break;
        }

        j -= MAX((offset_type)data->good[i], data->bad[value] - m + 1 + i);

        if (--update_counter==0)
        {
            update_progress_indicator(src, j);
            update_counter = priv->update_interval;
        }

        if (check_abort_request(src))
            break;
    }

    // Store the current offset, we'll use it if the user chooses "find next"
    if (found)
        priv->start_offset = j;

    return found;
}


static gboolean search_text_forward (GViewerSearcher *src)
{
    auto priv = searcher_priv (src);

    offset_type m, n, j;
    int i;
    gboolean found = FALSE;
    char_type value;
    GViewerBMChartypeData *data = priv->ct_data;

    m = data->pattern_len;
    n = priv->max_offset;
    j = priv->start_offset;
    int update_counter = priv->update_interval;

    while (j <= n - m)
    {
        offset_type t, delta;

        delta = m - 1;
        t = j;
        while (delta--)
            t = gv_input_get_next_char_offset(priv->imd, t);
        for (i = m - 1; i >= 0; --i)
        {
            value = gv_input_mode_get_utf8_char(priv->imd, t);
            t = gv_input_get_previous_char_offset(priv->imd, t);
            if (!bm_chartype_equal(data, i, value))
                break;
        }

        // Found a match
        if (i < 0)
        {
            priv->search_result = j;

            // Advance the current offset, from which "find next" will begin
            j = gv_input_get_next_char_offset(priv->imd, j);

            found = TRUE;
            break;
        }

        // didn't find a match, calculate new index
        delta = bm_chartype_get_advancement(data, i, value);
        while (delta--)
            j = gv_input_get_next_char_offset(priv->imd, j);

        if (--update_counter==0)
        {
            update_progress_indicator(src, j);
            update_counter = priv->update_interval;
        }

        if (check_abort_request(src))
            break;
    }

    // Store the current offset, we'll use it if the user chooses "find next"
    if (found)
        priv->start_offset = j;

    return found;
}


static gboolean search_text_backward (GViewerSearcher *src)
{
    auto priv = searcher_priv (src);

    offset_type m, j;
    int i;
    gboolean found = FALSE;
    int update_counter;
    char_type value;
    GViewerBMChartypeData *data;

    data = priv->ct_reverse_data;

    m = data->pattern_len;
    j = priv->start_offset;

    update_counter = priv->update_interval;

    j = gv_input_get_previous_char_offset(priv->imd, j);
    while (j >= m)
    {
        offset_type t, delta;

        delta = m - 1;
        t = j;
        while (delta--)
            t = gv_input_get_previous_char_offset(priv->imd, t);

        for (i = m - 1; i >= 0; --i)
        {
            value = gv_input_mode_get_utf8_char(priv->imd, t);
            t = gv_input_get_next_char_offset(priv->imd, t);
            if (!bm_chartype_equal(data, i, value))
                break;
        }

        // Found a match
        if (i < 0)
        {
            priv->search_result = gv_input_get_next_char_offset(priv->imd, j);
            found = TRUE;
            break;
        }

        // didn't find a match, calculate new index
        delta = bm_chartype_get_advancement(data, i, value);
        while (delta--)
            j = gv_input_get_previous_char_offset(priv->imd, j);

        if (--update_counter==0)
        {
            update_progress_indicator(src, j);
            update_counter = priv->update_interval;
        }

        if (check_abort_request(src))
            break;
    }

    // Store the current offset, we'll use it if the user chooses "find next"
    if (found)
        priv->start_offset = j;

    return found;
}


gboolean g_viewer_searcher_search (GViewerSearcher *src, gboolean forward, GnomeCmdCallback<guint> progress, gpointer user_data)
{
    auto priv = searcher_priv (src);
    g_return_val_if_fail (priv->imd != nullptr, FALSE);

    priv->search_forward = forward;
    priv->progress = progress;
    priv->progress_user_data = user_data;

    g_atomic_int_set (&priv->abort_indicator, 0);
    update_progress_indicator(src, priv->start_offset);

    gboolean found;

    if (priv->searchmode==TEXT)
        found = (priv->search_forward) ? search_text_forward(src) : search_text_backward(src);
    else
        found = (priv->search_forward) ? search_hex_forward(src) : search_hex_backward(src);

    return found;
}
