/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#include "atmos_decoder.h"
#include "audio_buffers.h"
#include "audio_content.h"
#include "audio_decoder.h"
#include "audio_processor.h"
#include "compose.hpp"
#include "config.h"
#include "content_audio.h"
#include "content_video.h"
#include "dcp_content.h"
#include "dcp_decoder.h"
#include "dcpomatic_log.h"
#include "decoder.h"
#include "decoder_factory.h"
#include "ffmpeg_content.h"
#include "film.h"
#include "frame_rate_change.h"
#include "image.h"
#include "image_decoder.h"
#include "job.h"
#include "log.h"
#include "maths_util.h"
#include "piece.h"
#include "player.h"
#include "player_video.h"
#include "playlist.h"
#include "ratio.h"
#include "raw_image_proxy.h"
#include "render_text.h"
#include "shuffler.h"
#include "text_content.h"
#include "text_decoder.h"
#include "timer.h"
#include "video_decoder.h"
#include <dcp/reel.h>
#include <dcp/reel_closed_caption_asset.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/reel_subtitle_asset.h>
#include <algorithm>
#include <iostream>
#include <stdint.h>

#include "i18n.h"


using std::copy;
using std::cout;
using std::dynamic_pointer_cast;
using std::list;
using std::make_pair;
using std::make_shared;
using std::make_shared;
using std::max;
using std::min;
using std::min;
using std::pair;
using std::shared_ptr;
using std::vector;
using std::weak_ptr;
using boost::optional;
using boost::scoped_ptr;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


int const PlayerProperty::VIDEO_CONTAINER_SIZE = 700;
int const PlayerProperty::PLAYLIST = 701;
int const PlayerProperty::FILM_CONTAINER = 702;
int const PlayerProperty::FILM_VIDEO_FRAME_RATE = 703;
int const PlayerProperty::DCP_DECODE_REDUCTION = 704;
int const PlayerProperty::PLAYBACK_LENGTH = 705;
int const PlayerProperty::IGNORE_VIDEO = 706;
int const PlayerProperty::IGNORE_AUDIO = 707;
int const PlayerProperty::IGNORE_TEXT = 708;
int const PlayerProperty::ALWAYS_BURN_OPEN_SUBTITLES = 709;
int const PlayerProperty::PLAY_REFERENCED = 710;


Player::Player (shared_ptr<const Film> film, Image::Alignment subtitle_alignment)
	: _film (film)
	, _suspended (0)
	, _ignore_video(false)
	, _ignore_audio(false)
	, _ignore_text(false)
	, _always_burn_open_subtitles(false)
	, _fast(false)
	, _tolerant (film->tolerant())
	, _play_referenced(false)
	, _audio_merger(film->audio_frame_rate())
	, _subtitle_alignment (subtitle_alignment)
{
	construct ();
}


Player::Player (shared_ptr<const Film> film, shared_ptr<const Playlist> playlist_)
	: _film (film)
	, _playlist (playlist_)
	, _suspended (0)
	, _ignore_video(false)
	, _ignore_audio(false)
	, _ignore_text(false)
	, _always_burn_open_subtitles(false)
	, _fast(false)
	, _tolerant (film->tolerant())
	, _play_referenced(false)
	, _audio_merger(film->audio_frame_rate())
{
	construct ();
}


void
Player::construct ()
{
	auto film = _film.lock();
	DCPOMATIC_ASSERT(film);

	connect();
	set_video_container_size(film->frame_size());

	film_change (ChangeType::DONE, Film::Property::AUDIO_PROCESSOR);

	setup_pieces ();
	seek (DCPTime (), true);
}


void
Player::connect()
{
	auto film = _film.lock();
	DCPOMATIC_ASSERT(film);

	_film_changed_connection = film->Change.connect(bind(&Player::film_change, this, _1, _2));
	/* The butler must hear about this first, so since we are proxying this through to the butler we must
	   be first.
	*/
	_playlist_change_connection = playlist()->Change.connect (bind (&Player::playlist_change, this, _1), boost::signals2::at_front);
	_playlist_content_change_connection = playlist()->ContentChange.connect (bind(&Player::playlist_content_change, this, _1, _3, _4));
}


Player::Player(Player&& other)
	: _film(other._film)
	, _playlist(std::move(other._playlist))
	, _suspended(other._suspended.load())
	, _pieces(std::move(other._pieces))
	, _video_container_size(other._video_container_size.load())
	, _black_image(std::move(other._black_image))
	, _ignore_video(other._ignore_video.load())
	, _ignore_audio(other._ignore_audio.load())
	, _ignore_text(other._ignore_text.load())
	, _always_burn_open_subtitles(other._always_burn_open_subtitles.load())
	, _fast(other._fast.load())
	, _tolerant(other._tolerant)
	, _play_referenced(other._play_referenced.load())
	, _next_video_time(other._next_video_time)
	, _next_audio_time(other._next_audio_time)
	, _dcp_decode_reduction(other._dcp_decode_reduction.load())
	, _last_video(std::move(other._last_video))
	, _audio_merger(std::move(other._audio_merger))
	, _shuffler(std::move(other._shuffler))
	, _delay(std::move(other._delay))
	, _stream_states(std::move(other._stream_states))
	, _black(std::move(other._black))
	, _silent(std::move(other._silent))
	, _active_texts(std::move(other._active_texts))
	, _audio_processor(std::move(other._audio_processor))
	, _playback_length(other._playback_length.load())
	, _subtitle_alignment(other._subtitle_alignment)
{
	connect();
}


