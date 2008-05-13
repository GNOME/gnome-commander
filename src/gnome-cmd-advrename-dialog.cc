/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman

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
#include <sys/types.h>
#include <regex.h>
#include <unistd.h>
#include <errno.h>
#include "gnome-cmd-includes.h"
#include "gnome-cmd-advrename-dialog.h"
#include "gnome-cmd-advrename-lexer.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-clist.h"
#include "gnome-cmd-data.h"
#include "tags/gnome-cmd-tags.h"
#include "utils.h"

using namespace std;


// static GdkColor black   = {0,0,0,0};
// static GdkColor red     = {0,0xffff,0,0};


typedef struct
{
    GnomeCmdFile *finfo;
    gchar *new_name;
} RenameEntry;


enum {DIR_MENU, FILE_MENU, COUNTER_MENU, DATE_MENU, METATAG_MENU, NUM_MENUS};


struct _GnomeCmdAdvrenameDialogPrivate
{
    GList *files;
    GList *entries;
    PatternEntry *sel_entry;
    AdvrenameDefaults *defaults;

    GtkWidget *pat_list;
    GtkWidget *res_list;
    GtkWidget *move_up_btn;
    GtkWidget *move_down_btn;
    GtkWidget *add_btn;
    GtkWidget *edit_btn;
    GtkWidget *remove_btn;
    GtkWidget *remove_all_btn;
    GtkWidget *templ_combo;
    GtkWidget *templ_entry;

    GtkWidget *menu[NUM_MENUS];
};


static GnomeCmdDialogClass *parent_class = NULL;

guint advrename_dialog_default_pat_column_width[ADVRENAME_DIALOG_PAT_NUM_COLUMNS] = {150, 150, 30};
guint advrename_dialog_default_res_column_width[ADVRENAME_DIALOG_RES_NUM_COLUMNS] = {200, 200};


static void do_test (GnomeCmdAdvrenameDialog *dialog);


inline void insert_tag (GnomeCmdAdvrenameDialog *dialog, const gchar *text)
{
    GtkEditable *editable = (GtkEditable *) dialog->priv->templ_entry;
    gint pos = gtk_editable_get_position (editable);

    gtk_editable_insert_text (editable, text, strlen(text), &pos);
    gtk_editable_set_position (editable, pos);
    gtk_widget_grab_focus ((GtkWidget *) editable);
    gtk_editable_select_region (editable, pos, pos);
}


static void insert_text_tag (gpointer data, guint n, GtkWidget *widget)
{
    static const gchar *placeholder[] = {"$g",
                                         "$p",
                                         "$N",
                                         "$n",
                                         "$e",
                                         "$c",
                                         "$c(2)",
                                         "$x(8)",
                                         "%x",
                                         "%Y-%m-%d",
                                         "%y-%m-%d",
                                         "%y.%m.%d",
                                         "%y%m%d",
                                         "%d.%m.%y",
                                         "%m-%d-%y",
                                         "%Y",
                                         "%y",
                                         "%m",
                                         "%b",
                                         "%d",
                                         "%X",
                                         "%H.%M.%S",
                                         "%H-%M-%S",
                                         "%H%M%S",
                                         "%H",
                                         "%M",
                                         "%S"};

    g_return_if_fail (n < G_N_ELEMENTS(placeholder));

    insert_tag ((GnomeCmdAdvrenameDialog *) data, placeholder[n]);
}


static void insert_num_tag(gpointer data, guint tag, GtkWidget *widget)
{
    gchar *s = g_strdup_printf ("$T(%s)", gcmd_tags_get_name((GnomeCmdTag) tag));
    insert_tag ((GnomeCmdAdvrenameDialog *) data, s);
    g_free (s);
}


static void on_menu_button_clicked(GtkButton *widget, gpointer data)
{
    GtkWidget *menu = (GtkWidget *) data;
    GdkEventButton *event = (GdkEventButton *) gtk_get_current_event();

    if (event == NULL)
        gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 1, gtk_get_current_event_time());
    else
        if (event->button == 1)
            gtk_menu_popup (GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);
}


