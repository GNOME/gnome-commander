/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2009 Piotr Eljasiak

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

#include <vector>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-tags-exiv2.h"
#include "utils.h"
#include "dict.h"

#include <gdk-pixbuf/gdk-pixbuf.h>

#ifdef HAVE_EXIV2
#include <exiv2/exif.hpp>
#include <exiv2/image.hpp>
#endif

using namespace std;
#ifdef HAVE_EXIV2
using namespace Exiv2;
#endif


#ifdef HAVE_EXIV2
static DICT<GnomeCmdTag> exiv2_tags(TAG_NONE);


template <typename T>
inline void readTags(GnomeCmdFileMetadata *metadata, const T &data)
{
    if (data.empty())  return;

    typename T::const_iterator end = data.end();

    for (typename T::const_iterator i = data.begin(); i != end; ++i)
    {
        GnomeCmdTag tag = exiv2_tags[i->key()];

        DEBUG('t', "\t%s (%s) = %s\n", i->key().c_str(), gcmd_tags_get_name(tag), i->toString().c_str());

        switch (tag)
        {
            case TAG_NONE:
                break;

            case TAG_EXIF_MAKERNOTE:
                metadata->addf(tag,_("unsupported tag (suppressed %u B of binary data)"), i->size());
                break;

            case TAG_EXIF_EXIFVERSION:
            case TAG_EXIF_FLASHPIXVERSION:
            case TAG_EXIF_INTEROPERABILITYVERSION:
                {
                    vector<byte> buff(i->value().size()+1);

                    i->value().copy(&buff[0],invalidByteOrder);
                    metadata->add(tag,(char *) &buff[0]);
                }
                break;

            case TAG_IMAGE_DESCRIPTION:
            case TAG_IPTC_CAPTION:
                metadata->add(TAG_FILE_DESCRIPTION,*i);      // add interpreted value
                metadata->add(tag,*i);                       // add interpreted value
                break;

            case TAG_IMAGE_KEYWORDS:
            case TAG_IPTC_KEYWORDS:
                metadata->add(TAG_FILE_KEYWORDS,*i);         // add interpreted value
                metadata->add(tag,*i);                       // add interpreted value
                break;

            case TAG_IMAGE_CREATOR:
            case TAG_IPTC_BYLINE:
                metadata->add(TAG_FILE_PUBLISHER,*i);        // add interpreted value
                metadata->add(tag,*i);                       // add interpreted value
                break;

            default:
                metadata->add(tag,*i);                       // add interpreted value
                break;
        }
    }
}
#endif


