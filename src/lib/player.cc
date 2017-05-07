/*
    Copyright (C) 2013-2017 Carl Hetherington <cth@carlh.net>

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

#include "player.h"
#include "film.h"
#include "audio_buffers.h"
#include "content_audio.h"
#include "dcp_content.h"
#include "job.h"
#include "image.h"
#include "raw_image_proxy.h"
#include "ratio.h"
#include "log.h"
#include "render_subtitles.h"
#include "config.h"
#include "content_video.h"
#include "player_video.h"
#include "frame_rate_change.h"
#include "audio_processor.h"
#include "playlist.h"
#include "referenced_reel_asset.h"
#include "decoder_factory.h"
#include "decoder.h"
#include "video_decoder.h"
#include "audio_decoder.h"
#include "subtitle_content.h"
#include "subtitle_decoder.h"
#include "ffmpeg_content.h"
#include "audio_content.h"
#include "content_subtitle.h"
#include "dcp_decoder.h"
#include "image_decoder.h"
#include "resampler.h"
#include "compose.hpp"
#include <dcp/reel.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/reel_subtitle_asset.h>
#include <dcp/reel_picture_asset.h>
#include <boost/foreach.hpp>
#include <stdint.h>
#include <algorithm>
#include <iostream>

#include "i18n.h"

#define LOG_GENERAL(...) _film->log()->log (String::compose (__VA_ARGS__), LogEntry::TYPE_GENERAL);

using std::list;
using std::cout;
using std::min;
using std::max;
using std::min;
using std::vector;
using std::pair;
using std::map;
using std::make_pair;
using std::copy;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;
using boost::optional;
using boost::scoped_ptr;

Player::Player (shared_ptr<const Film> film, shared_ptr<const Playlist> playlist)
	: _film (film)
	, _playlist (playlist)
	, _have_valid_pieces (false)
	, _ignore_video (false)
	, _ignore_audio (false)
	, _always_burn_subtitles (false)
	, _fast (false)
	, _play_referenced (false)
	, _audio_merger (_film->audio_frame_rate())
{
	_film_changed_connection = _film->Changed.connect (bind (&Player::film_changed, this, _1));
	_playlist_changed_connection = _playlist->Changed.connect (bind (&Player::playlist_changed, this));
	_playlist_content_changed_connection = _playlist->ContentChanged.connect (bind (&Player::playlist_content_changed, this, _1, _2, _3));
	set_video_container_size (_film->frame_size ());

	film_changed (Film::AUDIO_PROCESSOR);

	seek (DCPTime (), true);
}

void
Player::setup_pieces ()
{
	_pieces.clear ();

	BOOST_FOREACH (shared_ptr<Content> i, _playlist->content ()) {

		if (!i->paths_valid ()) {
			continue;
		}

		shared_ptr<Decoder> decoder = decoder_factory (i, _film->log());
		FrameRateChange frc (i->active_video_frame_rate(), _film->video_frame_rate());

		if (!decoder) {
			/* Not something that we can decode; e.g. Atmos content */
			continue;
		}

		if (decoder->video && _ignore_video) {
			decoder->video->set_ignore ();
		}

		if (decoder->audio && _ignore_audio) {
			decoder->audio->set_ignore ();
		}

		shared_ptr<DCPDecoder> dcp = dynamic_pointer_cast<DCPDecoder> (decoder);
		if (dcp && _play_referenced) {
			dcp->set_decode_referenced ();
		}

		shared_ptr<Piece> piece (new Piece (i, decoder, frc));
		_pieces.push_back (piece);

		if (decoder->video) {
			decoder->video->Data.connect (bind (&Player::video, this, weak_ptr<Piece> (piece), _1));
		}

		if (decoder->audio) {
			decoder->audio->Data.connect (bind (&Player::audio, this, weak_ptr<Piece> (piece), _1, _2));
		}

		if (decoder->subtitle) {
			decoder->subtitle->ImageStart.connect (bind (&Player::image_subtitle_start, this, weak_ptr<Piece> (piece), _1));
			decoder->subtitle->TextStart.connect (bind (&Player::text_subtitle_start, this, weak_ptr<Piece> (piece), _1));
			decoder->subtitle->Stop.connect (bind (&Player::subtitle_stop, this, weak_ptr<Piece> (piece), _1));
		}
	}

	BOOST_FOREACH (shared_ptr<Piece> i, _pieces) {
		if (i->content->audio) {
			BOOST_FOREACH (AudioStreamPtr j, i->content->audio->streams()) {
				_stream_states[j] = StreamState (i, i->content->position ());
			}
		}
	}

	if (!_play_referenced) {
		BOOST_FOREACH (shared_ptr<Piece> i, _pieces) {
			shared_ptr<DCPContent> dc = dynamic_pointer_cast<DCPContent> (i->content);
			if (dc) {
				if (dc->reference_video()) {
					_no_video.push_back (DCPTimePeriod (dc->position(), dc->end()));
				}
				if (dc->reference_audio()) {
					_no_audio.push_back (DCPTimePeriod (dc->position(), dc->end()));
				}
			}
		}
	}

	_last_video_time = optional<DCPTime> ();
	_last_audio_time = optional<DCPTime> ();
	_have_valid_pieces = true;
}

