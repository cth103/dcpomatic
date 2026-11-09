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


/** Who made an immersive audio track file (ST 429-18), as far as it can be seen (see doc/immersive_audio.md) */
enum class ImmersiveAudioOrigin
{
	/** Dolby Atmos: the frames start with Dolby's preamble, and the file's CompanyName starts with "Dolby" or an
	 *  object of its frames is coded with the silence gain prefix, which IAB Profile 1 does not allow
	 */
	DOLBY,
	/** Anything else */
	IAB
};


/** Read as little as possible of an immersive audio track file to see who made it: the MXF Identification set and
 *  the start of the first frame, and, when the frames have Dolby's preamble but the CompanyName is another one (a
 *  Dolby Atmos file wrapped again, by DCP-o-matic for example), the objects of up to 1000 frames.  This also reads
 *  IAB files that have no Dolby Atmos sub-descriptor, which dcp::AtmosAsset cannot open.  None of this is proof:
 *  any encoder can write the same bytes.
 *
 *  Throws FileError if the file cannot be read as a DCP data track file, or is encrypted.
 */
ImmersiveAudioOrigin immersive_audio_origin(boost::filesystem::path mxf);


#endif
