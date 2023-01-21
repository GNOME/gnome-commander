/**
 * @file searcher.cc
 * @brief Part of GNOME Commander - A GNOME based file manager
 *
 * @copyright (C) 2006 Assaf Gordon\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2023 Uwe Scholz\n
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


static void g_viewer_searcher_class_init(GViewerSearcherClass *klass);
static void g_viewer_searcher_init(GViewerSearcher *sp);
static void g_viewer_searcher_finalize(GObject *object);

enum SearchMode
{
    TEXT,
    HEX
};

struct GViewerSearcherPrivate
{
    // gint-s for the indicator's atomic operations
    gint abort_indicator;
    gint completed_indicator;
    gint progress_value;

    GThread *search_thread;

    GVInputModesData *imd;
    offset_type start_offset;
    offset_type max_offset;
    guint update_interval;

    offset_type search_result;
    gboolean search_reached_end;
    gboolean search_forward;
    GViewerBMChartypeData *ct_data;
    GViewerBMChartypeData *ct_reverse_data;

    GViewerBMByteData *b_data;
    GViewerBMByteData *b_reverse_data;

    enum SearchMode searchmode;
};

enum GViewerSearcherSignalType
{
    // Place Signal Types Here
    SIGNAL_TYPE_EXAMPLE,
    LAST_SIGNAL
};

/*static guint g_viewer_searcher_signals[LAST_SIGNAL] = { 0 };*/
static GObjectClass *parent_class = nullptr;


GType g_viewer_searcher_get_type()
{
    static GType type = 0;

    if (type == 0)
    {
        static const GTypeInfo our_info = {
            sizeof (GViewerSearcherClass),
            nullptr,
            nullptr,
            (GClassInitFunc) g_viewer_searcher_class_init,
            nullptr,
            nullptr,
            sizeof (GViewerSearcher),
            0,
            (GInstanceInitFunc) g_viewer_searcher_init,
        };

        type = g_type_register_static (G_TYPE_OBJECT, "GViewerSearcher", &our_info, (GTypeFlags) 0);
    }

    return type;
}


static void g_viewer_searcher_class_init(GViewerSearcherClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    parent_class = (GObjectClass *) g_type_class_peek_parent(klass);
    object_class->finalize = g_viewer_searcher_finalize;

    /* Create signals here:
       g_viewer_searcher_signals[SIGNAL_TYPE_EXAMPLE] = g_signal_new(...)
     */
}


static void g_viewer_searcher_init(GViewerSearcher *obj)
{
    obj->priv = g_new0 (GViewerSearcherPrivate, 1);
    // Initialize private members, etc.

    // obj->priv->abort_indicator = 0;
    // obj->priv->completed_indicator = 0;
    // obj->priv->progress_value = 0;

    // obj->priv->search_thread = nullptr;
}


static void g_viewer_searcher_finalize(GObject *object)
{
    GViewerSearcher *cobj = G_VIEWERSEARCHER (object);

    // Free private members, etc.
    if (cobj->priv)
    {
        if (cobj->priv->ct_data != nullptr)
        {
            free_bm_chartype_data(cobj->priv->ct_data);
            cobj->priv->ct_data = nullptr;
        }
        if (cobj->priv->ct_reverse_data != nullptr)
        {
            free_bm_chartype_data(cobj->priv->ct_reverse_data);
            cobj->priv->ct_reverse_data = nullptr;
        }
        if (cobj->priv->b_data != nullptr)
        {
            free_bm_byte_data(cobj->priv->b_data);
            cobj->priv->b_data = nullptr;
        }
        if (cobj->priv->b_reverse_data != nullptr)
        {
            free_bm_byte_data(cobj->priv->b_reverse_data);
            cobj->priv->b_reverse_data = nullptr;
        }
        g_free (cobj->priv);
        cobj->priv = nullptr;
    }

    G_OBJECT_CLASS (parent_class)->finalize(object);
}


GViewerSearcher *g_viewer_searcher_new()
{
    GViewerSearcher *obj = G_VIEWERSEARCHER (g_object_new (G_TYPE_VIEWERSEARCHER, nullptr));

    return obj;
}