void gcmd_tags_exiv2_init()
{
#ifdef HAVE_EXIV2
    static struct
    {
        GnomeCmdTag tag;
        const gchar *name;
    }
    exiv2_data[] = {
                    {TAG_EXIF_APERTUREVALUE, "Exif.CanonSi.ApertureValue"},
                    {TAG_EXIF_APERTUREVALUE, "Exif.Olympus.ApertureValue"},
                    {TAG_EXIF_APERTUREVALUE, "Exif.Photo.ApertureValue"},
                    {TAG_EXIF_ARTIST, "Exif.Image.Artist"},
                    {TAG_EXIF_BATTERYLEVEL, "Exif.Image.BatteryLevel"},
                    {TAG_EXIF_BITSPERSAMPLE, "Exif.Image.BitsPerSample"},
                    {TAG_EXIF_BRIGHTNESSVALUE, "Exif.MinoltaCsNew.Brightness"},
                    {TAG_EXIF_BRIGHTNESSVALUE, "Exif.Olympus.Brightness"},
                    {TAG_EXIF_BRIGHTNESSVALUE, "Exif.Photo.BrightnessValue"},
                    {TAG_EXIF_CFAPATTERN, "Exif.Image.CFAPattern"},
                    {TAG_EXIF_CFAPATTERN, "Exif.Photo.CFAPattern"},
                    {TAG_EXIF_CFAREPEATPATTERNDIM, "Exif.Image.CFARepeatPatternDim"},
                    {TAG_EXIF_COLORSPACE, "Exif.MinoltaCs7D.ColorSpace"},
                    {TAG_EXIF_COLORSPACE, "Exif.Nikon3.ColorSpace"},
                    {TAG_EXIF_COLORSPACE, "Exif.Photo.ColorSpace"},
                    {TAG_EXIF_COLORSPACE, "Exif.Sigma.ColorSpace"},
                    {TAG_EXIF_COMPONENTSCONFIGURATION, "Exif.Photo.ComponentsConfiguration"},
                    {TAG_EXIF_COMPRESSEDBITSPERPIXEL, "Exif.Photo.CompressedBitsPerPixel"},
                    {TAG_EXIF_COMPRESSION, "Exif.Image.Compression"},
                    {TAG_EXIF_CONTRAST, "Exif.CanonCs.Contrast"},
                    {TAG_EXIF_CONTRAST, "Exif.MinoltaCs5D.Contrast"},
                    {TAG_EXIF_CONTRAST, "Exif.MinoltaCs7D.Contrast"},
                    {TAG_EXIF_CONTRAST, "Exif.MinoltaCsNew.Contrast"},
                    {TAG_EXIF_CONTRAST, "Exif.Olympus.Contrast"},
                    {TAG_EXIF_CONTRAST, "Exif.Panasonic.Contrast"},
                    {TAG_EXIF_CONTRAST, "Exif.Photo.Contrast"},
                    {TAG_EXIF_CONTRAST, "Exif.Sigma.Contrast"},
                    {TAG_EXIF_COPYRIGHT, "Exif.Image.Copyright"},
                    {TAG_EXIF_CUSTOMRENDERED, "Exif.Photo.CustomRendered"},
                    {TAG_EXIF_DATETIME, "Exif.Image.DateTime"},
                    {TAG_EXIF_DATETIMEDIGITIZED, "Exif.Photo.DateTimeDigitized"},
                    {TAG_EXIF_DATETIMEORIGINAL, "Exif.Photo.DateTimeOriginal"},
                    {TAG_EXIF_DEVICESETTINGDESCRIPTION, "Exif.Canon.CameraSettings"},
                    {TAG_EXIF_DEVICESETTINGDESCRIPTION, "Exif.Minolta.CameraSettings5D"},
                    {TAG_EXIF_DEVICESETTINGDESCRIPTION, "Exif.Minolta.CameraSettings7D"},
                    {TAG_EXIF_DEVICESETTINGDESCRIPTION, "Exif.Minolta.CameraSettingsStdNew"},
                    {TAG_EXIF_DEVICESETTINGDESCRIPTION, "Exif.Minolta.CameraSettingsStdOld"},
                    {TAG_EXIF_DEVICESETTINGDESCRIPTION, "Exif.Minolta.CameraSettingsZ1"},
                    {TAG_EXIF_DEVICESETTINGDESCRIPTION, "Exif.Photo.DeviceSettingDescription"},
                    {TAG_EXIF_DIGITALZOOMRATIO, "Exif.Photo.DigitalZoomRatio"},
                    {TAG_EXIF_DOCUMENTNAME, "Exif.Image.DocumentName"},
                    {TAG_EXIF_EXIFVERSION, "Exif.Photo.ExifVersion"},
                    {TAG_EXIF_EXPOSUREBIASVALUE, "Exif.Photo.ExposureBiasValue"},
                    {TAG_EXIF_EXPOSUREINDEX, "Exif.Photo.ExposureIndex"},
                    {TAG_EXIF_EXPOSUREMODE, "Exif.MinoltaCs5D.ExposureMode"},
                    {TAG_EXIF_EXPOSUREMODE, "Exif.MinoltaCs7D.ExposureMode"},
                    {TAG_EXIF_EXPOSUREMODE, "Exif.MinoltaCsNew.ExposureMode"},
                    {TAG_EXIF_EXPOSUREMODE, "Exif.Photo.ExposureMode"},
                    {TAG_EXIF_EXPOSUREMODE, "Exif.Sigma.ExposureMode"},
                    {TAG_EXIF_FILESOURCE, "Exif.Fujifilm.FileSource"},
                    {TAG_EXIF_FILESOURCE, "Exif.Photo.FileSource"},
                    {TAG_EXIF_FILLORDER, "Exif.Image.FillOrder"},
                    {TAG_EXIF_FLASHENERGY, "Exif.Photo.FlashEnergy"},
                    {TAG_EXIF_FLASHPIXVERSION, "Exif.Photo.FlashpixVersion"},
                    {TAG_EXIF_FOCALLENGTHIN35MMFILM, "Exif.Photo.FocalLengthIn35mmFilm"},
                    {TAG_EXIF_FOCALPLANERESOLUTIONUNIT, "Exif.Photo.FocalPlaneResolutionUnit"},
                    {TAG_EXIF_FOCALPLANEXRESOLUTION, "Exif.Photo.FocalPlaneXResolution"},
                    {TAG_EXIF_FOCALPLANEYRESOLUTION, "Exif.Photo.FocalPlaneYResolution"},
                    {TAG_EXIF_GAINCONTROL, "Exif.Photo.GainControl"},
                    {TAG_EXIF_GPSALTITUDE, "Exif.GPSInfo.GPSAltitude"},
                    {TAG_EXIF_GPSALTITUDEREF, "Exif.GPSInfo.GPSAltitudeRef"},
                    {TAG_EXIF_GPSLATITUDE, "Exif.GPSInfo.GPSLatitude"},
                    {TAG_EXIF_GPSLATITUDEREF, "Exif.GPSInfo.GPSLatitudeRef"},
                    {TAG_EXIF_GPSLONGITUDE, "Exif.GPSInfo.GPSLongitude"},
                    {TAG_EXIF_GPSLONGITUDEREF, "Exif.GPSInfo.GPSLongitudeRef"},
                    {TAG_EXIF_GPSVERSIONID, "Exif.GPSInfo.GPSVersionID"},
                    {TAG_EXIF_IMAGEDESCRIPTION, "Exif.Image.ImageDescription"},
                    {TAG_EXIF_IMAGELENGTH, "Exif.Image.ImageLength"},
                    {TAG_EXIF_IMAGERESOURCES, "Exif.Image.ImageResources"},
                    {TAG_EXIF_IMAGEUNIQUEID, "Exif.Photo.ImageUniqueID"},
                    {TAG_EXIF_INTERCOLORPROFILE, "Exif.Image.InterColorProfile"},
                    {TAG_EXIF_INTEROPERABILITYINDEX, "Exif.Iop.InteroperabilityIndex"},
                    {TAG_EXIF_INTEROPERABILITYVERSION, "Exif.Iop.InteroperabilityVersion"},
                    {TAG_EXIF_ISOSPEEDRATINGS, "Exif.Photo.ISOSpeedRatings"},
                    {TAG_EXIF_JPEGINTERCHANGEFORMAT, "Exif.Image.JPEGInterchangeFormat"},
                    {TAG_EXIF_JPEGINTERCHANGEFORMATLENGTH, "Exif.Image.JPEGInterchangeFormatLength"},
                    {TAG_EXIF_JPEGPROC, "Exif.Image.JPEGProc"},
                    {TAG_EXIF_LIGHTSOURCE, "Exif.Nikon3.LightSource"},
                    {TAG_EXIF_LIGHTSOURCE, "Exif.Photo.LightSource"},
                    {TAG_EXIF_MAKERNOTE, "Exif.Photo.MakerNote"},
                    {TAG_EXIF_MAXAPERTUREVALUE, "Exif.Photo.MaxApertureValue"},
                    {TAG_EXIF_NEWSUBFILETYPE, "Exif.Image.NewSubfileType"},
                    {TAG_EXIF_OECF, "Exif.Photo.OECF"},
                    {TAG_EXIF_PHOTOMETRICINTERPRETATION, "Exif.Image.PhotometricInterpretation"},
                    {TAG_EXIF_PIXELXDIMENSION, "Exif.Photo.PixelXDimension"},
                    {TAG_EXIF_PIXELYDIMENSION, "Exif.Photo.PixelYDimension"},
                    {TAG_EXIF_PLANARCONFIGURATION, "Exif.Image.PlanarConfiguration"},
                    {TAG_EXIF_PRIMARYCHROMATICITIES, "Exif.Image.PrimaryChromaticities"},
                    {TAG_EXIF_REFERENCEBLACKWHITE, "Exif.Image.ReferenceBlackWhite"},
                    {TAG_EXIF_RELATEDIMAGEFILEFORMAT, "Exif.Iop.RelatedImageFileFormat"},
                    {TAG_EXIF_RELATEDIMAGELENGTH, "Exif.Iop.RelatedImageLength"},
                    {TAG_EXIF_RELATEDIMAGEWIDTH, "Exif.Iop.RelatedImageWidth"},
                    {TAG_EXIF_RELATEDSOUNDFILE, "Exif.Photo.RelatedSoundFile"},
                    {TAG_EXIF_RESOLUTIONUNIT, "Exif.Image.ResolutionUnit"},
                    {TAG_EXIF_ROWSPERSTRIP, "Exif.Image.RowsPerStrip"},
                    {TAG_EXIF_SAMPLESPERPIXEL, "Exif.Image.SamplesPerPixel"},
                    {TAG_EXIF_SATURATION, "Exif.CanonCs.Saturation"},
                    {TAG_EXIF_SATURATION, "Exif.MinoltaCs5D.Saturation"},
                    {TAG_EXIF_SATURATION, "Exif.MinoltaCs7D.Saturation"},
                    {TAG_EXIF_SATURATION, "Exif.MinoltaCsNew.Saturation"},
                    {TAG_EXIF_SATURATION, "Exif.Nikon3.Saturation"},
                    {TAG_EXIF_SATURATION, "Exif.Photo.Saturation"},
                    {TAG_EXIF_SATURATION, "Exif.Sigma.Saturation"},
                    {TAG_EXIF_SCENECAPTURETYPE, "Exif.Photo.SceneCaptureType"},
                    {TAG_EXIF_SCENETYPE, "Exif.Photo.SceneType"},
                    {TAG_EXIF_SENSINGMETHOD, "Exif.Photo.SensingMethod"},
                    {TAG_EXIF_SHARPNESS, "Exif.CanonCs.Sharpness"},
                    {TAG_EXIF_SHARPNESS, "Exif.Fujifilm.Sharpness"},
                    {TAG_EXIF_SHARPNESS, "Exif.MinoltaCs5D.Sharpness"},
                    {TAG_EXIF_SHARPNESS, "Exif.MinoltaCs7D.Sharpness"},
                    {TAG_EXIF_SHARPNESS, "Exif.MinoltaCsNew.Sharpness"},
                    {TAG_EXIF_SHARPNESS, "Exif.Photo.Sharpness"},
                    {TAG_EXIF_SHARPNESS, "Exif.Sigma.Sharpness"},
                    {TAG_EXIF_SHUTTERSPEEDVALUE, "Exif.CanonSi.ShutterSpeedValue"},
                    {TAG_EXIF_SHUTTERSPEEDVALUE, "Exif.Photo.ShutterSpeedValue"},
                    {TAG_EXIF_SPATIALFREQUENCYRESPONSE, "Exif.Photo.SpatialFrequencyResponse"},
                    {TAG_EXIF_SPECTRALSENSITIVITY, "Exif.Photo.SpectralSensitivity"},
                    {TAG_EXIF_STRIPBYTECOUNTS, "Exif.Image.StripByteCounts"},
                    {TAG_EXIF_STRIPOFFSETS, "Exif.Image.StripOffsets"},
                    {TAG_EXIF_SUBIFDS, "Exif.Image.SubIFDs"},
                    {TAG_EXIF_SUBJECTAREA, "Exif.Photo.SubjectArea"},
                    {TAG_EXIF_SUBJECTDISTANCE, "Exif.CanonSi.SubjectDistance"},
                    {TAG_EXIF_SUBJECTDISTANCE, "Exif.Photo.SubjectDistance"},
                    {TAG_EXIF_SUBJECTDISTANCERANGE, "Exif.Photo.SubjectDistanceRange"},
                    {TAG_EXIF_SUBJECTLOCATION, "Exif.Photo.SubjectLocation"},
                    {TAG_EXIF_SUBSECTIME, "Exif.Photo.SubSecTime"},
                    {TAG_EXIF_SUBSECTIMEDIGITIZED, "Exif.Photo.SubSecTimeDigitized"},
                    {TAG_EXIF_SUBSECTIMEORIGINAL, "Exif.Photo.SubSecTimeOriginal"},
                    {TAG_EXIF_TRANSFERFUNCTION, "Exif.Image.TransferFunction"},
                    {TAG_EXIF_TRANSFERRANGE, "Exif.Image.TransferRange"},
                    {TAG_EXIF_WHITEPOINT, "Exif.Image.WhitePoint"},
                    {TAG_EXIF_XMLPACKET, "Exif.Image.XMLPacket"},
                    {TAG_EXIF_XRESOLUTION, "Exif.Image.XResolution"},
                    {TAG_EXIF_YCBCRCOEFFICIENTS, "Exif.Image.YCbCrCoefficients"},
                    {TAG_EXIF_YCBCRPOSITIONING, "Exif.Image.YCbCrPositioning"},
                    {TAG_EXIF_YCBCRSUBSAMPLING, "Exif.Image.YCbCrSubSampling"},
                    {TAG_EXIF_YRESOLUTION, "Exif.Image.YResolution"},
                    {TAG_IMAGE_COMMENTS, "Exif.Image.XPComment"},
                    {TAG_IMAGE_COMMENTS, "Exif.Photo.UserComment"},
                    {TAG_IMAGE_CREATOR, "Exif.Image.XPAuthor"},
                    {TAG_IMAGE_EXPOSUREPROGRAM, "Exif.CanonCs.ExposureProgram"},
                    {TAG_IMAGE_EXPOSUREPROGRAM, "Exif.Photo.ExposureProgram"},
                    {TAG_IMAGE_EXPOSURETIME, "Exif.MinoltaCs5D.ExposureTime"},
                    {TAG_IMAGE_EXPOSURETIME, "Exif.MinoltaCs7D.ExposureTime"},
                    {TAG_IMAGE_EXPOSURETIME, "Exif.MinoltaCsNew.ExposureTime"},
                    {TAG_IMAGE_EXPOSURETIME, "Exif.Photo.ExposureTime"},
                    {TAG_IMAGE_FLASH, "Exif.MinoltaCs5D.Flash"},
                    {TAG_IMAGE_FLASH, "Exif.MinoltaCs7D.Flash"},
                    {TAG_IMAGE_FLASH, "Exif.MinoltaCsNew.Flash"},
                    {TAG_IMAGE_FLASH, "Exif.Photo.Flash"},
                    {TAG_IMAGE_FNUMBER, "Exif.MinoltaCs5D.FNumber"},
                    {TAG_IMAGE_FNUMBER, "Exif.MinoltaCs7D.FNumber"},
                    {TAG_IMAGE_FNUMBER, "Exif.MinoltaCsNew.FNumber"},
                    {TAG_IMAGE_FNUMBER, "Exif.Photo.FNumber"},
                    {TAG_IMAGE_FOCALLENGTH, "Exif.MinoltaCsNew.FocalLength"},
                    {TAG_IMAGE_FOCALLENGTH, "Exif.Photo.FocalLength"},
                    {TAG_IMAGE_HEIGHT, "Exif.CanonPi.ImageHeight"},
                    {TAG_IMAGE_HEIGHT, "Exif.Olympus.ImageHeight"},
                    {TAG_IMAGE_ISOSPEED, "Exif.CanonCs.ISOSpeed"},
                    {TAG_IMAGE_ISOSPEED, "Exif.CanonSi.ISOSpeed"},
                    {TAG_IMAGE_ISOSPEED, "Exif.MinoltaCs5D.ISOSpeed"},
                    {TAG_IMAGE_ISOSPEED, "Exif.MinoltaCs7D.ISOSpeed"},
                    {TAG_IMAGE_ISOSPEED, "Exif.MinoltaCsNew.ISOSpeed"},
                    {TAG_IMAGE_ISOSPEED, "Exif.Nikon1.ISOSpeed"},
                    {TAG_IMAGE_ISOSPEED, "Exif.Nikon2.ISOSpeed"},
                    {TAG_IMAGE_ISOSPEED, "Exif.Nikon3.ISOSpeed"},
                    {TAG_IMAGE_ISOSPEED, "Exif.Olympus.ISOSpeed"},
                    {TAG_IMAGE_KEYWORDS, "Exif.Image.XPKeywords"},
                    {TAG_IMAGE_KEYWORDS, "Iptc.Application2.Keywords"},
                    {TAG_IMAGE_MAKE, "Exif.Image.Make"},
                    {TAG_IMAGE_METERINGMODE, "Exif.CanonCs.MeteringMode"},
                    {TAG_IMAGE_METERINGMODE, "Exif.MinoltaCs5D.MeteringMode"},
                    {TAG_IMAGE_METERINGMODE, "Exif.MinoltaCsNew.MeteringMode"},
                    {TAG_IMAGE_METERINGMODE, "Exif.Photo.MeteringMode"},
                    {TAG_IMAGE_METERINGMODE, "Exif.Sigma.MeteringMode"},
                    {TAG_IMAGE_MODEL, "Exif.Image.Model"},
                    {TAG_IMAGE_ORIENTATION, "Exif.Image.Orientation"},
                    {TAG_IMAGE_ORIENTATION, "Iptc.Application2.ImageOrientation"},
                    {TAG_IMAGE_SOFTWARE, "Exif.Image.Software"},
                    {TAG_IMAGE_SOFTWARE, "Exif.Sigma.Software"},
                    {TAG_IMAGE_TITLE, "Exif.Image.XPTitle"},
                    {TAG_IMAGE_WHITEBALANCE, "Exif.CanonSi.WhiteBalance"},
                    {TAG_IMAGE_WHITEBALANCE, "Exif.Fujifilm.WhiteBalance"},
                    {TAG_IMAGE_WHITEBALANCE, "Exif.MinoltaCs5D.WhiteBalance"},
                    {TAG_IMAGE_WHITEBALANCE, "Exif.MinoltaCs7D.WhiteBalance"},
                    {TAG_IMAGE_WHITEBALANCE, "Exif.MinoltaCsNew.WhiteBalance"},
                    {TAG_IMAGE_WHITEBALANCE, "Exif.Nikon1.WhiteBalance"},
                    {TAG_IMAGE_WHITEBALANCE, "Exif.Nikon2.WhiteBalance"},
                    {TAG_IMAGE_WHITEBALANCE, "Exif.Nikon3.WhiteBalance"},
                    {TAG_IMAGE_WHITEBALANCE, "Exif.Olympus.WhiteBalance"},
                    {TAG_IMAGE_WHITEBALANCE, "Exif.Panasonic.WhiteBalance"},
                    {TAG_IMAGE_WHITEBALANCE, "Exif.Photo.WhiteBalance"},
                    {TAG_IMAGE_WHITEBALANCE, "Exif.Sigma.WhiteBalance"},
                    {TAG_IMAGE_WIDTH, "Exif.CanonPi.ImageWidth"},
                    {TAG_IMAGE_WIDTH, "Exif.Image.ImageWidth"},
                    {TAG_IMAGE_WIDTH, "Exif.Olympus.ImageWidth"},
                    {TAG_IPTC_ACTIONADVISED, "Iptc.Application2.ActionAdvised"},
                    {TAG_IPTC_ARMID, "Iptc.Envelope.ARMId"},
                    {TAG_IPTC_ARMVERSION, "Iptc.Envelope.ARMVersion"},
                    {TAG_IPTC_AUDIODURATION, "Iptc.Application2.AudioDuration"},
                    {TAG_IPTC_AUDIOOUTCUE, "Iptc.Application2.AudioOutcue"},
                    {TAG_IPTC_AUDIOTYPE, "Iptc.Application2.AudioType"},
                    {TAG_IPTC_BYLINE, "Iptc.Application2.Byline"},
                    {TAG_IPTC_BYLINETITLE, "Iptc.Application2.BylineTitle"},
                    {TAG_IPTC_CAPTION, "Iptc.Application2.Caption"},
                    {TAG_IPTC_CATEGORY, "Iptc.Application2.Category"},
                    {TAG_IPTC_CHARACTERSET, "Iptc.Envelope.CharacterSet"},
                    {TAG_IPTC_CITY, "Iptc.Application2.City"},
                    {TAG_IPTC_CONTACT, "Iptc.Application2.Contact"},
                    {TAG_IPTC_COUNTRYCODE, "Iptc.Application2.CountryCode"},
                    {TAG_IPTC_COUNTRYNAME, "Iptc.Application2.CountryName"},
                    {TAG_IPTC_CREDIT, "Iptc.Application2.Credit"},
                    {TAG_IPTC_DATECREATED, "Iptc.Application2.DateCreated"},
                    {TAG_IPTC_DATESENT, "Iptc.Envelope.DateSent"},
                    {TAG_IPTC_DESTINATION, "Iptc.Envelope.Destination"},
                    {TAG_IPTC_EDITORIALUPDATE, "Iptc.Application2.EditorialUpdate"},
                    {TAG_IPTC_EDITSTATUS, "Iptc.Application2.EditStatus"},
                    {TAG_IPTC_ENVELOPEPRIORITY, "Iptc.Envelope.EnvelopePriority"},
                    {TAG_IPTC_EXPIRATIONDATE, "Iptc.Application2.ExpirationDate"},
                    {TAG_IPTC_EXPIRATIONTIME, "Iptc.Application2.ExpirationTime"},
                    {TAG_IPTC_FILEFORMAT, "Iptc.Envelope.FileFormat"},
                    {TAG_IPTC_FILEVERSION, "Iptc.Envelope.FileVersion"},
                    {TAG_IPTC_FIXTUREID, "Iptc.Application2.FixtureId"},
                    {TAG_IPTC_HEADLINE, "Iptc.Application2.Headline"},
                    {TAG_IPTC_IMAGETYPE, "Iptc.Application2.ImageType"},
                    {TAG_IPTC_MODELVERSION, "Iptc.Envelope.ModelVersion"},
                    {TAG_IPTC_OBJECTATTRIBUTE, "Iptc.Application2.ObjectAttribute"},
                    {TAG_IPTC_OBJECTCYCLE, "Iptc.Application2.ObjectCycle"},
                    {TAG_IPTC_OBJECTNAME, "Iptc.Application2.ObjectName"},
                    {TAG_IPTC_OBJECTTYPE, "Iptc.Application2.ObjectType"},
                    {TAG_IPTC_PRODUCTID, "Iptc.Envelope.ProductId"},
                    {TAG_IPTC_PROGRAMVERSION, "Iptc.Application2.ProgramVersion"},
                    {TAG_IPTC_RASTERIZEDCAPTION, "Iptc.Application2.RasterizedCaption"},
                    {TAG_IPTC_RECORDVERSION, "Iptc.Application2.RecordVersion"},
                    {TAG_IPTC_RELEASEDATE, "Iptc.Application2.ReleaseDate"},
                    {TAG_IPTC_RELEASETIME, "Iptc.Application2.ReleaseTime"},
                    {TAG_IPTC_SERVICEID, "Iptc.Envelope.ServiceId"},
                    {TAG_IPTC_SOURCE, "Iptc.Application2.Source"},
                    {TAG_IPTC_SPECIALINSTRUCTIONS, "Iptc.Application2.SpecialInstructions"},
                    {TAG_IPTC_SUBLOCATION, "Iptc.Application2.SubLocation"},
                    {TAG_IPTC_TIMECREATED, "Iptc.Application2.TimeCreated"},
                    {TAG_IPTC_TIMESENT, "Iptc.Envelope.TimeSent"},
                    {TAG_IPTC_UNO, "Iptc.Envelope.UNO"},
                    {TAG_IPTC_URGENCY, "Iptc.Application2.Urgency"}
                   };

    load_data (exiv2_tags, exiv2_data, G_N_ELEMENTS(exiv2_data));
#endif
}