void
Player::playlist_content_changed (weak_ptr<Content> w, int property, bool frequent)
{
	shared_ptr<Content> c = w.lock ();
	if (!c) {
		return;
	}

	if (
		property == ContentProperty::POSITION ||
		property == ContentProperty::LENGTH ||
		property == ContentProperty::TRIM_START ||
		property == ContentProperty::TRIM_END ||
		property == ContentProperty::PATH ||
		property == VideoContentProperty::FRAME_TYPE ||
		property == DCPContentProperty::NEEDS_ASSETS ||
		property == DCPContentProperty::NEEDS_KDM ||
		property == SubtitleContentProperty::COLOUR ||
		property == SubtitleContentProperty::OUTLINE ||
		property == SubtitleContentProperty::SHADOW ||
		property == SubtitleContentProperty::EFFECT_COLOUR ||
		property == FFmpegContentProperty::SUBTITLE_STREAM ||
		property == VideoContentProperty::COLOUR_CONVERSION
		) {

		_have_valid_pieces = false;
		Changed (frequent);

	} else if (
		property == SubtitleContentProperty::LINE_SPACING ||
		property == SubtitleContentProperty::OUTLINE_WIDTH ||
		property == SubtitleContentProperty::Y_SCALE ||
		property == SubtitleContentProperty::FADE_IN ||
		property == SubtitleContentProperty::FADE_OUT ||
		property == ContentProperty::VIDEO_FRAME_RATE ||
		property == SubtitleContentProperty::USE ||
		property == SubtitleContentProperty::X_OFFSET ||
		property == SubtitleContentProperty::Y_OFFSET ||
		property == SubtitleContentProperty::X_SCALE ||
		property == SubtitleContentProperty::FONTS ||
		property == VideoContentProperty::CROP ||
		property == VideoContentProperty::SCALE ||
		property == VideoContentProperty::FADE_IN ||
		property == VideoContentProperty::FADE_OUT
		) {

		Changed (frequent);
	}
}

void
Player::set_video_container_size (dcp::Size s)
{
	if (s == _video_container_size) {
		return;
	}

	_video_container_size = s;

	_black_image.reset (new Image (AV_PIX_FMT_RGB24, _video_container_size, true));
	_black_image->make_black ();

	Changed (false);
}

void
Player::playlist_changed ()
{
	_have_valid_pieces = false;
	Changed (false);
}

void
Player::film_changed (Film::Property p)
{
	/* Here we should notice Film properties that affect our output, and
	   alert listeners that our output now would be different to how it was
	   last time we were run.
	*/

	if (p == Film::CONTAINER) {
		Changed (false);
	} else if (p == Film::VIDEO_FRAME_RATE) {
		/* Pieces contain a FrameRateChange which contains the DCP frame rate,
		   so we need new pieces here.
		*/
		_have_valid_pieces = false;
		Changed (false);
	} else if (p == Film::AUDIO_PROCESSOR) {
		if (_film->audio_processor ()) {
			_audio_processor = _film->audio_processor()->clone (_film->audio_frame_rate ());
		}
	}
}

