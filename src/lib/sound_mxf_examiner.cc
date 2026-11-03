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


#include "sound_mxf_content.h"
#include "sound_mxf_examiner.h"
#include <dcp/sound_asset.h>


using std::make_shared;
using std::shared_ptr;


SoundMXFExaminer::SoundMXFExaminer(shared_ptr<const SoundMXFContent> content)
	: _asset(make_shared<dcp::SoundAsset>(content->path(0)))
{

}


int
SoundMXFExaminer::audio_channels() const
{
	return _asset->channels();
}


Frame
SoundMXFExaminer::audio_length() const
{
	auto const edit_rate = _asset->edit_rate();
	return _asset->intrinsic_duration() * _asset->sampling_rate() * edit_rate.denominator / edit_rate.numerator;
}


int
SoundMXFExaminer::audio_frame_rate() const
{
	return _asset->sampling_rate();
}