Player&
Player::operator=(Player&& other)
{
	if (this == &other) {
		return *this;
	}

	_film = std::move(other._film);
	_playlist = std::move(other._playlist);
	_suspended = other._suspended.load();
	_pieces = std::move(other._pieces);
	_video_container_size = other._video_container_size.load();
	_black_image = std::move(other._black_image);
	_ignore_video = other._ignore_video.load();
	_ignore_audio = other._ignore_audio.load();
	_ignore_text = other._ignore_text.load();
	_always_burn_open_subtitles = other._always_burn_open_subtitles.load();
	_fast = other._fast.load();
	_tolerant = other._tolerant;
	_play_referenced = other._play_referenced.load();
	_next_video_time = other._next_video_time;
	_next_audio_time = other._next_audio_time;
	_dcp_decode_reduction = other._dcp_decode_reduction.load();
	_last_video = std::move(other._last_video);
	_audio_merger = std::move(other._audio_merger);
	_shuffler = std::move(other._shuffler);
	_delay = std::move(other._delay);
	_stream_states = std::move(other._stream_states);
	_black = std::move(other._black);
	_silent = std::move(other._silent);
	_active_texts = std::move(other._active_texts);
	_audio_processor = std::move(other._audio_processor);
	_playback_length = other._playback_length.load();
	_subtitle_alignment = other._subtitle_alignment;

	connect();

	return *this;
}


bool
have_video (shared_ptr<const Content> content)
{
	return static_cast<bool>(content->video) && content->video->use() && content->can_be_played();
}


bool
have_audio (shared_ptr<const Content> content)
{
	return static_cast<bool>(content->audio) && content->can_be_played();
}


void
Player::setup_pieces ()
{
	boost::mutex::scoped_lock lm (_mutex);

	auto old_pieces = _pieces;
	_pieces.clear ();

	auto film = _film.lock();
	if (!film) {
		return;
	}

	_playback_length = _playlist ? _playlist->length(film) : film->length();

	auto playlist_content = playlist()->content();
	bool const have_threed = std::any_of(
		playlist_content.begin(),
		playlist_content.end(),
		[](shared_ptr<const Content> c) {
			return c->video && (c->video->frame_type() == VideoFrameType::THREE_D_LEFT || c->video->frame_type() == VideoFrameType::THREE_D_RIGHT);
		});


	if (have_threed) {
		_shuffler.reset(new Shuffler());
		_shuffler->Video.connect(bind(&Player::video, this, _1, _2));
	}

	for (auto content: playlist()->content()) {

		if (!content->paths_valid()) {
			continue;
		}

		if (_ignore_video && _ignore_audio && content->text.empty()) {
			/* We're only interested in text and this content has none */
			continue;
		}

		shared_ptr<Decoder> old_decoder;
		for (auto j: old_pieces) {
			if (j->content == content) {
				old_decoder = j->decoder;
				break;
			}
		}

		auto decoder = decoder_factory(film, content, _fast, _tolerant, old_decoder);
		DCPOMATIC_ASSERT (decoder);

		FrameRateChange frc(film, content);

		if (decoder->video && _ignore_video) {
			decoder->video->set_ignore (true);
		}

		if (decoder->audio && _ignore_audio) {
			decoder->audio->set_ignore (true);
		}

		if (_ignore_text) {
			for (auto i: decoder->text) {
				i->set_ignore (true);
			}
		}

		auto dcp = dynamic_pointer_cast<DCPDecoder> (decoder);
		if (dcp) {
			dcp->set_decode_referenced (_play_referenced);
			if (_play_referenced) {
				dcp->set_forced_reduction (_dcp_decode_reduction);
			}
		}

		auto piece = make_shared<Piece>(content, decoder, frc);
		_pieces.push_back (piece);

		if (decoder->video) {
			if (have_threed) {
				/* We need a Shuffler to cope with 3D L/R video data arriving out of sequence */
				decoder->video->Data.connect (bind(&Shuffler::video, _shuffler.get(), weak_ptr<Piece>(piece), _1));
			} else {
				decoder->video->Data.connect (bind(&Player::video, this, weak_ptr<Piece>(piece), _1));
			}
		}

		if (decoder->audio) {
			decoder->audio->Data.connect (bind (&Player::audio, this, weak_ptr<Piece> (piece), _1, _2));
		}

		auto j = decoder->text.begin();

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

		if (decoder->atmos) {
			decoder->atmos->Data.connect (bind(&Player::atmos, this, weak_ptr<Piece>(piece), _1));
		}
	}

	_stream_states.clear ();
	for (auto i: _pieces) {
		if (i->content->audio) {
			for (auto j: i->content->audio->streams()) {
				_stream_states[j] = StreamState(i);
			}
		}
	}

	auto ignore_overlap = [](shared_ptr<VideoContent> v) {
		return v && v->use() && v->frame_type() != VideoFrameType::THREE_D_LEFT && v->frame_type() != VideoFrameType::THREE_D_RIGHT;
	};

	for (auto piece = _pieces.begin(); piece != _pieces.end(); ++piece) {
		if (ignore_overlap((*piece)->content->video)) {
			/* Look for content later in the content list with in-use video that overlaps this */
			auto const period = (*piece)->content->period(film);
			for (auto later_piece = std::next(piece); later_piece != _pieces.end(); ++later_piece) {
				if (ignore_overlap((*later_piece)->content->video)) {
					(*piece)->ignore_video = (*later_piece)->content->period(film).overlap(period);
				}
			}
		}
	}

	_black = Empty(film, playlist(), bind(&have_video, _1), _playback_length);
	_silent = Empty(film, playlist(), bind(&have_audio, _1), _playback_length);

	_next_video_time = boost::none;
	_next_video_eyes = Eyes::BOTH;
	_next_audio_time = boost::none;
}


