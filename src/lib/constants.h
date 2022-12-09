/*
    Copyright (C) 2012-2020 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/


#ifndef DCPOMATIC_CONSTANTS_H
#define DCPOMATIC_CONSTANTS_H


/** The maximum number of audio channels that we can have in a DCP */
#define MAX_DCP_AUDIO_CHANNELS 16
/** Message broadcast to find possible encoding servers */
#define DCPOMATIC_HELLO "I mean really, Ray, it's used."
/** Number of films to keep in history */
#define HISTORY_SIZE 10
#define REPORT_PROBLEM _("Please report this problem by using Help -> Report a problem or via email to carl@dcpomatic.com")
#define TEXT_FONT_ID "font"
/** Largest KDM size (in bytes) that will be accepted */
#define MAX_KDM_SIZE (256 * 1024)
/** Number of lines that closed caption viewers will display */
#define MAX_CLOSED_CAPTION_LINES 3
/** Maximum line length of closed caption viewers, according to SMPTE Bv2.1 */
#define MAX_CLOSED_CAPTION_LENGTH 32
/** Maximum size of a subtitle / closed caption MXF in bytes, according to SMPTE Bv2.1 */
#define MAX_TEXT_MXF_SIZE (115 * 1024 * 1024)
#define MAX_TEXT_MXF_SIZE_TEXT "115MB"
/** Maximum size of a font file, in bytes */
#define MAX_FONT_FILE_SIZE (640 * 1024)
#define MAX_FONT_FILE_SIZE_TEXT "640KB"
/** Maximum size of the XML part of a closed caption file, according to SMPTE Bv2.1 */
#define MAX_CLOSED_CAPTION_XML_SIZE (256 * 1024)
#define MAX_CLOSED_CAPTION_XML_SIZE_TEXT "256KB"
#define CERTIFICATE_VALIDITY_PERIOD (10 * 365)


#endif

