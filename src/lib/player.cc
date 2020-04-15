/*
    Copyright (C) 2013-2020 Carl Hetherington <cth@carlh.net>

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
#include "render_text.h"
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
#include "text_content.h"
#include "text_decoder.h"
#include "ffmpeg_content.h"
#include "audio_content.h"
#include "dcp_decoder.h"
#include "image_decoder.h"
#include "compose.hpp"
#include "shuffler.h"
#include "timer.h"
#include <dcp/reel.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/reel_subtitle_asset.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_closed_caption_asset.h>
#include <boost/foreach.hpp>
#include <stdint.h>
#include <algorithm>
#include <iostream>

#include "i18n.h"

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
using namespace dcpomatic;

int const PlayerProperty::VIDEO_CONTAINER_SIZE = 700;
int const PlayerProperty::PLAYLIST = 701;
int const PlayerProperty::FILM_CONTAINER = 702;
int const PlayerProperty::FILM_VIDEO_FRAME_RATE = 703;
int const PlayerProperty::DCP_DECODE_REDUCTION = 704;

Player::Player (shared_ptr<const Film> film, shared_ptr<const Playlist> playlist, DCPTime playback_length)
	: _film (film)
	, _playlist (playlist)
	, _suspended (0)
	, _ignore_video (false)
	, _ignore_audio (false)
	, _ignore_text (false)
	, _always_burn_open_subtitles (false)
	, _fast (false)
	, _tolerant (film->tolerant())
	, _play_referenced (false)
	, _audio_merger (_film->audio_frame_rate())
	, _shuffler (0)
	, _playback_length (playback_length)
{
	_film_changed_connection = _film->Change.connect (bind (&Player::film_change, this, _1, _2));
	/* The butler must hear about this first, so since we are proxying this through to the butler we must
	   be first.
	*/
	_playlist_change_connection = _playlist->Change.connect (bind (&Player::playlist_change, this, _1), boost::signals2::at_front);
	_playlist_content_change_connection = _playlist->ContentChange.connect (bind(&Player::playlist_content_change, this, _1, _3, _4));
	set_video_container_size (_film->frame_size ());

	film_change (CHANGE_TYPE_DONE, Film::AUDIO_PROCESSOR);

	setup_pieces ();
	seek (DCPTime (), true);
}

Player::~Player ()
{
	delete _shuffler;
}

void
Player::setup_pieces ()
{
	boost::mutex::scoped_lock lm (_mutex);
	setup_pieces_unlocked ();
}

bool
have_video (shared_ptr<const Content> content)
{
	return static_cast<bool>(content->video);
}

bool
have_audio (shared_ptr<const Content> content)
{
	return static_cast<bool>(content->audio);
}