gint * g_viewer_searcher_get_complete_indicator(GViewerSearcher *src)
{
    g_return_val_if_fail (src != nullptr, nullptr);
    g_return_val_if_fail (src->priv != nullptr, nullptr);

    return &src->priv->completed_indicator;
}


gint *g_viewer_searcher_get_abort_indicator(GViewerSearcher *src)
{
    g_return_val_if_fail (src != nullptr, nullptr);
    g_return_val_if_fail (src->priv != nullptr, nullptr);

    return &src->priv->abort_indicator;
}


gint *g_viewer_searcher_get_progress_indicator(GViewerSearcher *src)
{
    g_return_val_if_fail (src != nullptr, nullptr);
    g_return_val_if_fail (src->priv != nullptr, nullptr);

    return &src->priv->progress_value;
}


gboolean g_viewer_searcher_get_end_of_search(GViewerSearcher *src)
{
    g_return_val_if_fail (src != nullptr, TRUE);
    g_return_val_if_fail (src->priv != nullptr, TRUE);

    return src->priv->search_reached_end;
}


offset_type g_viewer_searcher_get_search_result(GViewerSearcher *src)
{
    g_return_val_if_fail (src != nullptr, 0);
    g_return_val_if_fail (src->priv != nullptr, 0);

    return src->priv->search_result;
}


void g_viewer_searcher_setup_new_text_search(GViewerSearcher *srchr,
                                             GVInputModesData *imd,
                                             offset_type start_offset,
                                             offset_type max_offset,
                                             const gchar *text,
                                             gboolean case_sensitive)
{
    g_return_if_fail (srchr != nullptr);
    g_return_if_fail (srchr->priv != nullptr);

    g_return_if_fail (srchr->priv->search_thread == nullptr);

    g_return_if_fail (imd != nullptr);
    g_return_if_fail (start_offset<=max_offset);

    g_return_if_fail (text != nullptr);
    g_return_if_fail (strlen(text)>0);

    srchr->priv->progress_value = 0;
    srchr->priv->imd = imd;
    srchr->priv->start_offset = start_offset;
    srchr->priv->max_offset = max_offset;

    srchr->priv->update_interval = srchr->priv->max_offset>1000 ? srchr->priv->max_offset/1000 : 10;

    srchr->priv->ct_data = create_bm_chartype_data(text, case_sensitive);
    g_return_if_fail (srchr->priv->ct_data != nullptr);

    gchar *rev_text = g_utf8_strreverse(text, -1);
    srchr->priv->ct_reverse_data = create_bm_chartype_data(rev_text, case_sensitive);
    g_free (rev_text);
    g_return_if_fail (srchr->priv->ct_reverse_data != nullptr);

    srchr->priv->searchmode = TEXT;
}


void g_viewer_searcher_setup_new_hex_search(GViewerSearcher *srchr,
                                            GVInputModesData *imd,
                                            offset_type start_offset,
                                            offset_type max_offset,
                                            const guint8 *buffer, guint buflen)
{
    g_return_if_fail (srchr != nullptr);
    g_return_if_fail (srchr->priv != nullptr);

    g_return_if_fail (srchr->priv->search_thread == nullptr);

    g_return_if_fail (imd != nullptr);
    g_return_if_fail (start_offset<=max_offset);

    g_return_if_fail (buffer != nullptr);
    g_return_if_fail (buflen>0);

    srchr->priv->progress_value = 0;
    srchr->priv->imd = imd;
    srchr->priv->start_offset = start_offset;
    srchr->priv->max_offset = max_offset;

    if (srchr->priv->max_offset>1000)
        srchr->priv->update_interval = srchr->priv->max_offset/1000;
    else
        srchr->priv->update_interval = 10;

    srchr->priv->b_data = create_bm_byte_data(buffer, buflen);
    g_return_if_fail (srchr->priv->b_data != nullptr);

    guint8 *rev_buffer = mem_reverse(buffer, buflen);
    srchr->priv->b_reverse_data = create_bm_byte_data(rev_buffer, buflen);
    g_free (rev_buffer);
    g_return_if_fail (srchr->priv->b_reverse_data != nullptr);

    srchr->priv->searchmode = HEX;
}