list<PositionImage>
Player::transform_image_subtitles (list<ImageSubtitle> subs) const
{
	list<PositionImage> all;

	for (list<ImageSubtitle>::const_iterator i = subs.begin(); i != subs.end(); ++i) {
		if (!i->image) {
			continue;
		}

		/* We will scale the subtitle up to fit _video_container_size */
		dcp::Size scaled_size (i->rectangle.width * _video_container_size.width, i->rectangle.height * _video_container_size.height);

		/* Then we need a corrective translation, consisting of two parts:
		 *
		 * 1.  that which is the result of the scaling of the subtitle by _video_container_size; this will be
		 *     rect.x * _video_container_size.width and rect.y * _video_container_size.height.
		 *
		 * 2.  that to shift the origin of the scale by subtitle_scale to the centre of the subtitle; this will be
		 *     (width_before_subtitle_scale * (1 - subtitle_x_scale) / 2) and
		 *     (height_before_subtitle_scale * (1 - subtitle_y_scale) / 2).
		 *
		 * Combining these two translations gives these expressions.
		 */

		all.push_back (
			PositionImage (
				i->image->scale (
					scaled_size,
					dcp::YUV_TO_RGB_REC601,
					i->image->pixel_format (),
					true,
					_fast
					),
				Position<int> (
					lrint (_video_container_size.width * i->rectangle.x),
					lrint (_video_container_size.height * i->rectangle.y)
					)
				)
			);
	}

	return all;
}

shared_ptr<PlayerVideo>
Player::black_player_video_frame () const
{
	return shared_ptr<PlayerVideo> (
		new PlayerVideo (
			shared_ptr<const ImageProxy> (new RawImageProxy (_black_image)),
			Crop (),
			optional<double> (),
			_video_container_size,
			_video_container_size,
			EYES_BOTH,
			PART_WHOLE,
			PresetColourConversion::all().front().conversion
		)
	);
}

Frame
Player::dcp_to_content_video (shared_ptr<const Piece> piece, DCPTime t) const
{
	DCPTime s = t - piece->content->position ();
	s = min (piece->content->length_after_trim(), s);
	s = max (DCPTime(), s + DCPTime (piece->content->trim_start(), piece->frc));

	/* It might seem more logical here to convert s to a ContentTime (using the FrameRateChange)
	   then convert that ContentTime to frames at the content's rate.  However this fails for
	   situations like content at 29.9978733fps, DCP at 30fps.  The accuracy of the Time type is not
	   enough to distinguish between the two with low values of time (e.g. 3200 in Time units).

	   Instead we convert the DCPTime using the DCP video rate then account for any skip/repeat.
	*/
	return s.frames_floor (piece->frc.dcp) / piece->frc.factor ();
}

DCPTime
Player::content_video_to_dcp (shared_ptr<const Piece> piece, Frame f) const
{
	/* See comment in dcp_to_content_video */
	DCPTime const d = DCPTime::from_frames (f * piece->frc.factor(), piece->frc.dcp) - DCPTime (piece->content->trim_start (), piece->frc);
	return max (DCPTime (), d + piece->content->position ());
}

Frame
Player::dcp_to_resampled_audio (shared_ptr<const Piece> piece, DCPTime t) const
{
	DCPTime s = t - piece->content->position ();
	s = min (piece->content->length_after_trim(), s);
	/* See notes in dcp_to_content_video */
	return max (DCPTime (), DCPTime (piece->content->trim_start (), piece->frc) + s).frames_floor (_film->audio_frame_rate ());
}

DCPTime
Player::resampled_audio_to_dcp (shared_ptr<const Piece> piece, Frame f) const
{
	/* See comment in dcp_to_content_video */
	DCPTime const d = DCPTime::from_frames (f, _film->audio_frame_rate()) - DCPTime (piece->content->trim_start(), piece->frc);
	return max (DCPTime (), d + piece->content->position ());
}

ContentTime
Player::dcp_to_content_time (shared_ptr<const Piece> piece, DCPTime t) const
{
	DCPTime s = t - piece->content->position ();
	s = min (piece->content->length_after_trim(), s);
	return max (ContentTime (), ContentTime (s, piece->frc) + piece->content->trim_start());
}

DCPTime
Player::content_time_to_dcp (shared_ptr<const Piece> piece, ContentTime t) const
{
	return max (DCPTime (), DCPTime (t - piece->content->trim_start(), piece->frc) + piece->content->position());
}

list<shared_ptr<Font> >
Player::get_subtitle_fonts ()
{
	if (!_have_valid_pieces) {
		setup_pieces ();
	}

	list<shared_ptr<Font> > fonts;
	BOOST_FOREACH (shared_ptr<Piece>& p, _pieces) {
		if (p->content->subtitle) {
			/* XXX: things may go wrong if there are duplicate font IDs
			   with different font files.
			*/
			list<shared_ptr<Font> > f = p->content->subtitle->fonts ();
			copy (f.begin(), f.end(), back_inserter (fonts));
		}
	}

	return fonts;
}

