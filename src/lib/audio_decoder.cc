/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "audio_decoder.h"
#include "audio_buffers.h"
#include "exceptions.h"
#include "log.h"

#include "i18n.h"

using std::stringstream;
using std::list;
using std::pair;
using std::cout;
using boost::optional;
using boost::shared_ptr;

AudioDecoder::AudioDecoder (shared_ptr<const Film> f)
	: Decoder (f)
	, _next_audio_frame (0)
{
}

#if 0
void
AudioDecoder::process_end ()
{
	if (_swr_context) {

		shared_ptr<const Film> film = _film.lock ();
		assert (film);
		
		shared_ptr<AudioBuffers> out (new AudioBuffers (film->audio_mapping().dcp_channels(), 256));
			
		while (1) {
			int const frames = swr_convert (_swr_context, (uint8_t **) out->data(), 256, 0, 0);

			if (frames < 0) {
				throw EncodeError (_("could not run sample-rate converter"));
			}

			if (frames == 0) {
				break;
			}

			out->set_frames (frames);
			_writer->write (out);
		}

	}
}
#endif

void
AudioDecoder::audio (shared_ptr<const AudioBuffers> data, AudioContent::Frame frame)
{
	Audio (data, frame);
	_next_audio_frame = frame + data->frames ();
}