static GtkWidget *create_placeholder_menu(GnomeCmdAdvrenameDialog *dialog, int menu_type)
{
    GtkItemFactoryEntry dir_items[] =     {{_("/Grandparent"), NULL, (GtkItemFactoryCallback) insert_text_tag, 0},
                                           {_("/Parent"), NULL, (GtkItemFactoryCallback) insert_text_tag, 1}};

    GtkItemFactoryEntry name_items[] =    {{_("/File name"), NULL, (GtkItemFactoryCallback) insert_text_tag, 2},
                                           {_("/File name without extension"), NULL, (GtkItemFactoryCallback) insert_text_tag, 3},
                                           {_("/File extension"), NULL, (GtkItemFactoryCallback) insert_text_tag, 4}};

    GtkItemFactoryEntry counter_items[] = {{_("/Counter"), NULL, (GtkItemFactoryCallback) insert_text_tag, 5},
                                           {_("/Counter (precision)"), NULL, (GtkItemFactoryCallback) insert_text_tag, 6},
                                           {_("/Hexadecimal random number (precision)"), NULL, (GtkItemFactoryCallback) insert_text_tag, 7}};

    GtkItemFactoryEntry date_items[] =    {{_("/Date/<locale>"), NULL, (GtkItemFactoryCallback) insert_text_tag, 8},
                                           {_("/Date/yyyy-mm-dd"), NULL, (GtkItemFactoryCallback) insert_text_tag, 9},
                                           {_("/Date/yy-mm-dd"), NULL, (GtkItemFactoryCallback) insert_text_tag, 10},
                                           {_("/Date/yy.mm.dd"), NULL, (GtkItemFactoryCallback) insert_text_tag, 11},
                                           {_("/Date/yymmdd"), NULL, (GtkItemFactoryCallback) insert_text_tag, 12},
                                           {_("/Date/dd.mm.yy"), NULL, (GtkItemFactoryCallback) insert_text_tag, 13},
                                           {_("/Date/mm-dd-yy"), NULL, (GtkItemFactoryCallback) insert_text_tag, 14},
                                           {_("/Date/yyyy"), NULL, (GtkItemFactoryCallback) insert_text_tag, 15},
                                           {_("/Date/yy"), NULL, (GtkItemFactoryCallback) insert_text_tag, 16},
                                           {_("/Date/mm"), NULL, (GtkItemFactoryCallback) insert_text_tag, 17},
                                           {_("/Date/mmm"), NULL, (GtkItemFactoryCallback) insert_text_tag, 18},
                                           {_("/Date/dd"), NULL, (GtkItemFactoryCallback) insert_text_tag, 19},
                                           {_("/Time/<locale>"), NULL, (GtkItemFactoryCallback) insert_text_tag, 20},
                                           {_("/Time/HH.MM.SS"), NULL, (GtkItemFactoryCallback) insert_text_tag, 21},
                                           {_("/Time/HH-MM-SS"), NULL, (GtkItemFactoryCallback) insert_text_tag, 22},
                                           {_("/Time/HHMMSS"), NULL, (GtkItemFactoryCallback) insert_text_tag, 23},
                                           {_("/Time/HH"), NULL, (GtkItemFactoryCallback) insert_text_tag, 24},
                                           {_("/Time/MM"), NULL, (GtkItemFactoryCallback) insert_text_tag, 25},
                                           {_("/Time/SS"), NULL, (GtkItemFactoryCallback) insert_text_tag, 26}};

    static GnomeCmdTag metatags[] = {
                                     TAG_FILE_NAME, TAG_FILE_PATH,
                                     TAG_FILE_LINK,
                                     TAG_FILE_SIZE,
                                     TAG_FILE_MODIFIED, TAG_FILE_ACCESSED,
                                     TAG_FILE_PERMISSIONS,
                                     TAG_FILE_FORMAT,
                                     TAG_FILE_PUBLISHER, TAG_FILE_DESCRIPTION, TAG_FILE_KEYWORDS, TAG_FILE_RANK,

                                     TAG_AUDIO_ALBUMARTIST, TAG_AUDIO_ALBUMGAIN, TAG_AUDIO_ALBUMPEAKGAIN,
                                     TAG_AUDIO_ALBUMTRACKCOUNT, TAG_AUDIO_ALBUM, TAG_AUDIO_ARTIST, TAG_AUDIO_BITRATE,
                                     TAG_AUDIO_CHANNELS, TAG_AUDIO_CODECVERSION, TAG_AUDIO_CODEC, TAG_AUDIO_COMMENT,
                                     TAG_AUDIO_COVERALBUMTHUMBNAILPATH, TAG_AUDIO_DISCNO,
                                     TAG_AUDIO_DURATION, TAG_AUDIO_DURATIONMMSS,
                                     TAG_AUDIO_GENRE, TAG_AUDIO_ISNEW, TAG_AUDIO_ISRC, TAG_AUDIO_LASTPLAY, TAG_AUDIO_LYRICS,
                                     TAG_AUDIO_MBALBUMARTISTID, TAG_AUDIO_MBALBUMID, TAG_AUDIO_MBARTISTID,
                                     TAG_AUDIO_MBTRACKID, TAG_AUDIO_PERFORMER, TAG_AUDIO_PLAYCOUNT,
                                     TAG_AUDIO_RELEASEDATE, TAG_AUDIO_SAMPLERATE, TAG_AUDIO_TITLE, TAG_AUDIO_TRACKGAIN,
                                     TAG_AUDIO_TRACKNO, TAG_AUDIO_TRACKPEAKGAIN, TAG_AUDIO_YEAR,
                                     TAG_AUDIO_MPEG_CHANNELMODE, TAG_AUDIO_MPEG_LAYER, TAG_AUDIO_MPEG_VERSION,

                                     TAG_DOC_AUTHOR, TAG_DOC_CREATOR, TAG_DOC_TITLE,
                                     TAG_DOC_SUBJECT, TAG_DOC_DESCRIPTION,
                                     TAG_DOC_CATEGORY, TAG_DOC_KEYWORDS, TAG_DOC_REVISIONCOUNT,
                                     TAG_DOC_PAGECOUNT, TAG_DOC_PARAGRAPHCOUNT, TAG_DOC_LINECOUNT,
                                     TAG_DOC_WORDCOUNT, TAG_DOC_BYTECOUNT,
                                     TAG_DOC_CELLCOUNT, TAG_DOC_CHARACTERCOUNT,
                                     TAG_DOC_CODEPAGE, TAG_DOC_COMMENTS, TAG_DOC_COMPANY,
                                     TAG_DOC_DATECREATED, TAG_DOC_DATEMODIFIED,
                                     TAG_DOC_DICTIONARY,
                                     TAG_DOC_EDITINGDURATION, TAG_DOC_GENERATOR,
                                     TAG_DOC_HIDDENSLIDECOUNT,
                                     TAG_DOC_IMAGECOUNT, TAG_DOC_INITIALCREATOR,
                                     TAG_DOC_LANGUAGE,
                                     TAG_DOC_LASTPRINTED, TAG_DOC_LASTSAVEDBY,
                                     TAG_DOC_LOCALESYSTEMDEFAULT, TAG_DOC_MMCLIPCOUNT,
                                     TAG_DOC_MANAGER, TAG_DOC_NOTECOUNT, TAG_DOC_OBJECTCOUNT,
                                     TAG_DOC_PRESENTATIONFORMAT, TAG_DOC_PRINTDATE,
                                     TAG_DOC_PRINTEDBY, TAG_DOC_SCALE, TAG_DOC_SECURITY,
                                     TAG_DOC_SLIDECOUNT, TAG_DOC_SPREADSHEETCOUNT,
                                     TAG_DOC_TABLECOUNT, TAG_DOC_TEMPLATE,
                                     TAG_DOC_CASESENSITIVE, TAG_DOC_LINKSDIRTY,

                                     TAG_IMAGE_ALBUM, TAG_IMAGE_MAKE, TAG_IMAGE_MODEL,
                                     TAG_IMAGE_COMMENTS, TAG_IMAGE_COPYRIGHT, TAG_IMAGE_CREATOR,
                                     TAG_IMAGE_DATE, TAG_IMAGE_DESCRIPTION, TAG_IMAGE_EXPOSUREPROGRAM,
                                     TAG_IMAGE_EXPOSURETIME, TAG_IMAGE_FLASH, TAG_IMAGE_FNUMBER,
                                     TAG_IMAGE_FOCALLENGTH, TAG_IMAGE_HEIGHT, TAG_IMAGE_ISOSPEED,
                                     TAG_IMAGE_KEYWORDS, TAG_IMAGE_METERINGMODE, TAG_IMAGE_ORIENTATION,
                                     TAG_IMAGE_SOFTWARE, TAG_IMAGE_TITLE, TAG_IMAGE_WHITEBALANCE,
                                     TAG_IMAGE_WIDTH,

                                     TAG_NONE,

                                     TAG_ID3_BAND,
                                     TAG_ID3_CONTENTTYPE,
                                     TAG_ID3_ALBUMSORTORDER, TAG_ID3_AUDIOCRYPTO,
                                     TAG_ID3_AUDIOSEEKPOINT,
                                     TAG_ID3_BPM, TAG_ID3_BUFFERSIZE, TAG_ID3_CDID,
                                     TAG_ID3_COMMERCIAL, TAG_ID3_COMPOSER, TAG_ID3_CONDUCTOR,
                                     TAG_ID3_CONTENTGROUP, TAG_ID3_CONTENTTYPE,
                                     TAG_ID3_COPYRIGHT,
                                     TAG_ID3_CRYPTOREG, TAG_ID3_DATE,
                                     TAG_ID3_EMPHASIS, TAG_ID3_ENCODEDBY,
                                     TAG_ID3_ENCODERSETTINGS, TAG_ID3_ENCODINGTIME, TAG_ID3_EQUALIZATION,
                                     TAG_ID3_EQUALIZATION2, TAG_ID3_EVENTTIMING, TAG_ID3_FILEOWNER,
                                     TAG_ID3_FILETYPE, TAG_ID3_FRAMES, TAG_ID3_GENERALOBJECT,
                                     TAG_ID3_GROUPINGREG, TAG_ID3_INITIALKEY,
                                     TAG_ID3_INVOLVEDPEOPLE, TAG_ID3_INVOLVEDPEOPLE2,
                                     TAG_ID3_LANGUAGE, TAG_ID3_LINKEDINFO,
                                     TAG_ID3_LYRICIST, TAG_ID3_MEDIATYPE, TAG_ID3_MIXARTIST,
                                     TAG_ID3_MOOD,
                                     TAG_ID3_MPEGLOOKUP,
                                     TAG_ID3_MUSICIANCREDITLIST,
                                     TAG_ID3_NETRADIOOWNER, TAG_ID3_NETRADIOSTATION,
                                     TAG_ID3_ORIGALBUM, TAG_ID3_ORIGARTIST, TAG_ID3_ORIGFILENAME,
                                     TAG_ID3_ORIGLYRICIST, TAG_ID3_ORIGRELEASETIME, TAG_ID3_ORIGYEAR,
                                     TAG_ID3_OWNERSHIP, TAG_ID3_PARTINSET, TAG_ID3_PERFORMERSORTORDER,
                                     TAG_ID3_PICTURE, TAG_ID3_PLAYCOUNTER, TAG_ID3_PLAYLISTDELAY,
                                     TAG_ID3_POPULARIMETER, TAG_ID3_POSITIONSYNC, TAG_ID3_PRIVATE,
                                     TAG_ID3_PRODUCEDNOTICE, TAG_ID3_PUBLISHER, TAG_ID3_RECORDINGDATES,
                                     TAG_ID3_RECORDINGTIME, TAG_ID3_RELEASETIME, TAG_ID3_REVERB,
                                     TAG_ID3_SETSUBTITLE, TAG_ID3_SIGNATURE,
                                     TAG_ID3_SIZE, TAG_ID3_SONGLEN, TAG_ID3_SUBTITLE, TAG_ID3_SYNCEDLYRICS,
                                     TAG_ID3_SYNCEDTEMPO, TAG_ID3_TAGGINGTIME, TAG_ID3_TERMSOFUSE,
                                     TAG_ID3_TIME, TAG_ID3_TITLESORTORDER,
                                     TAG_ID3_UNIQUEFILEID, TAG_ID3_UNSYNCEDLYRICS, TAG_ID3_USERTEXT,
                                     TAG_ID3_VOLUMEADJ, TAG_ID3_VOLUMEADJ2, TAG_ID3_WWWARTIST,
                                     TAG_ID3_WWWAUDIOFILE, TAG_ID3_WWWAUDIOSOURCE, TAG_ID3_WWWCOMMERCIALINFO,
                                     TAG_ID3_WWWCOPYRIGHT, TAG_ID3_WWWPAYMENT, TAG_ID3_WWWPUBLISHER,
                                     TAG_ID3_WWWRADIOPAGE, TAG_ID3_WWWUSER,

                                     TAG_EXIF_COPYRIGHT, TAG_EXIF_DATETIME,
                                     TAG_EXIF_EXPOSUREBIASVALUE, TAG_EXIF_EXPOSUREMODE, TAG_EXIF_EXPOSUREPROGRAM,
                                     TAG_EXIF_FLASH, TAG_EXIF_FLASHENERGY,
                                     TAG_EXIF_FNUMBER, TAG_EXIF_FOCALLENGTH,
                                     TAG_EXIF_ISOSPEEDRATINGS, TAG_EXIF_MAXAPERTUREVALUE,
                                     TAG_EXIF_METERINGMODE, TAG_EXIF_SHUTTERSPEEDVALUE, TAG_EXIF_WHITEBALANCE,
                                     TAG_EXIF_PIXELXDIMENSION, TAG_EXIF_PIXELYDIMENSION,
                                     TAG_EXIF_XRESOLUTION, TAG_EXIF_YRESOLUTION,
                                     TAG_EXIF_IMAGELENGTH, TAG_EXIF_IMAGEWIDTH,
                                     TAG_EXIF_CUSTOMRENDERED, TAG_EXIF_COLORSPACE,
                                     TAG_EXIF_DOCUMENTNAME, TAG_EXIF_USERCOMMENT,

                                     TAG_EXIF_APERTUREVALUE, TAG_EXIF_ARTIST, TAG_EXIF_BATTERYLEVEL,
                                     TAG_EXIF_BITSPERSAMPLE, TAG_EXIF_BRIGHTNESSVALUE,
                                     TAG_EXIF_CFAPATTERN, TAG_EXIF_COMPONENTSCONFIGURATION,
                                     TAG_EXIF_COMPRESSEDBITSPERPIXEL, TAG_EXIF_COMPRESSION, TAG_EXIF_CONTRAST,
                                     TAG_EXIF_DATETIMEDIGITIZED, TAG_EXIF_DATETIMEORIGINAL,
                                     TAG_EXIF_DEVICESETTINGDESCRIPTION, TAG_EXIF_DIGITALZOOMRATIO,
                                     TAG_EXIF_EXIFVERSION,
                                     TAG_EXIF_EXPOSUREINDEX,
                                     TAG_EXIF_EXPOSURETIME, TAG_EXIF_FILESOURCE,
                                     TAG_EXIF_FILLORDER,
                                     TAG_EXIF_FLASHPIXVERSION,
                                     TAG_EXIF_FOCALLENGTHIN35MMFILM, TAG_EXIF_FOCALPLANERESOLUTIONUNIT,
                                     TAG_EXIF_FOCALPLANEXRESOLUTION, TAG_EXIF_FOCALPLANEYRESOLUTION,
                                     TAG_EXIF_GAINCONTROL, TAG_EXIF_GAMMA, TAG_EXIF_GPSALTITUDE,
                                     TAG_EXIF_GPSLATITUDE, TAG_EXIF_GPSLONGITUDE,
                                     TAG_EXIF_GPSVERSIONID, TAG_EXIF_IMAGEDESCRIPTION, TAG_EXIF_IMAGEUNIQUEID,
                                     TAG_EXIF_INTERCOLORPROFILE, TAG_EXIF_INTEROPERABILITYINDEX, TAG_EXIF_INTEROPERABILITYVERSION,
                                     TAG_EXIF_IPTCNAA, TAG_EXIF_JPEGINTERCHANGEFORMAT,
                                     TAG_EXIF_JPEGINTERCHANGEFORMATLENGTH, TAG_EXIF_LIGHTSOURCE,
                                     TAG_EXIF_MAKE, TAG_EXIF_MAKERNOTE,
                                     TAG_EXIF_METERINGMODE, TAG_EXIF_MODEL, TAG_EXIF_NEWCFAPATTERN,
                                     TAG_EXIF_NEWSUBFILETYPE, TAG_EXIF_OECF, TAG_EXIF_ORIENTATION,
                                     TAG_EXIF_PHOTOMETRICINTERPRETATION, TAG_EXIF_PLANARCONFIGURATION,
                                     TAG_EXIF_PRIMARYCHROMATICITIES, TAG_EXIF_REFERENCEBLACKWHITE,
                                     TAG_EXIF_RELATEDIMAGEFILEFORMAT, TAG_EXIF_RELATEDIMAGELENGTH,
                                     TAG_EXIF_RELATEDIMAGEWIDTH, TAG_EXIF_RELATEDSOUNDFILE, TAG_EXIF_RESOLUTIONUNIT,
                                     TAG_EXIF_ROWSPERSTRIP, TAG_EXIF_SAMPLESPERPIXEL, TAG_EXIF_SATURATION,
                                     TAG_EXIF_SCENECAPTURETYPE, TAG_EXIF_SCENETYPE, TAG_EXIF_SENSINGMETHOD,
                                     TAG_EXIF_SHARPNESS, TAG_EXIF_SHUTTERSPEEDVALUE, TAG_EXIF_SOFTWARE,
                                     TAG_EXIF_SPATIALFREQUENCYRESPONSE, TAG_EXIF_SPECTRALSENSITIVITY,
                                     TAG_EXIF_STRIPBYTECOUNTS, TAG_EXIF_STRIPOFFSETS,
                                     TAG_EXIF_SUBJECTAREA, TAG_EXIF_SUBJECTDISTANCE, TAG_EXIF_SUBJECTDISTANCERANGE,
                                     TAG_EXIF_SUBJECTLOCATION, TAG_EXIF_SUBSECTIME, TAG_EXIF_SUBSECTIMEDIGITIZED,
                                     TAG_EXIF_SUBSECTIMEORIGINAL, TAG_EXIF_TIFFEPSTANDARDID, TAG_EXIF_TRANSFERFUNCTION,
                                     TAG_EXIF_TRANSFERRANGE, TAG_EXIF_WHITEPOINT,
                                     TAG_EXIF_YCBCRCOEFFICIENTS, TAG_EXIF_YCBCRPOSITIONING,
                                     TAG_EXIF_YCBCRSUBSAMPLING,

                                     TAG_IPTC_BYLINE, TAG_IPTC_BYLINETITLE, TAG_IPTC_CAPTION, TAG_IPTC_HEADLINE,
                                     TAG_IPTC_SUBLOCATION, TAG_IPTC_CITY, TAG_IPTC_PROVINCE,
                                     TAG_IPTC_COUNTRYCODE, TAG_IPTC_COUNTRYNAME,
                                     TAG_IPTC_CONTACT, TAG_IPTC_COPYRIGHTNOTICE, TAG_IPTC_CREDIT,
                                     TAG_IPTC_KEYWORDS,
                                     TAG_IPTC_DIGITALCREATIONDATE, TAG_IPTC_DIGITALCREATIONTIME,
                                     TAG_IPTC_IMAGEORIENTATION,
                                     TAG_IPTC_SPECIALINSTRUCTIONS, TAG_IPTC_URGENCY,

                                     TAG_IPTC_ACTIONADVISED, TAG_IPTC_ARMID, TAG_IPTC_ARMVERSION,
                                     TAG_IPTC_AUDIODURATION, TAG_IPTC_AUDIOOUTCUE, TAG_IPTC_AUDIOSAMPLINGRATE,
                                     TAG_IPTC_AUDIOSAMPLINGRES, TAG_IPTC_AUDIOTYPE,
                                     TAG_IPTC_CATEGORY, TAG_IPTC_CHARACTERSET, TAG_IPTC_CONFIRMEDDATASIZE,
                                     TAG_IPTC_CONTENTLOCCODE, TAG_IPTC_CONTENTLOCNAME,
                                     TAG_IPTC_DATECREATED, TAG_IPTC_DATESENT,
                                     TAG_IPTC_DESTINATION, TAG_IPTC_EDITORIALUPDATE, TAG_IPTC_EDITSTATUS,
                                     TAG_IPTC_ENVELOPENUM, TAG_IPTC_ENVELOPEPRIORITY, TAG_IPTC_EXPIRATIONDATE,
                                     TAG_IPTC_EXPIRATIONTIME, TAG_IPTC_FILEFORMAT, TAG_IPTC_FILEVERSION,
                                     TAG_IPTC_FIXTUREID, TAG_IPTC_IMAGETYPE, TAG_IPTC_LANGUAGEID,
                                     TAG_IPTC_MAXOBJECTSIZE, TAG_IPTC_MAXSUBFILESIZE, TAG_IPTC_MODELVERSION,
                                     TAG_IPTC_OBJECTATTRIBUTE, TAG_IPTC_OBJECTCYCLE, TAG_IPTC_OBJECTNAME,
                                     TAG_IPTC_OBJECTSIZEANNOUNCED, TAG_IPTC_OBJECTTYPE, TAG_IPTC_ORIGINATINGPROGRAM,
                                     TAG_IPTC_ORIGTRANSREF, TAG_IPTC_PREVIEWDATA, TAG_IPTC_PREVIEWFORMAT,
                                     TAG_IPTC_PREVIEWFORMATVER, TAG_IPTC_PRODUCTID, TAG_IPTC_PROGRAMVERSION,
                                     TAG_IPTC_PROVINCE, TAG_IPTC_RASTERIZEDCAPTION, TAG_IPTC_RECORDVERSION,
                                     TAG_IPTC_REFERENCEDATE, TAG_IPTC_REFERENCENUMBER, TAG_IPTC_REFERENCESERVICE,
                                     TAG_IPTC_RELEASEDATE, TAG_IPTC_RELEASETIME, TAG_IPTC_SERVICEID,
                                     TAG_IPTC_SIZEMODE, TAG_IPTC_SOURCE, TAG_IPTC_SUBFILE, TAG_IPTC_SUBJECTREFERENCE,
                                     TAG_IPTC_SUPPLCATEGORY, TAG_IPTC_TIMECREATED, TAG_IPTC_TIMESENT, TAG_IPTC_UNO,
                                     TAG_IPTC_URGENCY, TAG_IPTC_WRITEREDITOR,

                                     TAG_VORBIS_CONTACT, TAG_VORBIS_DESCRIPTION,
                                     TAG_VORBIS_LICENSE, TAG_VORBIS_LOCATION,
                                     TAG_VORBIS_MAXBITRATE, TAG_VORBIS_MINBITRATE,
                                     TAG_VORBIS_NOMINALBITRATE, TAG_VORBIS_ORGANIZATION,
                                     TAG_VORBIS_VENDOR, TAG_VORBIS_VERSION
                                    };

    GtkItemFactoryEntry *items[] = {dir_items,
                                    name_items,
                                    counter_items,
                                    date_items};

    static guint items_size[] = {G_N_ELEMENTS(dir_items),
                                 G_N_ELEMENTS(name_items),
                                 G_N_ELEMENTS(counter_items),
                                 G_N_ELEMENTS(date_items)};

    switch (menu_type)
    {
        case DIR_MENU:
        case FILE_MENU:
        case COUNTER_MENU:
        case DATE_MENU:
            {
                GtkItemFactory *ifac = gtk_item_factory_new (GTK_TYPE_MENU, "<main>", NULL);

                gtk_item_factory_create_items (ifac, items_size[menu_type], items[menu_type], dialog);

                return gtk_item_factory_get_widget (ifac, "<main>");
            }

        case METATAG_MENU:
            {
                GtkItemFactoryEntry *items = g_try_new0(GtkItemFactoryEntry, G_N_ELEMENTS(metatags));

                g_return_val_if_fail (items != NULL, NULL);

                for (guint i=0; i<G_N_ELEMENTS(metatags); ++i)
                {
                    GnomeCmdTag tag = metatags[i];
                    const gchar *class_name = gcmd_tags_get_class_name(tag);
                    GtkItemFactoryEntry *p = items+i;

                    if (!class_name || *class_name==0)
                    {
                        p->path = g_strdup("/");
                        p->item_type = "<Separator>";
                    }
                    else
                    {
                        p->path = g_strdup_printf ("/%s/%s", gcmd_tags_get_class_name(tag), gcmd_tags_get_title(tag));
                        p->callback = (GtkItemFactoryCallback) insert_num_tag;
                        p->callback_action = tag;
                    }
                }

                GtkItemFactory *ifac = gtk_item_factory_new (GTK_TYPE_MENU, "<main>", NULL);

                gtk_item_factory_create_items (ifac, G_N_ELEMENTS(metatags), items, dialog);

                for (guint i=0; i<G_N_ELEMENTS(metatags); ++i)
                    g_free (items[i].path);

                g_free (items);

                return gtk_item_factory_get_widget (ifac, "<main>");
            }

        default:
            return NULL;
    }
}