/** Set this player never to produce any video data */
void
Player::set_ignore_video ()
{
	_ignore_video = true;
}

/** Set whether or not this player should always burn text subtitles into the image,
 *  regardless of the content settings.
 *  @param burn true to always burn subtitles, false to obey content settings.
 */
void
Player::set_always_burn_subtitles (bool burn)
{
	_always_burn_subtitles = burn;
}

void
Player::set_fast ()
{
	_fast = true;
	_have_valid_pieces = false;
}

void
Player::set_play_referenced ()
{
	_play_referenced = true;
	_have_valid_pieces = false;
}

list<ReferencedReelAsset>
Player::get_reel_assets ()
{
	list<ReferencedReelAsset> a;

	BOOST_FOREACH (shared_ptr<Content> i, _playlist->content ()) {
		shared_ptr<DCPContent> j = dynamic_pointer_cast<DCPContent> (i);
		if (!j) {
			continue;
		}

		scoped_ptr<DCPDecoder> decoder;
		try {
			decoder.reset (new DCPDecoder (j, _film->log()));
		} catch (...) {
			return a;
		}

		int64_t offset = 0;
		BOOST_FOREACH (shared_ptr<dcp::Reel> k, decoder->reels()) {

			DCPOMATIC_ASSERT (j->video_frame_rate ());
			double const cfr = j->video_frame_rate().get();
			Frame const trim_start = j->trim_start().frames_round (cfr);
			Frame const trim_end = j->trim_end().frames_round (cfr);
			int const ffr = _film->video_frame_rate ();

			DCPTime const from = i->position() + DCPTime::from_frames (offset, _film->video_frame_rate());
			if (j->reference_video ()) {
				shared_ptr<dcp::ReelAsset> ra = k->main_picture ();
				DCPOMATIC_ASSERT (ra);
				ra->set_entry_point (ra->entry_point() + trim_start);
				ra->set_duration (ra->duration() - trim_start - trim_end);
				a.push_back (
					ReferencedReelAsset (ra, DCPTimePeriod (from, from + DCPTime::from_frames (ra->duration(), ffr)))
					);
			}

			if (j->reference_audio ()) {
				shared_ptr<dcp::ReelAsset> ra = k->main_sound ();
				DCPOMATIC_ASSERT (ra);
				ra->set_entry_point (ra->entry_point() + trim_start);
				ra->set_duration (ra->duration() - trim_start - trim_end);
				a.push_back (
					ReferencedReelAsset (ra, DCPTimePeriod (from, from + DCPTime::from_frames (ra->duration(), ffr)))
					);
			}

			if (j->reference_subtitle ()) {
				shared_ptr<dcp::ReelAsset> ra = k->main_subtitle ();
				DCPOMATIC_ASSERT (ra);
				ra->set_entry_point (ra->entry_point() + trim_start);
				ra->set_duration (ra->duration() - trim_start - trim_end);
				a.push_back (
					ReferencedReelAsset (ra, DCPTimePeriod (from, from + DCPTime::from_frames (ra->duration(), ffr)))
					);
			}

			/* Assume that main picture duration is the length of the reel */
			offset += k->main_picture()->duration ();
		}
	}

	return a;
}

