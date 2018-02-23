/*
    Copyright (C) 2013-2018 Carl Hetherington <cth@carlh.net>

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
#include "compose.hpp"
#include "shuffler.h"
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
	, _ignore_subtitle (false)
	, _always_burn_subtitles (false)
	, _fast (false)
	, _play_referenced (false)
	, _audio_merger (_film->audio_frame_rate())
	, _shuffler (0)
{
	_film_changed_connection = _film->Changed.connect (bind (&Player::film_changed, this, _1));
	_playlist_changed_connection = _playlist->Changed.connect (bind (&Player::playlist_changed, this));
	_playlist_content_changed_connection = _playlist->ContentChanged.connect (bind (&Player::playlist_content_changed, this, _1, _2, _3));
	set_video_container_size (_film->frame_size ());

	film_changed (Film::AUDIO_PROCESSOR);

	seek (DCPTime (), true);
}

Player::~Player ()
{
	delete _shuffler;
}

void
Player::setup_pieces ()
{
	_pieces.clear ();

	delete _shuffler;
	_shuffler = new Shuffler();
	_shuffler->Video.connect(bind(&Player::video, this, _1, _2));

	BOOST_FOREACH (shared_ptr<Content> i, _playlist->content ()) {

		if (!i->paths_valid ()) {
			continue;
		}

		shared_ptr<Decoder> decoder = decoder_factory (i, _film->log(), _fast);
		FrameRateChange frc (i->active_video_frame_rate(), _film->video_frame_rate());

		if (!decoder) {
			/* Not something that we can decode; e.g. Atmos content */
			continue;
		}

		if (decoder->video && _ignore_video) {
			decoder->video->set_ignore (true);
		}

		if (decoder->subtitle && _ignore_subtitle) {
			decoder->subtitle->set_ignore (true);
		}

		shared_ptr<DCPDecoder> dcp = dynamic_pointer_cast<DCPDecoder> (decoder);
		if (dcp) {
			dcp->set_decode_referenced (_play_referenced);
			if (_play_referenced) {
				dcp->set_forced_reduction (_dcp_decode_reduction);
			}
		}

		shared_ptr<Piece> piece (new Piece (i, decoder, frc));
		_pieces.push_back (piece);

		if (decoder->video) {
			if (i->video->frame_type() == VIDEO_FRAME_TYPE_3D_LEFT || i->video->frame_type() == VIDEO_FRAME_TYPE_3D_RIGHT) {
				/* We need a Shuffler to cope with 3D L/R video data arriving out of sequence */
				decoder->video->Data.connect (bind (&Shuffler::video, _shuffler, weak_ptr<Piece>(piece), _1));
			} else {
				decoder->video->Data.connect (bind (&Player::video, this, weak_ptr<Piece>(piece), _1));
			}
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

	_stream_states.clear ();
	BOOST_FOREACH (shared_ptr<Piece> i, _pieces) {
		if (i->content->audio) {
			BOOST_FOREACH (AudioStreamPtr j, i->content->audio->streams()) {
				_stream_states[j] = StreamState (i, i->content->position ());
			}
		}
	}

	_black = Empty (_film->content(), _film->length(), bind(&Content::video, _1));
	_silent = Empty (_film->content(), _film->length(), bind(&Content::audio, _1));

	_last_video_time = DCPTime ();
	_last_video_eyes = EYES_BOTH;
	_last_audio_time = DCPTime ();
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
		property == SubtitleContentProperty::EFFECT ||
		property == SubtitleContentProperty::EFFECT_COLOUR ||
		property == FFmpegContentProperty::SUBTITLE_STREAM ||
		property == FFmpegContentProperty::FILTERS ||
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
Player::black_player_video_frame (Eyes eyes) const
{
	return shared_ptr<PlayerVideo> (
		new PlayerVideo (
			shared_ptr<const ImageProxy> (new RawImageProxy (_black_image)),
			Crop (),
			optional<double> (),
			_video_container_size,
			_video_container_size,
			eyes,
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
	DCPTime const d = DCPTime::from_frames (f * piece->frc.factor(), piece->frc.dcp) - DCPTime(piece->content->trim_start(), piece->frc);
	return d + piece->content->position();
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
	return DCPTime::from_frames (f, _film->audio_frame_rate())
		- DCPTime (piece->content->trim_start(), piece->frc)
		+ piece->content->position();
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

void
Player::set_ignore_subtitle ()
{
	_ignore_subtitle = true;
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

/** Sets up the player to be faster, possibly at the expense of quality */
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
			decoder.reset (new DCPDecoder (j, _film->log(), false));
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

	if (_playlist->length() == DCPTime()) {
		/* Special case of an empty Film; just give one black frame */
		emit_video (black_player_video_frame(EYES_BOTH), DCPTime());
		return true;
	}

	/* Find the decoder or empty which is farthest behind where we are and make it emit some data */

	shared_ptr<Piece> earliest_content;
	optional<DCPTime> earliest_time;

	BOOST_FOREACH (shared_ptr<Piece> i, _pieces) {
		if (i->done) {
			continue;
		}

		DCPTime const t = content_time_to_dcp (i, i->decoder->position());
		if (t > i->content->end()) {
			i->done = true;
		} else {

			/* Given two choices at the same time, pick the one with a subtitle so we see it before
			   the video.
			*/
			if (!earliest_time || t < *earliest_time || (t == *earliest_time && i->decoder->subtitle)) {
				earliest_time = t;
				earliest_content = i;
			}
		}
	}

	bool done = false;

	enum {
		NONE,
		CONTENT,
		BLACK,
		SILENT
	} which = NONE;

	if (earliest_content) {
		which = CONTENT;
	}

	if (!_black.done() && (!earliest_time || _black.position() < *earliest_time)) {
		earliest_time = _black.position ();
		which = BLACK;
	}

	if (!_silent.done() && (!earliest_time || _silent.position() < *earliest_time)) {
		earliest_time = _silent.position ();
		which = SILENT;
	}

	switch (which) {
	case CONTENT:
		earliest_content->done = earliest_content->decoder->pass ();
		break;
	case BLACK:
		emit_video (black_player_video_frame(EYES_BOTH), _black.position());
		_black.set_position (_black.position() + one_video_frame());
		break;
	case SILENT:
	{
		DCPTimePeriod period (_silent.period_at_position());
		if (_last_audio_time) {
			/* Sometimes the thing that happened last finishes fractionally before
			   this silence.  Bodge the start time of the silence to fix it.  I'm
			   not sure if this is the right solution --- maybe the last thing should
			   be padded `forward' rather than this thing padding `back'.
			*/
			period.from = min(period.from, *_last_audio_time);
		}
		if (period.duration() > one_video_frame()) {
			period.to = period.from + one_video_frame();
		}
		fill_audio (period);
		_silent.set_position (period.to);
		break;
	}
	case NONE:
		done = true;
		break;
	}

	/* Emit any audio that is ready */

	/* Work out the time before which the audio is definitely all here.  This is the earliest last_push_end of one
	   of our streams, or the position of the _silent.
	*/
	DCPTime pull_to = _film->length ();
	for (map<AudioStreamPtr, StreamState>::const_iterator i = _stream_states.begin(); i != _stream_states.end(); ++i) {
		if (!i->second.piece->done && i->second.last_push_end < pull_to) {
			pull_to = i->second.last_push_end;
		}
	}
	if (!_silent.done() && _silent.position() < pull_to) {
		pull_to = _silent.position();
	}

	list<pair<shared_ptr<AudioBuffers>, DCPTime> > audio = _audio_merger.pull (pull_to);
	for (list<pair<shared_ptr<AudioBuffers>, DCPTime> >::iterator i = audio.begin(); i != audio.end(); ++i) {
		if (_last_audio_time && i->second < *_last_audio_time) {
			/* This new data comes before the last we emitted (or the last seek); discard it */
			pair<shared_ptr<AudioBuffers>, DCPTime> cut = discard_audio (i->first, i->second, *_last_audio_time);
			if (!cut.first) {
				continue;
			}
			*i = cut;
		} else if (_last_audio_time && i->second > *_last_audio_time) {
			/* There's a gap between this data and the last we emitted; fill with silence */
			fill_audio (DCPTimePeriod (*_last_audio_time, i->second));
		}

		emit_audio (i->first, i->second);
	}

	if (done) {
		_shuffler->flush ();
		for (list<pair<shared_ptr<PlayerVideo>, DCPTime> >::const_iterator i = _delay.begin(); i != _delay.end(); ++i) {
			do_emit_video(i->first, i->second);
		}
	}

	return done;
}

optional<PositionImage>
Player::subtitles_for_frame (DCPTime time) const
{
	list<PositionImage> subtitles;

	int const vfr = _film->video_frame_rate();

	BOOST_FOREACH (PlayerSubtitles i, _active_subtitles.get_burnt (DCPTimePeriod(time, time + DCPTime::from_frames(1, vfr)), _always_burn_subtitles)) {

		/* Image subtitles */
		list<PositionImage> c = transform_image_subtitles (i.image);
		copy (c.begin(), c.end(), back_inserter (subtitles));

		/* Text subtitles (rendered to an image) */
		if (!i.text.empty ()) {
			list<PositionImage> s = render_subtitles (i.text, i.fonts, _video_container_size, time, vfr);
			copy (s.begin(), s.end(), back_inserter (subtitles));
		}
	}

	if (subtitles.empty ()) {
		return optional<PositionImage> ();
	}

	return merge (subtitles);
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

	/* Time of the first frame we will emit */
	DCPTime const time = content_video_to_dcp (piece, video.frame);

	/* Discard if it's before the content's period or the last accurate seek.  We can't discard
	   if it's after the content's period here as in that case we still need to fill any gap between
	   `now' and the end of the content's period.
	*/
	if (time < piece->content->position() || (_last_video_time && time < *_last_video_time)) {
		return;
	}

	/* Fill gaps that we discover now that we have some video which needs to be emitted.
	   This is where we need to fill to.
	*/
	DCPTime fill_to = min (time, piece->content->end());

	if (_last_video_time) {
		DCPTime fill_from = max (*_last_video_time, piece->content->position());
		LastVideoMap::const_iterator last = _last_video.find (wp);
		if (_film->three_d()) {
			DCPTime j = fill_from;
			Eyes eyes = _last_video_eyes.get_value_or(EYES_LEFT);
			if (eyes == EYES_BOTH) {
				eyes = EYES_LEFT;
			}
			while (j < fill_to || eyes != video.eyes) {
				if (last != _last_video.end()) {
					shared_ptr<PlayerVideo> copy = last->second->shallow_copy();
					copy->set_eyes (eyes);
					emit_video (copy, j);
				} else {
					emit_video (black_player_video_frame(eyes), j);
				}
				if (eyes == EYES_RIGHT) {
					j += one_video_frame();
				}
				eyes = increment_eyes (eyes);
			}
		} else {
			for (DCPTime j = fill_from; j < fill_to; j += one_video_frame()) {
				if (last != _last_video.end()) {
					emit_video (last->second, j);
				} else {
					emit_video (black_player_video_frame(EYES_BOTH), j);
				}
			}
		}
	}

	_last_video[wp].reset (
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

	DCPTime t = time;
	for (int i = 0; i < frc.repeat; ++i) {
		if (t < piece->content->end()) {
			emit_video (_last_video[wp], t);
		}
		t += one_video_frame ();
	}
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

	/* Compute time in the DCP */
	DCPTime time = resampled_audio_to_dcp (piece, content_audio.frame);
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
		if (remaining_frames == 0) {
			return;
		}
		shared_ptr<AudioBuffers> cut (new AudioBuffers (content_audio.audio->channels(), remaining_frames));
		cut->copy_from (content_audio.audio.get(), remaining_frames, 0, 0);
		content_audio.audio = cut;
	}

	DCPOMATIC_ASSERT (content_audio.audio->frames() > 0);

	/* Gain */

	if (content->gain() != 0) {
		shared_ptr<AudioBuffers> gain (new AudioBuffers (content_audio.audio));
		gain->apply_gain (content->gain ());
		content_audio.audio = gain;
	}

	/* Remap */

	content_audio.audio = remap (content_audio.audio, _film->audio_channels(), stream->mapping());

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

	_active_subtitles.add_from (wp, ps, from);
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

	_active_subtitles.add_from (wp, ps, from);
}

void
Player::subtitle_stop (weak_ptr<Piece> wp, ContentTime to)
{
	if (!_active_subtitles.have (wp)) {
		return;
	}

	shared_ptr<Piece> piece = wp.lock ();
	if (!piece) {
		return;
	}

	DCPTime const dcp_to = content_time_to_dcp (piece, to);

	pair<PlayerSubtitles, DCPTime> from = _active_subtitles.add_to (wp, dcp_to);

	if (piece->content->subtitle->use() && !_always_burn_subtitles && !piece->content->subtitle->burn()) {
		Subtitle (from.first, DCPTimePeriod (from.second, dcp_to));
	}
}

void
Player::seek (DCPTime time, bool accurate)
{
	if (!_have_valid_pieces) {
		setup_pieces ();
	}

	if (_shuffler) {
		_shuffler->clear ();
	}

	_delay.clear ();

	if (_audio_processor) {
		_audio_processor->flush ();
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
		_last_video_eyes = EYES_LEFT;
		_last_audio_time = time;
	} else {
		_last_video_time = optional<DCPTime>();
		_last_video_eyes = optional<Eyes>();
		_last_audio_time = optional<DCPTime>();
	}

	_black.set_position (time);
	_silent.set_position (time);

	_last_video.clear ();
}

void
Player::emit_video (shared_ptr<PlayerVideo> pv, DCPTime time)
{
	/* We need a delay to give a little wiggle room to ensure that relevent subtitles arrive at the
	   player before the video that requires them.
	*/
	_delay.push_back (make_pair (pv, time));

	if (pv->eyes() == EYES_BOTH || pv->eyes() == EYES_RIGHT) {
		_last_video_time = time + one_video_frame();
	}
	_last_video_eyes = increment_eyes (pv->eyes());

	if (_delay.size() < 3) {
		return;
	}

	pair<shared_ptr<PlayerVideo>, DCPTime> to_do = _delay.front();
	_delay.pop_front();
	do_emit_video (to_do.first, to_do.second);
}

void
Player::do_emit_video (shared_ptr<PlayerVideo> pv, DCPTime time)
{
	if (pv->eyes() == EYES_BOTH || pv->eyes() == EYES_RIGHT) {
		_active_subtitles.clear_before (time);
	}

	optional<PositionImage> subtitles = subtitles_for_frame (time);
	if (subtitles) {
		pv->set_subtitle (subtitles.get ());
	}

	Video (pv, time);
}

void
Player::emit_audio (shared_ptr<AudioBuffers> data, DCPTime time)
{
	/* Log if the assert below is about to fail */
	if (_last_audio_time && time != *_last_audio_time) {
		_film->log()->log(String::compose("Out-of-sequence emit %1 vs %2", to_string(time), to_string(*_last_audio_time)), LogEntry::TYPE_WARNING);
	}

	/* This audio must follow on from the previous */
	DCPOMATIC_ASSERT (!_last_audio_time || time == *_last_audio_time);
	Audio (data, time);
	_last_audio_time = time + DCPTime::from_frames (data->frames(), _film->audio_frame_rate());
}

void
Player::fill_audio (DCPTimePeriod period)
{
	if (period.from == period.to) {
		return;
	}

	DCPOMATIC_ASSERT (period.from < period.to);

	DCPTime t = period.from;
	while (t < period.to) {
		DCPTime block = min (DCPTime::from_seconds (0.5), period.to - t);
		Frame const samples = block.frames_round(_film->audio_frame_rate());
		if (samples) {
			shared_ptr<AudioBuffers> silence (new AudioBuffers (_film->audio_channels(), samples));
			silence->make_silent ();
			emit_audio (silence, t);
		}
		t += block;
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

void
Player::set_dcp_decode_reduction (optional<int> reduction)
{
	if (reduction == _dcp_decode_reduction) {
		return;
	}

	_dcp_decode_reduction = reduction;
	_have_valid_pieces = false;
	Changed (false);
}
