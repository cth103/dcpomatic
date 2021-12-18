/*
    Copyright (C) 2017-2018 Carl Hetherington <cth@carlh.net>

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


#include "butler.h"
#include "cross.h"
#include "ffmpeg_encoder.h"
#include "film.h"
#include "image.h"
#include "job.h"
#include "log.h"
#include "player.h"
#include "player_video.h"
#include "compose.hpp"
#include <iostream>

#include "i18n.h"


using std::cout;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::weak_ptr;
using boost::bind;
using boost::optional;
using namespace dcpomatic;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


/** @param key Key to use to encrypt MP4 outputs */
FFmpegEncoder::FFmpegEncoder (
	shared_ptr<const Film> film,
	weak_ptr<Job> job,
	boost::filesystem::path output,
	ExportFormat format,
	bool mixdown_to_stereo,
	bool split_reels,
	bool audio_stream_per_channel,
	int x264_crf
	)
	: Encoder (film, job)
	, _history (200)
	, _output (output)
	, _format (format)
	, _split_reels (split_reels)
	, _audio_stream_per_channel (audio_stream_per_channel)
	, _x264_crf (x264_crf)
{
	_player->set_always_burn_open_subtitles ();
	_player->set_play_referenced ();

	int const ch = film->audio_channels ();

	AudioMapping map;
	if (mixdown_to_stereo) {
		_output_audio_channels = 2;
		map = AudioMapping (ch, 2);
		float const overall_gain = 2 / (4 + sqrt(2));
		float const minus_3dB = 1 / sqrt(2);
		if (ch == 2) {
			map.set (dcp::Channel::LEFT, 0, 1);
			map.set (dcp::Channel::RIGHT, 1, 1);
		} else if (ch == 4) {
			map.set (dcp::Channel::LEFT,   0, overall_gain);
			map.set (dcp::Channel::RIGHT,  1, overall_gain);
			map.set (dcp::Channel::CENTRE, 0, overall_gain * minus_3dB);
			map.set (dcp::Channel::CENTRE, 1, overall_gain * minus_3dB);
			map.set (dcp::Channel::LS,     0, overall_gain);
		} else if (ch >= 6) {
			map.set (dcp::Channel::LEFT,   0, overall_gain);
			map.set (dcp::Channel::RIGHT,  1, overall_gain);
			map.set (dcp::Channel::CENTRE, 0, overall_gain * minus_3dB);
			map.set (dcp::Channel::CENTRE, 1, overall_gain * minus_3dB);
			map.set (dcp::Channel::LS,     0, overall_gain);
			map.set (dcp::Channel::RS,     1, overall_gain);
		}
		/* XXX: maybe we should do something better for >6 channel DCPs */
	} else {
		/* Our encoders don't really want to encode any channel count between 9 and 15 inclusive,
		 * so let's just use 16 channel exports for any project with more than 8 channels.
		 */
		_output_audio_channels = ch > 8 ? 16 : ch;
		map = AudioMapping (ch, _output_audio_channels);
		for (int i = 0; i < ch; ++i) {
			map.set (i, i, 1);
		}
	}

	_butler = std::make_shared<Butler>(
		_film, _player, map, _output_audio_channels, bind(&PlayerVideo::force, FFmpegFileEncoder::pixel_format(format)), VideoRange::VIDEO, Image::Alignment::PADDED, false, false
		);
}