bool
Player::pass ()
{
	if (!_have_valid_pieces) {
		setup_pieces ();
	}

	shared_ptr<Piece> earliest;
	DCPTime earliest_content;

	BOOST_FOREACH (shared_ptr<Piece> i, _pieces) {
		if (!i->done) {
			DCPTime const t = i->content->position() + DCPTime (i->decoder->position(), i->frc);
			if (!earliest || t < earliest_content) {
				earliest_content = t;
				earliest = i;
			}
		}
	}

	if (earliest) {
		earliest->done = earliest->decoder->pass ();
		if (earliest->done && earliest->content->audio) {
			/* Flush the Player audio system for this piece */
			BOOST_FOREACH (AudioStreamPtr i, earliest->content->audio->streams()) {
				audio_flush (earliest, i);
			}
		}
	}

	if (_last_video_time) {
		fill_video (DCPTimePeriod (_last_video_time.get(), earliest ? earliest_content : _playlist->length()));
	} else if (_last_seek_time) {
		fill_video (DCPTimePeriod (_last_seek_time.get(), _last_seek_time.get() + one_video_frame ()));
	}

	/* XXX: fill audio */

	if (!earliest) {
		return true;
	}

	/* Emit any audio that is ready */

	DCPTime pull_from = _playlist->length ();
	for (map<AudioStreamPtr, StreamState>::const_iterator i = _stream_states.begin(); i != _stream_states.end(); ++i) {
		if (!i->second.piece->done && i->second.last_push_end < pull_from) {
			pull_from = i->second.last_push_end;
		}
	}

	list<pair<shared_ptr<AudioBuffers>, DCPTime> > audio = _audio_merger.pull (pull_from);
	for (list<pair<shared_ptr<AudioBuffers>, DCPTime> >::iterator i = audio.begin(); i != audio.end(); ++i) {
		if (_last_audio_time && i->second < _last_audio_time.get()) {
			/* There has been an accurate seek and we have received some audio before the seek time;
			   discard it.
			*/
			pair<shared_ptr<AudioBuffers>, DCPTime> cut = discard_audio (i->first, i->second, *_last_audio_time);
			if (!cut.first) {
				continue;
			}
			*i = cut;
		}

		if (_last_audio_time) {
			/* XXX: does this remain necessary? */
			fill_audio (DCPTimePeriod (_last_audio_time.get(), i->second));
		}

		Audio (i->first, i->second);
		_last_audio_time = i->second + DCPTime::from_frames(i->first->frames(), _film->audio_frame_rate());
	}

	return false;
}

optional<PositionImage>
Player::subtitles_for_frame (DCPTime time) const
{
	/* Get any subtitles */

	optional<PositionImage> subtitles;

	for (ActiveSubtitlesMap::const_iterator i = _active_subtitles.begin(); i != _active_subtitles.end(); ++i) {

		shared_ptr<Piece> sub_piece = i->first.lock ();
		if (!sub_piece) {
			continue;
		}

		if (!sub_piece->content->subtitle->use() || (!_always_burn_subtitles && !sub_piece->content->subtitle->burn())) {
			continue;
		}

		BOOST_FOREACH (ActiveSubtitles j, i->second) {

			if (j.from > time || (j.to && j.to.get() <= time)) {
				continue;
			}

			list<PositionImage> sub_images;

			/* Image subtitles */
			list<PositionImage> c = transform_image_subtitles (j.subs.image);
			copy (c.begin(), c.end(), back_inserter (sub_images));

			/* Text subtitles (rendered to an image) */
			if (!j.subs.text.empty ()) {
				list<PositionImage> s = render_subtitles (j.subs.text, j.subs.fonts, _video_container_size, time);
				copy (s.begin (), s.end (), back_inserter (sub_images));
			}

			if (!sub_images.empty ()) {
				subtitles = merge (sub_images);
			}
		}
	}

	return subtitles;
}

void
Player::video (weak_ptr<Piece> wp, ContentVideo video)
{
	shared_ptr<Piece> piece = wp.lock ();
	if (!piece) {
		return;
	}

	FrameRateChange frc(piece->content->active_video_frame_rate(), _film->video_frame_rate());
	if (frc.skip && (video.frame % 2) == 1) {
		return;
	}

	/* Time and period of the frame we will emit */
	DCPTime const time = content_video_to_dcp (piece, video.frame);
	DCPTimePeriod const period (time, time + one_video_frame());

	/* Discard if it's outside the content's period or if it's before the last accurate seek */
	if (time < piece->content->position() || time >= piece->content->end() || (_last_video_time && time < _last_video_time)) {
		return;
	}

	/* Fill gaps caused by (the hopefully rare event of) a decoder not emitting contiguous video */

	/* XXX: is this necessary? can it be done by the fill in pass? */
	if (_last_video_time) {
		fill_video (DCPTimePeriod (_last_video_time.get(), time));
	}

	_last_video.reset (
		new PlayerVideo (
			video.image,
			piece->content->video->crop (),
			piece->content->video->fade (video.frame),
			piece->content->video->scale().size (
				piece->content->video, _video_container_size, _film->frame_size ()
				),
			_video_container_size,
			video.eyes,
			video.part,
			piece->content->video->colour_conversion ()
			)
		);

	optional<PositionImage> subtitles = subtitles_for_frame (time);
	if (subtitles) {
		_last_video->set_subtitle (subtitles.get ());
	}

	Video (_last_video, time);

	_last_video_time = time + one_video_frame ();

	/* Clear any finished _active_subtitles */
	ActiveSubtitlesMap updated;
	for (ActiveSubtitlesMap::const_iterator i = _active_subtitles.begin(); i != _active_subtitles.end(); ++i) {
		list<ActiveSubtitles> as;
		BOOST_FOREACH (ActiveSubtitles j, i->second) {
			if (!j.to || j.to.get() >= time) {
				as.push_back (j);
			}
		}
		if (!as.empty ()) {
			updated[i->first] = as;
		}
	}
	_active_subtitles = updated;
}