static GtkWidget *create_button_with_menu (GnomeCmdAdvrenameDialog *dialog, gchar *label_text, int menu_type)
{
    GtkWidget *arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
    GtkWidget *label = gtk_label_new (label_text);
    GtkWidget *hbox = gtk_hbox_new (FALSE, 3);

    gtk_box_pack_start(GTK_BOX (hbox), label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX (hbox), arrow, FALSE, FALSE, 0);

    GtkWidget *button = gtk_button_new ();
    gtk_container_add (GTK_CONTAINER (button), hbox);

    dialog->priv->menu[menu_type] = create_placeholder_menu (dialog, menu_type);

    gtk_widget_set_events (button, GDK_BUTTON_PRESS_MASK);
    g_signal_connect (G_OBJECT(button), "clicked", G_CALLBACK(on_menu_button_clicked), dialog->priv->menu[menu_type]);

    gtk_widget_show_all(button);

    return button;
}


inline void free_data (GnomeCmdAdvrenameDialog *dialog)
{
    gnome_cmd_file_list_free (dialog->priv->files);

    for (GList *tmp = dialog->priv->entries; tmp; tmp = tmp->next)
    {
        RenameEntry *entry = (RenameEntry *) tmp->data;
        g_free (entry->new_name);
        g_free (entry);
    }
    g_list_free (dialog->priv->entries);

    g_free (dialog->priv);
    dialog->priv = NULL;
}


