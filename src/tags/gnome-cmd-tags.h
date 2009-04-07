/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2009 Piotr Eljasiak

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
*/

#ifndef __GNOME_CMD_TAGS_H__
#define __GNOME_CMD_TAGS_H__

#include <string>
#include <map>
#include <set>
#include <sstream>

#include "gnome-cmd-file.h"
#include "utils.h"

typedef enum
{
    TAG_NONE_CLASS  = 0,
    TAG_FILE        = -1,
    TAG_CHM         = 1 << 6,
    TAG_EXIF        = 1 << 7,
    TAG_IPTC        = 1 << 8,
    TAG_ICC         = 1 << 9,
    TAG_APE         = 1 << 10,
    TAG_FLAC        = 1 << 11,
    TAG_ID3         = 1 << 12,
    TAG_VORBIS      = 1 << 13,
    TAG_PDF         = 1 << 14,
    TAG_RPM         = 1 << 15,
    TAG_AUDIO       = TAG_APE | TAG_FLAC | TAG_ID3 | TAG_VORBIS,
    TAG_DOC         = TAG_CHM | TAG_PDF,
    TAG_IMAGE       = TAG_EXIF | TAG_IPTC | TAG_ICC
} GnomeCmdTagClass;


typedef enum
{
    TAG_NONE,
    TAG_AUDIO_ALBUMARTIST,                  // artist of the album
    TAG_AUDIO_ALBUMGAIN,                    // gain adjustment of album
    TAG_AUDIO_ALBUMPEAKGAIN,                // peak gain adjustment of album
    TAG_AUDIO_ALBUMTRACKCOUNT,              // total no. of tracks on the album
    TAG_AUDIO_ALBUM,                        // name of the album
    TAG_AUDIO_ARTIST,                       // artist of the track
    TAG_AUDIO_BITRATE,                      // bitrate in kbps
    TAG_AUDIO_CHANNELS,                     // number of channels in the audio (2 = stereo)
    TAG_AUDIO_CODECVERSION,                 // codec version
    TAG_AUDIO_CODEC,                        // codec encoding description
    TAG_AUDIO_COMMENT,                      // comments on the track
    TAG_AUDIO_COPYRIGHT,                    // copyright message
    TAG_AUDIO_COVERALBUMTHUMBNAILPATH,      // file path to thumbnail image of the cover album
    TAG_AUDIO_DISCNO,                       // specifies which disc the track is on
    TAG_AUDIO_DURATION,                     // duration of track in seconds
    TAG_AUDIO_DURATIONMMSS,                 // duration of track in MM:SS
    TAG_AUDIO_GENRE,                        // type of music classification for the track as defined in ID3 spec
    TAG_AUDIO_ISNEW,                        // set to "1" if track is new to the user (default "0")
    TAG_AUDIO_ISRC,                         // ISRC (international standard recording code)
    TAG_AUDIO_LASTPLAY,                     // when track was last played
    TAG_AUDIO_LYRICS,                       // lyrics of the track
    TAG_AUDIO_MBALBUMARTISTID,              // musicBrainz album artist ID in UUID format
    TAG_AUDIO_MBALBUMID,                    // musicBrainz album ID in UUID format
    TAG_AUDIO_MBARTISTID,                   // musicBrainz artist ID in UUID format
    TAG_AUDIO_MBTRACKID,                    // musicBrainz track ID in UUID format
    TAG_AUDIO_MPEG_CHANNELMODE,             // MPEG channel mode
    TAG_AUDIO_MPEG_COPYRIGHTED,             // "1" if the copyrighted bit is set
    TAG_AUDIO_MPEG_LAYER,                   // MPEG layer
    TAG_AUDIO_MPEG_ORIGINAL,                // "1" if the "original" bit is set
    TAG_AUDIO_MPEG_VERSION,                 // MPEG version
    TAG_AUDIO_PERFORMER,                    // name of the performer/conductor of the music
    TAG_AUDIO_PLAYCOUNT,                    // number of times the track has been played
    TAG_AUDIO_RELEASEDATE,                  // date track was released
    TAG_AUDIO_SAMPLERATE,                   // sample rate in Hz
    TAG_AUDIO_TITLE,                        // title of the track
    TAG_AUDIO_TRACKGAIN,                    // gain adjustment of track
    TAG_AUDIO_TRACKNO,                      // position of track on the album
    TAG_AUDIO_TRACKPEAKGAIN,                // peak gain adjustment of track
    TAG_AUDIO_YEAR,                         // year
    TAG_DOC_AUTHOR,                         // name of the author
    TAG_DOC_BYTECOUNT,                      // number of bytes in the document
    TAG_DOC_CASESENSITIVE,                  // case sensitive
    TAG_DOC_CATEGORY,                       // category
    TAG_DOC_CELLCOUNT,                      // number of cells in the spreadsheet document
    TAG_DOC_CHARACTERCOUNT,                 // number of characters in the document
    TAG_DOC_CODEPAGE,                       // the MS codepage to encode strings for metadata
    TAG_DOC_COMMENTS,                       // user definable free text
    TAG_DOC_COMPANY,                        // organization that the <Doc.Creator> entity is associated with
    TAG_DOC_CREATED,                        // datetime document was originally created
    TAG_DOC_CREATOR,                        // an entity primarily responsible for making the content of the resource, typically a person, organization, or service
    TAG_DOC_DATECREATED,                    // date associated with an event in the life cycle of the resource (creation/publication date)
    TAG_DOC_DATEMODIFIED,                   // the last time the document was saved
    TAG_DOC_DESCRIPTION,                    // an account of the content of the resource
    TAG_DOC_DICTIONARY,                     // dictionary
    TAG_DOC_EDITINGDURATION,                // the total time taken until the last modification
    TAG_DOC_GENERATOR,                      // the application that generated this document
    TAG_DOC_HIDDENSLIDECOUNT,               // number of hidden slides in the presentation document
    TAG_DOC_IMAGECOUNT,                     // number of images in the document
    TAG_DOC_INITIALCREATOR,                 // specifies the name of the person who created the document initially
    TAG_DOC_KEYWORDS,                       // searchable, indexable keywords
    TAG_DOC_LANGUAGE,                       // the locale language of the intellectual content of the resource
    TAG_DOC_LASTPRINTED,                    // the last time this document was printed
    TAG_DOC_LASTSAVEDBY,                    // the entity that made the last change to the document, typically a person, organization, or service
    TAG_DOC_LINECOUNT,                      // number of lines in the document
    TAG_DOC_LINKSDIRTY,                     // links dirty
    TAG_DOC_LOCALESYSTEMDEFAULT,            // identifier representing the default system locale
    TAG_DOC_MMCLIPCOUNT,                    // number of multimedia clips in the document
    TAG_DOC_MANAGER,                        // name of the manager of <Doc.Creator> entity
    TAG_DOC_NOTECOUNT,                      // number of "notes" in the document
    TAG_DOC_OBJECTCOUNT,                    // number of objects (OLE and other graphics) in the document
    TAG_DOC_PAGECOUNT,                      // number of pages in the document
    TAG_DOC_PARAGRAPHCOUNT,                 // number of paragraphs in the document
    TAG_DOC_PRESENTATIONFORMAT,             // type of presentation, like "On-screen Show", "SlideView", etc
    TAG_DOC_PRINTDATE,                      // specifies the date and time when the document was last printed
    TAG_DOC_PRINTEDBY,                      // specifies the name of the last person who printed the document
    TAG_DOC_REVISIONCOUNT,                  // number of revision on the document
    TAG_DOC_SCALE,                          // scale
    TAG_DOC_SECURITY,                       // one of: "Password protected", "Read-only recommended", "Read-only enforced" or "Locked for annotations"
    TAG_DOC_SLIDECOUNT,                     // number of slides in the presentation document
    TAG_DOC_SPREADSHEETCOUNT,               // number of pages in the document
    TAG_DOC_SUBJECT,                        // document subject
    TAG_DOC_TABLECOUNT,                     // number of tables in the document
    TAG_DOC_TEMPLATE,                       // the template file that is been used to generate this document
    TAG_DOC_TITLE,                          // title of the document
    TAG_DOC_WORDCOUNT,                      // number of words in the document
    TAG_EXIF_APERTUREVALUE,                 // lens aperture
    TAG_EXIF_ARTIST,                        // person who created the image
    TAG_EXIF_BATTERYLEVEL,                  // battery level
    TAG_EXIF_BITSPERSAMPLE,                 // number of bits per component
    TAG_EXIF_BRIGHTNESSVALUE,               // value of brightness
    TAG_EXIF_CFAPATTERN,                    // color filter array (CFA) geometric pattern of the image sensor when a one-chip color area sensor is used
    TAG_EXIF_CFAREPEATPATTERNDIM,
    TAG_EXIF_COLORSPACE,                    // color space information
    TAG_EXIF_COMPONENTSCONFIGURATION,       // information specific to compressed data. The channels of each component are arranged in order from the 1st component to the 4th. For uncompressed data the data arrangement is given in the <PhotometricInterpretation> tag. However, since <PhotometricInterpretation> can only express the order of Y, Cb and Cr, is provided for cases when compressed data uses components other than Y, Cb, and Cr and to enable support of other sequences
    TAG_EXIF_COMPRESSEDBITSPERPIXEL,        // information specific to compressed data. The compression mode used for a compressed image is indicated in unit bits per pixel
    TAG_EXIF_COMPRESSION,                   // compression scheme
    TAG_EXIF_CONTRAST,                      // direction of contrast processing applied by the camera
    TAG_EXIF_COPYRIGHT,                     // copyright holder
    TAG_EXIF_CUSTOMRENDERED,                // use of special processing on image data, such as rendering geared to output. When special processing is performed, the reader is expected to disable or minimize any further processing
    TAG_EXIF_DATETIME,                      // file change date and time
    TAG_EXIF_DATETIMEDIGITIZED,             // date and time when the image was stored as digital data
    TAG_EXIF_DATETIMEORIGINAL,              // date and time when the original image data was generated
    TAG_EXIF_DEVICESETTINGDESCRIPTION,      // information on the picture-taking conditions of a particular camera model
    TAG_EXIF_DIGITALZOOMRATIO,              // digital zoom ratio If the numerator of the recorded value is 0, this indicates that digital zoom was not used
    TAG_EXIF_DOCUMENTNAME,
    TAG_EXIF_EXIFIFDPOINTER,                // pointer to the Exif IFD
    TAG_EXIF_EXIFVERSION,                   // Exif standard version
    TAG_EXIF_EXPOSUREBIASVALUE,             // exposure bias
    TAG_EXIF_EXPOSUREINDEX,                 // exposure index
    TAG_EXIF_EXPOSUREMODE,                  // exposure mode set In auto-bracketing mode, the camera shoots a series of frames of the same scene at different exposure settings
    TAG_EXIF_EXPOSUREPROGRAM,               // class of the program used by the camera to set exposure when the picture is taken
    TAG_EXIF_EXPOSURETIME,                  // exposure time, given in seconds (sec)
    TAG_EXIF_FILESOURCE,                    // image source
    TAG_EXIF_FILLORDER,
    TAG_EXIF_FLASH,                         // is recorded when an image is taken using a strobe light (flash)
    TAG_EXIF_FLASHENERGY,                   // strobe energy at the time the image is captured, as measured in Beam Candle Power Seconds (BCPS)
    TAG_EXIF_FLASHPIXVERSION,               // FlashPix format version supported by a FPXR file
    TAG_EXIF_FNUMBER,                       // F number
    TAG_EXIF_FOCALLENGTH,                   // actual focal length of the lens, in mm. Conversion is not made to the focal length of a 35 mm film camera
    TAG_EXIF_FOCALLENGTHIN35MMFILM,         // equivalent focal length assuming a 35mm film camera, in mm. A value of 0 means the focal length is unknown. Note that differs from the FocalLength tag
    TAG_EXIF_FOCALPLANERESOLUTIONUNIT,      // unit for measuring <FocalPlaneXResolution> and <FocalPlaneYResolution>. This value is the same as the <ResolutionUnit>
    TAG_EXIF_FOCALPLANEXRESOLUTION,         // number of pixels in the image width (X) direction per <FocalPlaneResolutionUnit> on the camera focal plane
    TAG_EXIF_FOCALPLANEYRESOLUTION,         // number of pixels in the image height (V) direction per <FocalPlaneResolutionUnit> on the camera focal plane
    TAG_EXIF_GAINCONTROL,                   // degree of overall image gain adjustment
    TAG_EXIF_GAMMA,                         // value of coefficient gamma
    TAG_EXIF_GPSALTITUDE,                   // altitude based on the reference in GPSAltitudeRef. Altitude is expressed as one RATIONAL value. The reference unit is meters
    TAG_EXIF_GPSALTITUDEREF,                // altitude used as the reference altitude. If the reference is sea level and the altitude is above sea level, 0 is given. If the altitude is below sea level, a value of 1 is given and the altitude is indicated as an absolute value in the GSPAltitude tag. The reference unit is meters. Note that is BYTE type, unlike other reference tags
    TAG_EXIF_GPSINFOIFDPOINTER,             // pointer to the GPS Info IFD
    TAG_EXIF_GPSLATITUDE,                   // latitude. The latitude is expressed as three RATIONAL values giving the degrees, minutes, and seconds, respectively. When degrees, minutes and seconds are expressed, the format is dd/1,mm/1,ss/1. When degrees and minutes are used and, for example, fractions of minutes are given up to two decimal places, the format is dd/1,mmmm/100,0/1
    TAG_EXIF_GPSLATITUDEREF,                // latitude indicator ('N' indicates north, 'S' - south latitude)
    TAG_EXIF_GPSLONGITUDE,                  // longitude
    TAG_EXIF_GPSLONGITUDEREF,               // Indicates whether the longitude is east or west longitude. ASCII 'E' indicates east longitude, and 'W' is west longitude
    TAG_EXIF_GPSVERSIONID,                  // version of <GPSInfoIFD>
    TAG_EXIF_IMAGEDESCRIPTION,              // image title
    TAG_EXIF_IMAGELENGTH,                   // image height
    TAG_EXIF_IMAGERESOURCES,
    TAG_EXIF_IMAGEUNIQUEID,                 // indicates an identifier assigned uniquely to each image. It is recorded as an ASCII string equivalent to hexadecimal notation and 128-bit fixed length
    TAG_EXIF_IMAGEWIDTH,                    // image width
    TAG_EXIF_INTERCOLORPROFILE,             // color profile
    TAG_EXIF_INTEROPERABILITYIFDPOINTER,    // interoperability IFD is composed of tags which stores the information to ensure the Interoperability and pointed by the following tag located in Exif IFD. The Interoperability structure of Interoperability IFD is the same as TIFF defined IFD structure but does not contain the image data characteristically compared with normal TIFF IFD
    TAG_EXIF_INTEROPERABILITYINDEX,         // identification of the Interoperability rule. Use \"R98\" for stating ExifR98 Rules. Four bytes used including the termination code (NULL). see the separate volume of Recommended Exif Interoperability Rules (ExifR98) for other tags used for ExifR98
    TAG_EXIF_INTEROPERABILITYVERSION,
    TAG_EXIF_IPTCNAA,
    TAG_EXIF_ISOSPEEDRATINGS,               // ISO Speed and ISO Latitude of the camera or input device as specified in ISO 12232
    TAG_EXIF_JPEGINTERCHANGEFORMAT,         // offset to JPEG SOI
    TAG_EXIF_JPEGINTERCHANGEFORMATLENGTH,   // bytes of JPEG data
    TAG_EXIF_JPEGPROC,
    TAG_EXIF_LIGHTSOURCE,                   // kind of light source
    TAG_EXIF_MAKE,                          // image input equipment manufacturer
    TAG_EXIF_MAKERNOTE,                     // tag for manufacturers of Exif writers to record any desired information. The contents are up to the manufacturer
    TAG_EXIF_MAXAPERTUREVALUE,              // smallest F number of the lens
    TAG_EXIF_METERINGMODE,                  // metering mode
    TAG_EXIF_MODEL,                         // image input equipment model
    TAG_EXIF_NEWCFAPATTERN,                 // color filter array (CFA) geometric pattern of the image sensor when a one-chip color area sensor is used. It does not apply to all sensing methods
    TAG_EXIF_NEWSUBFILETYPE,                // general indication of the kind of data contained in this subfile
    TAG_EXIF_OECF,                          // Opto-Electronic Conversion Function (OECF) specified in ISO 14524
    TAG_EXIF_ORIENTATION,                   // orientation of image
    TAG_EXIF_PHOTOMETRICINTERPRETATION,     // pixel composition
    TAG_EXIF_PIXELXDIMENSION,               // Information specific to compressed data. When a compressed file is recorded, the valid width of the meaningful image must be recorded in this tag, whether or not there is padding data or a restart marker. should not exist in an uncompressed file
    TAG_EXIF_PIXELYDIMENSION,               // Information specific to compressed data. When a compressed file is recorded, the valid height of the meaningful image must be recorded in this tag, whether or not there is padding data or a restart marker. should not exist in an uncompressed file
    TAG_EXIF_PLANARCONFIGURATION,           // image data arrangement
    TAG_EXIF_PRIMARYCHROMATICITIES,         // chromaticities of primaries
    TAG_EXIF_REFERENCEBLACKWHITE,           // pair of black and white reference values
    TAG_EXIF_RELATEDIMAGEFILEFORMAT,
    TAG_EXIF_RELATEDIMAGELENGTH,
    TAG_EXIF_RELATEDIMAGEWIDTH,
    TAG_EXIF_RELATEDSOUNDFILE,              // name of an audio file related to the image data
    TAG_EXIF_RESOLUTIONUNIT,                // unit of X and Y resolution
    TAG_EXIF_ROWSPERSTRIP,                  // number of rows per strip
    TAG_EXIF_SAMPLESPERPIXEL,               // number of components
    TAG_EXIF_SATURATION,                    // direction of saturation processing applied by the camera
    TAG_EXIF_SCENECAPTURETYPE,              // capture type of scene
    TAG_EXIF_SCENETYPE,                     // type of scene
    TAG_EXIF_SENSINGMETHOD,                 // image sensor type on the camera or input device
    TAG_EXIF_SHARPNESS,                     // direction of sharpness processing applied by the camera
    TAG_EXIF_SHUTTERSPEEDVALUE,             // shutter speed
    TAG_EXIF_SOFTWARE,                      // software used
    TAG_EXIF_SPATIALFREQUENCYRESPONSE,      // camera or input device spatial frequency table and SFR values in the direction of image width, image height, and diagonal direction, as specified in ISO 12233
    TAG_EXIF_SPECTRALSENSITIVITY,           // spectral sensitivity of each channel of the camera used. The tag value is an ASCII string compatible with the standard developed by the ASTM Technical Committee
    TAG_EXIF_STRIPBYTECOUNTS,               // bytes per compressed strip
    TAG_EXIF_STRIPOFFSETS,                  // image data location
    TAG_EXIF_SUBIFDS,                       // defined by Adobe Corporation to enable TIFF Trees within a TIFF file
    TAG_EXIF_SUBJECTAREA,                   // location and area of the main subject in the overall scene
    TAG_EXIF_SUBJECTDISTANCE,               // distance to the subject, given in meters
    TAG_EXIF_SUBJECTDISTANCERANGE,          // distance to the subject
    TAG_EXIF_SUBJECTLOCATION,               // location of the main subject in the scene. The value of represents the pixel at the center of the main subject relative to the left edge, prior to rotation processing as per the <Rotation> tag. The first value X column number and second Y row number
    TAG_EXIF_SUBSECTIME,                    // fractions of seconds for the <DateTime> tag
    TAG_EXIF_SUBSECTIMEDIGITIZED,           // fractions of seconds for the <DateTimeDigitized> tag
    TAG_EXIF_SUBSECTIMEORIGINAL,            // fractions of seconds for the <DateTimeOriginal> tag
    TAG_EXIF_TIFFEPSTANDARDID,
    TAG_EXIF_TRANSFERFUNCTION,              // transfer function for the image, described in tabular style
    TAG_EXIF_TRANSFERRANGE,
    TAG_EXIF_USERCOMMENT,                   // user comments
    TAG_EXIF_WHITEBALANCE,                  // white balance mode set
    TAG_EXIF_WHITEPOINT,                    // white point chromaticity
    TAG_EXIF_XMLPACKET,                     // XMP Metadata
    TAG_EXIF_XRESOLUTION,                   // image resolution in width direction
    TAG_EXIF_YCBCRCOEFFICIENTS,             // color space transformation matrix coefficients
    TAG_EXIF_YCBCRPOSITIONING,              // Y and C positioning
    TAG_EXIF_YCBCRSUBSAMPLING,              // subsampling ratio of Y to C
    TAG_EXIF_YRESOLUTION,                   // image resolution in height direction
    TAG_FILE_ACCESSED,                      // last access datetime
    TAG_FILE_CONTENT,                       // file's contents filtered as plain text (IE as stored by the indexer)
    TAG_FILE_DESCRIPTION,                   // editable free text/notes
    TAG_FILE_FORMAT,                        // MIME type of the file or if a directory it should contain value "Folder"
    TAG_FILE_KEYWORDS,                      // editable array of keywords
    TAG_FILE_LINK,                          // URI of link target
    TAG_FILE_MODIFIED,                      // last modified datetime
    TAG_FILE_NAME,                          // file name excluding path but including the file extension
    TAG_FILE_PATH,                          // full file path of file excluding the filename
    TAG_FILE_PERMISSIONS,                   // permission string in unix format eg "-rw-r--r--"
    TAG_FILE_PUBLISHER,                     // editable DC type for the name of the publisher of the file (EG dc:publisher field in RSS feed)
    TAG_FILE_RANK,                          // editable file rank for grading favourites. Value should be in the range 1..10
    TAG_FILE_SIZE,                          // size of the file in bytes or if a directory no. of items it contains
    TAG_ID3_ALBUMSORTORDER,                 // album sort order
    TAG_ID3_AUDIOCRYPTO,                    // audio encryption
    TAG_ID3_AUDIOSEEKPOINT,                 // audio seek point index
    TAG_ID3_BAND,                           // band
    TAG_ID3_BPM,                            // BPM (beats per minute)
    TAG_ID3_BUFFERSIZE,                     // recommended buffer size
    TAG_ID3_CDID,                           // music CD identifier
    TAG_ID3_COMMERCIAL,                     // commercial frame
    TAG_ID3_COMPOSER,                       // composer
    TAG_ID3_CONDUCTOR,                      // conductor
    TAG_ID3_CONTENTGROUP,                   // content group description
    TAG_ID3_CONTENTTYPE,                    // type of music classification for the track as defined in ID3 spec
    TAG_ID3_COPYRIGHT,                      // copyright message
    TAG_ID3_CRYPTOREG,                      // encryption method registration
    TAG_ID3_DATE,                           // date
    TAG_ID3_EMPHASIS,                       // emphasis
    TAG_ID3_ENCODEDBY,                      // encoded by
    TAG_ID3_ENCODERSETTINGS,                // software
    TAG_ID3_ENCODINGTIME,                   // encoding time
    TAG_ID3_EQUALIZATION,                   // equalization
    TAG_ID3_EQUALIZATION2,                  // equalisation (2)
    TAG_ID3_EVENTTIMING,                    // event timing codes
    TAG_ID3_FILEOWNER,                      // file owner
    TAG_ID3_FILETYPE,                       // file type
    TAG_ID3_FRAMES,                         // number of frames
    TAG_ID3_GENERALOBJECT,                  // general encapsulated object
    TAG_ID3_GROUPINGREG,                    // group identification registration
    TAG_ID3_INITIALKEY,                     // initial key
    TAG_ID3_INVOLVEDPEOPLE,                 // involved people list
    TAG_ID3_INVOLVEDPEOPLE2,                // involved people list
    TAG_ID3_LANGUAGE,                       // language(s)
    TAG_ID3_LINKEDINFO,                     // linked information
    TAG_ID3_LYRICIST,                       // lyricist
    TAG_ID3_MEDIATYPE,                      // media type
    TAG_ID3_MIXARTIST,                      // interpreted, remixed, or otherwise modified by
    TAG_ID3_MOOD,                           // mood
    TAG_ID3_MPEGLOOKUP,                     // MPEG location lookup table
    TAG_ID3_MUSICIANCREDITLIST,             // musician credits list
    TAG_ID3_NETRADIOOWNER,                  // internet radio station owner
    TAG_ID3_NETRADIOSTATION,                // internet radio station name
    TAG_ID3_ORIGALBUM,                      // original album
    TAG_ID3_ORIGARTIST,                     // original artist(s)
    TAG_ID3_ORIGFILENAME,                   // original filename
    TAG_ID3_ORIGLYRICIST,                   // original lyricist(s)
    TAG_ID3_ORIGRELEASETIME,                // original release time
    TAG_ID3_ORIGYEAR,                       // original release year
    TAG_ID3_OWNERSHIP,                      // ownership frame
    TAG_ID3_PARTINSET,                      // part of a set
    TAG_ID3_PERFORMERSORTORDER,             // performer sort order
    TAG_ID3_PICTURE,                        // attached picture
    TAG_ID3_PLAYCOUNTER,                    // play counter
    TAG_ID3_PLAYLISTDELAY,                  // playlist delay
    TAG_ID3_POPULARIMETER,                  // popularimeter
    TAG_ID3_POSITIONSYNC,                   // position synchronisation frame
    TAG_ID3_PRIVATE,                        // private frame
    TAG_ID3_PRODUCEDNOTICE,                 // produced notice
    TAG_ID3_PUBLISHER,                      // publisher
    TAG_ID3_RECORDINGDATES,                 // recording dates
    TAG_ID3_RECORDINGTIME,                  // recording time
    TAG_ID3_RELEASETIME,                    // release time
    TAG_ID3_REVERB,                         // reverb
    TAG_ID3_SETSUBTITLE,                    // set subtitle
    TAG_ID3_SIGNATURE,                      // signature frame
    TAG_ID3_SIZE,                           // size
    TAG_ID3_SONGLEN,                        // length
    TAG_ID3_SUBTITLE,                       // subtitle
    TAG_ID3_SYNCEDLYRICS,                   // synchronized lyric
    TAG_ID3_SYNCEDTEMPO,                    // synchronized tempo codes
    TAG_ID3_TAGGINGTIME,                    // tagging time
    TAG_ID3_TERMSOFUSE,                     // terms of use
    TAG_ID3_TIME,                           // time
    TAG_ID3_TITLESORTORDER,                 // title sort order
    TAG_ID3_UNIQUEFILEID,                   // unique file identifier
    TAG_ID3_UNSYNCEDLYRICS,                 // unsynchronized lyric
    TAG_ID3_USERTEXT,                       // user defined text information
    TAG_ID3_VOLUMEADJ,                      // relative volume adjustment
    TAG_ID3_VOLUMEADJ2,                     // relative volume adjustment (2)
    TAG_ID3_WWWARTIST,                      // official artist
    TAG_ID3_WWWAUDIOFILE,                   // official audio file webpage
    TAG_ID3_WWWAUDIOSOURCE,                 // official audio source webpage
    TAG_ID3_WWWCOMMERCIALINFO,              // commercial information
    TAG_ID3_WWWCOPYRIGHT,                   // copyright
    TAG_ID3_WWWPAYMENT,                     // payment
    TAG_ID3_WWWPUBLISHER,                   // official publisher webpage
    TAG_ID3_WWWRADIOPAGE,                   // official internet radio station homepage
    TAG_ID3_WWWUSER,                        // user defined URL link
    TAG_IMAGE_WIDTH,                        // width in pixels
    TAG_IMAGE_HEIGHT,                       // height in pixels
    TAG_IMAGE_ALBUM,                        // name of an album the image belongs to
    TAG_IMAGE_MAKE,                         // make of camera used to take the image
    TAG_IMAGE_MODEL,                        // model of camera used to take the image
    TAG_IMAGE_COMMENTS,                     // user definable free text
    TAG_IMAGE_COPYRIGHT,                    // embedded copyright message
    TAG_IMAGE_CREATOR,                      // name of the author
    TAG_IMAGE_DATE,                         // datetime image was originally created
    TAG_IMAGE_DESCRIPTION,                  // description of the image
    TAG_IMAGE_EXPOSUREPROGRAM,              // the program used by the camera to set exposure when the picture is taken. EG Manual, Normal, Aperture priority etc
    TAG_IMAGE_EXPOSURETIME,                 // exposure time used to capture the photo in seconds
    TAG_IMAGE_FLASH,                        // set to 1 if flash was fired
    TAG_IMAGE_FNUMBER,                      // diameter of the aperture relative to the effective focal length of the lens.
    TAG_IMAGE_FOCALLENGTH,                  // focal length of lens in mm
    TAG_IMAGE_ISOSPEED,                     // ISO speed used to acquire the document contents. For example, 100, 200, 400, etc.
    TAG_IMAGE_KEYWORDS,                     // string of keywords
    TAG_IMAGE_METERINGMODE,                 // metering mode used to acquire the image (IE Unknown, Average, CenterWeightedAverage, Spot, MultiSpot, Pattern, Partial)
    TAG_IMAGE_ORIENTATION,                  // represents the orientation of the image wrt camera IE "top,left" or "bottom,right"
    TAG_IMAGE_SOFTWARE,                     // software used to produce/enhance the image
    TAG_IMAGE_TITLE,                        // title of image
    TAG_IMAGE_WHITEBALANCE,                 // white balance setting of the camera when the picture was taken (auto or manual)
    TAG_IPTC_ACTIONADVISED,                 // type of action that this object provides to a previous object
    TAG_IPTC_ARMID,                         // Abstract Relationship Method (ARM)
    TAG_IPTC_ARMVERSION,                    // version of the Abstract Relationship Method (ARM)
    TAG_IPTC_AUDIODURATION,                 // running time of the audio data in the form HHMMSS
    TAG_IPTC_AUDIOOUTCUE,                   // content at the end of the audio data
    TAG_IPTC_AUDIOSAMPLINGRATE,             // sampling rate in Hz of the audio data
    TAG_IPTC_AUDIOSAMPLINGRES,              // number of bits in each audio sample
    TAG_IPTC_AUDIOTYPE,                     // number of channels and type of audio (music, text, etc.) in the object
    TAG_IPTC_BYLINE,                        // name of the creator of the object
    TAG_IPTC_BYLINETITLE,                   // title of the creator or creators of the object
    TAG_IPTC_CAPTION,                       // textual description of the data
    TAG_IPTC_CATEGORY,                      // subject of the object in the opinion of the provider (Deprecated)
    TAG_IPTC_CHARACTERSET,                  // control functions used for the announcement, invocation or designation of coded character sets
    TAG_IPTC_CITY,                          // city of object origin
    TAG_IPTC_CONFIRMEDDATASIZE,             // total size of the object data
    TAG_IPTC_CONTACT,                       // person or organization which can provide further background information on the object
    TAG_IPTC_CONTENTLOCCODE,                // code of a country/geographical location referenced by the content of the object
    TAG_IPTC_CONTENTLOCNAME,                // full, publishable name of a country/geographical location referenced by the content of the object
    TAG_IPTC_COPYRIGHTNOTICE,               // any necessary copyright notice
    TAG_IPTC_COUNTRYCODE,                   // code of the country/primary location where the object was created
    TAG_IPTC_COUNTRYNAME,                   // name of the country/primary location where the object was created
    TAG_IPTC_CREDIT,                        // provider of the object, not necessarily the owner/creator
    TAG_IPTC_DATECREATED,                   // date the intellectual content of the object was created rather than the date of the creation of the physical representation
    TAG_IPTC_DATESENT,                      // day the service sent the material
    TAG_IPTC_DESTINATION,                   // routing information
    TAG_IPTC_DIGITALCREATIONDATE,           // date when the digital representation of the object was created
    TAG_IPTC_DIGITALCREATIONTIME,           // time when the digital representation of the object was created
    TAG_IPTC_EDITORIALUPDATE,               // type of update this object provides to a previous object
    TAG_IPTC_EDITSTATUS,                    // status of the object, according to the practice of the provider
    TAG_IPTC_ENVELOPENUM,                   // number unique for the date in 1:70 and the service ID in 1:30
    TAG_IPTC_ENVELOPEPRIORITY,              // specifies the envelope handling priority and not the editorial urgency
    TAG_IPTC_EXPIRATIONDATE,                // designates the latest date the provider intends the object to be used
    TAG_IPTC_EXPIRATIONTIME,                // designates the latest time the provider intends the object to be used
    TAG_IPTC_FILEFORMAT,                    // file format of the data described by this metadata
    TAG_IPTC_FILEVERSION,                   // version of the file format
    TAG_IPTC_FIXTUREID,                     // objects that recur often and predictably, enabling users to immediately find or recall such an object
    TAG_IPTC_HEADLINE,                      // publishable entry providing a synopsis of the contents of the object
    TAG_IPTC_IMAGEORIENTATION,              // layout of the image area: 'P' for portrait, 'L' for landscape, and 'S' for square
    TAG_IPTC_IMAGETYPE,                     // data format of the image object
    TAG_IPTC_KEYWORDS,                      // used to indicate specific information retrieval words
    TAG_IPTC_LANGUAGEID,                    // major national language of the object, according to the 2-letter codes of ISO 639:1988
    TAG_IPTC_MAXOBJECTSIZE,                 // largest possible size of the object if the size is not known
    TAG_IPTC_MAXSUBFILESIZE,                // maximum size for a subfile dataset (8:10) containing a portion of the object data
    TAG_IPTC_MODELVERSION,                  // version of IIM part 1
    TAG_IPTC_OBJECTATTRIBUTE,               // defines the nature of the object independent of the subject
    TAG_IPTC_OBJECTCYCLE,                   // where 'a' is morning, 'p' is evening, 'b' is both
    TAG_IPTC_OBJECTNAME,                    // shorthand reference for the object
    TAG_IPTC_OBJECTSIZEANNOUNCED,           // total size of the object data if it is known
    TAG_IPTC_OBJECTTYPE,                    // distinguishes between different types of objects within the IIM
    TAG_IPTC_ORIGINATINGPROGRAM,            // type of program used to originate the object
    TAG_IPTC_ORIGTRANSREF,                  // a code representing the location of original transmission
    TAG_IPTC_PREVIEWDATA,                   // object preview data
    TAG_IPTC_PREVIEWFORMAT,                 // binary value indicating the file format of the object preview data in dataset 2:202
    TAG_IPTC_PREVIEWFORMATVER,              // version of the preview file format specified in 2:200
    TAG_IPTC_PRODUCTID,                     // product ID
    TAG_IPTC_PROGRAMVERSION,                // version of the originating program
    TAG_IPTC_PROVINCE,                      // Province/State where the object originates
    TAG_IPTC_RASTERIZEDCAPTION,             // contains rasterized object description and is used where characters that have not been coded are required for the caption
    TAG_IPTC_RECORDVERSION,                 // version of the IIM, Part 2
    TAG_IPTC_REFERENCEDATE,                 // date of a prior envelope to which the current object refers
    TAG_IPTC_REFERENCENUMBER,               // Envelope Number of a prior envelope to which the current object refers
    TAG_IPTC_REFERENCESERVICE,              // Service Identifier of a prior envelope to which the current object refers
    TAG_IPTC_RELEASEDATE,                   // designates the earliest date the provider intends the object to be used
    TAG_IPTC_RELEASETIME,                   // designates the earliest time the provider intends the object to be used
    TAG_IPTC_SERVICEID,                     // identifies the provider and product
    TAG_IPTC_SIZEMODE,                      // set to 0 if the size of the object is known and 1 if not known
    TAG_IPTC_SOURCE,                        // original owner of the intellectual content of the object
    TAG_IPTC_SPECIALINSTRUCTIONS,           // other editorial instructions concerning the use of the object
    TAG_IPTC_STATE,                         // Province/State where the object originates
    TAG_IPTC_SUBFILE,                       // object data itself
    TAG_IPTC_SUBJECTREFERENCE,              // structured definition of the subject matter
    TAG_IPTC_SUBLOCATION,                   // location within a city from which the object originates
    TAG_IPTC_SUPPLCATEGORY,                 // further refines the subject of the object (Deprecated)
    TAG_IPTC_TIMECREATED,                   // time the intellectual content of the object was created rather than the date of the creation of the physical representation
    TAG_IPTC_TIMESENT,                      // time the service sent the material
    TAG_IPTC_UNO,                           // eternal, globally unique identification for the object, independent of provider and for any media form
    TAG_IPTC_URGENCY,                       // specifies the editorial urgency of content and not necessarily the envelope handling priority
    TAG_IPTC_WRITEREDITOR,                  // name of the person involved in the writing, editing or correcting the object or caption/abstract
    TAG_PDF_PAGESIZE,                       // page size format
    TAG_PDF_PAGEWIDTH,                      // page width in mm
    TAG_PDF_PAGEHEIGHT,                     // page height in mm
    TAG_PDF_VERSION,                        // the PDF version of the document
    TAG_PDF_PRODUCER,                       // the application that converted the document to PDF
    TAG_PDF_EMBEDDEDFILES,                  // number of embedded files in the document
    TAG_PDF_OPTIMIZED,                      // set to "1" if optimized for network access
    TAG_PDF_PRINTING,                       // set to "1" if printing is allowed
    TAG_PDF_HIRESPRINTING,                  // set to "1" if high resolution printing is allowed
    TAG_PDF_COPYING,                        // set to "1" if copying the contents is allowed
    TAG_PDF_MODIFYING,                      // set to "1" if modifying the contents is allowed
    TAG_PDF_DOCASSEMBLY,                    // set to "1" if inserting, rotating, or deleting pages and creating navigation elements is allowed
    TAG_PDF_COMMENTING,                     // set to "1" if adding or modifying text annotations is allowed
    TAG_PDF_FORMFILLING,                    // set to "1" if filling of form fields is allowed
    TAG_PDF_ACCESSIBILITYSUPPORT,           // set to "1" if accessibility support (eg. screen readers) is enabled
    TAG_VORBIS_CONTACT,                     // contact information for the creators or distributors of the track
    TAG_VORBIS_DESCRIPTION,                 // a textual description of the data
    TAG_VORBIS_LICENSE,                     // license information
    TAG_VORBIS_LOCATION,                    // location where track was recorded
    TAG_VORBIS_MAXBITRATE,                  // maximum bitrate in kbps
    TAG_VORBIS_MINBITRATE,                  // minimum bitrate in kbps
    TAG_VORBIS_NOMINALBITRATE,              // nominal bitrate in kbps
    TAG_VORBIS_ORGANIZATION,                // organization producing the track
    TAG_VORBIS_VENDOR,                      // Vorbis vendor ID
    TAG_VORBIS_VERSION                      // Vorbis version
} GnomeCmdTag;