void
Player::playlist_content_change (ChangeType type, int property, bool frequent)
{
	auto film = _film.lock();
	if (!film) {
		return;
	}

	if (property == VideoContentProperty::CROP) {
		if (type == ChangeType::DONE) {
			boost::mutex::scoped_lock lm (_mutex);
			for (auto const& i: _delay) {
				i.first->reset_metadata(film, _video_container_size);
			}
		}
	} else {
		if (type == ChangeType::PENDING) {
			/* The player content is probably about to change, so we can't carry on
			   until that has happened and we've rebuilt our pieces.  Stop pass()
			   and seek() from working until then.
			*/
			++_suspended;
		} else if (type == ChangeType::DONE) {
			/* A change in our content has gone through.  Re-build our pieces. */
			setup_pieces ();
			--_suspended;
		} else if (type == ChangeType::CANCELLED) {
			--_suspended;
		}
	}

	Change (type, property, frequent);
}


void
Player::set_video_container_size (dcp::Size s)
{
	ChangeSignaller<Player, int> cc(this, PlayerProperty::VIDEO_CONTAINER_SIZE);

	if (s == _video_container_size) {
		cc.abort();
		return;
	}

	_video_container_size = s;

	{
		boost::mutex::scoped_lock lm(_black_image_mutex);
		_black_image = make_shared<Image>(AV_PIX_FMT_RGB24, _video_container_size, Image::Alignment::PADDED);
		_black_image->make_black ();
	}
}


