/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include <config.h>

#include <stdio.h>
#include <stdarg.h>

#include <vector>

#ifdef HAVE_EXIF
#include <libexif/exif-content.h>
#endif

#ifdef HAVE_IPTC
#include <libiptcdata/iptc-data.h>
#endif

#ifdef HAVE_LCMS
#include <icc34.h>
#include <lcms.h>
#endif

#ifdef HAVE_ID3
#include <id3.h>
#endif

#include "gnome-cmd-includes.h"
#include "gnome-cmd-tags-libs.h"
#include "gnome-cmd-tags-audio.h"
#include "gnome-cmd-tags-doc.h"
#include "gnome-cmd-tags-exif.h"
#include "gnome-cmd-tags-icc.h"
#include "gnome-cmd-tags-id3.h"
#include "gnome-cmd-tags-iptcdata.h"
#include "utils.h"

using namespace std;


struct GnomeCmdTagName
{
    const gchar *name;
    GnomeCmdTagClass tag_class;
    GnomeCmdTag tag;
    const gchar *title;
    const gchar *description;
    guint libtag;
    guint libclass;
};


static GnomeCmdTagName metatags[NUMBER_OF_TAGS] = {{"", TAG_NONE_CLASS, TAG_NONE, "", ""},
                                                   {"Audio.AlbumArtist", TAG_AUDIO, TAG_AUDIO_ALBUMARTIST, N_("Album Artist"), N_("Artist of the album.")},
                                                   {"Audio.AlbumGain", TAG_AUDIO, TAG_AUDIO_ALBUMGAIN, N_("Album Gain"), N_("Gain adjustment of the album.")},
                                                   {"Audio.AlbumPeakGain", TAG_AUDIO, TAG_AUDIO_ALBUMPEAKGAIN, N_("Album Peak Gain"), N_("Peak gain adjustment of album.")},
                                                   {"Audio.AlbumTrackCount", TAG_AUDIO, TAG_AUDIO_ALBUMTRACKCOUNT, N_("Album Track Count"), N_("Total number of tracks on the album.")},
                                                   {"Audio.Album", TAG_AUDIO, TAG_AUDIO_ALBUM, N_("Album"), N_("Name of the album.")},
                                                   {"Audio.Artist", TAG_AUDIO, TAG_AUDIO_ARTIST, N_("Artist"), N_("Artist of the track.")},
                                                   {"Audio.Bitrate", TAG_AUDIO, TAG_AUDIO_BITRATE, N_("Bitrate"), N_("Bitrate in kbps.")},
                                                   {"Audio.Channels", TAG_AUDIO, TAG_AUDIO_CHANNELS, N_("Channels"), N_("Number of channels in the audio (2 = stereo).")},
                                                   {"Audio.CodecVersion", TAG_AUDIO, TAG_AUDIO_CODECVERSION, N_("Codec Version"), N_("Codec version.")},
                                                   {"Audio.Codec", TAG_AUDIO, TAG_AUDIO_CODEC, N_("Codec"), N_("Codec encoding description.")},
                                                   {"Audio.Comment", TAG_AUDIO, TAG_AUDIO_COMMENT, N_("Comment"), N_("Comments on the track.")},
                                                   {"Audio.CoverAlbumThumbnailPath", TAG_AUDIO, TAG_AUDIO_COVERALBUMTHUMBNAILPATH, N_("Cover album thumbnail path"), N_("File path to thumbnail image of the cover album.")},
                                                   {"Audio.DiscNo", TAG_AUDIO, TAG_AUDIO_DISCNO, N_("Disc Number"), N_("Specifies which disc the track is on.")},
                                                   {"Audio.Duration", TAG_AUDIO, TAG_AUDIO_DURATION, N_("Duration"), N_("Duration of track in seconds.")},
                                                   {"Audio.Genre", TAG_AUDIO, TAG_AUDIO_GENRE, N_("Genre"), N_("Type of music classification for the track as defined in ID3 spec.")},
                                                   {"Audio.IsNew", TAG_AUDIO, TAG_AUDIO_ISNEW, N_("Is new"), N_("Set to \"1\" if track is new to the user (default \"0\").")},
                                                   {"Audio.LastPlay", TAG_AUDIO, TAG_AUDIO_LASTPLAY, N_("Last Play"), N_("When track was last played.")},
                                                   {"Audio.Lyrics", TAG_AUDIO, TAG_AUDIO_LYRICS, N_("Lyrics"), N_("Lyrics of the track.")},
                                                   {"Audio.MBAlbumArtistID", TAG_AUDIO, TAG_AUDIO_MBALBUMARTISTID, N_("MB album artist ID"), N_("MusicBrainz album artist ID in UUID format.")},
                                                   {"Audio.MBAlbumID", TAG_AUDIO, TAG_AUDIO_MBALBUMID, N_("MB Album ID"), N_("MusicBrainz album ID in UUID format.")},
                                                   {"Audio.MBArtistID", TAG_AUDIO, TAG_AUDIO_MBARTISTID, N_("MB Artist ID"), N_("MusicBrainz artist ID in UUID format.")},
                                                   {"Audio.MBTrackID", TAG_AUDIO, TAG_AUDIO_MBTRACKID, N_("MB Track ID"), N_("MusicBrainz track ID in UUID format.")},
                                                   {"Audio.Performer", TAG_AUDIO, TAG_AUDIO_PERFORMER, N_("Performer"), N_("Name of the performer/conductor of the music.")},
                                                   {"Audio.PlayCount", TAG_AUDIO, TAG_AUDIO_PLAYCOUNT, N_("Play Count"), N_("Number of times the track has been played.")},
                                                   {"Audio.ReleaseDate", TAG_AUDIO, TAG_AUDIO_RELEASEDATE, N_("Release Date"), N_("Date track was released.")},
                                                   {"Audio.Samplerate", TAG_AUDIO, TAG_AUDIO_SAMPLERATE, N_("Sample Rate"), N_("Sample rate in Hz.")},
                                                   {"Audio.Title", TAG_AUDIO, TAG_AUDIO_TITLE, N_("Title"), N_("Title of the track.")},
                                                   {"Audio.TrackGain", TAG_AUDIO, TAG_AUDIO_TRACKGAIN, N_("Track Gain"), N_("Gain adjustment of the track.")},
                                                   {"Audio.TrackNo", TAG_AUDIO, TAG_AUDIO_TRACKNO, N_("Track Number"), N_("Position of track on the album.")},
                                                   {"Audio.TrackPeakGain", TAG_AUDIO, TAG_AUDIO_TRACKPEAKGAIN, N_("Track Peak Gain"), N_("Peak gain adjustment of track.")},
                                                   {"Doc.Author", TAG_DOC, TAG_DOC_CREATOR, N_("Author"), N_("Name of the author.")},
                                                   {"Doc.ByteCount", TAG_DOC, TAG_DOC_BYTECOUNT, N_("Byte Count"), N_("Number of bytes in the document.")},
                                                   {"Doc.CaseSensitive", TAG_DOC, TAG_DOC_CASESENSITIVE, N_("Case Sensitive"), N_("Case sensitive.")},
                                                   {"Doc.Category", TAG_DOC, TAG_DOC_CATEGORY, N_("Category"), N_("Category.")},
                                                   {"Doc.CellCount", TAG_DOC, TAG_DOC_CELLCOUNT, N_("Cell Count"), N_("Number of cells in the spreadsheet document.")},
                                                   {"Doc.CharacterCount", TAG_DOC, TAG_DOC_CHARACTERCOUNT, N_("Character Count"), N_("Number of characters in the document.")},
                                                   {"Doc.Codepage", TAG_DOC, TAG_DOC_CODEPAGE, N_("Codepage"), N_("The MS codepage to encode strings for metadata.")},
                                                   {"Doc.Comments", TAG_DOC, TAG_DOC_COMMENTS, N_("Comments"), N_("User definable free text.")},
                                                   {"Doc.Company", TAG_DOC, TAG_DOC_COMPANY, N_("Company"), N_("Organization that the <Doc.Creator> entity is associated with.")},
                                                   {"Doc.Created", TAG_DOC, TAG_DOC_DATECREATED, N_("Created"), N_("Datetime document was originally created.")},
                                                   {"Doc.Creator", TAG_DOC, TAG_DOC_CREATOR, N_("Creator"), N_("An entity primarily responsible for making the content of the resource, typically a person, organization, or service.")},
                                                   {"Doc.DateCreated", TAG_DOC, TAG_DOC_DATECREATED, N_("Date Created"), N_("Date associated with an event in the life cycle of the resource (creation/publication date).")},
                                                   {"Doc.DateModified", TAG_DOC, TAG_DOC_DATEMODIFIED, N_("Date Modified"), N_("The last time the document was saved.")},
                                                   {"Doc.Description", TAG_DOC, TAG_DOC_DESCRIPTION, N_("Description"), N_("An acccount of the content of the resource.")},
                                                   {"Doc.Dictionary", TAG_DOC, TAG_DOC_DICTIONARY, N_("Dictionary"), N_("Dictionary.")},
                                                   {"Doc.EditingDuration", TAG_DOC, TAG_DOC_EDITINGDURATION, N_("Editing Duration"), N_("The total time taken until the last modification.")},
                                                   {"Doc.Generator", TAG_DOC, TAG_DOC_GENERATOR, N_("Generator"), N_("The application that generated this document.")},
                                                   {"Doc.HiddenSlideCount", TAG_DOC, TAG_DOC_HIDDENSLIDECOUNT, N_("Hidden Slide Count"), N_("Number of hidden slides in the presentation document.")},
                                                   {"Doc.ImageCount", TAG_DOC, TAG_DOC_IMAGECOUNT, N_("Image Count"), N_("Number of images in the document.")},
                                                   {"Doc.InitialCreator", TAG_DOC, TAG_DOC_INITIALCREATOR, N_("Initial Creator"), N_("Specifies the name of the person who created the document initially.")},
                                                   {"Doc.Keyword", TAG_DOC, TAG_DOC_KEYWORDS, N_("Keywords"), N_("Searchable, indexable keywords.")},
                                                   {"Doc.Keywords", TAG_DOC, TAG_DOC_KEYWORDS, N_("Keywords"), N_("Searchable, indexable keywords.")},
                                                   {"Doc.Language", TAG_DOC, TAG_DOC_LANGUAGE, N_("Language"), N_("The locale language of the intellectual content of the resource.")},
                                                   {"Doc.LastPrinted", TAG_DOC, TAG_DOC_LASTPRINTED, N_("Last Printed"), N_("The last time this document was printed.")},
                                                   {"Doc.LastSavedBy", TAG_DOC, TAG_DOC_LASTSAVEDBY, N_("Last Saved By"), N_("The entity that made the last change to the document, typically a person, organization, or service.")},
                                                   {"Doc.LineCount", TAG_DOC, TAG_DOC_LINECOUNT, N_("Line Count"), N_("Number of lines in the document.")},
                                                   {"Doc.LinksDirty", TAG_DOC, TAG_DOC_LINKSDIRTY, N_("Links Dirty"), N_("Links dirty.")},
                                                   {"Doc.LocaleSystemDefault", TAG_DOC, TAG_DOC_LOCALESYSTEMDEFAULT, N_("Locale System Default"), N_("Identifier representing the default system locale.")},
                                                   {"Doc.MMClipCount", TAG_DOC, TAG_DOC_MMCLIPCOUNT, N_("Multimedia Clip Count"), N_("Number of multimedia clips in the document.")},
                                                   {"Doc.Manager", TAG_DOC, TAG_DOC_MANAGER, N_("Manager"), N_("Name of the manager of <Doc.Creator> entity.")},
                                                   {"Doc.NoteCount", TAG_DOC, TAG_DOC_NOTECOUNT, N_("Note Count"), N_("Number of \"notes\" in the document.")},
                                                   {"Doc.ObjectCount", TAG_DOC, TAG_DOC_OBJECTCOUNT, N_("Object Count"), N_("Number of objects (OLE and other graphics) in the document.")},
                                                   {"Doc.PageCount", TAG_DOC, TAG_DOC_PAGECOUNT, N_("Page Count"), N_("Number of pages in the document.")},
                                                   {"Doc.ParagraphCount", TAG_DOC, TAG_DOC_PARAGRAPHCOUNT, N_("Paragraph Count"), N_("Number of paragraphs in the document.")},
                                                   {"Doc.PresentationFormat", TAG_DOC, TAG_DOC_PRESENTATIONFORMAT, N_("Presentation Format"), N_("Type of presentation, like \"On-screen Show\", \"SlideView\", etc.")},
                                                   {"Doc.PrintDate", TAG_DOC, TAG_DOC_PRINTDATE, N_("Print Date"), N_("Specifies the date and time when the document was last printed.")},
                                                   {"Doc.PrintedBy", TAG_DOC, TAG_DOC_PRINTEDBY, N_("Printed By"), N_("Specifies the name of the last person who printed the document.")},
                                                   {"Doc.RevisionCount", TAG_DOC, TAG_DOC_REVISIONCOUNT, N_("Revision Count"), N_("Number of revision on the document.")},
                                                   {"Doc.Scale", TAG_DOC, TAG_DOC_SCALE, N_("Scale"), N_("Scale.")},
                                                   {"Doc.Security", TAG_DOC, TAG_DOC_SECURITY, N_("Security"), N_("One of: \"Password protected\", \"Read-only recommended\", \"Read-only enforced\" or \"Locked for annotations\".")},
                                                   {"Doc.SlideCount", TAG_DOC, TAG_DOC_SLIDECOUNT, N_("Slide Count"), N_("Number of slides in the presentation document.")},
                                                   {"Doc.SpreadsheetCount", TAG_DOC, TAG_DOC_SPREADSHEETCOUNT, N_("Spreadsheet Count"), N_("Number of pages in the document.")},
                                                   {"Doc.Subject", TAG_DOC, TAG_DOC_SUBJECT, N_("Subject"), N_("Document subject.")},
                                                   {"Doc.TableCount", TAG_DOC, TAG_DOC_TABLECOUNT, N_("Table Count"), N_("Number of tables in the document.")},
                                                   {"Doc.Template", TAG_DOC, TAG_DOC_TEMPLATE, N_("Template"), N_("The template file that is been used to generate this document.")},
                                                   {"Doc.Title", TAG_DOC, TAG_DOC_TITLE, N_("Title"), N_("Title of the document.")},
                                                   {"Doc.WordCount", TAG_DOC, TAG_DOC_WORDCOUNT, N_("Word Count"), N_("Number of words in the document.")},
                                                   {"Exif.ApertureValue", TAG_EXIF, TAG_EXIF_APERTUREVALUE, N_("Aperture"), N_("The lens aperture. The unit is the APEX value.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_APERTURE_VALUE
#endif
                                                   },
                                                   {"Exif.Artist", TAG_EXIF, TAG_EXIF_ARTIST, N_("Artist"), N_("Name of the camera owner, photographer or image creator. The detailed format is not specified, but it is recommended that the information be written for ease of Interoperability. When the field is left blank, it is treated as unknown.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_ARTIST
#endif
                                                   },
                                                   {"Exif.BatteryLevel", TAG_EXIF, TAG_EXIF_BATTERYLEVEL, N_("Battery Level"), N_("Battery Level.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_BATTERY_LEVEL
#endif
                                                   },
                                                   {"Exif.BitsPerSample", TAG_EXIF, TAG_EXIF_BITSPERSAMPLE, N_("Bits per Sample"), N_("The number of bits per image component. Each component of the image is 8 bits, so the value for this tag is 8. In JPEG compressed data a JPEG marker is used instead of this tag.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_BITS_PER_SAMPLE
#endif
                                                   },
                                                   {"Exif.BrightnessValue", TAG_EXIF, TAG_EXIF_BRIGHTNESSVALUE, N_("Brightness"), N_("The value of brightness. The unit is the APEX value. Ordinarily it is given in the range of -99.99 to 99.99.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_BRIGHTNESS_VALUE
#endif
                                                   },
                                                   {"Exif.CFAPattern", TAG_EXIF, TAG_EXIF_CFAPATTERN, N_("CFA Pattern"), N_("The color filter array (CFA) geometric pattern of the image sensor when a one-chip color area sensor is used. It does not apply to all sensing methods.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_CFA_PATTERN
#endif
                                                   },
                                                   {"Exif.CFARepeatPatternDim", TAG_EXIF, TAG_EXIF_CFAREPEATPATTERNDIM, N_("CFA Repeat Pattern Dim"), N_("CFA Repeat Pattern Dim.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_CFA_REPEAT_PATTERN_DIM
#endif
                                                   },
                                                   {"Exif.ColorSpace", TAG_EXIF, TAG_EXIF_COLORSPACE, N_("Color Space"), N_("The color space information tag is always recorded as the color space specifier. Normally sRGB (=1) is used to define the color space based on the PC monitor conditions and environment. If a color space other than sRGB is used, Uncalibrated (=FFFF.H) is set. Image data recorded as Uncalibrated can be treated as sRGB when it is converted to FlashPix.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_COLOR_SPACE
#endif
                                                   },
                                                   {"Exif.ComponentsConfiguration", TAG_EXIF, TAG_EXIF_COMPONENTSCONFIGURATION, N_("Components Configuration"), N_("Information specific to compressed data. The channels of each component are arranged in order from the 1st component to the 4th. For uncompressed data the data arrangement is given in the <Exif.PhotometricInterpretation> tag. However, since <Exif.PhotometricInterpretation> can only express the order of Y, Cb and Cr, this tag is provided for cases when compressed data uses components other than Y, Cb, and Cr and to enable support of other sequences.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_COMPONENTS_CONFIGURATION
#endif
                                                   },
                                                   {"Exif.CompressedBitsPerPixel", TAG_EXIF, TAG_EXIF_COMPRESSEDBITSPERPIXEL, N_("Compressed Bits per Pixel"), N_("Information specific to compressed data. The compression mode used for a compressed image is indicated in unit bits per pixel.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_COMPRESSED_BITS_PER_PIXEL
#endif
                                                   },
                                                   {"Exif.Compression", TAG_EXIF, TAG_EXIF_COMPRESSION, N_("Compression"), N_("The compression scheme used for the image data. When a primary image is JPEG compressed, this designation is not necessary and is omitted. When thumbnails use JPEG compression, this tag value is set to 6.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_COMPRESSION
#endif
                                                   },
                                                   {"Exif.Contrast", TAG_EXIF, TAG_EXIF_CONTRAST, N_("Contrast"), N_("The direction of contrast processing applied by the camera when the image was shot.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_CONTRAST
#endif
                                                   },
                                                   {"Exif.Copyright", TAG_EXIF, TAG_EXIF_COPYRIGHT, N_("Copyright"), N_("Copyright information. The tag is used to indicate both the photographer and editor copyrights. It is the copyright notice of the person or organization claiming rights to the image. The Interoperability copyright statement including date and rights should be written in this field; e.g., \"Copyright, John Smith, 19xx. All rights reserved.\". The field records both the photographer and editor copyrights, with each recorded in a separate part of the statement. When there is a clear distinction between the photographer and editor copyrights, these are to be written in the order of photographer followed by editor copyright, separated by NULL (in this case, since the statement also ends with a NULL, there are two NULL codes) (see example 1). When only the photographer is given, it is terminated by one NULL code. When only the editor copyright is given, the photographer copyright part consists of one space followed by a terminating NULL code, then the editor copyright is given. When the field is left blank, it is treated as unknown.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_COPYRIGHT
#endif
                                                   },
                                                   {"Exif.CustomRendered", TAG_EXIF, TAG_EXIF_CUSTOMRENDERED, N_("Custom Rendered"), N_("The use of special processing on image data, such as rendering geared to output. When special processing is performed, the reader is expected to disable or minimize any further processing.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_CUSTOM_RENDERED
#endif
                                                   },
                                                   {"Exif.DateTime", TAG_EXIF, TAG_EXIF_DATETIME, N_("Date and Time"), N_("The date and time of image creation.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_DATE_TIME
#endif
                                                   },
                                                   {"Exif.DateTimeDigitized", TAG_EXIF, TAG_EXIF_DATETIMEDIGITIZED, N_("Date and Time (digitized)"), N_("The date and time when the image was stored as digital data.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_DATE_TIME_DIGITIZED
#endif
                                                   },
                                                   {"Exif.DateTimeOriginal", TAG_EXIF, TAG_EXIF_DATETIMEORIGINAL, N_("Date and Time (original)"), N_("The date and time when the original image data was generated. For a digital still camera the date and time the picture was taken are recorded.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_DATE_TIME_ORIGINAL
#endif
                                                   },
                                                   {"Exif.DeviceSettingDescription", TAG_EXIF, TAG_EXIF_DEVICESETTINGDESCRIPTION, N_("Device Setting Description"), N_("Information on the picture-taking conditions of a particular camera model. The tag is used only to indicate the picture-taking conditions in the reader.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_DEVICE_SETTING_DESCRIPTION
#endif
                                                   },
                                                   {"Exif.DigitalZoomRatio", TAG_EXIF, TAG_EXIF_DIGITALZOOMRATIO, N_("Digital Zoom Ratio"), N_("The digital zoom ratio when the image was shot. If the numerator of the recorded value is 0, this indicates that digital zoom was not used.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_DIGITAL_ZOOM_RATIO
#endif
                                                   },
                                                   {"Exif.DocumentName", TAG_EXIF, TAG_EXIF_DOCUMENTNAME, N_("Document Name"), N_("Document Name.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_DOCUMENT_NAME
#endif
                                                   },
                                                   {"Exif.ExifIfdPointer", TAG_EXIF, TAG_EXIF_EXIFIFDPOINTER, N_("Exif IFD Pointer"), N_("A pointer to the Exif IFD. Interoperability, Exif IFD has the same structure as that of the IFD specified in TIFF.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_EXIF_IFD_POINTER
#endif
                                                   },
                                                   {"Exif.ExifVersion", TAG_EXIF, TAG_EXIF_EXIFVERSION, N_("Exif Version"), N_("The version of Exif standard supported. Nonexistence of this field is taken to mean nonconformance to the standard.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_EXIF_VERSION
#endif
                                                   },
                                                   {"Exif.ExposureBiasValue", TAG_EXIF, TAG_EXIF_EXPOSUREBIASVALUE, N_("Exposure Bias"), N_("The exposure bias. The units is the APEX value. Ordinarily it is given in the range of -99.99 to 99.99.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_EXPOSURE_BIAS_VALUE
#endif
                                                   },
                                                   {"Exif.ExposureIndex", TAG_EXIF, TAG_EXIF_EXPOSUREINDEX, N_("Exposure Index"), N_("The exposure index selected on the camera or input device at the time the image is captured.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_EXPOSURE_INDEX
#endif
                                                   },
                                                   {"Exif.ExposureMode", TAG_EXIF, TAG_EXIF_EXPOSUREMODE, N_("Exposure Mode"), N_("This tag indicates the exposure mode set when the image was shot. In auto-bracketing mode, the camera shoots a series of frames of the same scene at different exposure settings.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_EXPOSURE_MODE
#endif
                                                   },
                                                   {"Exif.ExposureProgram", TAG_EXIF, TAG_EXIF_EXPOSUREPROGRAM, N_("Exposure Program"), N_("The class of the program used by the camera to set exposure when the picture is taken.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_EXPOSURE_PROGRAM
#endif
                                                   },
                                                   {"Exif.ExposureTime", TAG_EXIF, TAG_EXIF_EXPOSURETIME, N_("Exposure Time"), N_("Exposure time, given in seconds.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_EXPOSURE_TIME
#endif
                                                   },
                                                   {"Exif.FileSource", TAG_EXIF, TAG_EXIF_FILESOURCE, N_("File Source"), N_("Indicates the image source. If a DSC recorded the image, this tag value of this tag always be set to 3, indicating that the image was recorded on a DSC.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_FILE_SOURCE
#endif
                                                   },
                                                   {"Exif.FillOrder", TAG_EXIF, TAG_EXIF_FILLORDER, N_("Fill Order"), N_("Fill Order.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_FILL_ORDER
#endif
                                                   },
                                                   {"Exif.Flash", TAG_EXIF, TAG_EXIF_FLASH, N_("Flash"), N_("This tag is recorded when an image is taken using a strobe light (flash).")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_FLASH
#endif
                                                   },
                                                   {"Exif.FlashEnergy", TAG_EXIF, TAG_EXIF_FLASHENERGY, N_("Flash Energy"), N_("Indicates the strobe energy at the time the image is captured, as measured in Beam Candle Power Seconds (BCPS).")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_FLASH_ENERGY
#endif
                                                   },
                                                   {"Exif.FlashPixVersion", TAG_EXIF, TAG_EXIF_FLASHPIXVERSION, N_("FlashPixVersion"), N_("The FlashPix format version supported by a FPXR file.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_FLASH_PIX_VERSION
#endif
                                                   },
                                                   {"Exif.FNumber", TAG_EXIF, TAG_EXIF_FNUMBER, N_("F Number"), N_("The F number.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_FNUMBER
#endif
                                                   },
                                                   {"Exif.FocalLength", TAG_EXIF, TAG_EXIF_FOCALLENGTH, N_("Focal Length"), N_("The actual focal length of the lens, in mm. Conversion is not made to the focal length of a 35 mm film camera.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_FOCAL_LENGTH
#endif
                                                   },
                                                   {"Exif.FocalLengthIn35mmFilm", TAG_EXIF, TAG_EXIF_FOCALLENGTHIN35MMFILM, N_("Focal Length In 35mm Film"), N_("This tag indicates the equivalent focal length assuming a 35mm film camera, in mm. A value of 0 means the focal length is unknown. Note that this tag differs from the <Exif.FocalLength> tag.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_FOCAL_LENGTH_IN_35MM_FILM
#endif
                                                   },
                                                   {"Exif.FocalPlaneResolutionUnit", TAG_EXIF, TAG_EXIF_FOCALPLANERESOLUTIONUNIT, N_("Focal Plane Resolution Unit"), N_("Indicates the unit for measuring <Exif.FocalPlaneXResolution> and <Exif.FocalPlaneYResolution>. This value is the same as the <Exif.ResolutionUnit>.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_FOCAL_PLANE_RESOLUTION_UNIT
#endif
                                                   },
                                                   {"Exif.FocalPlaneXResolution", TAG_EXIF, TAG_EXIF_FOCALPLANEXRESOLUTION, N_("Focal Plane x-Resolution"), N_("Indicates the number of pixels in the image width (X) direction per <Exif.FocalPlaneResolutionUnit> on the camera focal plane.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_FOCAL_PLANE_X_RESOLUTION
#endif
                                                   },
                                                   {"Exif.FocalPlaneYResolution", TAG_EXIF, TAG_EXIF_FOCALPLANEYRESOLUTION, N_("Focal Plane y-Resolution"), N_("Indicates the number of pixels in the image height (V) direction per <Exif.FocalPlaneResolutionUnit> on the camera focal plane.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_FOCAL_PLANE_Y_RESOLUTION
#endif
                                                   },
                                                   {"Exif.GainControl", TAG_EXIF, TAG_EXIF_GAINCONTROL, N_("Gain Control"), N_("This tag indicates the degree of overall image gain adjustment.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_GAIN_CONTROL
#endif
                                                   },
                                                   {"Exif.Gamma", TAG_EXIF, TAG_EXIF_GAMMA, N_("Gamma"), N_("Indicates the value of coefficient gamma.")
#ifdef HAVE_EXIF
//                                                   , EXIF_TAG_GAMMA
#endif
                                                   },
                                                   {"Exif.GPS.Altitude", TAG_EXIF, TAG_EXIF_GPSALTITUDE, N_("Altitude"), N_("Indicates the altitude based on the reference in <Exif.GPS.AltitudeRef>. The reference unit is meters.")
#ifdef HAVE_EXIF
//                                                   , EXIF_TAG_GPS_ALTITUDE
#endif
                                                   },
                                                   {"Exif.GPS.AltitudeRef", TAG_EXIF, TAG_EXIF_GPSALTITUDEREF, N_("Altitude Reference"), N_("Indicates the altitude used as the reference altitude. If the reference is sea level and the altitude is above sea level, 0 is given. If the altitude is below sea level, a value of 1 is given and the altitude is indicated as an absolute value in the <Exif.GPS.Altitude> tag. The reference unit is meters.")
#ifdef HAVE_EXIF
//                                                   , EXIF_TAG_GPS_ALTITUDE_REF
#endif
                                                   },
                                                   {"Exif.GPS.InfoIFDPointer", TAG_EXIF, TAG_EXIF_GPSINFOIFDPOINTER, N_("GPS Info IFDPointer"), N_("A pointer to the GPS Info IFD. The Interoperability structure of the GPS Info IFD, like that of Exif IFD, has no image data.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_GPS_INFO_IFD_POINTER
#endif
                                                   },
                                                   {"Exif.GPS.Latitude", TAG_EXIF, TAG_EXIF_GPSLATITUDE, N_("Latitude"), N_("Indicates the latitude. The latitude is expressed as three RATIONAL values giving the degrees, minutes, and seconds, respectively. When degrees, minutes and seconds are expressed, the format is dd/1,mm/1,ss/1. When degrees and minutes are used and, for example, fractions of minutes are given up to two decimal places, the format is dd/1,mmmm/100,0/1.")
#ifdef HAVE_EXIF
//                                                   , EXIF_TAG_GPS_LATITUDE
#endif
                                                   },
                                                   {"Exif.GPS.LatitudeRef", TAG_EXIF, TAG_EXIF_GPSLATITUDEREF, N_("North or South Latitude"), N_("Indicates whether the latitude is north or south latitude. The ASCII value 'N' indicates north latitude, and 'S' is south latitude.")
#ifdef HAVE_EXIF
//                                                   , EXIF_TAG_GPS_LATITUDE_REF
#endif
                                                   },
                                                   {"Exif.GPS.Longitude", TAG_EXIF, TAG_EXIF_GPSLONGITUDE, N_("Longitude"), N_("Indicates the longitude. The longitude is expressed as three RATIONAL values giving the degrees, minutes, and seconds, respectively. When degrees, minutes and seconds are expressed, the format is ddd/1,mm/1,ss/1. When degrees and minutes are used and, for example, fractions of minutes are given up to two decimal places, the format is ddd/1,mmmm/100,0/1.")
#ifdef HAVE_EXIF
//                                                   , EXIF_TAG_GPS_LONGITUDE
#endif
                                                   },
                                                   {"Exif.GPS.LongitudeRef", TAG_EXIF, TAG_EXIF_GPSLONGITUDEREF, N_("East or West Longitude"), N_("Indicates whether the longitude is east or west longitude. ASCII 'E' indicates east longitude, and 'W' is west longitude.")
#ifdef HAVE_EXIF
//                                                   , EXIF_TAG_GPS_LONGITUDE_REF
#endif
                                                   },
                                                   {"Exif.GPS.VersionID", TAG_EXIF, TAG_EXIF_GPSVERSIONID, N_("GPS tag version"), N_("Indicates the version of <Exif.GPS.InfoIFD>. This tag is mandatory when <Exif.GPS.Info> tag is present.")
#ifdef HAVE_EXIF
//                                                   , EXIF_TAG_GPS_VERSION_ID
#endif
                                                   },
                                                   {"Exif.ImageDescription", TAG_EXIF, TAG_EXIF_IMAGEDESCRIPTION, N_("Image Description"), N_("A character string giving the title of the image. Two-bytes character codes cannot be used. When a 2-bytes code is necessary, the Exif Private tag <Exif.UserComment> is to be used.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_IMAGE_DESCRIPTION
#endif
                                                   },
                                                   {"Exif.ImageLength", TAG_EXIF, TAG_EXIF_IMAGELENGTH, N_("Image Length"), N_("The number of rows of image data. In JPEG compressed data a JPEG marker is used instead of this tag.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_IMAGE_LENGTH
#endif
                                                   },
                                                   {"Exif.ImageResources", TAG_EXIF, TAG_EXIF_IMAGERESOURCES, N_("Image Resources Block"), N_("Image Resources Block.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_IMAGE_RESOURCES
#endif
                                                   },
                                                   {"Exif.ImageUniqueID", TAG_EXIF, TAG_EXIF_IMAGEUNIQUEID, N_("Image Unique ID"), N_("This tag indicates an identifier assigned uniquely to each image. It is recorded as an ASCII string equivalent to hexadecimal notation and 128-bit fixed length.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_IMAGE_UNIQUE_ID
#endif
                                                   },
                                                   {"Exif.ImageWidth", TAG_EXIF, TAG_EXIF_IMAGEWIDTH, N_("Image Width"), N_("The number of columns of image data, equal to the number of pixels per row. In JPEG compressed data a JPEG marker is used instead of this tag.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_IMAGE_WIDTH
#endif
                                                   },
                                                   {"Exif.InterColorProfile", TAG_EXIF, TAG_EXIF_INTERCOLORPROFILE, N_("Inter Color Profile"), N_("Inter Color Profile.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_INTER_COLOR_PROFILE
#endif
                                                   },
                                                   {"Exif.InteroperabilityIFDPointer", TAG_EXIF, TAG_EXIF_INTEROPERABILITYIFDPOINTER, N_("Interoperability IFD Pointer"), N_("Interoperability IFD is composed of tags which stores the information to ensure the Interoperability and pointed by the following tag located in Exif IFD. The Interoperability structure of Interoperability IFD is the same as TIFF defined IFD structure but does not contain the image data characteristically compared with normal TIFF IFD.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_INTEROPERABILITY_IFD_POINTER
#endif
                                                   },
                                                   {"Exif.InteroperabilityIndex", TAG_EXIF, TAG_EXIF_INTEROPERABILITYINDEX, N_("Interoperability Index"), N_("Indicates the identification of the Interoperability rule. Use \"R98\" for stating ExifR98 Rules. Four bytes used including the termination code (NULL).")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_INTEROPERABILITY_INDEX
#endif
                                                   },
                                                   {"Exif.InteroperabilityVersion", TAG_EXIF, TAG_EXIF_INTEROPERABILITYVERSION, N_("Interoperability Version"), N_("Interoperability Version.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_INTEROPERABILITY_VERSION
#endif
                                                   },
                                                   {"Exif.IPTC_NAA", TAG_EXIF, TAG_EXIF_IPTCNAA, N_("IPTC/NAA"), N_("IPTC/NAA")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_IPTC_NAA
#endif
                                                   },
                                                   {"Exif.ISOSpeedRatings", TAG_EXIF, TAG_EXIF_ISOSPEEDRATINGS, N_("ISO Speed Ratings"), N_("Indicates the ISO Speed and ISO Latitude of the camera or input device as specified in ISO 12232.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_ISO_SPEED_RATINGS
#endif
                                                   },
                                                   {"Exif.JPEGInterchangeFormat", TAG_EXIF, TAG_EXIF_JPEGINTERCHANGEFORMAT, N_("JPEG Interchange Format"), N_("The offset to the start byte (SOI) of JPEG compressed thumbnail data. This is not used for primary image JPEG data.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_JPEG_INTERCHANGE_FORMAT
#endif
                                                   },
                                                   {"Exif.JPEGInterchangeFormatLength", TAG_EXIF, TAG_EXIF_JPEGINTERCHANGEFORMATLENGTH, N_("JPEG Interchange Format Length"), N_("The number of bytes of JPEG compressed thumbnail data. This is not used for primary image JPEG data. JPEG thumbnails are not divided but are recorded as a continuous JPEG bitstream from SOI to EOI. Appn and COM markers should not be recorded. Compressed thumbnails must be recorded in no more than 64 Kbytes, including all other data to be recorded in APP1.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_JPEG_INTERCHANGE_FORMAT_LENGTH
#endif
                                                   },
                                                   {"Exif.JPEGProc", TAG_EXIF, TAG_EXIF_JPEGPROC, N_("JPEG Proc"), N_("JPEG Procedure")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_JPEG_PROC
#endif
                                                   },
                                                   {"Exif.LightSource", TAG_EXIF, TAG_EXIF_LIGHTSOURCE, N_("Light Source"), N_("The kind of light source.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_LIGHT_SOURCE
#endif
                                                   },
                                                   {"Exif.Make", TAG_EXIF, TAG_EXIF_MAKE, N_("Manufacturer"), N_("The manufacturer of the recording equipment. This is the manufacturer of the DSC, scanner, video digitizer or other equipment that generated the image. When the field is left blank, it is treated as unknown.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_MAKE
#endif
                                                   },
                                                   {"Exif.MakerNote", TAG_EXIF, TAG_EXIF_MAKERNOTE, N_("Maker Note"), N_("A tag for manufacturers of Exif writers to record any desired information. The contents are up to the manufacturer.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_MAKER_NOTE
#endif
                                                   },
                                                   {"Exif.MaxApertureValue", TAG_EXIF, TAG_EXIF_MAXAPERTUREVALUE, N_("Max Aperture Value"), N_("The smallest F number of the lens. The unit is the APEX value. Ordinarily it is given in the range of 00.00 to 99.99, but it is not limited to this range.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_MAX_APERTURE_VALUE
#endif
                                                   },
                                                   {"Exif.MeteringMode", TAG_EXIF, TAG_EXIF_METERINGMODE, N_("Metering Mode"), N_("The metering mode.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_METERING_MODE
#endif
                                                   },
                                                   {"Exif.Model", TAG_EXIF, TAG_EXIF_MODEL, N_("Model"), N_("The model name or model number of the equipment. This is the model name or number of the DSC, scanner, video digitizer or other equipment that generated the image. When the field is left blank, it is treated as unknown.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_MODEL
#endif
                                                   },
                                                   {"Exif.CFAPattern", TAG_EXIF, TAG_EXIF_NEWCFAPATTERN, N_("CFA Pattern"), N_("Indicates the color filter array (CFA) geometric pattern of the image sensor when a one-chip color area sensor is used. It does not apply to all sensing methods.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_NEW_CFA_PATTERN
#endif
                                                   },
                                                   {"Exif.NewSubfileType", TAG_EXIF, TAG_EXIF_NEWSUBFILETYPE, N_("New Subfile Type"), N_("A general indication of the kind of data contained in this subfile.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_NEW_SUBFILE_TYPE
#endif
                                                   },
                                                   {"Exif.OECF", TAG_EXIF, TAG_EXIF_OECF, N_("OECF"), N_("Indicates the Opto-Electoric Conversion Function (OECF) specified in ISO 14524. <Exif.OECF> is the relationship between the camera optical input and the image values.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_OECF
#endif
                                                   },
                                                   {"Exif.Orientation", TAG_EXIF, TAG_EXIF_ORIENTATION, N_("Orientation"), N_("The image orientation viewed in terms of rows and columns.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_ORIENTATION
#endif
                                                   },
                                                   {"Exif.PhotometricInterpretation", TAG_EXIF, TAG_EXIF_PHOTOMETRICINTERPRETATION, N_("Photometric Interpretation"), N_("The pixel composition. In JPEG compressed data a JPEG marker is used instead of this tag.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_PHOTOMETRIC_INTERPRETATION
#endif
                                                   },
                                                   {"Exif.PixelXDimension", TAG_EXIF, TAG_EXIF_PIXELXDIMENSION, N_("Pixel X Dimension"), N_("Information specific to compressed data. When a compressed file is recorded, the valid width of the meaningful image must be recorded in this tag, whether or not there is padding data or a restart marker. This tag should not exist in an uncompressed file.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_PIXEL_X_DIMENSION
#endif
                                                   },
                                                   {"Exif.PixelYDimension", TAG_EXIF, TAG_EXIF_PIXELYDIMENSION, N_("Pixel Y Dimension"), N_("Information specific to compressed data. When a compressed file is recorded, the valid height of the meaningful image must be recorded in this tag, whether or not there is padding data or a restart marker. This tag should not exist in an uncompressed file. Since data padding is unnecessary in the vertical direction, the number of lines recorded in this valid image height tag will in fact be the same as that recorded in the SOF.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_PIXEL_Y_DIMENSION
#endif
                                                   },
                                                   {"Exif.PlanarConfiguration", TAG_EXIF, TAG_EXIF_PLANARCONFIGURATION, N_("Planar Configuration"), N_("Indicates whether pixel components are recorded in a chunky or planar format. In JPEG compressed files a JPEG marker is used instead of this tag. If this field does not exist, the TIFF default of 1 (chunky) is assumed.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_PLANAR_CONFIGURATION
#endif
                                                   },
                                                   {"Exif.PrimaryChromaticities", TAG_EXIF, TAG_EXIF_PRIMARYCHROMATICITIES, N_("Primary Chromaticities"), N_("The chromaticity of the three primary colors of the image. Normally this tag is not necessary, since colorspace is specified in <Exif.ColorSpace> tag.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_PRIMARY_CHROMATICITIES
#endif
                                                   },
                                                   {"Exif.ReferenceBlackWhite", TAG_EXIF, TAG_EXIF_REFERENCEBLACKWHITE, N_("Reference Black/White"), N_("The reference black point value and reference white point value. No defaults are given in TIFF, but the values below are given as defaults here. The color space is declared in a color space information tag, with the default being the value that gives the optimal image characteristics Interoperability these conditions.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_REFERENCE_BLACK_WHITE
#endif
                                                   },
                                                   {"Exif.RelatedImageFileFormat", TAG_EXIF, TAG_EXIF_RELATEDIMAGEFILEFORMAT, N_("Related Image File Format"), N_("Related Image File Format.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_RELATED_IMAGE_FILE_FORMAT
#endif
                                                   },
                                                   {"Exif.RelatedImageLength", TAG_EXIF, TAG_EXIF_RELATEDIMAGELENGTH, N_("Related Image Length"), N_("Related Image Length.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_RELATED_IMAGE_LENGTH
#endif
                                                   },
                                                   {"Exif.RelatedImageWidth", TAG_EXIF, TAG_EXIF_RELATEDIMAGEWIDTH, N_("Related Image Width"), N_("Related Image Width.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_RELATED_IMAGE_WIDTH
#endif
                                                   },
                                                   {"Exif.RelatedSoundFile", TAG_EXIF, TAG_EXIF_RELATEDSOUNDFILE, N_("Related Sound File"), N_("This tag is used to record the name of an audio file related to the image data. The only relational information recorded here is the Exif audio file name and extension (an ASCII string consisting of 8 characters + '.' + 3 characters). The path is not recorded. When using this tag, audio files must be recorded in conformance to the Exif audio format. Writers are also allowed to store the data such as Audio within APP2 as FlashPix extension stream data. Audio files must be recorded in conformance to the Exif audio format. If multiple files are mapped to one file, the above format is used to record just one audio file name. If there are multiple audio files, the first recorded file is given. When there are three Exif audio files \"SND00001.WAV\", \"SND00002.WAV\" and \"SND00003.WAV\", the Exif image file name for each of them, \"DSC00001.JPG\", is indicated. By combining multiple relational information, a variety of playback possibilities can be supported. The method of using relational information is left to the implementation on the playback side. Since this information is an ASCII character string, it is terminated by NULL. When this tag is used to map audio files, the relation of the audio file to image data must also be indicated on the audio file end.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_RELATED_SOUND_FILE
#endif
                                                   },
                                                   {"Exif.ResolutionUnit", TAG_EXIF, TAG_EXIF_RESOLUTIONUNIT, N_("Resolution Unit"), N_("The unit for measuring <Exif.XResolution> and <Exif.YResolution>. The same unit is used for both <Exif.XResolution> and <Exif.YResolution>. If the image resolution is unknown, 2 (inches) is designated.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_RESOLUTION_UNIT
#endif
                                                   },
                                                   {"Exif.RowsPerStrip", TAG_EXIF, TAG_EXIF_ROWSPERSTRIP, N_("Rows per Strip"), N_("The number of rows per strip. This is the number of rows in the image of one strip when an image is divided into strips. With JPEG compressed data this designation is not needed and is omitted.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_ROWS_PER_STRIP
#endif
                                                   },
                                                   {"Exif.SamplesPerPixel", TAG_EXIF, TAG_EXIF_SAMPLESPERPIXEL, N_("Samples per Pixel"), N_("The number of components per pixel. Since this standard applies to RGB and YCbCr images, the value set for this tag is 3. In JPEG compressed data a JPEG marker is used instead of this tag.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_SAMPLES_PER_PIXEL
#endif
                                                   },
                                                   {"Exif.Saturation", TAG_EXIF, TAG_EXIF_SATURATION, N_("Saturation"), N_("This tag indicates the direction of saturation processing applied by the camera when the image was shot.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_SATURATION
#endif
                                                   },
                                                   {"Exif.SceneCaptureType", TAG_EXIF, TAG_EXIF_SCENECAPTURETYPE, N_("Scene Capture Type"), N_("This tag indicates the type of scene that was shot. It can also be used to record the mode in which the image was shot. Note that this differs from <Exif.SceneType> tag.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_SCENE_CAPTURE_TYPE
#endif
                                                   },
                                                   {"Exif.SceneType", TAG_EXIF, TAG_EXIF_SCENETYPE, N_("Scene Type"), N_("Indicates the type of scene. If a DSC recorded the image, this tag value must always be set to 1, indicating that the image was directly photographed.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_SCENE_TYPE
#endif
                                                   },
                                                   {"Exif.SensingMethod", TAG_EXIF, TAG_EXIF_SENSINGMETHOD, N_("Sensing Method"), N_("Indicates the image sensor type on the camera or input device.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_SENSING_METHOD
#endif
                                                   },
                                                   {"Exif.Sharpness", TAG_EXIF, TAG_EXIF_SHARPNESS, N_("Sharpness"), N_("This tag indicates the direction of sharpness processing applied by the camera when the image was shot.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_SHARPNESS
#endif
                                                   },
                                                   {"Exif.ShutterSpeedValue", TAG_EXIF, TAG_EXIF_SHUTTERSPEEDVALUE, N_("Shutter speed"), N_("Shutter speed. The unit is the APEX (Additive System of Photographic Exposure) setting.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_SHUTTER_SPEED_VALUE
#endif
                                                   },
                                                   {"Exif.Software", TAG_EXIF, TAG_EXIF_SOFTWARE, N_("Software"), N_("This tag records the name and version of the software or firmware of the camera or image input device used to generate the image. When the field is left blank, it is treated as unknown.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_SOFTWARE
#endif
                                                   },
                                                   {"Exif.SpatialFrequencyResponse", TAG_EXIF, TAG_EXIF_SPATIALFREQUENCYRESPONSE, N_("Spatial Frequency Response"), N_("This tag records the camera or input device spatial frequency table and SFR values in the direction of image width, image height, and diagonal direction, as specified in ISO 12233.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_SPATIAL_FREQUENCY_RESPONSE
#endif
                                                   },
                                                   {"Exif.SpectralSensitivity", TAG_EXIF, TAG_EXIF_SPECTRALSENSITIVITY, N_("Spectral Sensitivity"), N_("Indicates the spectral sensitivity of each channel of the camera used.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_SPECTRAL_SENSITIVITY
#endif
                                                   },
                                                   {"Exif.StripByteCounts", TAG_EXIF, TAG_EXIF_STRIPBYTECOUNTS, N_("Strip Byte Count"), N_("The total number of bytes in each strip. With JPEG compressed data this designation is not needed and is omitted.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_STRIP_BYTE_COUNTS
#endif
                                                   },
                                                   {"Exif.StripOffsets", TAG_EXIF, TAG_EXIF_STRIPOFFSETS, N_("Strip Offsets"), N_("For each strip, the byte offset of that strip. It is recommended that this be selected so the number of strip bytes does not exceed 64 Kbytes. With JPEG compressed data this designation is not needed and is omitted.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_STRIP_OFFSETS
#endif
                                                   },
                                                   {"Exif.SubIFDs", TAG_EXIF, TAG_EXIF_SUBIFDS, N_("Sub IFD Offsets"), N_("Defined by Adobe Corporation to enable TIFF Trees within a TIFF file.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_SUB_IFDS
#endif
                                                   },
                                                   {"Exif.SubjectArea", TAG_EXIF, TAG_EXIF_SUBJECTAREA, N_("Subject Area"), N_("This tag indicates the location and area of the main subject in the overall scene.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_SUBJECT_AREA
#endif
                                                   },
                                                   {"Exif.SubjectDistance", TAG_EXIF, TAG_EXIF_SUBJECTDISTANCE, N_("Subject Distance"), N_("The distance to the subject, given in meters.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_SUBJECT_DISTANCE
#endif
                                                   },
                                                   {"Exif.SubjectDistanceRange", TAG_EXIF, TAG_EXIF_SUBJECTDISTANCERANGE, N_("Subject Distance Range"), N_("This tag indicates the distance to the subject.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_SUBJECT_DISTANCE_RANGE
#endif
                                                   },
                                                   {"Exif.SubjectLocation", TAG_EXIF, TAG_EXIF_SUBJECTLOCATION, N_("Subject Location"), N_("Indicates the location of the main subject in the scene. The value of this tag represents the pixel at the center of the main subject relative to the left edge, prior to rotation processing as per the <Exif.Rotation> tag. The first value indicates the X column number and second indicates the Y row number.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_SUBJECT_LOCATION
#endif
                                                   },
                                                   {"Exif.SubsecTime", TAG_EXIF, TAG_EXIF_SUBSECTIME, N_("Subsec Time"), N_("A tag used to record fractions of seconds for the <Exif.DateTime> tag.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_SUB_SEC_TIME
#endif
                                                   },
                                                   {"Exif.SubSecTimeDigitized", TAG_EXIF, TAG_EXIF_SUBSECTIMEDIGITIZED, N_("Subsec Time Digitized"), N_("A tag used to record fractions of seconds for the <Exif.DateTimeDigitized> tag.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_SUB_SEC_TIME_DIGITIZED
#endif
                                                   },
                                                   {"Exif.SubSecTimeOriginal", TAG_EXIF, TAG_EXIF_SUBSECTIMEORIGINAL, N_("Subsec Time Original"), N_("A tag used to record fractions of seconds for the <Exif.DateTimeOriginal> tag.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_SUB_SEC_TIME_ORIGINAL
#endif
                                                   },
                                                   {"Exif.TIFF/EPStandardID", TAG_EXIF, TAG_EXIF_TIFFEPSTANDARDID, N_("TIFF/EP Standard ID"), N_("TIFF/EP Standard ID.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_TIFF_EP_STANDARD_ID
#endif
                                                   },
                                                   {"Exif.TransferFunction", TAG_EXIF, TAG_EXIF_TRANSFERFUNCTION, N_("Transfer Function"), N_("A transfer function for the image, described in tabular style. Normally this tag is not necessary, since color space is specified in <Exif.ColorSpace> tag.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_TRANSFER_FUNCTION
#endif
                                                   },
                                                   {"Exif.TransferRange", TAG_EXIF, TAG_EXIF_TRANSFERRANGE, N_("Transfer Range"), N_("Transfer Range.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_TRANSFER_RANGE
#endif
                                                   },
                                                   {"Exif.UserComment", TAG_EXIF, TAG_EXIF_USERCOMMENT, N_("User Comment"), N_("A tag for Exif users to write keywords or comments on the image besides those in <Exif.ImageDescription>, and without the character code limitations of the <Exif.ImageDescription> tag. The character code used in the <Exif.UserComment> tag is identified based on an ID code in a fixed 8-byte area at the start of the tag data area. The unused portion of the area is padded with NULL (\"00.h\"). ID codes are assigned by means of registration. The value of CountN is determinated based on the 8 bytes in the character code area and the number of bytes in the user comment part. Since the TYPE is not ASCII, NULL termination is not necessary. The ID code for the <Exif.UserComment> area may be a Defined code such as JIS or ASCII, or may be Undefined. The Undefined name is UndefinedText, and the ID code is filled with 8 bytes of all \"NULL\" (\"00.H\"). An Exif reader that reads the <Exif.UserComment> tag must have a function for determining the ID code. This function is not required in Exif readers that do not use the <Exif.UserComment> tag. When a <Exif.UserComment> area is set aside, it is recommended that the ID code be ASCII and that the following user comment part be filled with blank characters [20.H].")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_USER_COMMENT
#endif
                                                   },
                                                   {"Exif.WhiteBalance", TAG_EXIF, TAG_EXIF_WHITEBALANCE, N_("White Balance"), N_("The white balance mode set when the image was shot.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_WHITE_BALANCE
#endif
                                                   },
                                                   {"Exif.WhitePoint", TAG_EXIF, TAG_EXIF_WHITEPOINT, N_("White Point"), N_("The chromaticity of the white point of the image. Normally this tag is not necessary, since color space is specified in <Exif.ColorSpace> tag.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_WHITE_POINT
#endif
                                                   },
                                                   {"Exif.XMLPacket", TAG_EXIF, TAG_EXIF_XMLPACKET, N_("XML Packet"), N_("XMP Metadata.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_XML_PACKET
#endif
                                                   },
                                                   {"Exif.XResolution", TAG_EXIF, TAG_EXIF_XRESOLUTION, N_("x Resolution"), N_("The number of pixels per <Exif.ResolutionUnit> in the <Exif.ImageWidth> direction. When the image resolution is unknown, 72 [dpi] is designated.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_X_RESOLUTION
#endif
                                                   },
                                                   {"Exif.YCbCrCoefficients", TAG_EXIF, TAG_EXIF_YCBCRCOEFFICIENTS, N_("YCbCr Coefficients"), N_("The matrix coefficients for transformation from RGB to YCbCr image data. No default is given in TIFF; but here \"Color Space Guidelines\" is used as the default. The color space is declared in a color space information tag, with the default being the value that gives the optimal image characteristics Interoperability this condition.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_YCBCR_COEFFICIENTS
#endif
                                                   },
                                                   {"Exif.YCbCrPositioning", TAG_EXIF, TAG_EXIF_YCBCRPOSITIONING, N_("YCbCr Positioning"), N_("The position of chrominance components in relation to the luminance component. This field is designated only for JPEG compressed data or uncompressed YCbCr data. The TIFF default is 1 (centered); but when Y:Cb:Cr = 4:2:2 it is recommended that 2 (co-sited) be used to record data, in order to improve the image quality when viewed on TV systems. When this field does not exist, the reader shall assume the TIFF default. In the case of Y:Cb:Cr = 4:2:0, the TIFF default (centered) is recommended. If the reader does not have the capability of supporting both kinds of <Exif.YCbCrPositioning>, it shall follow the TIFF default regardless of the value in this field. It is preferable that readers be able to support both centered and co-sited positioning.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_YCBCR_POSITIONING
#endif
                                                   },
                                                   {"Exif.YCbCrSubSampling", TAG_EXIF, TAG_EXIF_YCBCRSUBSAMPLING, N_("YCbCr Sub-Sampling"), N_("The sampling ratio of chrominance components in relation to the luminance component. In JPEG compressed data a JPEG marker is used instead of this tag.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_YCBCR_SUB_SAMPLING
#endif
                                                   },
                                                   {"Exif.YResolution", TAG_EXIF, TAG_EXIF_YRESOLUTION, N_("y Resolution"), N_("The number of pixels per <Exif.ResolutionUnit> in the <Exif.ImageLength> direction. The same value as <Exif.XResolution> is designated.")
#ifdef HAVE_EXIF
                                                   , EXIF_TAG_Y_RESOLUTION
#endif
                                                   },
                                                   {"File.Accessed", TAG_FILE, TAG_FILE_ACCESSED, N_("Accessed"), N_("Last access datetime.")},
                                                   {"File.Content", TAG_FILE, TAG_FILE_CONTENT, N_("Content"), N_("File's contents filtered as plain text.")},
                                                   {"File.Description", TAG_FILE, TAG_FILE_DESCRIPTION, N_("Description"), N_("Editable free text/notes.")},
                                                   {"File.Format", TAG_FILE, TAG_FILE_FORMAT, N_("Format"), N_("MIME type of the file or if a directory it should contain value \"Folder\"")},
                                                   {"File.IconPath", TAG_FILE, TAG_FILE_ICONPATH, N_("Icon Path"), N_("Editable file URI for a custom icon for the file.")},
                                                   {"File.Keywords", TAG_FILE, TAG_FILE_KEYWORDS, N_("Keywords"), N_("Editable array of keywords.")},
                                                   {"File.LargeThumbnailPath", TAG_FILE, TAG_FILE_LARGETHUMBNAILPATH, N_("Large Thumbnail Path"), N_("Editable file URI for a larger thumbnail of the file suitable for previews.")},
                                                   {"File.Link", TAG_FILE, TAG_FILE_LINK, N_("Link"), N_("URI of link target.")},
                                                   {"File.Modified", TAG_FILE, TAG_FILE_MODIFIED, N_("Modified"), N_("Last modified datetime.")},
                                                   {"File.Name", TAG_FILE, TAG_FILE_NAME, N_("Name"), N_("File name excluding path but including the file extension.")},
                                                   {"File.Path", TAG_FILE, TAG_FILE_PATH, N_("Path"), N_("Full file path of file excluding the filename.")},
                                                   {"File.Permissions", TAG_FILE, TAG_FILE_PERMISSIONS, N_("Permissions"), N_("Permission string in unix format eg \"-rw-r--r--\".")},
                                                   {"File.Publisher", TAG_FILE, TAG_FILE_PUBLISHER, N_("Publisher"), N_("Editable DC type for the name of the publisher of the file (EG dc:publisher field in RSS feed).")},
                                                   {"File.Rank", TAG_FILE, TAG_FILE_RANK, N_("Rank"), N_("Editable file rank for grading favourites. Value should be in the range 1..10.")},
                                                   {"File.Size", TAG_FILE, TAG_FILE_SIZE, N_("Size"), N_("Size of the file in bytes or if a directory number of items it contains.")},
                                                   {"File.SmallThumbnailPath", TAG_FILE, TAG_FILE_SMALLTHUMBNAILPATH, N_("Small Thumbnail Path"), N_("Editable file URI for a small thumbnail of the file suitable for use in icon views.")},
                                                   {"ID3.Album", TAG_ID3, TAG_ID3_ALBUM, N_("Album"), N_("Name of the album.")
#ifdef HAVE_ID3
                                                   , ID3FID_ALBUM
#endif
                                                   },
                                                   {"ID3.AlbumSortOrder", TAG_ID3, TAG_ID3_ALBUMSORTORDER, N_("Album Sort Order"), N_("String which should be used instead of the album name for sorting purposes.")
#ifdef HAVE_ID3
                                                   , ID3FID_ALBUMSORTORDER
#endif
                                                   },
                                                   {"ID3.AudioCrypto", TAG_ID3, TAG_ID3_AUDIOCRYPTO, N_("Audio Encryption"), N_("Frame indicates if the audio stream is encrypted, and by whom.")
#ifdef HAVE_ID3
                                                   , ID3FID_AUDIOCRYPTO
#endif
                                                   },
                                                   {"ID3.AudioSeekPoint", TAG_ID3, TAG_ID3_AUDIOSEEKPOINT, N_("Audio Seek Point"), N_("Fractional offset within the audio data, providing a starting point from which to find an appropriate point to start decoding.")
#ifdef HAVE_ID3
                                                   , ID3FID_AUDIOSEEKPOINT
#endif
                                                   },
                                                   {"ID3.Band", TAG_ID3, TAG_ID3_BAND, N_("Band"), N_("Additional information about the performers in the recording.")
#ifdef HAVE_ID3
                                                   , ID3FID_BAND
#endif
                                                   },
                                                   {"ID3.Bitrate", TAG_ID3, TAG_ID3_BITRATE, N_("Bitrate"), N_("Bitrate in kbps.")},
                                                   {"ID3.BPM", TAG_ID3, TAG_ID3_BPM, N_("BPM"), N_("BPM (beats per minute).")
#ifdef HAVE_ID3
                                                   , ID3FID_BPM
#endif
                                                   },
                                                   {"ID3.BufferSize", TAG_ID3, TAG_ID3_BUFFERSIZE, N_("Buffer Size"), N_("Recommended buffer size.")
#ifdef HAVE_ID3
                                                   , ID3FID_BUFFERSIZE
#endif
                                                   },
                                                   {"ID3.CDID", TAG_ID3, TAG_ID3_CDID, N_("CD ID"), N_("Music CD identifier.")
#ifdef HAVE_ID3
                                                   , ID3FID_CDID
#endif
                                                   },
                                                   {"ID3.ChannelMode", TAG_ID3, TAG_ID3_CHANNELMODE, N_("Channel mode"), N_("Channel mode.")},
                                                   {"ID3.Channels", TAG_ID3, TAG_ID3_CHANNELS, N_("Channels"), N_("Number of channels in the audio (2 = stereo).")},
                                                   {"ID3.Comment", TAG_ID3, TAG_ID3_COMMENT, N_("Comment"), N_("Comments on the track.")
#ifdef HAVE_ID3
                                                   , ID3FID_COMMENT
#endif
                                                   },
                                                   {"ID3.Commercial", TAG_ID3, TAG_ID3_COMMERCIAL, N_("Commercial"), N_("Commercial frame.")
#ifdef HAVE_ID3
                                                   , ID3FID_COMMERCIAL
#endif
                                                   },
                                                   {"ID3.Composer", TAG_ID3, TAG_ID3_COMPOSER, N_("Composer"), N_("Composer.")
#ifdef HAVE_ID3
                                                   , ID3FID_COMPOSER
#endif
                                                   },
                                                   {"ID3.Conductor", TAG_ID3, TAG_ID3_CONDUCTOR, N_("Conductor"), N_("Conductor.")
#ifdef HAVE_ID3
                                                   , ID3FID_CONDUCTOR
#endif
                                                   },
                                                   {"ID3.ContentGroup", TAG_ID3, TAG_ID3_CONTENTGROUP, N_("Content Group"), N_("Content group description.")
#ifdef HAVE_ID3
                                                   , ID3FID_CONTENTGROUP
#endif
                                                   },
                                                   {"ID3.ContentType", TAG_ID3, TAG_ID3_CONTENTTYPE, N_("Content Type"), N_("Type of music classification for the track as defined in ID3 spec.")
#ifdef HAVE_ID3
                                                   , ID3FID_CONTENTTYPE
#endif
                                                   },
                                                   {"ID3.Copyright", TAG_ID3, TAG_ID3_COPYRIGHT, N_("Copyright"), N_("Copyright message.")
#ifdef HAVE_ID3
                                                   , ID3FID_COPYRIGHT
#endif
                                                   },
                                                   {"ID3.CryptoReg", TAG_ID3, TAG_ID3_CRYPTOREG, N_("Encryption Registration"), N_("Encryption method registration.")
#ifdef HAVE_ID3
                                                   , ID3FID_CRYPTOREG
#endif
                                                   },
                                                   {"ID3.Date", TAG_ID3, TAG_ID3_DATE, N_("Date"), N_("Date.")
#ifdef HAVE_ID3
                                                   , ID3FID_DATE
#endif
                                                   },
                                                   {"ID3.Duration", TAG_ID3, TAG_ID3_DURATION, N_("Duration"), N_("Duration of track in seconds.")},
                                                   {"ID3.Duration.MMSS", TAG_ID3, TAG_ID3_DURATIONMMSS, N_("Duration [MM:SS]"), N_("Duration of track as MM:SS.")},
                                                   {"ID3.Emphasis", TAG_ID3, TAG_ID3_EMPHASIS, N_("Emphasis"), N_("Emphasis.")},
                                                   {"ID3.EncodedBy", TAG_ID3, TAG_ID3_ENCODEDBY, N_("Encoded By"), N_("Person or organisation that encoded the audio file. This field may contain a copyright message, if the audio file also is copyrighted by the encoder.")
#ifdef HAVE_ID3
                                                   , ID3FID_ENCODEDBY
#endif
                                                   },
                                                   {"ID3.EncoderSettings", TAG_ID3, TAG_ID3_ENCODERSETTINGS, N_("Encoder Settings"), N_("Software.")
#ifdef HAVE_ID3
                                                   , ID3FID_ENCODERSETTINGS
#endif
                                                   },
                                                   {"ID3.EncodingTime", TAG_ID3, TAG_ID3_ENCODINGTIME, N_("Encoding Time"), N_("Encoding time.")
#ifdef HAVE_ID3
                                                   , ID3FID_ENCODINGTIME
#endif
                                                   },
                                                   {"ID3.Equalization", TAG_ID3, TAG_ID3_EQUALIZATION, N_("Equalization"), N_("Equalization.")
#ifdef HAVE_ID3
                                                   , ID3FID_EQUALIZATION
#endif
                                                   },
                                                   {"ID3.Equalization2", TAG_ID3, TAG_ID3_EQUALIZATION2, N_("Equalization 2"), N_("Equalisation curve predifine within the audio file.")
#ifdef HAVE_ID3
                                                   , ID3FID_EQUALIZATION2
#endif
                                                   },
                                                   {"ID3.EventTiming", TAG_ID3, TAG_ID3_EVENTTIMING, N_("Event Timing"), N_("Event timing codes.")
#ifdef HAVE_ID3
                                                   , ID3FID_EVENTTIMING
#endif
                                                   },
                                                   {"ID3.FileOwner", TAG_ID3, TAG_ID3_FILEOWNER, N_("File Owner"), N_("File owner.")
#ifdef HAVE_ID3
                                                   , ID3FID_FILEOWNER
#endif
                                                   },
                                                   {"ID3.FileType", TAG_ID3, TAG_ID3_FILETYPE, N_("File Type"), N_("File type.")
#ifdef HAVE_ID3
                                                   , ID3FID_FILETYPE
#endif
                                                   },
                                                   {"ID3.Frames", TAG_ID3, TAG_ID3_FRAMES, N_("Frames"), N_("Number of frames.")},
                                                   {"ID3.GeneralObject", TAG_ID3, TAG_ID3_GENERALOBJECT, N_("General Object"), N_("General encapsulated object.")
#ifdef HAVE_ID3
                                                   , ID3FID_GENERALOBJECT
#endif
                                                   },
                                                   {"ID3.Genre", TAG_ID3, TAG_ID3_GENRE, N_("Genre"), N_("Type of music classification for the track as defined in ID3 spec.")},
                                                   {"ID3.GroupingReg", TAG_ID3, TAG_ID3_GROUPINGREG, N_("Grouping Registration"), N_("Group identification registration.")
#ifdef HAVE_ID3
                                                   , ID3FID_GROUPINGREG
#endif
                                                   },
                                                   {"ID3.InitialKey", TAG_ID3, TAG_ID3_INITIALKEY, N_("Initial Key"), N_("Initial key.")
#ifdef HAVE_ID3
                                                   , ID3FID_INITIALKEY
#endif
                                                   },
                                                   {"ID3.InvolvedPeople", TAG_ID3, TAG_ID3_INVOLVEDPEOPLE, N_("Involved People"), N_("Involved people list.")
#ifdef HAVE_ID3
                                                   , ID3FID_INVOLVEDPEOPLE
#endif
                                                   },
                                                   {"ID3.InvolvedPeople2", TAG_ID3, TAG_ID3_INVOLVEDPEOPLE2, N_("InvolvedPeople2"), N_("Involved people list.")
#ifdef HAVE_ID3
                                                   , ID3FID_INVOLVEDPEOPLE2
#endif
                                                   },
                                                   {"ID3.ISRC", TAG_ID3, TAG_ID3_ISRC, N_("ISRC"), N_("ISRC (international standard recording code).")
#ifdef HAVE_ID3
                                                   , ID3FID_ISRC
#endif
                                                   },
                                                   {"ID3.Language", TAG_ID3, TAG_ID3_LANGUAGE, N_("Language"), N_("Language.")
#ifdef HAVE_ID3
                                                   , ID3FID_LANGUAGE
#endif
                                                   },
                                                   {"ID3.LeadArtist", TAG_ID3, TAG_ID3_LEADARTIST, N_("Lead Artist"), N_("Lead performer(s).")
#ifdef HAVE_ID3
                                                   , ID3FID_LEADARTIST
#endif
                                                   },
                                                   {"ID3.LinkedInfo", TAG_ID3, TAG_ID3_LINKEDINFO, N_("Linked Info"), N_("Linked information.")
#ifdef HAVE_ID3
                                                   , ID3FID_LINKEDINFO
#endif
                                                   },
                                                   {"ID3.Lyricist", TAG_ID3, TAG_ID3_LYRICIST, N_("Lyricist"), N_("Lyricist.")
#ifdef HAVE_ID3
                                                   , ID3FID_LYRICIST
#endif
                                                   },
                                                   {"ID3.MediaType", TAG_ID3, TAG_ID3_MEDIATYPE, N_("Media Type"), N_("Media type.")
#ifdef HAVE_ID3
                                                   , ID3FID_MEDIATYPE
#endif
                                                   },
                                                   {"ID3.MixArtist", TAG_ID3, TAG_ID3_MIXARTIST, N_("Mix Artist"), N_("Interpreted, remixed, or otherwise modified by.")
#ifdef HAVE_ID3
                                                   , ID3FID_MIXARTIST
#endif
                                                   },
                                                   {"ID3.Mood", TAG_ID3, TAG_ID3_MOOD, N_("Mood"), N_("Mood.")
#ifdef HAVE_ID3
                                                   , ID3FID_MOOD
#endif
                                                   },
                                                   {"ID3.MPEG.Layer", TAG_ID3, TAG_ID3_MPEGLAYER, N_("Layer"), N_("MPEG layer.")},
                                                   {"ID3.MPEG.Lookup", TAG_ID3, TAG_ID3_MPEGLOOKUP, N_("MPEG Lookup"), N_("MPEG location lookup table.")
#ifdef HAVE_ID3
                                                   , ID3FID_MPEGLOOKUP
#endif
                                                   },
                                                   {"ID3.MPEG.Version", TAG_ID3, TAG_ID3_MPEGVERSION, N_("MPEG Version"), N_("MPEG version.")},
                                                   {"ID3.MusicianCreditList", TAG_ID3, TAG_ID3_MUSICIANCREDITLIST, N_("Musician Credit List"), N_("Musician credits list.")
#ifdef HAVE_ID3
                                                   , ID3FID_MUSICIANCREDITLIST
#endif
                                                   },
                                                   {"ID3.NetRadioOwner", TAG_ID3, TAG_ID3_NETRADIOOWNER, N_("Net Radio Owner"), N_("Internet radio station owner.")
#ifdef HAVE_ID3
                                                   , ID3FID_NETRADIOOWNER
#endif
                                                   },
                                                   {"ID3.NetRadiostation", TAG_ID3, TAG_ID3_NETRADIOSTATION, N_("Net Radiostation"), N_("Internet radio station name.")
#ifdef HAVE_ID3
                                                   , ID3FID_NETRADIOSTATION
#endif
                                                   },
                                                   {"ID3.OriginalAlbum", TAG_ID3, TAG_ID3_ORIGALBUM, N_("Original Album"), N_("Original album.")
#ifdef HAVE_ID3
                                                   , ID3FID_ORIGALBUM
#endif
                                                   },
                                                   {"ID3.OriginalArtist", TAG_ID3, TAG_ID3_ORIGARTIST, N_("Original Artist"), N_("Original artist.")
#ifdef HAVE_ID3
                                                   , ID3FID_ORIGARTIST
#endif
                                                   },
                                                   {"ID3.OriginalFileName", TAG_ID3, TAG_ID3_ORIGFILENAME, N_("Original File Name"), N_("Original filename.")
#ifdef HAVE_ID3
                                                   , ID3FID_ORIGFILENAME
#endif
                                                   },
                                                   {"ID3.OriginalLyricist", TAG_ID3, TAG_ID3_ORIGLYRICIST, N_("Original Lyricist"), N_("Original lyricist.")
#ifdef HAVE_ID3
                                                   , ID3FID_ORIGLYRICIST
#endif
                                                   },
                                                   {"ID3.OriginalReleaseTime", TAG_ID3, TAG_ID3_ORIGRELEASETIME, N_("Original Release Time"), N_("Original release time.")
#ifdef HAVE_ID3
                                                   , ID3FID_ORIGRELEASETIME
#endif
                                                   },
                                                   {"ID3.OriginalYear", TAG_ID3, TAG_ID3_ORIGYEAR, N_("Original Year"), N_("Original release year.")
#ifdef HAVE_ID3
                                                   , ID3FID_ORIGYEAR
#endif
                                                   },
                                                   {"ID3.Ownership", TAG_ID3, TAG_ID3_OWNERSHIP, N_("Ownership"), N_("Ownership frame.")
#ifdef HAVE_ID3
                                                   , ID3FID_OWNERSHIP
#endif
                                                   },
                                                   {"ID3.PartInSet", TAG_ID3, TAG_ID3_PARTINSET, N_("Part of a Set"), N_("Part of a set the audio came from.")
#ifdef HAVE_ID3
                                                   , ID3FID_PARTINSET
#endif
                                                   },
                                                   {"ID3.PerformerSortOrder", TAG_ID3, TAG_ID3_PERFORMERSORTORDER, N_("Performer Sort Order"), N_("Performer sort order.")
#ifdef HAVE_ID3
                                                   , ID3FID_PERFORMERSORTORDER
#endif
                                                   },
                                                   {"ID3.Picture", TAG_ID3, TAG_ID3_PICTURE, N_("Picture"), N_("Attached picture.")
#ifdef HAVE_ID3
                                                   , ID3FID_PICTURE
#endif
                                                   },
                                                   {"ID3.PlayCounter", TAG_ID3, TAG_ID3_PLAYCOUNTER, N_("Play Counter"), N_("Number of times a file has been played.")
#ifdef HAVE_ID3
                                                   , ID3FID_PLAYCOUNTER
#endif
                                                   },
                                                   {"ID3.PlaylistDelay", TAG_ID3, TAG_ID3_PLAYLISTDELAY, N_("Playlist Delay"), N_("Playlist delay.")
#ifdef HAVE_ID3
                                                   , ID3FID_PLAYLISTDELAY
#endif
                                                   },
                                                   {"ID3.Popularimeter", TAG_ID3, TAG_ID3_POPULARIMETER, N_("Popularimeter"), N_("Rating of the audio file.")
#ifdef HAVE_ID3
                                                   , ID3FID_POPULARIMETER
#endif
                                                   },
                                                   {"ID3.PositionSync", TAG_ID3, TAG_ID3_POSITIONSYNC, N_("Position Sync"), N_("Position synchronisation frame.")
#ifdef HAVE_ID3
                                                   , ID3FID_POSITIONSYNC
#endif
                                                   },
                                                   {"ID3.Private", TAG_ID3, TAG_ID3_PRIVATE, N_("Private"), N_("Private frame.")
#ifdef HAVE_ID3
                                                   , ID3FID_PRIVATE
#endif
                                                   },
                                                   {"ID3.ProducedNotice", TAG_ID3, TAG_ID3_PRODUCEDNOTICE, N_("Produced Notice"), N_("Produced notice.")
#ifdef HAVE_ID3
                                                   , ID3FID_PRODUCEDNOTICE
#endif
                                                   },
                                                   {"ID3.Publisher", TAG_ID3, TAG_ID3_PUBLISHER, N_("Publisher"), N_("Publisher.")
#ifdef HAVE_ID3
                                                   , ID3FID_PUBLISHER
#endif
                                                   },
                                                   {"ID3.RecordingDates", TAG_ID3, TAG_ID3_RECORDINGDATES, N_("Recording Dates"), N_("Recording dates.")
#ifdef HAVE_ID3
                                                   , ID3FID_RECORDINGDATES
#endif
                                                   },
                                                   {"ID3.RecordingTime", TAG_ID3, TAG_ID3_RECORDINGTIME, N_("Recording Time"), N_("Recording time.")
#ifdef HAVE_ID3
                                                   , ID3FID_RECORDINGTIME
#endif
                                                   },
                                                   {"ID3.ReleaseTime", TAG_ID3, TAG_ID3_RELEASETIME, N_("Release Time"), N_("Release time.")
#ifdef HAVE_ID3
                                                   , ID3FID_RELEASETIME
#endif
                                                   },
                                                   {"ID3.Reverb", TAG_ID3, TAG_ID3_REVERB, N_("Reverb"), N_("Reverb.")
#ifdef HAVE_ID3
                                                   , ID3FID_REVERB
#endif
                                                   },
                                                   {"ID3.SampleRate", TAG_ID3, TAG_ID3_SAMPLERATE, N_("Sample Rate"), N_("Sample rate in Hz.")},
                                                   {"ID3.SetSubtitle", TAG_ID3, TAG_ID3_SETSUBTITLE, N_("Set Subtitle"), N_("Subtitle of the part of a set this track belongs to.")
#ifdef HAVE_ID3
                                                   , ID3FID_SETSUBTITLE
#endif
                                                   },
                                                   {"ID3.Signature", TAG_ID3, TAG_ID3_SIGNATURE, N_("Signature"), N_("Signature frame.")
#ifdef HAVE_ID3
                                                   , ID3FID_SIGNATURE
#endif
                                                   },
                                                   {"ID3.Size", TAG_ID3, TAG_ID3_SIZE, N_("Size"), N_("Size of the audio file in bytes, excluding the ID3 tag.")
#ifdef HAVE_ID3
                                                   , ID3FID_SIZE
#endif
                                                   },
                                                   {"ID3.SongLength", TAG_ID3, TAG_ID3_SONGLEN, N_("Song length"), N_("Length of the song in milliseconds.")
#ifdef HAVE_ID3
                                                   , ID3FID_SONGLEN
#endif
                                                   },
                                                   {"ID3.Subtitle", TAG_ID3, TAG_ID3_SUBTITLE, N_("Subtitle"), N_("Subtitle.")
#ifdef HAVE_ID3
                                                   , ID3FID_SUBTITLE
#endif
                                                   },
                                                   {"ID3.Syncedlyrics", TAG_ID3, TAG_ID3_SYNCEDLYRICS, N_("Syncedlyrics"), N_("Synchronized lyric.")
#ifdef HAVE_ID3
                                                   , ID3FID_SYNCEDLYRICS
#endif
                                                   },
                                                   {"ID3.SyncedTempo", TAG_ID3, TAG_ID3_SYNCEDTEMPO, N_("Synchronized Tempo"), N_("Synchronized tempo codes.")
#ifdef HAVE_ID3
                                                   , ID3FID_SYNCEDTEMPO
#endif
                                                   },
                                                   {"ID3.TaggingTime", TAG_ID3, TAG_ID3_TAGGINGTIME, N_("Tagging Time"), N_("Tagging time.")
#ifdef HAVE_ID3
                                                   , ID3FID_TAGGINGTIME
#endif
                                                   },
                                                   {"ID3.TermsOfUse", TAG_ID3, TAG_ID3_TERMSOFUSE, N_("Terms Of Use"), N_("Terms of use.")
#ifdef HAVE_ID3
                                                   , ID3FID_TERMSOFUSE
#endif
                                                   },
                                                   {"ID3.Time", TAG_ID3, TAG_ID3_TIME, N_("Time"), N_("Time.")
#ifdef HAVE_ID3
                                                   , ID3FID_TIME
#endif
                                                   },
                                                   {"ID3.Title", TAG_ID3, TAG_ID3_TITLE, N_("Title"), N_("Title of the track.")
#ifdef HAVE_ID3
                                                   , ID3FID_TITLE
#endif
                                                   },
                                                   {"ID3.Titlesortorder", TAG_ID3, TAG_ID3_TITLESORTORDER, N_("Titlesortorder"), N_("Title sort order.")
#ifdef HAVE_ID3
                                                   , ID3FID_TITLESORTORDER
#endif
                                                   },
                                                   {"ID3.TrackNo", TAG_ID3, TAG_ID3_TRACKNUM, N_("Track number"), N_("Position of track on the album.")
#ifdef HAVE_ID3
                                                   , ID3FID_TRACKNUM
#endif
                                                   },
                                                   {"ID3.UniqueFileID", TAG_ID3, TAG_ID3_UNIQUEFILEID, N_("Unique File ID"), N_("Unique file identifier.")
#ifdef HAVE_ID3
                                                   , ID3FID_UNIQUEFILEID
#endif
                                                   },
                                                   {"ID3.UnsyncedLyrics", TAG_ID3, TAG_ID3_UNSYNCEDLYRICS, N_("Unsynchronized Lyrics"), N_("Unsynchronized lyric.")
#ifdef HAVE_ID3
                                                   , ID3FID_UNSYNCEDLYRICS
#endif
                                                   },
                                                   {"ID3.UserText", TAG_ID3, TAG_ID3_USERTEXT, N_("User Text"), N_("User defined text information.")
#ifdef HAVE_ID3
                                                   , ID3FID_USERTEXT
#endif
                                                   },
                                                   {"ID3.VolumeAdj", TAG_ID3, TAG_ID3_VOLUMEADJ, N_("Volume Adjustment"), N_("Relative volume adjustment.")
#ifdef HAVE_ID3
                                                   , ID3FID_VOLUMEADJ
#endif
                                                   },
                                                   {"ID3.VolumeAdj2", TAG_ID3, TAG_ID3_VOLUMEADJ2, N_("Volume Adjustment 2"), N_("Relative volume adjustment.")
#ifdef HAVE_ID3
                                                   , ID3FID_VOLUMEADJ2
#endif
                                                   },
                                                   {"ID3.WWWArtist", TAG_ID3, TAG_ID3_WWWARTIST, N_("WWW Artist"), N_("Official artist.")
#ifdef HAVE_ID3
                                                   , ID3FID_WWWARTIST
#endif
                                                   },
                                                   {"ID3.WWWAudioFile", TAG_ID3, TAG_ID3_WWWAUDIOFILE, N_("WWW Audio File"), N_("Official audio file webpage.")
#ifdef HAVE_ID3
                                                   , ID3FID_WWWAUDIOFILE
#endif
                                                   },
                                                   {"ID3.WWWAudioSource", TAG_ID3, TAG_ID3_WWWAUDIOSOURCE, N_("WWW Audio Source"), N_("Official audio source webpage.")
#ifdef HAVE_ID3
                                                   , ID3FID_WWWAUDIOSOURCE
#endif
                                                   },
                                                   {"ID3.WWWCommercialInfo", TAG_ID3, TAG_ID3_WWWCOMMERCIALINFO, N_("WWW Commercial Info"), N_("URL pointing at a webpage containing commercial information.")
#ifdef HAVE_ID3
                                                   , ID3FID_WWWCOMMERCIALINFO
#endif
                                                   },
                                                   {"ID3.WWWCopyright", TAG_ID3, TAG_ID3_WWWCOPYRIGHT, N_("WWW Copyright"), N_("URL pointing at a webpage that holds copyright.")
#ifdef HAVE_ID3
                                                   , ID3FID_WWWCOPYRIGHT
#endif
                                                   },
                                                   {"ID3.WWWPayment", TAG_ID3, TAG_ID3_WWWPAYMENT, N_("WWW Payment"), N_("URL pointing at a webpage that will handle the process of paying for this file.")
#ifdef HAVE_ID3
                                                   , ID3FID_WWWPAYMENT
#endif
                                                   },
                                                   {"ID3.WWWPublisher", TAG_ID3, TAG_ID3_WWWPUBLISHER, N_("WWW Publisher"), N_("URL pointing at the official webpage for the publisher.")
#ifdef HAVE_ID3
                                                   , ID3FID_WWWPUBLISHER
#endif
                                                   },
                                                   {"ID3.WWWRadioPage", TAG_ID3, TAG_ID3_WWWRADIOPAGE, N_("WWW Radio Page"), N_("Official internet radio station homepage.")
#ifdef HAVE_ID3
                                                   , ID3FID_WWWRADIOPAGE
#endif
                                                   },
                                                   {"ID3.WWWUser", TAG_ID3, TAG_ID3_WWWUSER, N_("WWW User"), N_("User defined URL link.")
#ifdef HAVE_ID3
                                                   , ID3FID_WWWUSER
#endif
                                                   },
                                                   {"ID3.Year", TAG_ID3, TAG_ID3_YEAR, N_("Year"), N_("Year.")
#ifdef HAVE_ID3
                                                   , ID3FID_YEAR
#endif
                                                   },
                                                   {"Image.Album", TAG_IMAGE, TAG_IMAGE_ALBUM, N_("Album"), N_("Name of an album the image belongs to.")},
                                                   {"Image.CameraMake", TAG_IMAGE, TAG_IMAGE_CAMERAMAKE, N_("Camera Make"), N_("Make of camera used to take the image.")},
                                                   {"Image.CameraModel", TAG_IMAGE, TAG_IMAGE_CAMERAMODEL, N_("Camera Model"), N_("Model of camera used to take the image.")},
                                                   {"Image.Comments", TAG_IMAGE, TAG_IMAGE_COMMENTS, N_("Comments"), N_("User definable free text.")},
                                                   {"Image.Copyright", TAG_IMAGE, TAG_IMAGE_COPYRIGHT, N_("Copyright"), N_("Embedded copyright message.")},
                                                   {"Image.Creator", TAG_IMAGE, TAG_IMAGE_CREATOR, N_("Creator"), N_("Name of the author.")},
                                                   {"Image.Date", TAG_IMAGE, TAG_IMAGE_DATE, N_("Date"), N_("Datetime image was originally created.")},
                                                   {"Image.Description", TAG_IMAGE, TAG_IMAGE_DESCRIPTION, N_("Description"), N_("Description of the image.")},
                                                   {"Image.ExposureProgram", TAG_IMAGE, TAG_IMAGE_EXPOSUREPROGRAM, N_("Exposure Program"), N_("The program used by the camera to set exposure when the picture is taken. EG Manual, Normal, Aperture priority etc.")},
                                                   {"Image.ExposureTime", TAG_IMAGE, TAG_IMAGE_EXPOSURETIME, N_("Exposure Time"), N_("Exposure time used to capture the photo in seconds.")},
                                                   {"Image.Flash", TAG_IMAGE, TAG_IMAGE_FLASH, N_("Flash"), N_("Set to \"1\" if flash was fired.")},
                                                   {"Image.Fnumber", TAG_IMAGE, TAG_IMAGE_FNUMBER, N_("F number"), N_("Diameter of the aperture relative to the effective focal length of the lens.")},
                                                   {"Image.FocalLength", TAG_IMAGE, TAG_IMAGE_FOCALLENGTH, N_("Focal Length"), N_("Focal length of lens in mm.")},
                                                   {"Image.Height", TAG_IMAGE, TAG_IMAGE_HEIGHT, N_("Height"), N_("Height in pixels.")},
                                                   {"Image.ISOSpeed", TAG_IMAGE, TAG_IMAGE_ISOSPEED, N_("ISO Speed"), N_("ISO speed used to acquire the document contents. For example, 100, 200, 400, etc.")},
                                                   {"Image.Keywords", TAG_IMAGE, TAG_IMAGE_KEYWORDS, N_("Keywords"), N_("String of keywords.")},
                                                   {"Image.MeteringMode", TAG_IMAGE, TAG_IMAGE_METERINGMODE, N_("Metering Mode"), N_("Metering mode used to acquire the image (IE Unknown, Average, CenterWeightedAverage, Spot, MultiSpot, Pattern, Partial).")},
                                                   {"Image.Orientation", TAG_IMAGE, TAG_IMAGE_ORIENTATION, N_("Orientation"), N_("Represents the orientation of the image wrt camera (IE \"top,left\" or \"bottom,right\").")},
                                                   {"Image.Software", TAG_IMAGE, TAG_IMAGE_SOFTWARE, N_("Software"), N_("Software used to produce/enhance the image.")},
                                                   {"Image.Title", TAG_IMAGE, TAG_IMAGE_TITLE, N_("Title"), N_("Title of image.")},
                                                   {"Image.WhiteBalance", TAG_IMAGE, TAG_IMAGE_WHITEBALANCE, N_("White Balance"), N_("White balance setting of the camera when the picture was taken (auto or manual).")},
                                                   {"Image.Width", TAG_IMAGE, TAG_IMAGE_WIDTH, N_("Width"), N_("Width in pixels.")},
                                                   {"IPTC.ActionAdvised", TAG_IPTC, TAG_IPTC_ACTIONADVISED, N_("Action Advised"), N_("The type of action that this object provides to a previous object. '01' Object Kill, '02' Object Replace, '03' Object Append, '04' Object Reference.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_ACTION_ADVISED, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.ARMID", TAG_IPTC, TAG_IPTC_ARMID, N_("ARM Identifier"), N_("Identifies the Abstract Relationship Method (ARM).")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_ARM_ID, IPTC_RECORD_OBJECT_ENV
#endif
                                                   },
                                                   {"IPTC.ARMVersion", TAG_IPTC, TAG_IPTC_ARMVERSION, N_("ARM Version"), N_("Identifies the version of the Abstract Relationship Method (ARM).")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_ARM_VERSION, IPTC_RECORD_OBJECT_ENV
#endif
                                                   },
                                                   {"IPTC.AudioDuration", TAG_IPTC, TAG_IPTC_AUDIODURATION, N_("Audio Duration"), N_("The running time of the audio data in the form HHMMSS.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_AUDIO_DURATION, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.AudioOutcue", TAG_IPTC, TAG_IPTC_AUDIOOUTCUE, N_("Audio Outcue"), N_("The content at the end of the audio data.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_AUDIO_OUTCUE, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.AudioSamplingRate", TAG_IPTC, TAG_IPTC_AUDIOSAMPLINGRATE, N_("Audio Sampling Rate"), N_("The sampling rate in Hz of the audio data.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_AUDIO_SAMPLING_RATE, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.AudioSamplingRes", TAG_IPTC, TAG_IPTC_AUDIOSAMPLINGRES, N_("Audio Sampling Resolution"), N_("The number of bits in each audio sample.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_AUDIO_SAMPLING_RES, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.AudioType", TAG_IPTC, TAG_IPTC_AUDIOTYPE, N_("Audio Type"), N_("The number of channels and type of audio (music, text, etc.) in the object.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_AUDIO_TYPE, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.Byline", TAG_IPTC, TAG_IPTC_BYLINE, N_("By-line"), N_("Name of the creator of the object, e.g. writer, photographer or graphic artist (multiple values allowed).")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_BYLINE, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.BylineTitle", TAG_IPTC, TAG_IPTC_BYLINETITLE, N_("By-line Title"), N_("Title of the creator or creators of the object.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_BYLINE_TITLE, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.Caption", TAG_IPTC, TAG_IPTC_CAPTION, N_("Caption, Abstract"), N_("A textual description of the data")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_CAPTION, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.Category", TAG_IPTC, TAG_IPTC_CATEGORY, N_("Category"), N_("Identifies the subject of the object in the opinion of the provider (Deprecated).")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_CATEGORY, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.CharacterSet", TAG_IPTC, TAG_IPTC_CHARACTERSET, N_("Coded Character Set"), N_("Control functions used for the announcement, invocation or designation of coded character sets.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_CHARACTER_SET, IPTC_RECORD_OBJECT_ENV
#endif
                                                   },
                                                   {"IPTC.City", TAG_IPTC, TAG_IPTC_CITY, N_("City"), N_("City of object origin.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_CITY, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.ConfirmedDataSize", TAG_IPTC, TAG_IPTC_CONFIRMEDDATASIZE, N_("Confirmed Data Size"), N_("Total size of the object data.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_CONFIRMED_DATA_SIZE, IPTC_RECORD_POSTOBJ_DATA
#endif
                                                   },
                                                   {"IPTC.Contact", TAG_IPTC, TAG_IPTC_CONTACT, N_("Contact"), N_("The person or organization which can provide further background information on the object (multiple values allowed).")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_CONTACT, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.ContentLocCode", TAG_IPTC, TAG_IPTC_CONTENTLOCCODE, N_("Content Location Code"), N_("Indicates the code of a country/geographical location referenced by the content of the object (multiple values allowed).")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_CONTENT_LOC_CODE, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.ContentLocName", TAG_IPTC, TAG_IPTC_CONTENTLOCNAME, N_("Content Location Name"), N_("A full, publishable name of a country/geographical location referenced by the content of the object (multiple values allowed).")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_CONTENT_LOC_NAME, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.CopyrightNotice", TAG_IPTC, TAG_IPTC_COPYRIGHTNOTICE, N_("Copyright Notice"), N_("Any necessary copyright notice.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_COPYRIGHT_NOTICE, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.CountryCode", TAG_IPTC, TAG_IPTC_COUNTRYCODE, N_("Country Code"), N_("The code of the country/primary location where the object was created.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_COUNTRY_CODE, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.CountryName", TAG_IPTC, TAG_IPTC_COUNTRYNAME, N_("Country Name"), N_("The name of the country/primary location where the object was created.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_COUNTRY_NAME, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.Credit", TAG_IPTC, TAG_IPTC_CREDIT, N_("Credit"), N_("Identifies the provider of the object, not necessarily the owner/creator.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_CREDIT, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.DateCreated", TAG_IPTC, TAG_IPTC_DATECREATED, N_("Date Created"), N_("The date the intellectual content of the object was created rather than the date of the creation of the physical representation.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_DATE_CREATED, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.DateSent", TAG_IPTC, TAG_IPTC_DATESENT, N_("Date Sent"), N_("The day the service sent the material.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_DATE_SENT, IPTC_RECORD_OBJECT_ENV
#endif
                                                   },
                                                   {"IPTC.Destination", TAG_IPTC, TAG_IPTC_DESTINATION, N_("Destination"), N_("Routing information.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_DESTINATION, IPTC_RECORD_OBJECT_ENV
#endif
                                                   },
                                                   {"IPTC.DigitalCreationDate", TAG_IPTC, TAG_IPTC_DIGITALCREATIONDATE, N_("Digital Creation Date"), N_("The date the digital representation of the object was created.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_DIGITAL_CREATION_DATE, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.DigitalCreationTime", TAG_IPTC, TAG_IPTC_DIGITALCREATIONTIME, N_("Digital Creation Time"), N_("The time the digital representation of the object was created.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_DIGITAL_CREATION_TIME, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.EditorialUpdate", TAG_IPTC, TAG_IPTC_EDITORIALUPDATE, N_("Editorial Update"), N_("Indicates the type of update this object provides to a previous object. The link to the previous object is made using the ARM. '01' indicates an additional language.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_EDITORIAL_UPDATE, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.EditStatus", TAG_IPTC, TAG_IPTC_EDITSTATUS, N_("Edit Status"), N_("Status of the object, according to the practice of the provider.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_EDIT_STATUS, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.EnvelopeNum", TAG_IPTC, TAG_IPTC_ENVELOPENUM, N_("Envelope Number"), N_("A number unique for the date in 1:70 and the service ID in 1:30.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_ENVELOPE_NUM, IPTC_RECORD_OBJECT_ENV
#endif
                                                   },
                                                   {"IPTC.EnvelopePriority", TAG_IPTC, TAG_IPTC_ENVELOPEPRIORITY, N_("Envelope Priority"), N_("Specifies the envelope handling priority and not the editorial urgency. '1' for most urgent, '5' for normal, and '8' for least urgent. '9' is user-defined.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_ENVELOPE_PRIORITY, IPTC_RECORD_OBJECT_ENV
#endif
                                                   },
                                                   {"IPTC.ExpirationDate", TAG_IPTC, TAG_IPTC_EXPIRATIONDATE, N_("Expiration Date"), N_("Designates the latest date the provider intends the object to be used.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_EXPIRATION_DATE, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.ExpirationTime", TAG_IPTC, TAG_IPTC_EXPIRATIONTIME, N_("Expiration Time"), N_("Designates the latest time the provider intends the object to be used.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_EXPIRATION_TIME, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.FileFormat", TAG_IPTC, TAG_IPTC_FILEFORMAT, N_("File Format"), N_("File format of the data described by this metadata.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_FILE_FORMAT, IPTC_RECORD_OBJECT_ENV
#endif
                                                   },
                                                   {"IPTC.FileVersion", TAG_IPTC, TAG_IPTC_FILEVERSION, N_("File Version"), N_("Version of the file format.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_FILE_VERSION, IPTC_RECORD_OBJECT_ENV
#endif
                                                   },
                                                   {"IPTC.FixtureID", TAG_IPTC, TAG_IPTC_FIXTUREID, N_("Fixture Identifier"), N_("Identifies objects that recur often and predictably, enabling users to immediately find or recall such an object.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_FIXTURE_ID, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.Headline", TAG_IPTC, TAG_IPTC_HEADLINE, N_("Headline"), N_("A publishable entry providing a synopsis of the contents of the object.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_HEADLINE, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.ImageOrientation", TAG_IPTC, TAG_IPTC_IMAGEORIENTATION, N_("Image Orientation"), N_("The layout of the image area: 'P' for portrait, 'L' for landscape, and 'S' for square.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_IMAGE_ORIENTATION, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.ImageType", TAG_IPTC, TAG_IPTC_IMAGETYPE, N_("Image Type"), N_("Indicates the data format of the image object.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_IMAGE_TYPE, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.Keywords", TAG_IPTC, TAG_IPTC_KEYWORDS, N_("Keywords"), N_("Used to indicate specific information retrieval words (multiple values allowed).")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_KEYWORDS, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.LanguageID", TAG_IPTC, TAG_IPTC_LANGUAGEID, N_("Language Identifier"), N_("The major national language of the object, according to the 2-letter codes of ISO 639:1988.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_LANGUAGE_ID, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.MaxObjectSize", TAG_IPTC, TAG_IPTC_MAXOBJECTSIZE, N_("Maximum Object Size"), N_("The largest possible size of the object if the size is not known.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_MAX_OBJECT_SIZE, IPTC_RECORD_PREOBJ_DATA
#endif
                                                   },
                                                   {"IPTC.MaxSubfileSize", TAG_IPTC, TAG_IPTC_MAXSUBFILESIZE, N_("Max Subfile Size"), N_("The maximum size for a subfile dataset (8:10) containing a portion of the object data.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_MAX_SUBFILE_SIZE, IPTC_RECORD_PREOBJ_DATA
#endif
                                                   },
                                                   {"IPTC.ModelVersion", TAG_IPTC, TAG_IPTC_MODELVERSION, N_("Model Version"), N_("Version of IIM part 1.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_MODEL_VERSION, IPTC_RECORD_OBJECT_ENV
#endif
                                                   },
                                                   {"IPTC.ObjectAttribute", TAG_IPTC, TAG_IPTC_OBJECTATTRIBUTE, N_("Object Attribute Reference"), N_("Defines the nature of the object independent of the subject (multiple values allowed).")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_OBJECT_ATTRIBUTE, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.ObjectCycle", TAG_IPTC, TAG_IPTC_OBJECTCYCLE, N_("Object Cycle"), N_("Where 'a' is morning, 'p' is evening, 'b' is both.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_OBJECT_CYCLE, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.ObjectName", TAG_IPTC, TAG_IPTC_OBJECTNAME, N_("Object Name"), N_("A shorthand reference for the object.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_OBJECT_NAME, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.ObjectSizeAnnounced", TAG_IPTC, TAG_IPTC_OBJECTSIZEANNOUNCED, N_("Object Size Announced"), N_("The total size of the object data if it is known.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_SIZE_ANNOUNCED, IPTC_RECORD_PREOBJ_DATA
#endif
                                                   },
                                                   {"IPTC.ObjectType", TAG_IPTC, TAG_IPTC_OBJECTTYPE, N_("Object Type Reference"), N_("Distinguishes between different types of objects within the IIM.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_OBJECT_TYPE, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.OriginatingProgram", TAG_IPTC, TAG_IPTC_ORIGINATINGPROGRAM, N_("Originating Program"), N_("The type of program used to originate the object.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_ORIGINATING_PROGRAM, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.OrigTransRef", TAG_IPTC, TAG_IPTC_ORIGTRANSREF, N_("Original Transmission Reference"), N_("A code representing the location of original transmission.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_ORIG_TRANS_REF, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.PreviewData", TAG_IPTC, TAG_IPTC_PREVIEWDATA, N_("Preview Data"), N_("The object preview data")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_PREVIEW_DATA, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.PreviewFileFormat", TAG_IPTC, TAG_IPTC_PREVIEWFORMAT, N_("Preview File Format"), N_("Binary value indicating the file format of the object preview data in dataset 2:202.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_PREVIEW_FORMAT, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.PreviewFileFormatVer", TAG_IPTC, TAG_IPTC_PREVIEWFORMATVER, N_("Preview File Format Version"), N_("The version of the preview file format specified in 2:200.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_PREVIEW_FORMAT_VER, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.ProductID", TAG_IPTC, TAG_IPTC_PRODUCTID, N_("Product ID"), N_("Allows a provider to identify subsets of its overall service.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_PRODUCT_ID, IPTC_RECORD_OBJECT_ENV
#endif
                                                   },
                                                   {"IPTC.ProgramVersion", TAG_IPTC, TAG_IPTC_PROGRAMVERSION, N_("Program Version"), N_("The version of the originating program.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_PROGRAM_VERSION, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.Province", TAG_IPTC, TAG_IPTC_PROVINCE, N_("Province, State"), N_("The Province/State where the object originates.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_STATE, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.RasterizedCaption", TAG_IPTC, TAG_IPTC_RASTERIZEDCAPTION, N_("Rasterized Caption"), N_("Contains rasterized object description and is used where characters that have not been coded are required for the caption.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_RASTERIZED_CAPTION, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.RecordVersion", TAG_IPTC, TAG_IPTC_RECORDVERSION, N_("Record Version"), N_("Identifies the version of the IIM, Part 2")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_RECORD_VERSION, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.RefDate", TAG_IPTC, TAG_IPTC_REFERENCEDATE, N_("Reference Date"), N_("The date of a prior envelope to which the current object refers.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_REFERENCE_DATE, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.RefNumber", TAG_IPTC, TAG_IPTC_REFERENCENUMBER, N_("Reference Number"), N_("The Envelope Number of a prior envelope to which the current object refers.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_REFERENCE_NUMBER, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.RefService", TAG_IPTC, TAG_IPTC_REFERENCESERVICE, N_("Reference Service"), N_("The Service Identifier of a prior envelope to which the current object refers.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_REFERENCE_SERVICE, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.ReleaseDate", TAG_IPTC, TAG_IPTC_RELEASEDATE, N_("Release Date"), N_("Designates the earliest date the provider intends the object to be used.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_RELEASE_DATE, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.ReleaseTime", TAG_IPTC, TAG_IPTC_RELEASETIME, N_("Release Time"), N_("Designates the earliest time the provider intends the object to be used.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_RELEASE_TIME, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.ServiceID", TAG_IPTC, TAG_IPTC_SERVICEID, N_("Service Identifier"), N_("Identifies the provider and product.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_SERVICE_ID, IPTC_RECORD_OBJECT_ENV
#endif
                                                   },
                                                   {"IPTC.SizeMode", TAG_IPTC, TAG_IPTC_SIZEMODE, N_("Size Mode"), N_("Set to 0 if the size of the object is known and 1 if not known.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_SIZE_MODE, IPTC_RECORD_PREOBJ_DATA
#endif
                                                   },
                                                   {"IPTC.Source", TAG_IPTC, TAG_IPTC_SOURCE, N_("Source"), N_("The original owner of the intellectual content of the object.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_SOURCE, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.SpecialInstructions", TAG_IPTC, TAG_IPTC_SPECIALINSTRUCTIONS, N_("Special Instructions"), N_("Other editorial instructions concerning the use of the object.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_SPECIAL_INSTRUCTIONS, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.State", TAG_IPTC, TAG_IPTC_STATE, N_("Province, State"), N_("The Province/State where the object originates.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_STATE, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.Subfile", TAG_IPTC, TAG_IPTC_SUBFILE, N_("Subfile"), N_("The object data itself. Subfiles must be sequential so that the subfiles may be reassembled.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_SUBFILE, IPTC_RECORD_OBJ_DATA
#endif
                                                   },
                                                   {"IPTC.SubjectRef", TAG_IPTC, TAG_IPTC_SUBJECTREFERENCE, N_("Subject Reference"), N_("A structured definition of the subject matter. It must contain an IPR, an 8 digit Subject Reference Number and an optional Subject Name, Subject Matter Name, and Subject Detail Name each separated by a colon (:).")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_SUBJECT_REFERENCE, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.Sublocation", TAG_IPTC, TAG_IPTC_SUBLOCATION, N_("Sub-location"), N_("The location within a city from which the object originates.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_SUBLOCATION, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.SupplCategory", TAG_IPTC, TAG_IPTC_SUPPLCATEGORY, N_("Supplemental Category"), N_("Further refines the subject of the object (Deprecated).")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_SUPPL_CATEGORY, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.TimeCreated", TAG_IPTC, TAG_IPTC_TIMECREATED, N_("Time Created"), N_("The time the intellectual content of the object was created rather than the date of the creation of the physical representation (multiple values allowed).")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_TIME_CREATED, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.TimeSent", TAG_IPTC, TAG_IPTC_TIMESENT, N_("Time Sent"), N_("The time the service sent the material.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_TIME_SENT, IPTC_RECORD_OBJECT_ENV
#endif
                                                   },
                                                   {"IPTC.UNO", TAG_IPTC, TAG_IPTC_UNO, N_("Unique Name of Object"), N_("An eternal, globally unique identification for the object, independent of provider and for any media form.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_UNO, IPTC_RECORD_OBJECT_ENV
#endif
                                                   },
                                                   {"IPTC.Urgency", TAG_IPTC, TAG_IPTC_URGENCY, N_("Urgency"), N_("Specifies the editorial urgency of content and not necessarily the envelope handling priority. '1' is most urgent, '5' normal, and '8' least urgent.")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_URGENCY, IPTC_RECORD_APP_2
#endif
                                                   },
                                                   {"IPTC.WriterEditor", TAG_IPTC, TAG_IPTC_WRITEREDITOR, N_("Writer/Editor"), N_("The name of the person involved in the writing, editing or correcting the object or caption/abstract (multiple values allowed)")
#ifdef HAVE_IPTC
                                                   , IPTC_TAG_WRITER_EDITOR, IPTC_RECORD_APP_2
#endif
                                                   }
                                                   };


static char empty_string[] = "";
static char int_buff[4096];

#ifndef HAVE_GSF
static char no_support_for_libgsf_tags_string[] = N_("<OLE2 and ODF tags not supported>");
#endif


static int tagcmp(const void *t1, const void *t2)
{
    return strcasecmp(((GnomeCmdTagName *) t1)->name,((GnomeCmdTagName *) t2)->name);
}


void GnomeCmdFileMetadata_New::addf(const GnomeCmdTag tag, const gchar *fmt, ...)
{
    static vector<char> buff(16);

    va_list args;

    while (TRUE)
    {
        // Try to print in the allocated space
        va_start (args, fmt);
        int n = vsnprintf (&buff[0], buff.size(), fmt, args);
        va_end (args);

        // If that worked, return the string
        if (n > -1 && n < buff.size())
          break;

        buff.resize(n > -1 ? n+1 : buff.size()*2);
    }

    metadata[tag] = &buff[0];
}


void gcmd_tags_init()
{
    gcmd_tags_libgsf_init();
}


void gcmd_tags_shutdown()
{
    gcmd_tags_libgsf_shutdown();
}


GnomeCmdFileMetadata_New *gcmd_tags_bulk_load(GnomeCmdFile *finfo);
{
    g_return_val_if_fail (finfo != NULL, NULL);

    // gcmd_tags_file_load_metadata(finfo);
    // gcmd_tags_exiv2_load_metadata(finfo);
    // gcmd_tags_taglib_load_metadata(finfo);
    gcmd_tags_libgsf_load_metadata(finfo);

    return finfo->metadata;
}


const gchar *gcmd_tags_image_get_value(GnomeCmdFile *finfo, GnomeCmdTag tag);


GnomeCmdTag gcmd_tags_get_tag_by_name(const gchar *tag_name, const GnomeCmdTagClass tag_class)
{
    GnomeCmdTagName tag;

    if (tag_class==TAG_NONE_CLASS)
    {
        switch (tag_class)
        {
            case TAG_AUDIO:
                tag.name = "Audio.";
                break;

            case TAG_CHM:
                tag.name = "CHM.";
                break;

            case TAG_DOC:
                tag.name = "Doc.";
                break;

            case TAG_EXIF:
                tag.name = "Exif.";
                break;

            case TAG_FILE:
                tag.name = "File.";
                break;

            case TAG_ICC:
                tag.name = "ICC.";
                break;

            case TAG_ID3:
                tag.name = "ID3.";
                break;

            case TAG_IMAGE:
                tag.name = "Image.";
                break;

            case TAG_IPTC:
                tag.name = "IPTC.";
                break;

            case TAG_RPM:
                tag.name = "RPM.";
                break;

            default:
                tag.name = empty_string;
                break;
        }

        tag.name = g_strconcat(tag.name,tag_name,NULL);
    }
    else
        tag.name = tag_name;

    GnomeCmdTagName *elem = (GnomeCmdTagName *) bsearch(&tag,metatags,NUMBER_OF_TAGS,sizeof(GnomeCmdTagName),tagcmp);

    if (tag_class==TAG_NONE_CLASS)
        g_free((gpointer) tag.name);

    return elem ? elem->tag : TAG_NONE;
}


GnomeCmdTag *gcmd_tags_get_pointer_to_tag(const GnomeCmdTag tag)
{
    return &(metatags[ tag<NUMBER_OF_TAGS ? tag : TAG_NONE ].tag);
}


const gchar *gcmd_tags_get_name(const GnomeCmdTag tag)
{
    return metatags[ tag<NUMBER_OF_TAGS ? tag : TAG_NONE ].name;
}


const gchar *gcmd_tags_get_class_name(const GnomeCmdTag tag)
{
    switch (metatags[ tag<NUMBER_OF_TAGS ? tag : TAG_NONE ].tag_class)
    {
        case TAG_AUDIO:
            return _("Audio");

        case TAG_CHM:
            return "CHM";

        case TAG_DOC:
            return _("Doc");

        case TAG_EXIF:
            return "Exif";

        case TAG_FILE:
            return _("File");

        case TAG_ICC:
            return "ICC";

        case TAG_ID3:
            return "ID3";

        case TAG_IMAGE:
            return _("Image");

        case TAG_IPTC:
            return "IPTC";

        case TAG_RPM:
            return "RPM";

        default:
            break;
    }

    return empty_string;
}


const gchar *gcmd_tags_get_value(GnomeCmdFile *finfo, const GnomeCmdTagClass tag_class, const GnomeCmdTag tag)
{
    g_return_val_if_fail (finfo != NULL, empty_string);

    switch (tag_class)
    {
        case TAG_EXIF : gcmd_tags_libexif_get_value(finfo, metatags[tag].libtag);
                        break;

        case TAG_IPTC : gcmd_tags_libiptcdata_get_value(finfo, metatags[tag].libclass, metatags[tag].libtag);
                        break;

        default:
            return gcmd_tags_get_value (finfo, tag);
    }

    return empty_string;
}


const gchar *gcmd_tags_get_value(GnomeCmdFile *finfo, const GnomeCmdTag tag)
{
    const gchar *ret_val = empty_string;

    g_return_val_if_fail (finfo != NULL, empty_string);

    if (tag>=NUMBER_OF_TAGS)
        return ret_val;

    switch (metatags[tag].tag_class)
    {
        case TAG_AUDIO: ret_val = gcmd_tags_audio_get_value(finfo, tag);
                        break;

        case TAG_ID3  : ret_val = gcmd_tags_id3lib_get_value(finfo, tag);
                        break;

        case TAG_CHM  : break;

        case TAG_DOC  : if (!finfo->metadata)
                            return ret_val;
#ifndef HAVE_GSF
                        return _(no_support_for_libgsf_tags_string);
#endif
                        gcmd_tags_libgsf_load_metadata(finfo);
                        ret_val = finfo->metadata->operator [] (tag).c_str();
                        break;

        case TAG_FILE : break;

        case TAG_IMAGE: ret_val = gcmd_tags_image_get_value(finfo, tag);
                        break;

        case TAG_EXIF : ret_val = gcmd_tags_libexif_get_value(finfo, metatags[tag].libtag);
                        break;

        case TAG_IPTC : ret_val = gcmd_tags_libiptcdata_get_value(finfo, metatags[tag].libclass, metatags[tag].libtag);
                        break;

        case TAG_ICC  : ret_val = gcmd_tags_icclib_get_value(finfo, metatags[tag].libclass, metatags[tag].libtag);
                        break;

        case TAG_RPM  : break;

        default:        break;
    }

    return ret_val ? ret_val : empty_string;
}


const gchar *gcmd_tags_get_title(const GnomeCmdTag tag)
{
    return _(metatags[ tag<NUMBER_OF_TAGS ? tag : TAG_NONE ].title);
}


const gchar *gcmd_tags_get_description(const GnomeCmdTag tag)
{
    return _(metatags[ tag<NUMBER_OF_TAGS ? tag : TAG_NONE ].description);
}

// -----------------------------------------------------------------------------

const gchar *gcmd_tags_image_get_value(GnomeCmdFile *finfo, const GnomeCmdTag tag)
{
    const gchar *ret_val = NULL;

    switch (tag)
    {
        case TAG_IMAGE_CAMERAMAKE:
#ifdef HAVE_EXIF
            ret_val = gcmd_tags_libexif_get_value(finfo, metatags[TAG_EXIF_MAKE].libtag);
            if (ret_val)  break;
#endif
#ifdef HAVE_LCMS
#endif
            break;

        case TAG_IMAGE_CAMERAMODEL:
#ifdef HAVE_IPTC
            ret_val = gcmd_tags_libiptcdata_get_value(finfo, metatags[TAG_IPTC_MODELVERSION].libclass, metatags[TAG_IPTC_MODELVERSION].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_EXIF
            ret_val = gcmd_tags_libexif_get_value(finfo, metatags[TAG_EXIF_MODEL].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_LCMS
#endif
            break;

        case TAG_IMAGE_COMMENTS:
#ifdef HAVE_EXIF
            ret_val = gcmd_tags_libexif_get_value(finfo, metatags[TAG_EXIF_USERCOMMENT].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_LCMS
#endif
            break;

        case TAG_IMAGE_COPYRIGHT:
#ifdef HAVE_IPTC
            ret_val = gcmd_tags_libiptcdata_get_value(finfo, metatags[TAG_IPTC_COPYRIGHTNOTICE].libclass, metatags[TAG_IPTC_COPYRIGHTNOTICE].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_EXIF
            ret_val = gcmd_tags_libexif_get_value(finfo, metatags[TAG_EXIF_COPYRIGHT].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_LCMS
#endif
            break;

        case TAG_IMAGE_CREATOR:
#ifdef HAVE_IPTC
            ret_val = gcmd_tags_libiptcdata_get_value(finfo, metatags[TAG_IPTC_BYLINE].libclass, metatags[TAG_IPTC_BYLINE].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_EXIF
            ret_val = gcmd_tags_libexif_get_value(finfo, metatags[TAG_EXIF_ARTIST].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_LCMS
#endif
            break;

        case TAG_IMAGE_DATE:
#ifdef HAVE_IPTC
            ret_val = gcmd_tags_libiptcdata_get_value(finfo, metatags[TAG_IPTC_TIMECREATED].libclass, metatags[TAG_IPTC_TIMECREATED].libtag);

            if (!ret_val || !*ret_val)
                ret_val = gcmd_tags_libiptcdata_get_value(finfo, metatags[TAG_IPTC_DATECREATED].libclass, metatags[TAG_IPTC_DATECREATED].libtag);
            else
            {
                gchar *time = g_strdup(ret_val);

                ret_val = gcmd_tags_libiptcdata_get_value(finfo, metatags[TAG_IPTC_DATECREATED].libclass, metatags[TAG_IPTC_DATECREATED].libtag);

                if (ret_val)
                {
                    g_stpcpy(int_buff, " ");
                    g_strlcat (int_buff, time, sizeof(int_buff));
                }

                g_free(time);
            }

            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_EXIF
            ret_val = gcmd_tags_libexif_get_value(finfo, metatags[TAG_EXIF_DATETIMEORIGINAL].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_LCMS
#endif
            break;

        case TAG_IMAGE_DESCRIPTION:
#ifdef HAVE_IPTC
            ret_val = gcmd_tags_libiptcdata_get_value(finfo, metatags[TAG_IPTC_CAPTION].libclass, metatags[TAG_IPTC_CAPTION].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_EXIF
            ret_val = gcmd_tags_libexif_get_value(finfo, metatags[TAG_EXIF_IMAGEDESCRIPTION].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_LCMS
#endif
            break;

        case TAG_IMAGE_EXPOSUREPROGRAM:
#ifdef HAVE_EXIF
            ret_val = gcmd_tags_libexif_get_value(finfo, metatags[TAG_EXIF_EXPOSUREPROGRAM].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_LCMS
#endif
            break;

        case TAG_IMAGE_EXPOSURETIME:
#ifdef HAVE_EXIF
            ret_val = gcmd_tags_libexif_get_value(finfo, metatags[TAG_EXIF_EXPOSURETIME].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_LCMS
#endif
            break;

        case TAG_IMAGE_FLASH:
#ifdef HAVE_EXIF
            ret_val = gcmd_tags_libexif_get_value(finfo, metatags[TAG_EXIF_FLASH].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_LCMS
#endif
            break;

        case TAG_IMAGE_FNUMBER:
#ifdef HAVE_EXIF
            ret_val = gcmd_tags_libexif_get_value(finfo, metatags[TAG_EXIF_FNUMBER].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_LCMS
#endif
            break;

        case TAG_IMAGE_FOCALLENGTH:
#ifdef HAVE_EXIF
            ret_val = gcmd_tags_libexif_get_value(finfo, metatags[TAG_EXIF_FOCALLENGTH].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_LCMS
#endif
            break;

        case TAG_IMAGE_HEIGHT:
#ifdef HAVE_EXIF
            ret_val = gcmd_tags_libexif_get_value(finfo, metatags[TAG_EXIF_IMAGELENGTH].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_LCMS
#endif
            break;

        case TAG_IMAGE_ISOSPEED:
#ifdef HAVE_EXIF
            ret_val = gcmd_tags_libexif_get_value(finfo, metatags[TAG_EXIF_ISOSPEEDRATINGS].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_LCMS
#endif
            break;

        case TAG_IMAGE_KEYWORDS:
#ifdef HAVE_IPTC
            ret_val = gcmd_tags_libiptcdata_get_value(finfo, metatags[TAG_IPTC_KEYWORDS].libclass, metatags[TAG_IPTC_KEYWORDS].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_EXIF
            ret_val = gcmd_tags_libexif_get_value(finfo, metatags[TAG_EXIF_USERCOMMENT].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_LCMS
#endif
            break;

        case TAG_IMAGE_METERINGMODE:
#ifdef HAVE_EXIF
            ret_val = gcmd_tags_libexif_get_value(finfo, metatags[TAG_EXIF_METERINGMODE].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_LCMS
#endif
            break;

        case TAG_IMAGE_ORIENTATION:
#ifdef HAVE_IPTC
            ret_val = gcmd_tags_libiptcdata_get_value(finfo, metatags[TAG_IPTC_IMAGEORIENTATION].libclass, metatags[TAG_IPTC_IMAGEORIENTATION].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_EXIF
            ret_val = gcmd_tags_libexif_get_value(finfo, metatags[TAG_EXIF_ORIENTATION].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_LCMS
#endif
            break;

        case TAG_IMAGE_SOFTWARE:
#ifdef HAVE_IPTC
            ret_val = gcmd_tags_libiptcdata_get_value(finfo, metatags[TAG_IPTC_PROGRAMVERSION].libclass, metatags[TAG_IPTC_PROGRAMVERSION].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_EXIF
            ret_val = gcmd_tags_libexif_get_value(finfo, metatags[TAG_EXIF_SOFTWARE].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_LCMS
#endif
            break;

        case TAG_IMAGE_TITLE:
#ifdef HAVE_IPTC
            ret_val = gcmd_tags_libiptcdata_get_value(finfo, metatags[TAG_IPTC_HEADLINE].libclass, metatags[TAG_IPTC_HEADLINE].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_EXIF
            ret_val = gcmd_tags_libexif_get_value(finfo, metatags[TAG_EXIF_IMAGEDESCRIPTION].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_LCMS
#endif
            break;

        case TAG_IMAGE_WIDTH:
#ifdef HAVE_EXIF
            ret_val = gcmd_tags_libexif_get_value(finfo, metatags[TAG_EXIF_IMAGEWIDTH].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_LCMS
#endif
            break;

        case TAG_IMAGE_WHITEBALANCE:
#ifdef HAVE_EXIF
            ret_val = gcmd_tags_libexif_get_value(finfo, metatags[TAG_EXIF_WHITEBALANCE].libtag);
            if (ret_val && *ret_val)  break;
#endif
#ifdef HAVE_LCMS
#endif
            break;

        default       :
            break;
    }

    return ret_val;
}


const gchar *gcmd_tags_image_get_value_by_name(GnomeCmdFile *finfo, const gchar *tag_name)
{
    return empty_string;
}