inline void format_entry (PatternEntry *entry, gchar **text)
{
    text[0] = entry->from;
    text[1] = entry->to;
    text[2] = entry->case_sens ? _("Yes") : _("No");
    text[3] = NULL;
}


inline RenameEntry *rename_entry_new (void)
{
    return g_new0 (RenameEntry, 1);
}


inline PatternEntry *get_pattern_entry_from_row (GnomeCmdAdvrenameDialog *dialog, gint row)
{
    return (PatternEntry *) gtk_clist_get_row_data (GTK_CLIST (dialog->priv->pat_list), row);
}


inline gint get_row_from_rename_entry (GnomeCmdAdvrenameDialog *dialog, RenameEntry *entry)
{
    return gtk_clist_find_row_from_data (GTK_CLIST (dialog->priv->res_list), entry);
}


inline void add_rename_entry (GnomeCmdAdvrenameDialog *dialog, GnomeCmdFile *finfo)
{
    gint row;
    gchar *text[4],
          *fname = get_utf8 (finfo->info->name);
    RenameEntry *entry = rename_entry_new ();

    entry->finfo = finfo;

    text[0] = fname;
    text[1] = NULL;
    text[2] = NULL;
    text[3] = NULL;

    row = gtk_clist_append (GTK_CLIST (dialog->priv->res_list), text);
    gtk_clist_set_row_data (GTK_CLIST (dialog->priv->res_list), row, entry);
    dialog->priv->entries = g_list_append (dialog->priv->entries, entry);

    g_free (fname);
}


