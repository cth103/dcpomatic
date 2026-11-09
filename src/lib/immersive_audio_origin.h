/*
    Copyright (C) 2026 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_IMMERSIVE_AUDIO_ORIGIN_H
#define DCPOMATIC_IMMERSIVE_AUDIO_ORIGIN_H


#include <boost/filesystem.hpp>


/** Who made an immersive audio track file (ST 429-18), as far as its metadata says */
enum class ImmersiveAudioOrigin
{
	/** Dolby Atmos: the first frame starts with Dolby's preamble and the file's CompanyName starts with "Dolby" */
	DOLBY,
	/** Anything else: IAB from another encoder, or Dolby frames wrapped again by another program */
	IAB
};


/** Read as little as possible of an immersive audio track file to see who made it: the MXF Identification set and
 *  the start of the first frame.  This also reads IAB files that have no Dolby Atmos sub-descriptor, which
 *  dcp::AtmosAsset cannot open.  Both things it looks at are metadata, so they can be forged.
 *
 *  Throws FileError if the file cannot be read as a DCP data track file, or is encrypted.
 */
ImmersiveAudioOrigin immersive_audio_origin(boost::filesystem::path mxf);


#endif