void gcmd_tags_init();
void gcmd_tags_shutdown();

GnomeCmdFileMetadata *gcmd_tags_bulk_load(GnomeCmdFile *f);

const gchar *gcmd_tags_get_name(const GnomeCmdTag tag);
const GnomeCmdTagClass gcmd_tags_get_class(const GnomeCmdTag tag);
const gchar *gcmd_tags_get_class_name(const GnomeCmdTag tag);
const gchar *gcmd_tags_get_value(GnomeCmdFile *f, const GnomeCmdTag tag);
const gchar *gcmd_tags_get_title(const GnomeCmdTag tag);
const gchar *gcmd_tags_get_description(const GnomeCmdTag tag);

// gcmd_tags_get_tag_by_name() returns tag for given tag_name (eg. AlbumArtist) in the specified tag_class (here TAG_AUDIO)
// if tag_class is omitted, tag_name must contain fully qualified tag name (eg. Audio.AlbumArtist)

GnomeCmdTag gcmd_tags_get_tag_by_name(const gchar *tag_name, const GnomeCmdTagClass tag_class=TAG_NONE_CLASS);

// gcmd_tags_get_value_by_name() returns metatag value for given tag_name (eg. AlbumArtist) in the specified tag_class (here TAG_AUDIO)
// if tag_class is omitted, tag_name must contain fully qualified tag name (eg. Audio.AlbumArtist)