void gcmd_tags_exiv2_load_metadata(GnomeCmdFile *f)
{
    g_return_if_fail (f != NULL);
    g_return_if_fail (f->info != NULL);

    if (f->metadata && f->metadata->is_accessed(TAG_IMAGE))  return;

    if (!f->metadata)
        f->metadata = new GnomeCmdFileMetadata;

    if (!f->metadata)  return;

    f->metadata->mark_as_accessed(TAG_IMAGE);
#ifdef HAVE_EXIV2
    f->metadata->mark_as_accessed(TAG_EXIF);
    f->metadata->mark_as_accessed(TAG_IPTC);
#endif

    if (!gnome_cmd_file_is_local(f))  return;

    gchar *fname = gnome_cmd_file_get_real_path (f);

    DEBUG('t', "Loading image metadata for '%s'\n", fname);

#ifdef HAVE_EXIV2
    try
    {
        Image::AutoPtr image = ImageFactory::open(fname);

        image->readMetadata();

        readTags(f->metadata, image->exifData());
        readTags(f->metadata, image->iptcData());
    }

    catch (AnyError &e)
    {
    }
#endif

    gint width, height;
    GdkPixbufFormat *fmt = gdk_pixbuf_get_file_info (fname, &width, &height);

    g_free (fname);

    if (!fmt)
        return;

    f->metadata->addf (TAG_IMAGE_WIDTH, "%i", width);
    f->metadata->addf (TAG_IMAGE_HEIGHT, "%i", height);
}
