/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_FILM_PROPERTY_H
#define DCPOMATIC_FILM_PROPERTY_H


/** Identifiers for the parts of a Film's state; used for signalling changes.
 *  This could go in Film but separating it out saves a lot of includes of
 *  film.h
 */
enum class FilmProperty {
	NONE,
	NAME,
	USE_ISDCF_NAME,
	/** The playlist's content list has changed (i.e. content has been added or removed) */
	CONTENT,
	/** The order of content in the playlist has changed */
	CONTENT_ORDER,
	DCP_CONTENT_TYPE,
	CONTAINER,
	RESOLUTION,
	ENCRYPTED,
	VIDEO_BIT_RATE,
	VIDEO_FRAME_RATE,
	AUDIO_FRAME_RATE,
	AUDIO_CHANNELS,
	/** The setting of _three_d has changed */
	THREE_D,
	SEQUENCE,
	INTEROP,
	VIDEO_ENCODING,
	LIMIT_TO_SMPTE_BV20,
	AUDIO_PROCESSOR,
	REEL_TYPE,
	REEL_LENGTH,
	CUSTOM_REEL_BOUNDARIES,
	REENCODE_J2K,
	MARKERS,
	RATINGS,
	CONTENT_VERSIONS,
	NAME_LANGUAGE,
	AUDIO_LANGUAGE,
	RELEASE_TERRITORY,
	SIGN_LANGUAGE_VIDEO_LANGUAGE,
	VERSION_NUMBER,
	STATUS,
	CHAIN,
	DISTRIBUTOR,
	FACILITY,
	STUDIO,
	TEMP_VERSION,
	PRE_RELEASE,
	RED_BAND,
	TWO_D_VERSION_OF_THREE_D,
	LUMINANCE,
	TERRITORY_TYPE,
};


#endif