inline void update_move_buttons (GnomeCmdAdvrenameDialog *dialog, int row)
{
    if (row == 0)
    {
        gtk_widget_set_sensitive (dialog->priv->move_up_btn, FALSE);
        gtk_widget_set_sensitive (dialog->priv->move_down_btn, g_list_length (dialog->priv->defaults->patterns) > 1);
    }
    else
        if (row == g_list_length (dialog->priv->defaults->patterns) - 1)
        {
            gtk_widget_set_sensitive (dialog->priv->move_down_btn, FALSE);
            gtk_widget_set_sensitive (dialog->priv->move_up_btn, g_list_length (dialog->priv->defaults->patterns) > 1);
        }
        else
        {
            gtk_widget_set_sensitive (dialog->priv->move_up_btn, TRUE);
            gtk_widget_set_sensitive (dialog->priv->move_down_btn, TRUE);
        }
}


inline void update_remove_all_button (GnomeCmdAdvrenameDialog *dialog)
{
    gtk_widget_set_sensitive (dialog->priv->remove_all_btn, dialog->priv->defaults->patterns != NULL);
}


inline void add_pattern_entry (GnomeCmdAdvrenameDialog *dialog, PatternEntry *entry)
{
    gint row;
    gchar *text[4];

    if (!entry) return;

    format_entry (entry, text);

    row = gtk_clist_append (GTK_CLIST (dialog->priv->pat_list), text);
    //gtk_clist_set_foreground (GTK_CLIST (dialog->priv->pat_list), row, entry->malformed_pattern ? &red : &black);
    gtk_clist_set_row_data (GTK_CLIST (dialog->priv->pat_list), row, entry);
    update_move_buttons (dialog, GTK_CLIST (dialog->priv->pat_list)->focus_row);
}


inline gchar *update_entry (PatternEntry *entry, GnomeCmdStringDialog *string_dialog, const gchar **values)
{
    if (!values[0])
        return g_strdup (_("Invalid source pattern"));

    GtkWidget *case_check = lookup_widget (GTK_WIDGET (string_dialog), "case_check");

    g_free (entry->from);
    g_free (entry->to);
    entry->from = g_strdup (values[0]);
    entry->to = g_strdup (values[1]);
    entry->case_sens = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (case_check));

    return NULL;
}


static gboolean on_add_rule_dialog_ok (GnomeCmdStringDialog *string_dialog, const gchar **values, GnomeCmdAdvrenameDialog *dialog)
{
    PatternEntry *entry = g_new0 (PatternEntry, 1);

    gchar *error_desc = update_entry (entry, string_dialog, values);

    if (error_desc != NULL)
    {
        gnome_cmd_string_dialog_set_error_desc (string_dialog, error_desc);
        return FALSE;
    }

    add_pattern_entry (dialog, entry);
    dialog->priv->defaults->patterns = g_list_append (dialog->priv->defaults->patterns, entry);
    update_remove_all_button (dialog);
    do_test (dialog);

    return TRUE;
}


static gboolean on_edit_rule_dialog_ok (GnomeCmdStringDialog *string_dialog, const gchar **values, GnomeCmdAdvrenameDialog *dialog)
{
    GtkWidget *pat_list = dialog->priv->pat_list;
    gint row = GTK_CLIST (pat_list)->focus_row;
    PatternEntry *entry = (PatternEntry *) g_list_nth_data (dialog->priv->defaults->patterns, row);

    g_return_val_if_fail (entry != NULL, TRUE);

    gchar *error_desc = update_entry (entry, string_dialog, values);
    if (error_desc != NULL)
    {
        gnome_cmd_string_dialog_set_error_desc (string_dialog, error_desc);
        return FALSE;
    }

    gchar *text[4];

    format_entry (entry, text);
    //gtk_clist_set_foreground (GTK_CLIST (pat_list), row, entry->malformed_pattern ? &red : &black);
    gtk_clist_set_text (GTK_CLIST (pat_list), row, 0, text[0]);
    gtk_clist_set_text (GTK_CLIST (pat_list), row, 1, text[1]);
    gtk_clist_set_text (GTK_CLIST (pat_list), row, 2, text[2]);
    gtk_clist_set_text (GTK_CLIST (pat_list), row, 3, text[3]);
    update_remove_all_button (dialog);
    do_test (dialog);

    return TRUE;
}


static GtkWidget *create_rule_dialog (GnomeCmdAdvrenameDialog *parent_dialog, const gchar *title, GnomeCmdStringDialogCallback on_ok_func, PatternEntry *entry)
{
    // Translators: this is a part of dialog for replacing text 'Replace this:' -> 'With this:'
    const gchar *labels[] = {_("Replace this:"), _("With this:")};

    GtkWidget *dialog = gnome_cmd_string_dialog_new (title, labels, 2, on_ok_func, parent_dialog);
    gtk_widget_ref (dialog);
    gtk_object_set_data_full (GTK_OBJECT (parent_dialog), "rule-dialog", dialog, (GtkDestroyNotify) gtk_widget_unref);
    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dialog), 0, entry?entry->from:"");
    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dialog), 1, entry?entry->to:"");

    GtkWidget *case_check = create_check (dialog, _("Case sensitive matching"), "case_check");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (case_check), entry?entry->case_sens:FALSE);
    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dialog), case_check);

    gtk_widget_show (dialog);

    return dialog;
}


static gchar *apply_one_pattern (gchar *in, PatternEntry *entry, int eflags)
{
    regex_t re_exp;
    regmatch_t re_match_info;
    gchar *out = NULL;

    if (!in || !entry) return NULL;

    if (regcomp (&re_exp, entry->from, (entry->case_sens ? REG_EXTENDED : REG_EXTENDED|REG_ICASE)) != 0)
        entry->malformed_pattern = TRUE;
    else
    {
        if (regexec (&re_exp, in, 1, &re_match_info, eflags) == 0)
        {
            if (strcmp (entry->from, "$") == 0)
                out = g_strconcat (in, entry->to, NULL);
            else
                if (strcmp (entry->from, "^") == 0)
                    out = g_strconcat (entry->to, in, NULL);
                else
                {
                    gint match_size = re_match_info.rm_eo - re_match_info.rm_so;

                    if (match_size)
                    {
                        gchar **v;
                        gchar *match = (gchar *) g_malloc (match_size+1);
                        gchar *tail;

                        g_utf8_strncpy (match, in+re_match_info.rm_so, match_size);
                        match[match_size] = '\0';
                        v = g_strsplit (in, match, 2);
                        tail = apply_one_pattern (v[1], entry, eflags|REG_NOTBOL|REG_NOTEOL);
                        out = g_strjoin (NULL, v[0], entry->to, tail, NULL);
                        g_free (tail);
                        g_free (match);
                        g_strfreev (v);
                    }
                }
        }
    }

    regfree (&re_exp);

    return out ? out : g_strdup (in);
}