void
Player::setup_pieces_unlocked ()
{
	list<shared_ptr<Piece> > old_pieces = _pieces;
	_pieces.clear ();

	delete _shuffler;
	_shuffler = new Shuffler();
	_shuffler->Video.connect(bind(&Player::video, this, _1, _2));

	BOOST_FOREACH (shared_ptr<Content> i, _playlist->content ()) {

		if (!i->paths_valid ()) {
			continue;
		}

		if (_ignore_video && _ignore_audio && i->text.empty()) {
			/* We're only interested in text and this content has none */
			continue;
		}

		shared_ptr<Decoder> old_decoder;
		BOOST_FOREACH (shared_ptr<Piece> j, old_pieces) {
			if (j->content == i) {
				old_decoder = j->decoder;
				break;
			}
		}

		shared_ptr<Decoder> decoder = decoder_factory (_film, i, _fast, _tolerant, old_decoder);
		FrameRateChange frc (_film, i);

		if (!decoder) {
			/* Not something that we can decode; e.g. Atmos content */
			continue;
		}

		if (decoder->video && _ignore_video) {
			decoder->video->set_ignore (true);
		}

		if (decoder->audio && _ignore_audio) {
			decoder->audio->set_ignore (true);
		}

		if (_ignore_text) {
			BOOST_FOREACH (shared_ptr<TextDecoder> i, decoder->text) {
				i->set_ignore (true);
			}
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

		list<shared_ptr<TextDecoder> >::const_iterator j = decoder->text.begin();

		while (j != decoder->text.end()) {
			(*j)->BitmapStart.connect (
				bind(&Player::bitmap_text_start, this, weak_ptr<Piece>(piece), weak_ptr<const TextContent>((*j)->content()), _1)
				);
			(*j)->PlainStart.connect (
				bind(&Player::plain_text_start, this, weak_ptr<Piece>(piece), weak_ptr<const TextContent>((*j)->content()), _1)
				);
			(*j)->Stop.connect (
				bind(&Player::subtitle_stop, this, weak_ptr<Piece>(piece), weak_ptr<const TextContent>((*j)->content()), _1)
				);

			++j;
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

	_black = Empty (_film, _playlist, bind(&have_video, _1), _playback_length);
	_silent = Empty (_film, _playlist, bind(&have_audio, _1), _playback_length);

	_last_video_time = DCPTime ();
	_last_video_eyes = EYES_BOTH;
	_last_audio_time = DCPTime ();
}

void
Player::playlist_content_change (ChangeType type, int property, bool frequent)
{
	if (type == CHANGE_TYPE_PENDING) {
		/* The player content is probably about to change, so we can't carry on
		   until that has happened and we've rebuilt our pieces.  Stop pass()
		   and seek() from working until then.
		*/
		++_suspended;
	} else if (type == CHANGE_TYPE_DONE) {
		/* A change in our content has gone through.  Re-build our pieces. */
		setup_pieces ();
		--_suspended;
	} else if (type == CHANGE_TYPE_CANCELLED) {
		--_suspended;
	}

	Change (type, property, frequent);
}

void
Player::set_video_container_size (dcp::Size s)
{
	Change (CHANGE_TYPE_PENDING, PlayerProperty::VIDEO_CONTAINER_SIZE, false);

	{
		boost::mutex::scoped_lock lm (_mutex);

		if (s == _video_container_size) {
			lm.unlock ();
			Change (CHANGE_TYPE_CANCELLED, PlayerProperty::VIDEO_CONTAINER_SIZE, false);
			return;
		}

		_video_container_size = s;

		_black_image.reset (new Image (AV_PIX_FMT_RGB24, _video_container_size, true));
		_black_image->make_black ();
	}

	Change (CHANGE_TYPE_DONE, PlayerProperty::VIDEO_CONTAINER_SIZE, false);
}

void
Player::playlist_change (ChangeType type)
{
	if (type == CHANGE_TYPE_DONE) {
		setup_pieces ();
	}
	Change (type, PlayerProperty::PLAYLIST, false);
}

void
Player::film_change (ChangeType type, Film::Property p)
{
	/* Here we should notice Film properties that affect our output, and
	   alert listeners that our output now would be different to how it was
	   last time we were run.
	*/

	if (p == Film::CONTAINER) {
		Change (type, PlayerProperty::FILM_CONTAINER, false);
	} else if (p == Film::VIDEO_FRAME_RATE) {
		/* Pieces contain a FrameRateChange which contains the DCP frame rate,
		   so we need new pieces here.
		*/
		if (type == CHANGE_TYPE_DONE) {
			setup_pieces ();
		}
		Change (type, PlayerProperty::FILM_VIDEO_FRAME_RATE, false);
	} else if (p == Film::AUDIO_PROCESSOR) {
		if (type == CHANGE_TYPE_DONE && _film->audio_processor ()) {
			boost::mutex::scoped_lock lm (_mutex);
			_audio_processor = _film->audio_processor()->clone (_film->audio_frame_rate ());
		}
	} else if (p == Film::AUDIO_CHANNELS) {
		if (type == CHANGE_TYPE_DONE) {
			boost::mutex::scoped_lock lm (_mutex);
			_audio_merger.clear ();
		}
	}
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
			PresetColourConversion::all().front().conversion,
			VIDEO_RANGE_FULL,
			boost::weak_ptr<Content>(),
			boost::optional<Frame>(),
			false
		)
	);
}

Frame
Player::dcp_to_content_video (shared_ptr<const Piece> piece, DCPTime t) const
{
	DCPTime s = t - piece->content->position ();
	s = min (piece->content->length_after_trim(_film), s);
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
	s = min (piece->content->length_after_trim(_film), s);
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
	s = min (piece->content->length_after_trim(_film), s);
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
	boost::mutex::scoped_lock lm (_mutex);

	list<shared_ptr<Font> > fonts;
	BOOST_FOREACH (shared_ptr<Piece> i, _pieces) {
		BOOST_FOREACH (shared_ptr<TextContent> j, i->content->text) {
			/* XXX: things may go wrong if there are duplicate font IDs
			   with different font files.
			*/
			list<shared_ptr<Font> > f = j->fonts ();
			copy (f.begin(), f.end(), back_inserter (fonts));
		}
	}

	return fonts;
}

/** Set this player never to produce any video data */
void
Player::set_ignore_video ()
{
	boost::mutex::scoped_lock lm (_mutex);
	_ignore_video = true;
	setup_pieces_unlocked ();
}

void
Player::set_ignore_audio ()
{
	boost::mutex::scoped_lock lm (_mutex);
	_ignore_audio = true;
	setup_pieces_unlocked ();
}

void
Player::set_ignore_text ()
{
	boost::mutex::scoped_lock lm (_mutex);
	_ignore_text = true;
	setup_pieces_unlocked ();
}

/** Set the player to always burn open texts into the image regardless of the content settings */
void
Player::set_always_burn_open_subtitles ()
{
	boost::mutex::scoped_lock lm (_mutex);
	_always_burn_open_subtitles = true;
}

/** Sets up the player to be faster, possibly at the expense of quality */
void
Player::set_fast ()
{
	boost::mutex::scoped_lock lm (_mutex);
	_fast = true;
	setup_pieces_unlocked ();
}

void
Player::set_play_referenced ()
{
	boost::mutex::scoped_lock lm (_mutex);
	_play_referenced = true;
	setup_pieces_unlocked ();
}

static void
maybe_add_asset (list<ReferencedReelAsset>& a, shared_ptr<dcp::ReelAsset> r, Frame reel_trim_start, Frame reel_trim_end, DCPTime from, int const ffr)
{
	DCPOMATIC_ASSERT (r);
	r->set_entry_point (r->entry_point().get_value_or(0) + reel_trim_start);
	r->set_duration (r->actual_duration() - reel_trim_start - reel_trim_end);
	if (r->actual_duration() > 0) {
		a.push_back (
			ReferencedReelAsset(r, DCPTimePeriod(from, from + DCPTime::from_frames(r->actual_duration(), ffr)))
			);
	}
}

list<ReferencedReelAsset>
Player::get_reel_assets ()
{
	/* Does not require a lock on _mutex as it's only called from DCPEncoder */

	list<ReferencedReelAsset> a;

	BOOST_FOREACH (shared_ptr<Content> i, _playlist->content ()) {
		shared_ptr<DCPContent> j = dynamic_pointer_cast<DCPContent> (i);
		if (!j) {
			continue;
		}

		scoped_ptr<DCPDecoder> decoder;
		try {
			decoder.reset (new DCPDecoder (_film, j, false, false, shared_ptr<DCPDecoder>()));
		} catch (...) {
			return a;
		}

		DCPOMATIC_ASSERT (j->video_frame_rate ());
		double const cfr = j->video_frame_rate().get();
		Frame const trim_start = j->trim_start().frames_round (cfr);
		Frame const trim_end = j->trim_end().frames_round (cfr);
		int const ffr = _film->video_frame_rate ();

		/* position in the asset from the start */
		int64_t offset_from_start = 0;
		/* position in the asset from the end */
		int64_t offset_from_end = 0;
		BOOST_FOREACH (shared_ptr<dcp::Reel> k, decoder->reels()) {
			/* Assume that main picture duration is the length of the reel */
			offset_from_end += k->main_picture()->actual_duration();
		}

		BOOST_FOREACH (shared_ptr<dcp::Reel> k, decoder->reels()) {

			/* Assume that main picture duration is the length of the reel */
			int64_t const reel_duration = k->main_picture()->actual_duration();

			/* See doc/design/trim_reels.svg */
			Frame const reel_trim_start = min(reel_duration, max(int64_t(0), trim_start - offset_from_start));
			Frame const reel_trim_end =   min(reel_duration, max(int64_t(0), reel_duration - (offset_from_end - trim_end)));

			DCPTime const from = i->position() + DCPTime::from_frames (offset_from_start, _film->video_frame_rate());
			if (j->reference_video ()) {
				maybe_add_asset (a, k->main_picture(), reel_trim_start, reel_trim_end, from, ffr);
			}

			if (j->reference_audio ()) {
				maybe_add_asset (a, k->main_sound(), reel_trim_start, reel_trim_end, from, ffr);
			}

			if (j->reference_text (TEXT_OPEN_SUBTITLE)) {
				maybe_add_asset (a, k->main_subtitle(), reel_trim_start, reel_trim_end, from, ffr);
			}

			if (j->reference_text (TEXT_CLOSED_CAPTION)) {
				BOOST_FOREACH (shared_ptr<dcp::ReelClosedCaptionAsset> l, k->closed_captions()) {
					maybe_add_asset (a, l, reel_trim_start, reel_trim_end, from, ffr);
				}
			}

			offset_from_start += reel_duration;
			offset_from_end -= reel_duration;
		}
	}

	return a;
}

bool
Player::pass ()
{
	boost::mutex::scoped_lock lm (_mutex);

	if (_suspended) {
		/* We can't pass in this state */
		return false;
	}

	if (_playback_length == DCPTime()) {
		/* Special; just give one black frame */
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

		DCPTime const t = content_time_to_dcp (i, max(i->decoder->position(), i->content->trim_start()));
		if (t > i->content->end(_film)) {
			i->done = true;
		} else {

			/* Given two choices at the same time, pick the one with texts so we see it before
			   the video.
			*/
			if (!earliest_time || t < *earliest_time || (t == *earliest_time && !i->decoder->text.empty())) {
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

	if (!_black.done() && !_ignore_video && (!earliest_time || _black.position() < *earliest_time)) {
		earliest_time = _black.position ();
		which = BLACK;
	}

	if (!_silent.done() && !_ignore_audio && (!earliest_time || _silent.position() < *earliest_time)) {
		earliest_time = _silent.position ();
		which = SILENT;
	}

	switch (which) {
	case CONTENT:
	{
		earliest_content->done = earliest_content->decoder->pass ();
		shared_ptr<DCPContent> dcp = dynamic_pointer_cast<DCPContent>(earliest_content->content);
		if (dcp && !_play_referenced && dcp->reference_audio()) {
			/* We are skipping some referenced DCP audio content, so we need to update _last_audio_time
			   to `hide' the fact that no audio was emitted during the referenced DCP (though
			   we need to behave as though it was).
			*/
			_last_audio_time = dcp->end (_film);
		}
		break;
	}
	case BLACK:
		emit_video (black_player_video_frame(EYES_BOTH), _black.position());
		_black.set_position (_black.position() + one_video_frame());
		break;
	case SILENT:
	{
		DCPTimePeriod period (_silent.period_at_position());
		if (_last_audio_time) {
			/* Sometimes the thing that happened last finishes fractionally before
			   or after this silence.  Bodge the start time of the silence to fix it.
			   I think this is nothing to worry about since we will just add or
			   remove a little silence at the end of some content.
			*/
			int64_t const error = labs(period.from.get() - _last_audio_time->get());
			/* Let's not worry about less than a frame at 24fps */
			int64_t const too_much_error = DCPTime::from_frames(1, 24).get();
			if (error >= too_much_error) {
				_film->log()->log(String::compose("Silence starting before or after last audio by %1", error), LogEntry::TYPE_ERROR);
			}
			DCPOMATIC_ASSERT (error < too_much_error);
			period.from = *_last_audio_time;
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
	DCPTime pull_to = _playback_length;
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

/** @return Open subtitles for the frame at the given time, converted to images */
optional<PositionImage>
Player::open_subtitles_for_frame (DCPTime time) const
{
	list<PositionImage> captions;
	int const vfr = _film->video_frame_rate();

	BOOST_FOREACH (
		PlayerText j,
		_active_texts[TEXT_OPEN_SUBTITLE].get_burnt(DCPTimePeriod(time, time + DCPTime::from_frames(1, vfr)), _always_burn_open_subtitles)
		) {

		/* Bitmap subtitles */
		BOOST_FOREACH (BitmapText i, j.bitmap) {
			if (!i.image) {
				continue;
			}

			/* i.image will already have been scaled to fit _video_container_size */
			dcp::Size scaled_size (i.rectangle.width * _video_container_size.width, i.rectangle.height * _video_container_size.height);

			captions.push_back (
				PositionImage (
					i.image,
					Position<int> (
						lrint (_video_container_size.width * i.rectangle.x),
						lrint (_video_container_size.height * i.rectangle.y)
						)
					)
				);
		}

		/* String subtitles (rendered to an image) */
		if (!j.string.empty ()) {
			list<PositionImage> s = render_text (j.string, j.fonts, _video_container_size, time, vfr);
			copy (s.begin(), s.end(), back_inserter (captions));
		}
	}

	if (captions.empty ()) {
		return optional<PositionImage> ();
	}

	return merge (captions);
}

void
Player::video (weak_ptr<Piece> wp, ContentVideo video)
{
	shared_ptr<Piece> piece = wp.lock ();
	if (!piece) {
		return;
	}

	FrameRateChange frc (_film, piece->content);
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
	DCPTime fill_to = min (time, piece->content->end(_film));

	if (_last_video_time) {
		DCPTime fill_from = max (*_last_video_time, piece->content->position());

		/* Fill if we have more than half a frame to do */
		if ((fill_to - fill_from) > one_video_frame() / 2) {
			LastVideoMap::const_iterator last = _last_video.find (wp);
			if (_film->three_d()) {
				Eyes fill_to_eyes = video.eyes;
				if (fill_to_eyes == EYES_BOTH) {
					fill_to_eyes = EYES_LEFT;
				}
				if (fill_to == piece->content->end(_film)) {
					/* Don't fill after the end of the content */
					fill_to_eyes = EYES_LEFT;
				}
				DCPTime j = fill_from;
				Eyes eyes = _last_video_eyes.get_value_or(EYES_LEFT);
				if (eyes == EYES_BOTH) {
					eyes = EYES_LEFT;
				}
				while (j < fill_to || eyes != fill_to_eyes) {
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
	}

	_last_video[wp].reset (
		new PlayerVideo (
			video.image,
			piece->content->video->crop (),
			piece->content->video->fade (_film, video.frame),
			piece->content->video->scale().size (
				piece->content->video, _video_container_size, _film->frame_size ()
				),
			_video_container_size,
			video.eyes,
			video.part,
			piece->content->video->colour_conversion(),
			piece->content->video->range(),
			piece->content,
			video.frame,
			false
			)
		);

	DCPTime t = time;
	for (int i = 0; i < frc.repeat; ++i) {
		if (t < piece->content->end(_film)) {
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

	int const rfr = content->resampled_frame_rate (_film);

	/* Compute time in the DCP */
	DCPTime time = resampled_audio_to_dcp (piece, content_audio.frame);
	/* And the end of this block in the DCP */
	DCPTime end = time + DCPTime::from_frames(content_audio.audio->frames(), rfr);

	/* Remove anything that comes before the start or after the end of the content */
	if (time < piece->content->position()) {
		pair<shared_ptr<AudioBuffers>, DCPTime> cut = discard_audio (content_audio.audio, time, piece->content->position());
		if (!cut.first) {
			/* This audio is entirely discarded */
			return;
		}
		content_audio.audio = cut.first;
		time = cut.second;
	} else if (time > piece->content->end(_film)) {
		/* Discard it all */
		return;
	} else if (end > piece->content->end(_film)) {
		Frame const remaining_frames = DCPTime(piece->content->end(_film) - time).frames_round(rfr);
		if (remaining_frames == 0) {
			return;
		}
		content_audio.audio.reset (new AudioBuffers(content_audio.audio, remaining_frames, 0));
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
Player::bitmap_text_start (weak_ptr<Piece> wp, weak_ptr<const TextContent> wc, ContentBitmapText subtitle)
{
	shared_ptr<Piece> piece = wp.lock ();
	shared_ptr<const TextContent> text = wc.lock ();
	if (!piece || !text) {
		return;
	}

	/* Apply content's subtitle offsets */
	subtitle.sub.rectangle.x += text->x_offset ();
	subtitle.sub.rectangle.y += text->y_offset ();

	/* Apply a corrective translation to keep the subtitle centred after the scale that is coming up */
	subtitle.sub.rectangle.x -= subtitle.sub.rectangle.width * ((text->x_scale() - 1) / 2);
	subtitle.sub.rectangle.y -= subtitle.sub.rectangle.height * ((text->y_scale() - 1) / 2);

	/* Apply content's subtitle scale */
	subtitle.sub.rectangle.width *= text->x_scale ();
	subtitle.sub.rectangle.height *= text->y_scale ();

	PlayerText ps;
	shared_ptr<Image> image = subtitle.sub.image;

	/* We will scale the subtitle up to fit _video_container_size */
	int const width = subtitle.sub.rectangle.width * _video_container_size.width;
	int const height = subtitle.sub.rectangle.height * _video_container_size.height;
	if (width == 0 || height == 0) {
		return;
	}

	dcp::Size scaled_size (width, height);
	ps.bitmap.push_back (BitmapText(image->scale(scaled_size, dcp::YUV_TO_RGB_REC601, image->pixel_format(), true, _fast), subtitle.sub.rectangle));
	DCPTime from (content_time_to_dcp (piece, subtitle.from()));

	_active_texts[text->type()].add_from (wc, ps, from);
}

void
Player::plain_text_start (weak_ptr<Piece> wp, weak_ptr<const TextContent> wc, ContentStringText subtitle)
{
	shared_ptr<Piece> piece = wp.lock ();
	shared_ptr<const TextContent> text = wc.lock ();
	if (!piece || !text) {
		return;
	}

	PlayerText ps;
	DCPTime const from (content_time_to_dcp (piece, subtitle.from()));

	if (from > piece->content->end(_film)) {
		return;
	}

	BOOST_FOREACH (dcp::SubtitleString s, subtitle.subs) {
		s.set_h_position (s.h_position() + text->x_offset ());
		s.set_v_position (s.v_position() + text->y_offset ());
		float const xs = text->x_scale();
		float const ys = text->y_scale();
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
		ps.string.push_back (StringText (s, text->outline_width()));
		ps.add_fonts (text->fonts ());
	}

	_active_texts[text->type()].add_from (wc, ps, from);
}

void
Player::subtitle_stop (weak_ptr<Piece> wp, weak_ptr<const TextContent> wc, ContentTime to)
{
	shared_ptr<const TextContent> text = wc.lock ();
	if (!text) {
		return;
	}

	if (!_active_texts[text->type()].have(wc)) {
		return;
	}

	shared_ptr<Piece> piece = wp.lock ();
	if (!piece) {
		return;
	}

	DCPTime const dcp_to = content_time_to_dcp (piece, to);

	if (dcp_to > piece->content->end(_film)) {
		return;
	}

	pair<PlayerText, DCPTime> from = _active_texts[text->type()].add_to (wc, dcp_to);

	bool const always = (text->type() == TEXT_OPEN_SUBTITLE && _always_burn_open_subtitles);
	if (text->use() && !always && !text->burn()) {
		Text (from.first, text->type(), text->dcp_track().get_value_or(DCPTextTrack()), DCPTimePeriod (from.second, dcp_to));
	}
}

void
Player::seek (DCPTime time, bool accurate)
{
	boost::mutex::scoped_lock lm (_mutex);

	if (_suspended) {
		/* We can't seek in this state */
		return;
	}

	if (_shuffler) {
		_shuffler->clear ();
	}

	_delay.clear ();

	if (_audio_processor) {
		_audio_processor->flush ();
	}

	_audio_merger.clear ();
	for (int i = 0; i < TEXT_COUNT; ++i) {
		_active_texts[i].clear ();
	}

	BOOST_FOREACH (shared_ptr<Piece> i, _pieces) {
		if (time < i->content->position()) {
			/* Before; seek to the start of the content.  Even if this request is for an inaccurate seek
			   we must seek this (following) content accurately, otherwise when we come to the end of the current
			   content we may not start right at the beginning of the next, causing a gap (if the next content has
			   been trimmed to a point between keyframes, or something).
			*/
			i->decoder->seek (dcp_to_content_time (i, i->content->position()), true);
			i->done = false;
		} else if (i->content->position() <= time && time < i->content->end(_film)) {
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
		for (int i = 0; i < TEXT_COUNT; ++i) {
			_active_texts[i].clear_before (time);
		}
	}

	optional<PositionImage> subtitles = open_subtitles_for_frame (time);
	if (subtitles) {
		pv->set_text (subtitles.get ());
	}

	Video (pv, time);
}

void
Player::emit_audio (shared_ptr<AudioBuffers> data, DCPTime time)
{
	/* Log if the assert below is about to fail */
	if (_last_audio_time && labs(time.get() - _last_audio_time->get()) > 1) {
		_film->log()->log(String::compose("Out-of-sequence emit %1 vs %2", to_string(time), to_string(*_last_audio_time)), LogEntry::TYPE_WARNING);
	}

	/* This audio must follow on from the previous, allowing for half a sample (at 48kHz) leeway */
	DCPOMATIC_ASSERT (!_last_audio_time || labs(time.get() - _last_audio_time->get()) < 2);
	Audio (data, time, _film->audio_frame_rate());
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
	shared_ptr<AudioBuffers> cut (new AudioBuffers(audio, remaining_frames, discard_frames));
	return make_pair(cut, time + discard_time);
}

void
Player::set_dcp_decode_reduction (optional<int> reduction)
{
	Change (CHANGE_TYPE_PENDING, PlayerProperty::DCP_DECODE_REDUCTION, false);

	{
		boost::mutex::scoped_lock lm (_mutex);

		if (reduction == _dcp_decode_reduction) {
			lm.unlock ();
			Change (CHANGE_TYPE_CANCELLED, PlayerProperty::DCP_DECODE_REDUCTION, false);
			return;
		}

		_dcp_decode_reduction = reduction;
		setup_pieces_unlocked ();
	}

	Change (CHANGE_TYPE_DONE, PlayerProperty::DCP_DECODE_REDUCTION, false);
}

optional<DCPTime>
Player::content_time_to_dcp (shared_ptr<Content> content, ContentTime t)
{
	boost::mutex::scoped_lock lm (_mutex);

	BOOST_FOREACH (shared_ptr<Piece> i, _pieces) {
		if (i->content == content) {
			return content_time_to_dcp (i, t);
		}
	}

	/* We couldn't find this content; perhaps things are being changed over */
	return optional<DCPTime>();
}
