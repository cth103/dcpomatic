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


#include "audio_buffers.h"
#include "audio_content.h"
#include "audio_decoder.h"
#include "content_audio.h"
#include "decoder.h"
#include "decoder_factory.h"
#include "ffmpeg_content.h"
#include "film.h"
#include "frame_rate_change.h"
#include "util.h"
#include "wav_ends.h"
#include <boost/algorithm/string.hpp>


using std::dynamic_pointer_cast;
using std::make_shared;
using std::max;
using std::min;
using std::shared_ptr;
using dcpomatic::ContentTime;


/** @return samples [from, to) of a WAV at the film's sampling rate, ready for the film's sound asset, or to the end of
 *  the file if @param to is not given.
 */
static shared_ptr<const AudioBuffers>
samples(shared_ptr<const Film> film, shared_ptr<const Content> content, Frame from, boost::optional<Frame> to)
{
	auto const stream = content->audio->stream();
	auto result = make_shared<AudioBuffers>(film->audio_channels(), 0);

	auto decoder = decoder_factory(film, content, false, false, {});
	if (from > 0) {
		decoder->seek(ContentTime::from_frames(from, content->audio->resampled_frame_rate(film)), true);
	}
	bool finished = false;
	decoder->audio->Data.connect([&](AudioStreamPtr, ContentAudio data) {
		auto const start = max(from, data.frame);
		auto const end = min(to.get_value_or(data.frame + data.audio->frames()), data.frame + data.audio->frames());
		if (start < end) {
			auto part = make_shared<AudioBuffers>(data.audio, end - start, start - data.frame);
			result->append(remap(part, film->audio_channels(), stream->mapping()));
		}
		finished = to && end >= *to;
	});

	while (!finished && !decoder->pass()) {}

	result->apply_gain(content->audio->gain());
	return result;
}


bool
wav_keeps_whole_length(shared_ptr<const Film> film, shared_ptr<const Content> content)
{
	if (!dynamic_pointer_cast<const FFmpegContent>(content) || content->video || !content->audio || content->number_of_paths() != 1) {
		return false;
	}

	if (!boost::iequals(content->path(0).extension().string(), ".wav") || FrameRateChange(film, content).change_speed()) {
		return false;
	}

	auto const rate = static_cast<double>(film->video_frame_rate());
	return content->trim_start() == content->trim_start().round(rate) && content->trim_end() == content->trim_end().round(rate);
}


Frame
wav_head_frames(shared_ptr<const Film> film, shared_ptr<const Content> content)
{
	return content->trim_start().frames_round(film->video_frame_rate());
}


shared_ptr<const AudioBuffers>
wav_head_audio(shared_ptr<const Film> film, shared_ptr<const Content> content)
{
	auto const head = wav_head_frames(film, content) * film->audio_frame_rate() / film->video_frame_rate();
	if (head == 0) {
		return {};
	}
	return samples(film, content, 0, head);
}


shared_ptr<const AudioBuffers>
wav_tail_audio(shared_ptr<const Film> film, shared_ptr<const Content> content)
{
	auto const tail = content->trim_end().frames_round(film->audio_frame_rate());
	if (tail == 0) {
		return {};
	}
	auto const stream = content->audio->stream();
	auto const length = stream->length() * content->audio->resampled_frame_rate(film) / stream->frame_rate();
	return samples(film, content, length - tail, boost::none);
}