void
FFmpegEncoder::go ()
{
	{
		auto job = _job.lock ();
		DCPOMATIC_ASSERT (job);
		job->sub (_("Encoding"));
	}

	Waker waker;

	list<FileEncoderSet> file_encoders;

	int const files = _split_reels ? _film->reels().size() : 1;
	for (int i = 0; i < files; ++i) {

		boost::filesystem::path filename = _output;
		string extension = boost::filesystem::extension (filename);
		filename = boost::filesystem::change_extension (filename, "");

		if (files > 1) {
			/// TRANSLATORS: _reel%1 here is to be added to an export filename to indicate
			/// which reel it is.  Preserve the %1; it will be replaced with the reel number.
			filename = filename.string() + String::compose(_("_reel%1"), i + 1);
		}

		file_encoders.push_back (
			FileEncoderSet (
				_film->frame_size(),
				_film->video_frame_rate(),
				_film->audio_frame_rate(),
				_output_audio_channels,
				_format,
				_audio_stream_per_channel,
				_x264_crf,
				_film->three_d(),
				filename,
				extension
				)
			);
	}

	auto reel_periods = _film->reels ();
	auto reel = reel_periods.begin ();
	auto encoder = file_encoders.begin ();

	auto const video_frame = DCPTime::from_frames (1, _film->video_frame_rate ());
	int const audio_frames = video_frame.frames_round(_film->audio_frame_rate());
	std::vector<float> interleaved(_output_audio_channels * audio_frames);
	auto deinterleaved = make_shared<AudioBuffers>(_output_audio_channels, audio_frames);
	int const gets_per_frame = _film->three_d() ? 2 : 1;
	for (DCPTime i; i < _film->length(); i += video_frame) {

		if (file_encoders.size() > 1 && !reel->contains(i)) {
			/* Next reel and file */
			++reel;
			++encoder;
			DCPOMATIC_ASSERT (reel != reel_periods.end());
			DCPOMATIC_ASSERT (encoder != file_encoders.end());
		}

		for (int j = 0; j < gets_per_frame; ++j) {
			Butler::Error e;
			auto v = _butler->get_video (Butler::Behaviour::BLOCKING, &e);
			_butler->rethrow ();
			if (v.first) {
				auto fe = encoder->get (v.first->eyes());
				if (fe) {
					fe->video(v.first, v.second - reel->from);
				}
			} else {
				if (e.code != Butler::Error::Code::FINISHED) {
					throw DecodeError(String::compose("Error during decoding: %1", e.summary()));
				}
			}
		}

		_history.event ();

		{
			boost::mutex::scoped_lock lm (_mutex);
			_last_time = i;
		}

		auto job = _job.lock ();
		if (job) {
			job->set_progress (float(i.get()) / _film->length().get());
		}

		waker.nudge ();

		_butler->get_audio (Butler::Behaviour::BLOCKING, interleaved.data(), audio_frames);
		/* XXX: inefficient; butler interleaves and we deinterleave again */
		float* p = interleaved.data();
		for (int j = 0; j < audio_frames; ++j) {
			for (int k = 0; k < _output_audio_channels; ++k) {
				deinterleaved->data(k)[j] = *p++;
			}
		}
		encoder->audio (deinterleaved);
	}

	for (auto i: file_encoders) {
		i.flush ();
	}
}

optional<float>
FFmpegEncoder::current_rate () const
{
	return _history.rate ();
}

Frame
FFmpegEncoder::frames_done () const
{
	boost::mutex::scoped_lock lm (_mutex);
	return _last_time.frames_round (_film->video_frame_rate ());
}

FFmpegEncoder::FileEncoderSet::FileEncoderSet (
	dcp::Size video_frame_size,
	int video_frame_rate,
	int audio_frame_rate,
	int channels,
	ExportFormat format,
	bool audio_stream_per_channel,
	int x264_crf,
	bool three_d,
	boost::filesystem::path output,
	string extension
	)
{
	if (three_d) {
		/// TRANSLATORS: L here is an abbreviation for "left", to indicate the left-eye part of a 3D export
		_encoders[Eyes::LEFT] = make_shared<FFmpegFileEncoder>(
			video_frame_size, video_frame_rate, audio_frame_rate, channels, format,
			audio_stream_per_channel, x264_crf, String::compose("%1_%2%3", output.string(), _("L"), extension)
			);
		/// TRANSLATORS: R here is an abbreviation for "right", to indicate the right-eye part of a 3D export
		_encoders[Eyes::RIGHT] = make_shared<FFmpegFileEncoder>(
			video_frame_size, video_frame_rate, audio_frame_rate, channels, format,
			audio_stream_per_channel, x264_crf, String::compose("%1_%2%3", output.string(), _("R"), extension)
			);
	} else {
		_encoders[Eyes::BOTH] = make_shared<FFmpegFileEncoder>(
			video_frame_size, video_frame_rate, audio_frame_rate, channels, format,
			audio_stream_per_channel, x264_crf, String::compose("%1%2", output.string(), extension)
			);
	}
}

shared_ptr<FFmpegFileEncoder>
FFmpegEncoder::FileEncoderSet::get (Eyes eyes) const
{
	if (_encoders.size() == 1) {
		/* We are doing a 2D export... */
		if (eyes == Eyes::LEFT) {
			/* ...but we got some 3D data; put the left eye into the output... */
			eyes = Eyes::BOTH;
		} else if (eyes == Eyes::RIGHT) {
			/* ...and ignore the right eye.*/
			return {};
		}
	}

	auto i = _encoders.find (eyes);
	DCPOMATIC_ASSERT (i != _encoders.end());
	return i->second;
}

void
FFmpegEncoder::FileEncoderSet::flush ()
{
	for (auto& i: _encoders) {
		i.second->flush ();
	}
}

void
FFmpegEncoder::FileEncoderSet::audio (shared_ptr<AudioBuffers> a)
{
	for (auto& i: _encoders) {
		i.second->audio (a);
	}
}