inline const gchar *gcmd_tags_get_value_by_name(GnomeCmdFile *f, const gchar *tag_name, const GnomeCmdTagClass tag_class=TAG_NONE_CLASS)
{
    g_return_val_if_fail (f != NULL, "");

    return gcmd_tags_get_value(f, gcmd_tags_get_tag_by_name(tag_name, tag_class));
}


class GnomeCmdFileMetadata
{
  public:

    typedef std::map<GnomeCmdTag,std::set<std::string> >   METADATA_COLL;

  private:

    typedef std::map<GnomeCmdTagClass,gboolean>  ACCESSED_COLL;

    ACCESSED_COLL accessed;
    METADATA_COLL metadata;

    const std::string NODATA;

  public:

    GnomeCmdFileMetadata() {}                                                   // to make g++ 3.4 happy

    gboolean is_accessed (const GnomeCmdTagClass tag_class) const;
    gboolean is_accessed (const GnomeCmdTag tag) const;
    void mark_as_accessed (const GnomeCmdTagClass tag_class)        {  accessed[tag_class] = TRUE;  }
    void mark_as_accessed (const GnomeCmdTag tag);

    void add (const GnomeCmdTag tag, std::string value);
    void add (const GnomeCmdTag tag, const gchar *value);
    template <typename T>
    void add (const GnomeCmdTag tag, const T &value);
    void addf (const GnomeCmdTag tag, const gchar *fmt, ...);

