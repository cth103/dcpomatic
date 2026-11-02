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
#include "overlaps.h"
#include "util.h"
#include "wav_ends.h"
#include <boost/algorithm/string.hpp>
#include <algorithm>


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


/** @return true if @param content is a WAV that is the only sound in a reel of its own */
static bool
wav_is_whole_reel(shared_ptr<const Film> film, shared_ptr<const Content> content)
{
	if (!wav_keeps_whole_length(film, content)) {
		return false;
	}

	auto const period = dcpomatic::DCPTimePeriod(content->position(), content->end(film));
	auto const reels = film->reels();
	auto has_audio = [](shared_ptr<const Content> c) { return static_cast<bool>(c->audio); };
	return std::find(reels.begin(), reels.end(), period) != reels.end()
		&& dcpomatic::overlaps(film, film->content(), has_audio, period.from, period.to).size() == 1;
}


/** @return a digest of everything that decides the sound written for a WAV.  Equal digests mean equal sound */
static std::string
wav_digest(shared_ptr<const Content> content)
{
	auto const audio = content->audio;
	return fmt::format(
		"{}|{}|{}|{}|{}|{}|{}|{}",
		content->digest(),
		content->trim_start().get(),
		content->trim_end().get(),
		audio->gain(),
		audio->delay(),
		audio->fade_in().get(),
		audio->fade_out().get(),
		audio->stream()->mapping().digest()
		);
}


shared_ptr<const Content>
wav_shared_source(shared_ptr<const Film> film, shared_ptr<const Content> content)
{
	/* The Atmos sync signal changes from frame to frame, and an audio processor has state, so those reels differ */
	if (film->contains_atmos_content() || film->audio_processor() || !wav_is_whole_reel(film, content)) {
		return {};
	}

	/* A delay moves sound across the reel's boundaries */
	if (content->audio->delay() != 0) {
		return {};
	}

	auto const digest = wav_digest(content);
	shared_ptr<const Content> found;
	for (auto other: film->content()) {
		if (other == content || other->position() >= content->position() || !wav_is_whole_reel(film, other)) {
			continue;
		}
		if (wav_digest(other) == digest && (!found || other->position() < found->position())) {
			found = other;
		}
	}

	return found;
}


Frame
wav_head_frames(shared_ptr<const Film> film, shared_ptr<const Content> content)
{
	return content->trim_start().frames_round(film->video_frame_rate());
}


std::vector<shared_ptr<const Content>>
wav_group(shared_ptr<const Film> film, shared_ptr<const Content> content)
{
	auto has_audio = [](shared_ptr<const Content> c) { return static_cast<bool>(c->audio); };

	std::vector<shared_ptr<const Content>> group;
	for (auto other: dcpomatic::overlaps(film, film->content(), has_audio, content->position(), content->end(film))) {
		if (
			!wav_keeps_whole_length(film, other) ||
			other->position() != content->position() ||
			other->end(film) != content->end(film) ||
			other->trim_start() != content->trim_start() ||
			other->trim_end() != content->trim_end()
		   ) {
			return {};
		}
		group.push_back(other);
	}
	return group;
}


/** @return the buffers from @param get for each of @param wavs, mixed */
static shared_ptr<const AudioBuffers>
mix(std::vector<shared_ptr<const Content>> const& wavs, std::function<shared_ptr<const AudioBuffers> (shared_ptr<const Content>)> get)
{
	auto mixed = make_shared<AudioBuffers>(get(wavs.front()));
	for (size_t i = 1; i < wavs.size(); ++i) {
		auto const other = get(wavs[i]);
		mixed->accumulate_frames(other.get(), min(mixed->frames(), other->frames()), 0, 0);
	}
	return mixed;
}


shared_ptr<const AudioBuffers>
wav_head_audio(shared_ptr<const Film> film, std::vector<shared_ptr<const Content>> const& wavs)
{
	auto const head = wav_head_frames(film, wavs.front()) * film->audio_frame_rate() / film->video_frame_rate();
	if (head == 0) {
		return {};
	}
	return mix(wavs, [film, head](shared_ptr<const Content> content) { return samples(film, content, 0, head); });
}


shared_ptr<const AudioBuffers>
wav_tail_audio(shared_ptr<const Film> film, std::vector<shared_ptr<const Content>> const& wavs)
{
	auto const tail = wavs.front()->trim_end().frames_round(film->audio_frame_rate());
	if (tail == 0) {
		return {};
	}
	return mix(wavs, [film, tail](shared_ptr<const Content> content) {
		auto const stream = content->audio->stream();
		auto const length = stream->length() * content->audio->resampled_frame_rate(film) / stream->frame_rate();
		return samples(film, content, length - tail, boost::none);
	});
}