inline gchar *create_new_name (const gchar *name, GList *patterns)
{
    gchar *tmp = NULL;
    gchar *new_name = g_strdup (name);

    for (; patterns; patterns = patterns->next)
    {
        PatternEntry *entry = (PatternEntry *) patterns->data;

        tmp = new_name;

        new_name = apply_one_pattern (tmp, entry, 0);

        g_free (tmp);
    }

    return new_name;
}


inline void update_new_names (GnomeCmdAdvrenameDialog *dialog)
{
    const gchar *templ_string = gtk_entry_get_text (GTK_ENTRY (dialog->priv->templ_entry));

    gnome_cmd_advrename_reset_counter (dialog->priv->defaults->counter_start, dialog->priv->defaults->counter_precision, dialog->priv->defaults->counter_increment);
    gnome_cmd_advrename_parse_fname (templ_string);

    for (GList *tmp = dialog->priv->entries; tmp; tmp = tmp->next)
    {
        gchar fname[256];
        RenameEntry *entry = (RenameEntry *) tmp->data;
        GList *patterns = dialog->priv->defaults->patterns;

        gnome_cmd_advrename_gen_fname (fname, sizeof (fname), entry->finfo);

        entry->new_name = create_new_name (fname, patterns);
    }
}


static void redisplay_new_names (GnomeCmdAdvrenameDialog *dialog)
{
    for (GList *tmp = dialog->priv->entries; tmp; tmp = tmp->next)
    {
        RenameEntry *entry = (RenameEntry *) tmp->data;

        gtk_clist_set_text (GTK_CLIST (dialog->priv->res_list),
                            get_row_from_rename_entry (dialog, entry),
                            1,
                            entry->new_name);
    }
}


inline void change_names (GnomeCmdAdvrenameDialog *dialog)
{
    for (GList *tmp = dialog->priv->entries; tmp; tmp = tmp->next)
    {
        RenameEntry *entry = (RenameEntry *) tmp->data;

        if (strcmp (entry->finfo->info->name, entry->new_name) != 0)
            gnome_cmd_file_rename (entry->finfo, entry->new_name);
    }
}


static void on_rule_add (GtkButton *button, GnomeCmdAdvrenameDialog *dialog)
{
    create_rule_dialog (dialog, _("New Rule"), (GnomeCmdStringDialogCallback) on_add_rule_dialog_ok, NULL);
}


static void on_rule_edit (GtkButton *button, GnomeCmdAdvrenameDialog *dialog)
{
    create_rule_dialog (dialog, _("Edit Rule"), (GnomeCmdStringDialogCallback) on_edit_rule_dialog_ok, dialog->priv->sel_entry);
}


static void on_rule_remove (GtkButton *button, GnomeCmdAdvrenameDialog *dialog)
{
    GtkWidget *pat_list = dialog->priv->pat_list;
    PatternEntry *entry = get_pattern_entry_from_row (dialog, GTK_CLIST (pat_list)->focus_row);

    if (entry)
    {
        dialog->priv->defaults->patterns = g_list_remove (dialog->priv->defaults->patterns, entry);
        gtk_clist_remove (GTK_CLIST (pat_list), GTK_CLIST (pat_list)->focus_row);
        update_remove_all_button (dialog);
        do_test (dialog);
    }
}


static void on_rule_remove_all (GtkButton *button, GnomeCmdAdvrenameDialog *dialog)
{
    gtk_clist_clear (GTK_CLIST (dialog->priv->pat_list));
    g_list_free (dialog->priv->defaults->patterns);
    dialog->priv->defaults->patterns = NULL;
    update_remove_all_button (dialog);
    do_test (dialog);
}


static void on_pat_list_scroll_vertical (GtkCList *clist, GtkScrollType scroll_type, gfloat position, GnomeCmdAdvrenameDialog *dialog)
{
    gtk_clist_select_row (clist, clist->focus_row, 0);
}


static void on_rule_selected (GtkCList *list, gint row, gint column, GdkEventButton *event, GnomeCmdAdvrenameDialog *dialog)
{
    gtk_widget_set_sensitive (dialog->priv->remove_btn, TRUE);
    gtk_widget_set_sensitive (dialog->priv->edit_btn, TRUE);
    update_move_buttons (dialog, row);
    dialog->priv->sel_entry = (PatternEntry *) g_list_nth_data (dialog->priv->defaults->patterns, row);
}


static void on_rule_unselected (GtkCList *list, gint row, gint column, GdkEventButton *event, GnomeCmdAdvrenameDialog *dialog)
{
    gtk_widget_set_sensitive (dialog->priv->remove_btn, FALSE);
    gtk_widget_set_sensitive (dialog->priv->edit_btn, FALSE);
    gtk_widget_set_sensitive (dialog->priv->move_up_btn, FALSE);
    gtk_widget_set_sensitive (dialog->priv->move_down_btn, FALSE);
    dialog->priv->sel_entry = NULL;
}


static void on_rule_move_up (GtkButton *button, GnomeCmdAdvrenameDialog *dialog)
{
    GtkCList *clist = GTK_CLIST (dialog->priv->pat_list);

    if (clist->focus_row >= 1)
    {
        gtk_clist_row_move (clist, clist->focus_row, clist->focus_row-1);
        update_move_buttons (dialog, clist->focus_row);
        do_test (dialog);
    }
}


static void on_rule_move_down (GtkButton *button, GnomeCmdAdvrenameDialog *dialog)
{
    GtkCList *clist = GTK_CLIST (dialog->priv->pat_list);

    if (clist->focus_row >= 0)
    {
        gtk_clist_row_move (clist, clist->focus_row, clist->focus_row+1);
        update_move_buttons (dialog, clist->focus_row);
        do_test (dialog);
    }
}


static void on_rule_moved (GtkCList *clist, gint arg1, gint arg2, GnomeCmdAdvrenameDialog *dialog)
{
    GList *pats = dialog->priv->defaults->patterns;

    if (!pats
        || MAX (arg1, arg2) >= g_list_length (pats)
        || MIN (arg1, arg2) < 0
        || arg1 == arg2)
        return;

    gpointer data = g_list_nth_data (pats, arg1);

    pats = g_list_remove (pats, data);
    pats = g_list_insert (pats, data, arg2);

    dialog->priv->defaults->patterns =  pats;
}


inline void save_settings (GnomeCmdAdvrenameDialog *dialog)
{
    const gchar *template_string = gtk_entry_get_text (GTK_ENTRY (dialog->priv->templ_entry));

    history_add (dialog->priv->defaults->templates, g_strdup (template_string));
}


static void on_ok (GtkButton *button, GnomeCmdAdvrenameDialog *dialog)
{
    update_new_names (dialog);
    change_names (dialog);

    save_settings (dialog);
    free_data (dialog);
    gtk_widget_destroy (GTK_WIDGET (dialog));
}


static void on_cancel (GtkButton *button, GnomeCmdAdvrenameDialog *dialog)
{
    save_settings (dialog);
    free_data (dialog);
    gtk_widget_destroy (GTK_WIDGET (dialog));
}