static void update_progress_indicator (GViewerSearcher *src, offset_type pos)
{
    gdouble d = (pos*1000.0) / src->priv->max_offset;

    /*This is very bad... (besides being not atomic at all)
       TODO: replace with a gmutex */
    gint oldval = g_atomic_int_get (&src->priv->progress_value);
    g_atomic_int_compare_and_exchange (&src->priv->progress_value, oldval, (gint)d);
}


static gboolean check_abort_request (GViewerSearcher *src)
{
    return g_atomic_int_get (&src->priv->abort_indicator)!=0;
}


static gboolean search_hex_forward (GViewerSearcher *src)
{
    offset_type m, n, j;
    int i;
    gboolean found = FALSE;
    guint8 value;
    GViewerBMByteData *data = src->priv->b_data;

    m = data->pattern_len;
    n = src->priv->max_offset;
    j = src->priv->start_offset;
    int update_counter = src->priv->update_interval;

    while (j <= n - m)
    {
        for (i = m - 1; i >= 0; --i)
        {
            value = (guint8) gv_input_mode_get_raw_byte(src->priv->imd, i+j);
            if (data->pattern[i] != value)
                break;
        }

        if (i < 0)
        {
            src->priv->search_result = j;
            j ++;
            found = TRUE;
            break;
        }

        j += MAX((offset_type)data->good[i], data->bad[value] - m + 1 + i);

        if (--update_counter==0)
        {
            update_progress_indicator(src, j);
            update_counter = src->priv->update_interval;
        }

        if (check_abort_request(src))
            break;
    }

    // Store the current offset, we'll use it if the user chooses "find next"
    if (found)
        src->priv->start_offset = j;

    return found;
}


static gboolean search_hex_backward (GViewerSearcher *src)
{
    offset_type m, j;
    int i;
    gboolean found = FALSE;
    int update_counter;
    guint8 value;
    GViewerBMByteData *data;

    data = src->priv->b_reverse_data;

    m = data->pattern_len;
    j = src->priv->start_offset;
    update_counter = src->priv->update_interval;

    if (j>0)
        j--;
    while (j >= m)
    {
        for (i = m - 1; i >= 0; --i)
        {
            value = (guint8) gv_input_mode_get_raw_byte(src->priv->imd, j-i);
            if (data->pattern[i] != value)
                break;
        }

        if (i < 0)
        {
            src->priv->search_result = j;
            found = TRUE;
            break;
        }

        j -= MAX((offset_type)data->good[i], data->bad[value] - m + 1 + i);

        if (--update_counter==0)
        {
            update_progress_indicator(src, j);
            update_counter = src->priv->update_interval;
        }

        if (check_abort_request(src))
            break;
    }

    // Store the current offset, we'll use it if the user chooses "find next"
    if (found)
        src->priv->start_offset = j;

    return found;
}


static gboolean search_text_forward (GViewerSearcher *src)
{
    offset_type m, n, j;
    int i;
    gboolean found = FALSE;
    char_type value;
    GViewerBMChartypeData *data = src->priv->ct_data;

    m = data->pattern_len;
    n = src->priv->max_offset;
    j = src->priv->start_offset;
    int update_counter = src->priv->update_interval;

    while (j <= n - m)
    {
        offset_type t, delta;

        delta = m - 1;
        t = j;
        while (delta--)
            t = gv_input_get_next_char_offset(src->priv->imd, t);
        for (i = m - 1; i >= 0; --i)
        {
            value = gv_input_mode_get_utf8_char(src->priv->imd, t);
            t = gv_input_get_previous_char_offset(src->priv->imd, t);
            if (!bm_chartype_equal(data, i, value))
                break;
        }

        // Found a match
        if (i < 0)
        {
            src->priv->search_result = j;

            // Advance the current offset, from which "find next" will begin
            j = gv_input_get_next_char_offset(src->priv->imd, j);

            found = TRUE;
            break;
        }

        // didn't find a match, calculate new index
        delta = bm_chartype_get_advancement(data, i, value);
        while (delta--)
            j = gv_input_get_next_char_offset(src->priv->imd, j);

        if (--update_counter==0)
        {
            update_progress_indicator(src, j);
            update_counter = src->priv->update_interval;
        }

        if (check_abort_request(src))
            break;
    }

    // Store the current offset, we'll use it if the user chooses "find next"
    if (found)
        src->priv->start_offset = j;

    return found;
}


