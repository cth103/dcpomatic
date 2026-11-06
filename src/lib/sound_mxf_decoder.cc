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


#include "audio_content.h"
#include "audio_decoder.h"
#include "sound_mxf_content.h"
#include "sound_mxf_decoder.h"


using std::make_shared;
using std::shared_ptr;
using namespace dcpomatic;


SoundMXFDecoder::SoundMXFDecoder(shared_ptr<const Film> film, shared_ptr<const SoundMXFContent> content, bool fast)
	: Decoder(film)
	, _content(content)
	, _asset(make_shared<dcp::SoundAsset>(content->path(0)))
{
	audio = make_shared<AudioDecoder>(this, content->audio, fast);

	_reader = _asset->start_read();
	_reader->set_check_hmac(false);

	auto const edit_rate = _asset->edit_rate();
	_edit_rate = static_cast<double>(edit_rate.numerator) / edit_rate.denominator;
}


bool
SoundMXFDecoder::pass()
{
	auto const frame = _frame;

	if (frame >= _asset->intrinsic_duration()) {
		audio->flush();
		return true;
	}

	if (audio->ignore()) {
		/* The sound is not wanted: the file goes into the DCP as it is, or the film is being played without audio */
		audio->flush();
		return true;
	}

	auto data = sound_mxf_frame_audio(_reader->get_frame(frame), _content->audio->stream()->channels());
	audio->emit(film(), _content->audio->stream(), data, ContentTime::from_frames(frame, _edit_rate));

	++_frame;
	return false;
}


void
SoundMXFDecoder::seek(ContentTime t, bool accurate)
{
	Decoder::seek(t, accurate);
	_frame = t.frames_floor(_edit_rate);
}