void
Player::audio_flush (shared_ptr<Piece> piece, AudioStreamPtr stream)
{
	shared_ptr<AudioContent> content = piece->content->audio;
	DCPOMATIC_ASSERT (content);

	shared_ptr<Resampler> r = resampler (content, stream, false);
	if (!r) {
		return;
	}

	pair<shared_ptr<const AudioBuffers>, Frame> ro = r->flush ();
	if (ro.first->frames() == 0) {
		return;
	}

	ContentAudio content_audio;
	content_audio.audio = ro.first;
	content_audio.frame = ro.second;

	/* Compute time in the DCP */
	DCPTime time = resampled_audio_to_dcp (piece, content_audio.frame) + DCPTime::from_seconds (content->delay() / 1000.0);

	audio_transform (content, stream, content_audio, time);
}

/** Do our common processing on some audio */
void
Player::audio_transform (shared_ptr<AudioContent> content, AudioStreamPtr stream, ContentAudio content_audio, DCPTime time)
{
	DCPOMATIC_ASSERT (content_audio.audio->frames() > 0);

	/* Gain */

	if (content->gain() != 0) {
		shared_ptr<AudioBuffers> gain (new AudioBuffers (content_audio.audio));
		gain->apply_gain (content->gain ());
		content_audio.audio = gain;
	}

	/* Remap */

	shared_ptr<AudioBuffers> dcp_mapped (new AudioBuffers (_film->audio_channels(), content_audio.audio->frames()));
	dcp_mapped->make_silent ();

	AudioMapping map = stream->mapping ();
	for (int i = 0; i < map.input_channels(); ++i) {
		for (int j = 0; j < dcp_mapped->channels(); ++j) {
			if (map.get (i, static_cast<dcp::Channel> (j)) > 0) {
				dcp_mapped->accumulate_channel (
					content_audio.audio.get(),
					i,
					static_cast<dcp::Channel> (j),
					map.get (i, static_cast<dcp::Channel> (j))
					);
			}
		}
	}

	content_audio.audio = dcp_mapped;

	/* Process */

	if (_audio_processor) {
		content_audio.audio = _audio_processor->run (content_audio.audio, _film->audio_channels ());
	}

	/* Push */

	_audio_merger.push (content_audio.audio, time);
	DCPOMATIC_ASSERT (_stream_states.find (stream) != _stream_states.end ());
	_stream_states[stream].last_push_end = time + DCPTime::from_frames (content_audio.audio->frames(), _film->audio_frame_rate());
}

void
Player::audio (weak_ptr<Piece> wp, AudioStreamPtr stream, ContentAudio content_audio)
{
	DCPOMATIC_ASSERT (content_audio.audio->frames() > 0);

	shared_ptr<Piece> piece = wp.lock ();
	if (!piece) {
		return;
	}

	shared_ptr<AudioContent> content = piece->content->audio;
	DCPOMATIC_ASSERT (content);

	/* Resample */
	if (stream->frame_rate() != content->resampled_frame_rate()) {
		shared_ptr<Resampler> r = resampler (content, stream, true);
		pair<shared_ptr<const AudioBuffers>, Frame> ro = r->run (content_audio.audio, content_audio.frame);
		if (ro.first->frames() == 0) {
			return;
		}
		content_audio.audio = ro.first;
		content_audio.frame = ro.second;
	}

	/* Compute time in the DCP */
	DCPTime time = resampled_audio_to_dcp (piece, content_audio.frame) + DCPTime::from_seconds (content->delay() / 1000.0);
	/* And the end of this block in the DCP */
	DCPTime end = time + DCPTime::from_frames(content_audio.audio->frames(), content->resampled_frame_rate());

	/* Remove anything that comes before the start or after the end of the content */
	if (time < piece->content->position()) {
		pair<shared_ptr<AudioBuffers>, DCPTime> cut = discard_audio (content_audio.audio, time, piece->content->position());
		if (!cut.first) {
			/* This audio is entirely discarded */
			return;
		}
		content_audio.audio = cut.first;
		time = cut.second;
	} else if (time > piece->content->end()) {
		/* Discard it all */
		return;
	} else if (end > piece->content->end()) {
		Frame const remaining_frames = DCPTime(piece->content->end() - time).frames_round(_film->audio_frame_rate());
		DCPOMATIC_ASSERT (remaining_frames > 0);
		shared_ptr<AudioBuffers> cut (new AudioBuffers (content_audio.audio->channels(), remaining_frames));
		cut->copy_from (content_audio.audio.get(), remaining_frames, 0, 0);
		content_audio.audio = cut;
	}

	audio_transform (content, stream, content_audio, time);
}