void
Player::playlist_change (ChangeType type)
{
	if (type == ChangeType::DONE) {
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

	auto film = _film.lock();
	if (!film) {
		return;
	}

	if (p == Film::Property::CONTAINER) {
		Change (type, PlayerProperty::FILM_CONTAINER, false);
	} else if (p == Film::Property::VIDEO_FRAME_RATE) {
		/* Pieces contain a FrameRateChange which contains the DCP frame rate,
		   so we need new pieces here.
		*/
		if (type == ChangeType::DONE) {
			setup_pieces ();
		}
		Change (type, PlayerProperty::FILM_VIDEO_FRAME_RATE, false);
	} else if (p == Film::Property::AUDIO_PROCESSOR) {
		if (type == ChangeType::DONE && film->audio_processor ()) {
			boost::mutex::scoped_lock lm (_mutex);
			_audio_processor = film->audio_processor()->clone(film->audio_frame_rate());
		}
	} else if (p == Film::Property::AUDIO_CHANNELS) {
		if (type == ChangeType::DONE) {
			boost::mutex::scoped_lock lm (_mutex);
			_audio_merger.clear ();
		}
	}
}


shared_ptr<PlayerVideo>
Player::black_player_video_frame (Eyes eyes) const
{
	boost::mutex::scoped_lock lm(_black_image_mutex);

	return std::make_shared<PlayerVideo> (
		std::make_shared<const RawImageProxy>(_black_image),
		Crop(),
		optional<double>(),
		_video_container_size,
		_video_container_size,
		eyes,
		Part::WHOLE,
		PresetColourConversion::all().front().conversion,
		VideoRange::FULL,
		std::weak_ptr<Content>(),
		boost::optional<Frame>(),
		false
	);
}


Frame
Player::dcp_to_content_video (shared_ptr<const Piece> piece, DCPTime t) const
{
	auto film = _film.lock();
	DCPOMATIC_ASSERT(film);

	auto s = t - piece->content->position ();
	s = min (piece->content->length_after_trim(film), s);
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
	auto const d = DCPTime::from_frames (f * piece->frc.factor(), piece->frc.dcp) - DCPTime(piece->content->trim_start(), piece->frc);
	return d + piece->content->position();
}


Frame
Player::dcp_to_resampled_audio (shared_ptr<const Piece> piece, DCPTime t) const
{
	auto film = _film.lock();
	DCPOMATIC_ASSERT(film);

	auto s = t - piece->content->position ();
	s = min (piece->content->length_after_trim(film), s);
	/* See notes in dcp_to_content_video */
	return max (DCPTime(), DCPTime(piece->content->trim_start(), piece->frc) + s).frames_floor(film->audio_frame_rate());
}


DCPTime
Player::resampled_audio_to_dcp (shared_ptr<const Piece> piece, Frame f) const
{
	auto film = _film.lock();
	DCPOMATIC_ASSERT(film);

	/* See comment in dcp_to_content_video */
	return DCPTime::from_frames(f, film->audio_frame_rate())
		- DCPTime (piece->content->trim_start(), piece->frc)
		+ piece->content->position();
}


ContentTime
Player::dcp_to_content_time (shared_ptr<const Piece> piece, DCPTime t) const
{
	auto film = _film.lock();
	DCPOMATIC_ASSERT(film);

	auto s = t - piece->content->position ();
	s = min (piece->content->length_after_trim(film), s);
	return max (ContentTime (), ContentTime (s, piece->frc) + piece->content->trim_start());
}


DCPTime
Player::content_time_to_dcp (shared_ptr<const Piece> piece, ContentTime t) const
{
	return max (DCPTime(), DCPTime(t - piece->content->trim_start(), piece->frc) + piece->content->position());
}


vector<shared_ptr<Font>>
Player::get_subtitle_fonts ()
{
	boost::mutex::scoped_lock lm (_mutex);

	vector<shared_ptr<Font>> fonts;
	for (auto piece: _pieces) {
		for (auto text: piece->content->text) {
			auto text_fonts = text->fonts();
			copy (text_fonts.begin(), text_fonts.end(), back_inserter(fonts));
		}
	}

	return fonts;
}


/** Set this player never to produce any video data */
void
Player::set_ignore_video ()
{
	ChangeSignaller<Player, int> cc(this, PlayerProperty::IGNORE_VIDEO);
	_ignore_video = true;
	setup_pieces();
}


void
Player::set_ignore_audio ()
{
	ChangeSignaller<Player, int> cc(this, PlayerProperty::IGNORE_AUDIO);
	_ignore_audio = true;
	setup_pieces();
}


void
Player::set_ignore_text ()
{
	ChangeSignaller<Player, int> cc(this, PlayerProperty::IGNORE_TEXT);
	_ignore_text = true;
	setup_pieces();
}


/** Set the player to always burn open texts into the image regardless of the content settings */
void
Player::set_always_burn_open_subtitles ()
{
	ChangeSignaller<Player, int> cc(this, PlayerProperty::ALWAYS_BURN_OPEN_SUBTITLES);
	_always_burn_open_subtitles = true;
}


/** Sets up the player to be faster, possibly at the expense of quality */
void
Player::set_fast ()
{
	_fast = true;
	setup_pieces();
}


void
Player::set_play_referenced ()
{
	ChangeSignaller<Player, int> cc(this, PlayerProperty::PLAY_REFERENCED);
	_play_referenced = true;
	setup_pieces();
}


bool
Player::pass ()
{
	boost::mutex::scoped_lock lm (_mutex);

	if (_suspended) {
		/* We can't pass in this state */
		LOG_DEBUG_PLAYER_NC ("Player is suspended");
		return false;
	}

	auto film = _film.lock();

	if (_playback_length.load() == DCPTime() || !film) {
		/* Special; just give one black frame */
		emit_video (black_player_video_frame(Eyes::BOTH), DCPTime());
		return true;
	}

	/* Find the decoder or empty which is farthest behind where we are and make it emit some data */

	shared_ptr<Piece> earliest_content;
	optional<DCPTime> earliest_time;

	for (auto i: _pieces) {
		if (i->done) {
			continue;
		}

		auto const t = content_time_to_dcp (i, max(i->decoder->position(), i->content->trim_start()));
		if (t > i->content->end(film)) {
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
		LOG_DEBUG_PLAYER ("Calling pass() on %1", earliest_content->content->path(0));
		earliest_content->done = earliest_content->decoder->pass ();
		auto dcp = dynamic_pointer_cast<DCPContent>(earliest_content->content);
		if (dcp && !_play_referenced && dcp->reference_audio()) {
			/* We are skipping some referenced DCP audio content, so we need to update _next_audio_time
			   to `hide' the fact that no audio was emitted during the referenced DCP (though
			   we need to behave as though it was).
			*/
			_next_audio_time = dcp->end(film);
		}
		break;
	}
	case BLACK:
		LOG_DEBUG_PLAYER ("Emit black for gap at %1", to_string(_black.position()));
		emit_video (black_player_video_frame(Eyes::BOTH), _black.position());
		_black.set_position (_black.position() + one_video_frame());
		break;
	case SILENT:
	{
		LOG_DEBUG_PLAYER ("Emit silence for gap at %1", to_string(_silent.position()));
		DCPTimePeriod period (_silent.period_at_position());
		if (_next_audio_time) {
			/* Sometimes the thing that happened last finishes fractionally before
			   or after this silence.  Bodge the start time of the silence to fix it.
			   I think this is nothing to worry about since we will just add or
			   remove a little silence at the end of some content.
			*/
			int64_t const error = labs(period.from.get() - _next_audio_time->get());
			/* Let's not worry about less than a frame at 24fps */
			int64_t const too_much_error = DCPTime::from_frames(1, 24).get();
			if (error >= too_much_error) {
				film->log()->log(String::compose("Silence starting before or after last audio by %1", error), LogEntry::TYPE_ERROR);
			}
			DCPOMATIC_ASSERT (error < too_much_error);
			period.from = *_next_audio_time;
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
	   of our streams, or the position of the _silent.  First, though we choose only streams that are less than
	   ignore_streams_behind seconds behind the furthest ahead (we assume that if a stream has fallen that far
	   behind it has finished).  This is so that we don't withhold audio indefinitely awaiting data from a stream
	   that will never come, causing bugs like #2101.
	*/
	constexpr int ignore_streams_behind = 5;

	using state_pair = std::pair<AudioStreamPtr, StreamState>;

	/* Find streams that have pushed */
	std::vector<state_pair> have_pushed;
	std::copy_if(_stream_states.begin(), _stream_states.end(), std::back_inserter(have_pushed), [](state_pair const& a) { return static_cast<bool>(a.second.last_push_end); });

	/* Find the 'leading' stream (i.e. the one that pushed data most recently) */
	auto latest_last_push_end = std::max_element(
		have_pushed.begin(),
		have_pushed.end(),
		[](state_pair const& a, state_pair const& b) { return a.second.last_push_end.get() < b.second.last_push_end.get(); }
		);

	if (latest_last_push_end != have_pushed.end()) {
		LOG_DEBUG_PLAYER("Leading audio stream is in %1 at %2", latest_last_push_end->second.piece->content->path(0), to_string(latest_last_push_end->second.last_push_end.get()));
	}

	/* Now make a list of those streams that are less than ignore_streams_behind behind the leader */
	std::map<AudioStreamPtr, StreamState> alive_stream_states;
	for (auto const& i: _stream_states) {
		if (!i.second.last_push_end || (latest_last_push_end->second.last_push_end.get() - i.second.last_push_end.get()) < dcpomatic::DCPTime::from_seconds(ignore_streams_behind)) {
			alive_stream_states.insert(i);
		} else {
			LOG_DEBUG_PLAYER("Ignoring stream %1 because it is too far behind", i.second.piece->content->path(0));
		}
	}

	auto pull_to = _playback_length.load();
	for (auto const& i: alive_stream_states) {
		auto position = i.second.last_push_end.get_value_or(i.second.piece->content->position());
		if (!i.second.piece->done && position < pull_to) {
			pull_to = position;
		}
	}
	if (!_silent.done() && _silent.position() < pull_to) {
		pull_to = _silent.position();
	}

	LOG_DEBUG_PLAYER("Emitting audio up to %1", to_string(pull_to));
	auto audio = _audio_merger.pull (pull_to);
	for (auto i = audio.begin(); i != audio.end(); ++i) {
		if (_next_audio_time && i->second < *_next_audio_time) {
			/* This new data comes before the last we emitted (or the last seek); discard it */
			auto cut = discard_audio (i->first, i->second, *_next_audio_time);
			if (!cut.first) {
				continue;
			}
			*i = cut;
		} else if (_next_audio_time && i->second > *_next_audio_time) {
			/* There's a gap between this data and the last we emitted; fill with silence */
			fill_audio (DCPTimePeriod (*_next_audio_time, i->second));
		}

		emit_audio (i->first, i->second);
	}

	if (done) {
		if (_shuffler) {
			_shuffler->flush ();
		}
		for (auto const& i: _delay) {
			do_emit_video(i.first, i.second);
		}

		/* Perhaps we should have Empty entries for both eyes in the 3D case (somehow).
		 * However, if we have L and R video files, and one is shorter than the other,
		 * the fill code in ::video mostly takes care of filling in the gaps.
		 * However, since it fills at the point when it knows there is more video coming
		 * at time t (so it should fill any gap up to t) it can't do anything right at the
		 * end.  This is particularly bad news if the last frame emitted is a LEFT
		 * eye, as the MXF writer will complain about the 3D sequence being wrong.
		 * Here's a hack to workaround that particular case.
		 */
		if (_next_video_eyes && _next_video_time && *_next_video_eyes == Eyes::RIGHT) {
			do_emit_video (black_player_video_frame(Eyes::RIGHT), *_next_video_time);
		}
	}

	return done;
}


/** @return Open subtitles for the frame at the given time, converted to images */
optional<PositionImage>
Player::open_subtitles_for_frame (DCPTime time) const
{
	auto film = _film.lock();
	if (!film) {
		return {};
	}

	list<PositionImage> captions;
	int const vfr = film->video_frame_rate();

	for (
		auto j:
		_active_texts[TextType::OPEN_SUBTITLE].get_burnt(DCPTimePeriod(time, time + DCPTime::from_frames(1, vfr)), _always_burn_open_subtitles)
		) {

		/* Bitmap subtitles */
		for (auto i: j.bitmap) {
			if (!i.image) {
				continue;
			}

			/* i.image will already have been scaled to fit _video_container_size */
			dcp::Size scaled_size (i.rectangle.width * _video_container_size.load().width, i.rectangle.height * _video_container_size.load().height);

			captions.push_back (
				PositionImage (
					i.image,
					Position<int> (
						lrint(_video_container_size.load().width * i.rectangle.x),
						lrint(_video_container_size.load().height * i.rectangle.y)
						)
					)
				);
		}

		/* String subtitles (rendered to an image) */
		if (!j.string.empty()) {
			auto s = render_text(j.string, _video_container_size, time, vfr);
			copy (s.begin(), s.end(), back_inserter (captions));
		}
	}

	if (captions.empty()) {
		return {};
	}

	return merge (captions, _subtitle_alignment);
}


static
Eyes
increment_eyes (Eyes e)
{
	if (e == Eyes::LEFT) {
		return Eyes::RIGHT;
	}

	return Eyes::LEFT;
}


void
Player::video (weak_ptr<Piece> weak_piece, ContentVideo video)
{
	if (_suspended) {
		return;
	}

	auto piece = weak_piece.lock ();
	if (!piece) {
		return;
	}

	if (!piece->content->video->use()) {
		return;
	}

	auto film = _film.lock();
	if (!film) {
		return;
	}

	FrameRateChange frc(film, piece->content);
	if (frc.skip && (video.frame % 2) == 1) {
		return;
	}

	vector<Eyes> eyes_to_emit;

	if (!film->three_d()) {
		if (video.eyes == Eyes::RIGHT) {
			/* 2D film, 3D content: discard right */
			return;
		} else if (video.eyes == Eyes::LEFT) {
			/* 2D film, 3D content: emit left as "both" */
			video.eyes = Eyes::BOTH;
			eyes_to_emit = { Eyes::BOTH };
		}
	} else {
		if (video.eyes == Eyes::BOTH) {
			/* 3D film, 2D content; emit "both" for left and right */
			eyes_to_emit = { Eyes::LEFT, Eyes::RIGHT };
		}
	}

	if (eyes_to_emit.empty()) {
		eyes_to_emit = { video.eyes };
	}

	/* Time of the first frame we will emit */
	DCPTime const time = content_video_to_dcp (piece, video.frame);
	LOG_DEBUG_PLAYER("Received video frame %1 at %2", video.frame, to_string(time));

	/* Discard if it's before the content's period or the last accurate seek.  We can't discard
	   if it's after the content's period here as in that case we still need to fill any gap between
	   `now' and the end of the content's period.
	*/
	if (time < piece->content->position() || (_next_video_time && time < *_next_video_time)) {
		return;
	}

	if (piece->ignore_video && piece->ignore_video->contains(time)) {
		return;
	}

	/* Fill gaps that we discover now that we have some video which needs to be emitted.
	   This is where we need to fill to.
	*/
	DCPTime fill_to = min(time, piece->content->end(film));

	if (_next_video_time) {
		DCPTime fill_from = max (*_next_video_time, piece->content->position());

		/* Fill if we have more than half a frame to do */
		if ((fill_to - fill_from) > one_video_frame() / 2) {
			auto last = _last_video.find (weak_piece);
			if (film->three_d()) {
				auto fill_to_eyes = eyes_to_emit[0];
				if (fill_to_eyes == Eyes::BOTH) {
					fill_to_eyes = Eyes::LEFT;
				}
				if (fill_to == piece->content->end(film)) {
					/* Don't fill after the end of the content */
					fill_to_eyes = Eyes::LEFT;
				}
				auto j = fill_from;
				auto eyes = _next_video_eyes.get_value_or(Eyes::LEFT);
				if (eyes == Eyes::BOTH) {
					eyes = Eyes::LEFT;
				}
				while (j < fill_to || eyes != fill_to_eyes) {
					if (last != _last_video.end()) {
						LOG_DEBUG_PLAYER("Fill using last video at %1 in 3D mode", to_string(j));
						auto copy = last->second->shallow_copy();
						copy->set_eyes (eyes);
						emit_video (copy, j);
					} else {
						LOG_DEBUG_PLAYER("Fill using black at %1 in 3D mode", to_string(j));
						emit_video (black_player_video_frame(eyes), j);
					}
					if (eyes == Eyes::RIGHT) {
						j += one_video_frame();
					}
					eyes = increment_eyes (eyes);
				}
			} else {
				for (DCPTime j = fill_from; j < fill_to; j += one_video_frame()) {
					if (last != _last_video.end()) {
						emit_video (last->second, j);
					} else {
						emit_video (black_player_video_frame(Eyes::BOTH), j);
					}
				}
			}
		}
	}

	auto const content_video = piece->content->video;

	for (auto eyes: eyes_to_emit) {
		_last_video[weak_piece] = std::make_shared<PlayerVideo>(
			video.image,
			content_video->actual_crop(),
			content_video->fade(film, video.frame),
			scale_for_display(
				content_video->scaled_size(film->frame_size()),
				_video_container_size,
				film->frame_size(),
				content_video->pixel_quanta()
				),
			_video_container_size,
			eyes,
			video.part,
			content_video->colour_conversion(),
			content_video->range(),
			piece->content,
			video.frame,
			false
			);

		DCPTime t = time;
		for (int i = 0; i < frc.repeat; ++i) {
			if (t < piece->content->end(film)) {
				emit_video (_last_video[weak_piece], t);
			}
			t += one_video_frame ();
		}
	}
}


void
Player::audio (weak_ptr<Piece> weak_piece, AudioStreamPtr stream, ContentAudio content_audio)
{
	if (_suspended) {
		return;
	}

	DCPOMATIC_ASSERT (content_audio.audio->frames() > 0);

	auto piece = weak_piece.lock ();
	if (!piece) {
		return;
	}

	auto film = _film.lock();
	if (!film) {
		return;
	}

	auto content = piece->content->audio;
	DCPOMATIC_ASSERT (content);

	int const rfr = content->resampled_frame_rate(film);

	/* Compute time in the DCP */
	auto time = resampled_audio_to_dcp (piece, content_audio.frame);

	/* And the end of this block in the DCP */
	auto end = time + DCPTime::from_frames(content_audio.audio->frames(), rfr);
	LOG_DEBUG_PLAYER("Received audio frame %1 covering %2 to %3 (%4)", content_audio.frame, to_string(time), to_string(end), piece->content->path(0).filename());

	/* Remove anything that comes before the start or after the end of the content */
	if (time < piece->content->position()) {
		auto cut = discard_audio (content_audio.audio, time, piece->content->position());
		if (!cut.first) {
			/* This audio is entirely discarded */
			return;
		}
		content_audio.audio = cut.first;
		time = cut.second;
	} else if (time > piece->content->end(film)) {
		/* Discard it all */
		return;
	} else if (end > piece->content->end(film)) {
		Frame const remaining_frames = DCPTime(piece->content->end(film) - time).frames_round(rfr);
		if (remaining_frames == 0) {
			return;
		}
		content_audio.audio = make_shared<AudioBuffers>(content_audio.audio, remaining_frames, 0);
	}

	DCPOMATIC_ASSERT (content_audio.audio->frames() > 0);

	/* Gain and fade */

	auto const fade_coeffs = content->fade (stream, content_audio.frame, content_audio.audio->frames(), rfr);
	if (content->gain() != 0 || !fade_coeffs.empty()) {
		auto gain_buffers = make_shared<AudioBuffers>(content_audio.audio);
		if (!fade_coeffs.empty()) {
			/* Apply both fade and gain */
			DCPOMATIC_ASSERT (fade_coeffs.size() == static_cast<size_t>(gain_buffers->frames()));
			auto const channels = gain_buffers->channels();
			auto const frames = fade_coeffs.size();
			auto data = gain_buffers->data();
			auto const gain = db_to_linear (content->gain());
			for (auto channel = 0; channel < channels; ++channel) {
				for (auto frame = 0U; frame < frames; ++frame) {
					data[channel][frame] *= gain * fade_coeffs[frame];
				}
			}
		} else {
			/* Just apply gain */
			gain_buffers->apply_gain (content->gain());
		}
		content_audio.audio = gain_buffers;
	}

	/* Remap */

	content_audio.audio = remap(content_audio.audio, film->audio_channels(), stream->mapping());

	/* Process */

	if (_audio_processor) {
		content_audio.audio = _audio_processor->run(content_audio.audio, film->audio_channels());
	}

	/* Push */

	_audio_merger.push (content_audio.audio, time);
	DCPOMATIC_ASSERT (_stream_states.find (stream) != _stream_states.end ());
	_stream_states[stream].last_push_end = time + DCPTime::from_frames(content_audio.audio->frames(), film->audio_frame_rate());
}


void
Player::bitmap_text_start (weak_ptr<Piece> weak_piece, weak_ptr<const TextContent> weak_content, ContentBitmapText subtitle)
{
	if (_suspended) {
		return;
	}

	auto piece = weak_piece.lock ();
	auto content = weak_content.lock ();
	if (!piece || !content) {
		return;
	}

	PlayerText ps;
	for (auto& sub: subtitle.subs)
	{
		/* Apply content's subtitle offsets */
		sub.rectangle.x += content->x_offset ();
		sub.rectangle.y += content->y_offset ();

		/* Apply a corrective translation to keep the subtitle centred after the scale that is coming up */
		sub.rectangle.x -= sub.rectangle.width * ((content->x_scale() - 1) / 2);
		sub.rectangle.y -= sub.rectangle.height * ((content->y_scale() - 1) / 2);

		/* Apply content's subtitle scale */
		sub.rectangle.width *= content->x_scale ();
		sub.rectangle.height *= content->y_scale ();

		auto image = sub.image;

		/* We will scale the subtitle up to fit _video_container_size */
		int const width = sub.rectangle.width * _video_container_size.load().width;
		int const height = sub.rectangle.height * _video_container_size.load().height;
		if (width == 0 || height == 0) {
			return;
		}

		dcp::Size scaled_size (width, height);
		ps.bitmap.push_back (BitmapText(image->scale(scaled_size, dcp::YUVToRGB::REC601, image->pixel_format(), Image::Alignment::PADDED, _fast), sub.rectangle));
	}

	DCPTime from(content_time_to_dcp(piece, subtitle.from()));
	_active_texts[content->type()].add_from(weak_content, ps, from);
}


void
Player::plain_text_start (weak_ptr<Piece> weak_piece, weak_ptr<const TextContent> weak_content, ContentStringText subtitle)
{
	if (_suspended) {
		return;
	}

	auto piece = weak_piece.lock ();
	auto content = weak_content.lock ();
	auto film = _film.lock();
	if (!piece || !content || !film) {
		return;
	}

	PlayerText ps;
	DCPTime const from (content_time_to_dcp (piece, subtitle.from()));

	if (from > piece->content->end(film)) {
		return;
	}

	for (auto s: subtitle.subs) {
		s.set_h_position (s.h_position() + content->x_offset());
		s.set_v_position (s.v_position() + content->y_offset());
		float const xs = content->x_scale();
		float const ys = content->y_scale();
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
		ps.string.push_back (s);
	}

	_active_texts[content->type()].add_from(weak_content, ps, from);
}


void
Player::subtitle_stop (weak_ptr<Piece> weak_piece, weak_ptr<const TextContent> weak_content, ContentTime to)
{
	if (_suspended) {
		return;
	}

	auto content = weak_content.lock ();
	if (!content) {
		return;
	}

	if (!_active_texts[content->type()].have(weak_content)) {
		return;
	}

	auto piece = weak_piece.lock ();
	auto film = _film.lock();
	if (!piece || !film) {
		return;
	}

	DCPTime const dcp_to = content_time_to_dcp (piece, to);

	if (dcp_to > piece->content->end(film)) {
		return;
	}

	auto from = _active_texts[content->type()].add_to(weak_content, dcp_to);

	bool const always = (content->type() == TextType::OPEN_SUBTITLE && _always_burn_open_subtitles);
	if (content->use() && !always && !content->burn()) {
		Text (from.first, content->type(), content->dcp_track().get_value_or(DCPTextTrack()), DCPTimePeriod(from.second, dcp_to));
	}
}


void
Player::seek (DCPTime time, bool accurate)
{
	boost::mutex::scoped_lock lm (_mutex);
	LOG_DEBUG_PLAYER("Seek to %1 (%2accurate)", to_string(time), accurate ? "" : "in");

	if (_suspended) {
		/* We can't seek in this state */
		return;
	}

	auto film = _film.lock();
	if (!film) {
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
	std::for_each(_active_texts.begin(), _active_texts.end(), [](ActiveText& a) { a.clear(); });

	for (auto i: _pieces) {
		if (time < i->content->position()) {
			/* Before; seek to the start of the content.  Even if this request is for an inaccurate seek
			   we must seek this (following) content accurately, otherwise when we come to the end of the current
			   content we may not start right at the beginning of the next, causing a gap (if the next content has
			   been trimmed to a point between keyframes, or something).
			*/
			i->decoder->seek (dcp_to_content_time (i, i->content->position()), true);
			i->done = false;
		} else if (i->content->position() <= time && time < i->content->end(film)) {
			/* During; seek to position */
			i->decoder->seek (dcp_to_content_time (i, time), accurate);
			i->done = false;
		} else {
			/* After; this piece is done */
			i->done = true;
		}
	}

	if (accurate) {
		_next_video_time = time;
		_next_video_eyes = Eyes::LEFT;
		_next_audio_time = time;
	} else {
		_next_video_time = boost::none;
		_next_video_eyes = boost::none;
		_next_audio_time = boost::none;
	}

	_black.set_position (time);
	_silent.set_position (time);

	_last_video.clear ();

	for (auto& state: _stream_states) {
		state.second.last_push_end = boost::none;
	}
}


void
Player::emit_video (shared_ptr<PlayerVideo> pv, DCPTime time)
{
	auto film = _film.lock();
	DCPOMATIC_ASSERT(film);

	if (!film->three_d()) {
		if (pv->eyes() == Eyes::LEFT) {
			/* Use left-eye images for both eyes... */
			pv->set_eyes (Eyes::BOTH);
		} else if (pv->eyes() == Eyes::RIGHT) {
			/* ...and discard the right */
			return;
		}
	}

	/* We need a delay to give a little wiggle room to ensure that relevant subtitles arrive at the
	   player before the video that requires them.
	*/
	_delay.push_back (make_pair (pv, time));

	if (pv->eyes() == Eyes::BOTH || pv->eyes() == Eyes::RIGHT) {
		_next_video_time = time + one_video_frame();
	}
	_next_video_eyes = increment_eyes (pv->eyes());

	if (_delay.size() < 3) {
		return;
	}

	auto to_do = _delay.front();
	_delay.pop_front();
	do_emit_video (to_do.first, to_do.second);
}


void
Player::do_emit_video (shared_ptr<PlayerVideo> pv, DCPTime time)
{
	if (pv->eyes() == Eyes::BOTH || pv->eyes() == Eyes::RIGHT) {
		std::for_each(_active_texts.begin(), _active_texts.end(), [time](ActiveText& a) { a.clear_before(time); });
	}

	auto subtitles = open_subtitles_for_frame (time);
	if (subtitles) {
		pv->set_text (subtitles.get ());
	}

	Video (pv, time);
}


void
Player::emit_audio (shared_ptr<AudioBuffers> data, DCPTime time)
{
	auto film = _film.lock();
	DCPOMATIC_ASSERT(film);

	/* Log if the assert below is about to fail */
	if (_next_audio_time && labs(time.get() - _next_audio_time->get()) > 1) {
		film->log()->log(String::compose("Out-of-sequence emit %1 vs %2", to_string(time), to_string(*_next_audio_time)), LogEntry::TYPE_WARNING);
	}

	/* This audio must follow on from the previous, allowing for half a sample (at 48kHz) leeway */
	DCPOMATIC_ASSERT (!_next_audio_time || labs(time.get() - _next_audio_time->get()) < 2);
	Audio(data, time, film->audio_frame_rate());
	_next_audio_time = time + DCPTime::from_frames(data->frames(), film->audio_frame_rate());
}


void
Player::fill_audio (DCPTimePeriod period)
{
	auto film = _film.lock();
	DCPOMATIC_ASSERT(film);

	if (period.from == period.to) {
		return;
	}

	DCPOMATIC_ASSERT (period.from < period.to);

	DCPTime t = period.from;
	while (t < period.to) {
		DCPTime block = min (DCPTime::from_seconds (0.5), period.to - t);
		Frame const samples = block.frames_round(film->audio_frame_rate());
		if (samples) {
			auto silence = make_shared<AudioBuffers>(film->audio_channels(), samples);
			silence->make_silent ();
			emit_audio (silence, t);
		}
		t += block;
	}
}


DCPTime
Player::one_video_frame () const
{
	auto film = _film.lock();
	DCPOMATIC_ASSERT(film);

	return DCPTime::from_frames(1, film->video_frame_rate ());
}


pair<shared_ptr<AudioBuffers>, DCPTime>
Player::discard_audio (shared_ptr<const AudioBuffers> audio, DCPTime time, DCPTime discard_to) const
{
	auto film = _film.lock();
	DCPOMATIC_ASSERT(film);

	auto const discard_time = discard_to - time;
	auto const discard_frames = discard_time.frames_round(film->audio_frame_rate());
	auto remaining_frames = audio->frames() - discard_frames;
	if (remaining_frames <= 0) {
		return make_pair(shared_ptr<AudioBuffers>(), DCPTime());
	}
	auto cut = make_shared<AudioBuffers>(audio, remaining_frames, discard_frames);
	return make_pair(cut, time + discard_time);
}


void
Player::set_dcp_decode_reduction (optional<int> reduction)
{
	ChangeSignaller<Player, int> cc(this, PlayerProperty::DCP_DECODE_REDUCTION);

	if (reduction == _dcp_decode_reduction.load()) {
		cc.abort();
		return;
	}

	_dcp_decode_reduction = reduction;
	setup_pieces();
}


optional<DCPTime>
Player::content_time_to_dcp (shared_ptr<const Content> content, ContentTime t) const
{
	boost::mutex::scoped_lock lm (_mutex);

	for (auto i: _pieces) {
		if (i->content == content) {
			return content_time_to_dcp (i, t);
		}
	}

	/* We couldn't find this content; perhaps things are being changed over */
	return {};
}


optional<ContentTime>
Player::dcp_to_content_time (shared_ptr<const Content> content, DCPTime t) const
{
	boost::mutex::scoped_lock lm (_mutex);

	for (auto i: _pieces) {
		if (i->content == content) {
			return dcp_to_content_time (i, t);
		}
	}

	/* We couldn't find this content; perhaps things are being changed over */
	return {};
}


shared_ptr<const Playlist>
Player::playlist () const
{
	auto film = _film.lock();
	if (!film) {
		return {};
	}

	return _playlist ? _playlist : film->playlist();
}


void
Player::atmos (weak_ptr<Piece> weak_piece, ContentAtmos data)
{
	if (_suspended) {
		return;
	}

	auto film = _film.lock();
	DCPOMATIC_ASSERT(film);

	auto piece = weak_piece.lock ();
	DCPOMATIC_ASSERT (piece);

	auto const vfr = film->video_frame_rate();

	DCPTime const dcp_time = DCPTime::from_frames(data.frame, vfr) - DCPTime(piece->content->trim_start(), FrameRateChange(vfr, vfr));
	if (dcp_time < piece->content->position() || dcp_time >= (piece->content->end(film))) {
		return;
	}

	Atmos (data.data, dcp_time, data.metadata);
}


void
Player::signal_change(ChangeType type, int property)
{
	Change(type, property, false);
}