static void on_reset (GtkButton *button, GnomeCmdAdvrenameDialog *dialog)
{
    gtk_entry_set_text (GTK_ENTRY (dialog->priv->templ_entry), "$N");
    on_rule_remove_all (NULL, dialog);
    dialog->priv->defaults->counter_start = 1;
    dialog->priv->defaults->counter_precision = 1;
    dialog->priv->defaults->counter_increment = 1;
}


static void on_help (GtkButton *button, GnomeCmdAdvrenameDialog *dialog)
{
    gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-advanced-rename");
}


static gboolean on_dialog_keypress (GnomeCmdAdvrenameDialog *dialog, GdkEventKey *event)
{
    if (event->keyval == GDK_Escape)
    {
        gtk_widget_destroy (GTK_WIDGET (dialog));
        return TRUE;
    }

    return FALSE;
}


static void do_test (GnomeCmdAdvrenameDialog *dialog)
{
    update_new_names (dialog);
    redisplay_new_names (dialog);
}


static void on_templ_entry_changed (GtkEntry *entry, GnomeCmdAdvrenameDialog *dialog)
{
    if (dialog->priv->defaults->auto_update)
        do_test (dialog);
}


static gboolean on_templ_entry_keypress (GtkEntry *entry, GdkEventKey *event, GnomeCmdAdvrenameDialog *dialog)
{
    if (event->keyval == GDK_Return)
    {
        do_test (dialog);
        return TRUE;
    }

    return FALSE;
}


static gboolean on_template_options_ok (GnomeCmdStringDialog *string_dialog, const gchar **values, GnomeCmdAdvrenameDialog *dialog)
{
    guint start, precision, inc;

    if (sscanf (values[0], "%u", &start) != 1)
        return TRUE;

    if (sscanf (values[1], "%u", &inc) != 1)
        return TRUE;

    if (sscanf (values[2], "%u", &precision) != 1)
        return TRUE;

    dialog->priv->defaults->counter_start = start;
    dialog->priv->defaults->counter_increment = inc;
    dialog->priv->defaults->counter_precision = precision;
    dialog->priv->defaults->auto_update = gtk_toggle_button_get_active (
        GTK_TOGGLE_BUTTON (lookup_widget (GTK_WIDGET (string_dialog), "auto-update-check")));

    do_test (dialog);

    return TRUE;
}


static void on_template_options_clicked (GtkButton *button, GnomeCmdAdvrenameDialog *dialog)
{
    const gchar *labels[] = {
        _("Counter start value:"),
        _("Counter increment:"),
        _("Counter minimum digit count:")
    };
    gchar *s;

    GtkWidget *dlg = gnome_cmd_string_dialog_new (
        _("Template Options"), labels, 3,
        (GnomeCmdStringDialogCallback)on_template_options_ok, dialog);
    gtk_widget_ref (dlg);

    GtkWidget *check = create_check (GTK_WIDGET (dlg), _("Auto-update when the template is entered"), "auto-update-check");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), dialog->priv->defaults->auto_update);
    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dlg), check);

    s = g_strdup_printf ("%d", dialog->priv->defaults->counter_start);
    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dlg), 0, s);
    g_free (s);
    s = g_strdup_printf ("%d", dialog->priv->defaults->counter_increment);
    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dlg), 1, s);
    g_free (s);
    s = g_strdup_printf ("%d", dialog->priv->defaults->counter_precision);
    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dlg), 2, s);
    g_free (s);

    gtk_widget_show (dlg);
}


static void on_res_list_column_resize (GtkCList *clist, gint column, gint width, GnomeCmdAdvrenameDialog *dialog)
{
    advrename_dialog_default_res_column_width[column] = width;
}


static void on_pat_list_column_resize (GtkCList *clist, gint column, gint width, GnomeCmdAdvrenameDialog *dialog)
{
    advrename_dialog_default_pat_column_width[column] = width;
}