static gboolean search_text_backward (GViewerSearcher *src)
{
    offset_type m, j;
    int i;
    gboolean found = FALSE;
    int update_counter;
    char_type value;
    GViewerBMChartypeData *data;

    data = src->priv->ct_reverse_data;

    m = data->pattern_len;
    j = src->priv->start_offset;

    update_counter = src->priv->update_interval;

    j = gv_input_get_previous_char_offset(src->priv->imd, j);
    while (j >= m)
    {
        offset_type t, delta;

        delta = m - 1;
        t = j;
        while (delta--)
            t = gv_input_get_previous_char_offset(src->priv->imd, t);

        for (i = m - 1; i >= 0; --i)
        {
            value = gv_input_mode_get_utf8_char(src->priv->imd, t);
            t = gv_input_get_next_char_offset(src->priv->imd, t);
            if (!bm_chartype_equal(data, i, value))
                break;
        }

        // Found a match
        if (i < 0)
        {
            src->priv->search_result = gv_input_get_next_char_offset(src->priv->imd, j);
            found = TRUE;
            break;
        }

        // didn't find a match, calculate new index
        delta = bm_chartype_get_advancement(data, i, value);
        while (delta--)
            j = gv_input_get_previous_char_offset(src->priv->imd, j);

        if (--update_counter==0)
        {
            update_progress_indicator(src, j);
            update_counter = src->priv->update_interval;
        }

        if (check_abort_request(src))
            break;
    }

    // Store the current offset, we'll use it if the user chooses "find next"
    if (found)
        src->priv->start_offset = j;

    return found;
}


static gpointer search_func (gpointer user_data)
{
    g_return_val_if_fail (G_IS_VIEWERSEARCHER(user_data), nullptr);

    GViewerSearcher *src = G_VIEWERSEARCHER(user_data);
    
    g_return_val_if_fail (src->priv->imd != nullptr, nullptr);

    update_progress_indicator(src, src->priv->start_offset);

    gboolean found;
    
    if (src->priv->searchmode==TEXT)
        found = (src->priv->search_forward) ? search_text_forward(src) : search_text_backward(src);
    else
        found = (src->priv->search_forward) ? search_hex_forward(src) : search_hex_backward(src);

    src->priv->search_reached_end = !found;

    g_atomic_int_add(& src->priv->completed_indicator, 1);

    return nullptr;
}


void g_viewer_searcher_join(GViewerSearcher *src)
{
    g_return_if_fail (src != nullptr);
    g_return_if_fail (src->priv != nullptr);
    g_return_if_fail (src->priv->search_thread != nullptr);

    g_thread_join(src->priv->search_thread);
    src->priv->search_thread = nullptr;
}


void g_viewer_searcher_start_search(GViewerSearcher *src, gboolean forward)
{
    g_return_if_fail (src != nullptr);
    g_return_if_fail (src->priv != nullptr);
    g_return_if_fail (src->priv->search_thread == nullptr);

    // Reset indicators
    src->priv->abort_indicator = 0;
    src->priv->completed_indicator = 0;
    // src->priv->progress_value is NOT reset here, only in "setup_new_search"
    src->priv->search_reached_end = FALSE;

    src->priv->search_forward = forward;

    src->priv->search_thread = g_thread_new (nullptr, search_func, (gpointer) src);
    g_return_if_fail (src->priv->search_thread != nullptr);
}