    gboolean has_tag (const GnomeCmdTag tag);

    const std::string operator[] (const GnomeCmdTag tag);

    METADATA_COLL::const_iterator begin()                           {  return metadata.begin();     }
    METADATA_COLL::const_iterator end()                             {  return metadata.end();       }
};


inline gboolean GnomeCmdFileMetadata::is_accessed (const GnomeCmdTagClass tag_class) const
{
    ACCESSED_COLL::const_iterator elem = accessed.find(tag_class);

    return elem==accessed.end() ? FALSE : elem->second;
}


inline void GnomeCmdFileMetadata::add (const GnomeCmdTag tag, std::string value)
{
    if (value.empty())
        return;

    std::string::size_type end = value.find_last_not_of(" \t\n\r\0",std::string::npos,5);         // remove trailing whitespace from a string

    if (end==std::string::npos)
        return;

    value.erase(end+1);

    metadata[tag].insert(value);
}


inline void GnomeCmdFileMetadata::add (const GnomeCmdTag tag, const gchar *value)
{
    if (value && *value)
        add(tag, std::string(value));
}


template <typename T>
inline void GnomeCmdFileMetadata::add (const GnomeCmdTag tag, const T &value)
{
   std::ostringstream os;

   os << value;
   add(tag,os.str());
}


inline gboolean GnomeCmdFileMetadata::has_tag (const GnomeCmdTag tag)
{
    METADATA_COLL::const_iterator pos = metadata.find(tag);

    return pos!=metadata.end();
}


inline const std::string GnomeCmdFileMetadata::operator[] (const GnomeCmdTag tag)
{
    METADATA_COLL::const_iterator pos = metadata.find(tag);

    return pos==metadata.end() ? NODATA : join(pos->second, ", ");
}


#endif // __GNOME_CMD_TAGS_H__