static void on_dialog_size_allocate (GtkWidget *widget, GtkAllocation *allocation, GnomeCmdAdvrenameDialog *dialog)
{
    dialog->priv->defaults->width  = allocation->width;
    dialog->priv->defaults->height = allocation->height;
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GnomeCmdAdvrenameDialog *dialog = GNOME_CMD_ADVRENAME_DIALOG (object);

    g_free (dialog->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void class_init (GnomeCmdAdvrenameDialogClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GnomeCmdDialogClass *) gtk_type_class (gnome_cmd_dialog_get_type ());
    object_class->destroy = destroy;
    widget_class->map = ::map;
}


static void init (GnomeCmdAdvrenameDialog *in_dialog)
{
    GtkWidget *vbox;
    GtkWidget *sw;
    GtkWidget *bbox;
    GtkWidget *btn;
    GtkWidget *cat;
    GtkWidget *table;

    in_dialog->priv = g_new0 (GnomeCmdAdvrenameDialogPrivate, 1);

    in_dialog->priv->entries = NULL;
    in_dialog->priv->sel_entry = NULL;
    in_dialog->priv->defaults = gnome_cmd_data_get_advrename_defaults ();

    GtkAccelGroup *accel_group = gtk_accel_group_new ();

    GtkWidget *dialog = GTK_WIDGET (in_dialog);
    gtk_object_set_data (GTK_OBJECT (dialog), "dialog", dialog);
    gtk_window_set_title (GTK_WINDOW (dialog), _("Advanced Rename Tool"));
    gtk_window_set_default_size (GTK_WINDOW (dialog),
                                 in_dialog->priv->defaults->width,
                                 in_dialog->priv->defaults->height);
    gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);
    gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, TRUE, FALSE);
    gtk_signal_connect (GTK_OBJECT (dialog), "size-allocate",
                        GTK_SIGNAL_FUNC (on_dialog_size_allocate), dialog);


    // Template stuff
    table = create_table (dialog, 2, 2);
    cat = create_category (dialog, table, _("Template"));
    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dialog), cat);

    in_dialog->priv->templ_combo = create_combo (dialog);
    gtk_combo_set_case_sensitive (GTK_COMBO (in_dialog->priv->templ_combo), TRUE);
    in_dialog->priv->templ_entry = GTK_COMBO (in_dialog->priv->templ_combo)->entry;
    gtk_signal_connect (GTK_OBJECT (in_dialog->priv->templ_entry),
                        "key-press-event", GTK_SIGNAL_FUNC (on_templ_entry_keypress),
                        dialog);
    gtk_signal_connect (GTK_OBJECT (in_dialog->priv->templ_entry),
                        "changed", GTK_SIGNAL_FUNC (on_templ_entry_changed),
                        dialog);
    gtk_table_attach (GTK_TABLE (table), in_dialog->priv->templ_combo, 0, 1, 0, 1, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL), (GtkAttachOptions) (GTK_EXPAND|GTK_FILL), 0, 0);
    if (in_dialog->priv->defaults->templates->ents)
        gtk_combo_set_popdown_strings (GTK_COMBO (in_dialog->priv->templ_combo), in_dialog->priv->defaults->templates->ents);
    else
        gtk_entry_set_text (GTK_ENTRY (in_dialog->priv->templ_entry), "$N");

    bbox = create_vbuttonbox (dialog);
    table_add (table, bbox, 1, 0, GTK_FILL);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_START);

    btn = create_button (dialog, _("O_ptions..."), GTK_SIGNAL_FUNC (on_template_options_clicked));
    gtk_container_add (GTK_CONTAINER (bbox), btn);

    bbox = create_hbuttonbox (dialog);
    table_add (table, bbox, 0, 1, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));
    gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_START);

    btn = create_button_with_menu (in_dialog, _("Directory"), DIR_MENU);
    gtk_container_add (GTK_CONTAINER (bbox), btn);

    btn = create_button_with_menu (in_dialog, _("File"), FILE_MENU);
    gtk_container_add (GTK_CONTAINER (bbox), btn);

    btn = create_button_with_menu (in_dialog, _("Counter"),COUNTER_MENU);
    gtk_container_add (GTK_CONTAINER (bbox), btn);

    btn = create_button_with_menu (in_dialog, _("Date"), DATE_MENU);
    gtk_container_add (GTK_CONTAINER (bbox), btn);

    btn = create_button_with_menu (in_dialog, _("Metatag"), METATAG_MENU);
    gtk_container_add (GTK_CONTAINER (bbox), btn);

    // Regex stuff
    table = create_table (dialog, 2, 2);
    cat = create_category (dialog, table, _("Regex replacing"));
    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dialog), cat);

    sw = create_clist (dialog, "pat_list", 3, 16, GTK_SIGNAL_FUNC (on_rule_selected), GTK_SIGNAL_FUNC (on_rule_moved));
    gtk_table_attach (GTK_TABLE (table), sw, 0, 1, 0, 1, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL), (GtkAttachOptions) (GTK_EXPAND|GTK_FILL), 0, 0);
    create_clist_column (sw, 0, advrename_dialog_default_pat_column_width[0], _("Replace this"));
    create_clist_column (sw, 1, advrename_dialog_default_pat_column_width[1], _("With this"));
    create_clist_column (sw, 2, advrename_dialog_default_pat_column_width[2], _("Case sensitive"));

    in_dialog->priv->pat_list = lookup_widget (GTK_WIDGET (sw), "pat_list");
    gtk_signal_connect (GTK_OBJECT (in_dialog->priv->pat_list),
                        "unselect-row",
                        GTK_SIGNAL_FUNC (on_rule_unselected),
                        dialog);

    bbox = create_hbuttonbox (dialog);
    table_add (table, bbox, 0, 1, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));
    gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_START);

    in_dialog->priv->add_btn = create_stock_button (dialog, GTK_STOCK_ADD, GTK_SIGNAL_FUNC (on_rule_add));
    gtk_container_add (GTK_CONTAINER (bbox), in_dialog->priv->add_btn);

    in_dialog->priv->edit_btn = create_stock_button (dialog, GTK_STOCK_EDIT, GTK_SIGNAL_FUNC (on_rule_edit));
    gtk_container_add (GTK_CONTAINER (bbox), in_dialog->priv->edit_btn);
    gtk_widget_set_sensitive (GTK_WIDGET (in_dialog->priv->edit_btn), FALSE);

    in_dialog->priv->remove_btn = create_stock_button (dialog, GTK_STOCK_REMOVE, GTK_SIGNAL_FUNC (on_rule_remove));
    gtk_container_add (GTK_CONTAINER (bbox), in_dialog->priv->remove_btn);
    gtk_widget_set_sensitive (GTK_WIDGET (in_dialog->priv->remove_btn), FALSE);

    in_dialog->priv->remove_all_btn = create_button (dialog, _("Re_move All"), GTK_SIGNAL_FUNC (on_rule_remove_all));
    gtk_container_add (GTK_CONTAINER (bbox), in_dialog->priv->remove_all_btn);
    if (!in_dialog->priv->defaults->patterns)
        gtk_widget_set_sensitive (GTK_WIDGET (in_dialog->priv->remove_all_btn), FALSE);

    bbox = create_vbuttonbox (dialog);
    table_add (table, bbox, 1, 0, GTK_FILL);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_START);

    in_dialog->priv->move_up_btn = create_stock_button (dialog, GNOME_STOCK_BUTTON_UP, GTK_SIGNAL_FUNC (on_rule_move_up));
    gtk_container_add (GTK_CONTAINER (bbox), in_dialog->priv->move_up_btn);
    gtk_widget_set_sensitive (GTK_WIDGET (in_dialog->priv->move_up_btn), FALSE);

    in_dialog->priv->move_down_btn = create_stock_button (dialog, GNOME_STOCK_BUTTON_DOWN, GTK_SIGNAL_FUNC (on_rule_move_down));
    gtk_container_add (GTK_CONTAINER (bbox), in_dialog->priv->move_down_btn);
    gtk_widget_set_sensitive (GTK_WIDGET (in_dialog->priv->move_down_btn), FALSE);


    // Result list stuff
    vbox = create_vbox (dialog, FALSE, 0);
    cat = create_category (dialog, vbox, _("Result"));
    gnome_cmd_dialog_add_expanding_category (GNOME_CMD_DIALOG (dialog), cat);

    sw = create_clist (dialog, "res_list", 2, 16, NULL, NULL);
    gtk_container_add (GTK_CONTAINER (vbox), sw);
    create_clist_column (sw, 0, advrename_dialog_default_res_column_width[0], _("Current file names"));
    create_clist_column (sw, 1, advrename_dialog_default_res_column_width[1], _("New file names"));
    in_dialog->priv->res_list = lookup_widget (GTK_WIDGET (sw), "res_list");


    // Dialog stuff
    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GNOME_STOCK_BUTTON_HELP, GTK_SIGNAL_FUNC (on_help), dialog);
    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), _("Rese_t"), GTK_SIGNAL_FUNC (on_reset), dialog);
    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GNOME_STOCK_BUTTON_CANCEL, GTK_SIGNAL_FUNC (on_cancel), dialog);
    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GNOME_STOCK_BUTTON_OK, GTK_SIGNAL_FUNC (on_ok), dialog);

    gtk_widget_grab_focus (in_dialog->priv->pat_list);
    gtk_window_add_accel_group (GTK_WINDOW (dialog), accel_group);

    for (GList *tmp = in_dialog->priv->defaults->patterns; tmp; tmp = tmp->next)
    {
        PatternEntry *entry = (PatternEntry *) tmp->data;
        add_pattern_entry (in_dialog, entry);
    }

    gtk_signal_connect (GTK_OBJECT (dialog), "key-press-event", GTK_SIGNAL_FUNC (on_dialog_keypress), dialog);
    gtk_signal_connect_after (GTK_OBJECT (in_dialog->priv->pat_list),
                              "scroll-vertical",
                              GTK_SIGNAL_FUNC (on_pat_list_scroll_vertical),
                              dialog);
    gtk_signal_connect_after (GTK_OBJECT (in_dialog->priv->pat_list),
                              "resize-column",
                              GTK_SIGNAL_FUNC (on_pat_list_column_resize),
                              dialog);
    gtk_signal_connect_after (GTK_OBJECT (in_dialog->priv->res_list),
                              "resize-column",
                              GTK_SIGNAL_FUNC (on_res_list_column_resize),
                              dialog);

    if (in_dialog->priv->defaults->patterns)
        gtk_clist_select_row (GTK_CLIST (in_dialog->priv->pat_list), 0, 0);
}


/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_advrename_dialog_new (GList *files)
{
    GnomeCmdAdvrenameDialog *dialog = (GnomeCmdAdvrenameDialog *) gtk_type_new (gnome_cmd_advrename_dialog_get_type ());

    dialog->priv->files = gnome_cmd_file_list_copy (files);

    for (GList *tmp = dialog->priv->files; tmp; tmp = tmp->next)
    {
        GnomeCmdFile *finfo = (GnomeCmdFile *) tmp->data;
        if (strcmp (finfo->info->name, "..") != 0)
            add_rename_entry (dialog, finfo);
    }

    do_test (dialog);

    return GTK_WIDGET (dialog);
}


GtkType gnome_cmd_advrename_dialog_get_type (void)
{
    static GtkType dlg_type = 0;

    if (dlg_type == 0)
    {
        GtkTypeInfo dlg_info =
        {
            "GnomeCmdAdvrenameDialog",
            sizeof (GnomeCmdAdvrenameDialog),
            sizeof (GnomeCmdAdvrenameDialogClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        dlg_type = gtk_type_unique (gnome_cmd_dialog_get_type (), &dlg_info);
    }
    return dlg_type;
}