void
Player::image_subtitle_start (weak_ptr<Piece> wp, ContentImageSubtitle subtitle)
{
	shared_ptr<Piece> piece = wp.lock ();
	if (!piece) {
		return;
	}

	/* Apply content's subtitle offsets */
	subtitle.sub.rectangle.x += piece->content->subtitle->x_offset ();
	subtitle.sub.rectangle.y += piece->content->subtitle->y_offset ();

	/* Apply content's subtitle scale */
	subtitle.sub.rectangle.width *= piece->content->subtitle->x_scale ();
	subtitle.sub.rectangle.height *= piece->content->subtitle->y_scale ();

	/* Apply a corrective translation to keep the subtitle centred after that scale */
	subtitle.sub.rectangle.x -= subtitle.sub.rectangle.width * (piece->content->subtitle->x_scale() - 1);
	subtitle.sub.rectangle.y -= subtitle.sub.rectangle.height * (piece->content->subtitle->y_scale() - 1);

	PlayerSubtitles ps;
	ps.image.push_back (subtitle.sub);
	DCPTime from (content_time_to_dcp (piece, subtitle.from()));

	if (_active_subtitles.find(wp) == _active_subtitles.end()) {
		_active_subtitles[wp] = list<ActiveSubtitles>();
	}
	_active_subtitles[wp].push_back (ActiveSubtitles (ps, from));
}

void
Player::text_subtitle_start (weak_ptr<Piece> wp, ContentTextSubtitle subtitle)
{
	shared_ptr<Piece> piece = wp.lock ();
	if (!piece) {
		return;
	}

	PlayerSubtitles ps;
	DCPTime const from (content_time_to_dcp (piece, subtitle.from()));

	BOOST_FOREACH (dcp::SubtitleString s, subtitle.subs) {
		s.set_h_position (s.h_position() + piece->content->subtitle->x_offset ());
		s.set_v_position (s.v_position() + piece->content->subtitle->y_offset ());
		float const xs = piece->content->subtitle->x_scale();
		float const ys = piece->content->subtitle->y_scale();
		float size = s.size();

		/* Adjust size to express the common part of the scaling;
		   e.g. if xs = ys = 0.5 we scale size by 2.
		*/
		if (xs > 1e-5 && ys > 1e-5) {
			size *= 1 / min (1 / xs, 1 / ys);
		}
		s.set_size (size);

		/* Then express aspect ratio changes */
		if (fabs (1.0 - xs / ys) > dcp::ASPECT_ADJUST_EPSILON) {
			s.set_aspect_adjust (xs / ys);
		}

		s.set_in (dcp::Time(from.seconds(), 1000));
		ps.text.push_back (SubtitleString (s, piece->content->subtitle->outline_width()));
		ps.add_fonts (piece->content->subtitle->fonts ());
	}

	if (_active_subtitles.find(wp) == _active_subtitles.end()) {
		_active_subtitles[wp] = list<ActiveSubtitles> ();
	}
	_active_subtitles[wp].push_back (ActiveSubtitles (ps, from));
}

void
Player::subtitle_stop (weak_ptr<Piece> wp, ContentTime to)
{
	if (_active_subtitles.find (wp) == _active_subtitles.end ()) {
		return;
	}

	shared_ptr<Piece> piece = wp.lock ();
	if (!piece) {
		return;
	}

	DCPTime const dcp_to = content_time_to_dcp (piece, to);

	if (piece->content->subtitle->use() && !_always_burn_subtitles && !piece->content->subtitle->burn()) {
		Subtitle (_active_subtitles[wp].back().subs, DCPTimePeriod (_active_subtitles[wp].back().from, dcp_to));
	}

	_active_subtitles[wp].back().to = dcp_to;

	BOOST_FOREACH (SubtitleString& i, _active_subtitles[wp].back().subs.text) {
		i.set_out (dcp::Time(dcp_to.seconds(), 1000));
	}
}

void
Player::seek (DCPTime time, bool accurate)
{
	if (_audio_processor) {
		_audio_processor->flush ();
	}

	for (ResamplerMap::iterator i = _resamplers.begin(); i != _resamplers.end(); ++i) {
		i->second->flush ();
		i->second->reset ();
	}

	_audio_merger.clear ();
	_active_subtitles.clear ();

	BOOST_FOREACH (shared_ptr<Piece> i, _pieces) {
		if (time < i->content->position()) {
			/* Before; seek to 0 */
			i->decoder->seek (ContentTime(), accurate);
			i->done = false;
		} else if (i->content->position() <= time && time < i->content->end()) {
			/* During; seek to position */
			i->decoder->seek (dcp_to_content_time (i, time), accurate);
			i->done = false;
		} else {
			/* After; this piece is done */
			i->done = true;
		}
	}

	if (accurate) {
		_last_video_time = time;
		_last_audio_time = time;
	} else {
		_last_video_time = optional<DCPTime> ();
		_last_audio_time = optional<DCPTime> ();
	}

	_last_seek_time = time;
}

shared_ptr<Resampler>
Player::resampler (shared_ptr<const AudioContent> content, AudioStreamPtr stream, bool create)
{
	ResamplerMap::const_iterator i = _resamplers.find (make_pair (content, stream));
	if (i != _resamplers.end ()) {
		return i->second;
	}

	if (!create) {
		return shared_ptr<Resampler> ();
	}

	LOG_GENERAL (
		"Creating new resampler from %1 to %2 with %3 channels",
		stream->frame_rate(),
		content->resampled_frame_rate(),
		stream->channels()
		);

	shared_ptr<Resampler> r (
		new Resampler (stream->frame_rate(), content->resampled_frame_rate(), stream->channels())
		);

	_resamplers[make_pair(content, stream)] = r;
	return r;
}

void
Player::fill_video (DCPTimePeriod period)
{
	/* XXX: this may not work for 3D */
	BOOST_FOREACH (DCPTimePeriod i, subtract(period, _no_video)) {
		for (DCPTime j = i.from; j < i.to; j += one_video_frame()) {
			if (_playlist->video_content_at(j) && _last_video) {
				Video (shared_ptr<PlayerVideo> (new PlayerVideo (*_last_video)), j);
			} else {
				shared_ptr<PlayerVideo> black = black_player_video_frame ();
				optional<PositionImage> subtitles = subtitles_for_frame (j);
				if (subtitles) {
					black->set_subtitle (subtitles.get ());
				}
				Video (black, j);
			}
			_last_video_time = j;
		}
	}
}

void
Player::fill_audio (DCPTimePeriod period)
{
	BOOST_FOREACH (DCPTimePeriod i, subtract(period, _no_audio)) {
		DCPTime t = i.from;
		while (t < i.to) {
			DCPTime block = min (DCPTime::from_seconds (0.5), i.to - t);
			Frame const samples = block.frames_round(_film->audio_frame_rate());
			if (samples) {
				shared_ptr<AudioBuffers> silence (new AudioBuffers (_film->audio_channels(), samples));
				silence->make_silent ();
				Audio (silence, t);
			}
			t += block;
		}
	}
}

DCPTime
Player::one_video_frame () const
{
	return DCPTime::from_frames (1, _film->video_frame_rate ());
}

pair<shared_ptr<AudioBuffers>, DCPTime>
Player::discard_audio (shared_ptr<const AudioBuffers> audio, DCPTime time, DCPTime discard_to) const
{
	DCPTime const discard_time = discard_to - time;
	Frame const discard_frames = discard_time.frames_round(_film->audio_frame_rate());
	Frame remaining_frames = audio->frames() - discard_frames;
	if (remaining_frames <= 0) {
		return make_pair(shared_ptr<AudioBuffers>(), DCPTime());
	}
	shared_ptr<AudioBuffers> cut (new AudioBuffers (audio->channels(), remaining_frames));
	cut->copy_from (audio.get(), remaining_frames, discard_frames, 0);
	return make_pair(cut, time + discard_time);
}
