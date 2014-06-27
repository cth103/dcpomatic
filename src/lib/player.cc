/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

#include <stdint.h>
#include "player.h"
#include "film.h"
#include "ffmpeg_decoder.h"
#include "ffmpeg_content.h"
#include "image_decoder.h"
#include "image_content.h"
#include "sndfile_decoder.h"
#include "sndfile_content.h"
#include "subtitle_content.h"
#include "playlist.h"
#include "job.h"
#include "image.h"
#include "image_proxy.h"
#include "ratio.h"
#include "resampler.h"
#include "log.h"
#include "scaler.h"
#include "player_video_frame.h"
#include "frame_rate_change.h"

#define LOG_GENERAL(...) _film->log()->log (String::compose (__VA_ARGS__), Log::TYPE_GENERAL);

using std::list;
using std::cout;
using std::min;
using std::max;
using std::vector;
using std::pair;
using std::map;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;

Player::Player (shared_ptr<const Film> f, shared_ptr<const Playlist> p)
	: _film (f)
	, _playlist (p)
	, _video (true)
	, _audio (true)
	, _have_valid_pieces (false)
	, _video_position (0)
	, _audio_position (0)
	, _audio_merger (f->audio_channels(), bind (&Film::time_to_audio_frames, f.get(), _1), bind (&Film::audio_frames_to_time, f.get(), _1))
	, _last_emit_was_black (false)
{
	_playlist_changed_connection = _playlist->Changed.connect (bind (&Player::playlist_changed, this));
	_playlist_content_changed_connection = _playlist->ContentChanged.connect (bind (&Player::content_changed, this, _1, _2, _3));
	_film_changed_connection = _film->Changed.connect (bind (&Player::film_changed, this, _1));
	set_video_container_size (_film->frame_size ());
}

void
Player::disable_video ()
{
	_video = false;
}

void
Player::disable_audio ()
{
	_audio = false;
}

bool
Player::pass ()
{
	if (!_have_valid_pieces) {
		setup_pieces ();
	}

	Time earliest_t = TIME_MAX;
	shared_ptr<Piece> earliest;
	enum {
		VIDEO,
		AUDIO
	} type = VIDEO;

	for (list<shared_ptr<Piece> >::iterator i = _pieces.begin(); i != _pieces.end(); ++i) {
		if ((*i)->decoder->done () || (*i)->content->length_after_trim() == 0) {
			continue;
		}

		shared_ptr<VideoDecoder> vd = dynamic_pointer_cast<VideoDecoder> ((*i)->decoder);
		shared_ptr<AudioDecoder> ad = dynamic_pointer_cast<AudioDecoder> ((*i)->decoder);

		if (_video && vd) {
			if ((*i)->video_position < earliest_t) {
				earliest_t = (*i)->video_position;
				earliest = *i;
				type = VIDEO;
			}
		}

		if (_audio && ad && ad->has_audio ()) {
			if ((*i)->audio_position < earliest_t) {
				earliest_t = (*i)->audio_position;
				earliest = *i;
				type = AUDIO;
			}
		}
	}

	if (!earliest) {
		flush ();
		return true;
	}

	switch (type) {
	case VIDEO:
		if (earliest_t > _video_position) {
			emit_black ();
		} else {
			if (earliest->repeating ()) {
				earliest->repeat (this);
			} else {
				earliest->decoder->pass ();
			}
		}
		break;

	case AUDIO:
		if (earliest_t > _audio_position) {
			emit_silence (_film->time_to_audio_frames (earliest_t - _audio_position));
		} else {
			earliest->decoder->pass ();

			if (earliest->decoder->done()) {
				shared_ptr<AudioContent> ac = dynamic_pointer_cast<AudioContent> (earliest->content);
				assert (ac);
				shared_ptr<Resampler> re = resampler (ac, false);
				if (re) {
					shared_ptr<const AudioBuffers> b = re->flush ();
					if (b->frames ()) {
						process_audio (
							earliest,
							b,
							ac->audio_length() * ac->output_audio_frame_rate() / ac->content_audio_frame_rate(),
							true
							);
					}
				}
			}
		}
		break;
	}

	if (_audio) {
		boost::optional<Time> audio_done_up_to;
		for (list<shared_ptr<Piece> >::iterator i = _pieces.begin(); i != _pieces.end(); ++i) {
			if ((*i)->decoder->done ()) {
				continue;
			}

			shared_ptr<AudioDecoder> ad = dynamic_pointer_cast<AudioDecoder> ((*i)->decoder);
			if (ad && ad->has_audio ()) {
				audio_done_up_to = min (audio_done_up_to.get_value_or (TIME_MAX), (*i)->audio_position);
			}
		}

		if (audio_done_up_to) {
			TimedAudioBuffers<Time> tb = _audio_merger.pull (audio_done_up_to.get ());
			Audio (tb.audio, tb.time);
			_audio_position += _film->audio_frames_to_time (tb.audio->frames ());
		}
	}
		
	return false;
}

/** @param extra Amount of extra time to add to the content frame's time (for repeat) */
void
Player::process_video (weak_ptr<Piece> weak_piece, shared_ptr<const ImageProxy> image, Eyes eyes, Part part, bool same, VideoContent::Frame frame, Time extra)
{
	/* Keep a note of what came in so that we can repeat it if required */
	_last_incoming_video.weak_piece = weak_piece;
	_last_incoming_video.image = image;
	_last_incoming_video.eyes = eyes;
	_last_incoming_video.part = part;
	_last_incoming_video.same = same;
	_last_incoming_video.frame = frame;
	_last_incoming_video.extra = extra;
	
	shared_ptr<Piece> piece = weak_piece.lock ();
	if (!piece) {
		return;
	}

	shared_ptr<VideoContent> content = dynamic_pointer_cast<VideoContent> (piece->content);
	assert (content);

	FrameRateChange frc (content->video_frame_rate(), _film->video_frame_rate());
	if (frc.skip && (frame % 2) == 1) {
		return;
	}

	Time const relative_time = (frame * frc.factor() * TIME_HZ / _film->video_frame_rate());
	if (content->trimmed (relative_time)) {
		return;
	}

	Time const time = content->position() + relative_time + extra - content->trim_start ();
	libdcp::Size const image_size = content->scale().size (content, _video_container_size, _film->frame_size ());

	shared_ptr<PlayerVideoFrame> pi (
		new PlayerVideoFrame (
			image,
			content->crop(),
			image_size,
			_video_container_size,
			_film->scaler(),
			eyes,
			part,
			content->colour_conversion()
			)
		);
	
	if (_film->with_subtitles ()) {
		for (list<Subtitle>::const_iterator i = _subtitles.begin(); i != _subtitles.end(); ++i) {
			if (i->covers (time)) {
				/* This may be true for more than one of _subtitles, but the last (latest-starting)
				   one is the one we want to use, so that's ok.
				*/
				Position<int> const container_offset (
					(_video_container_size.width - image_size.width) / 2,
					(_video_container_size.height - image_size.width) / 2
					);
				
				pi->set_subtitle (i->out_image(), i->out_position() + container_offset);
			}
		}
	}

	/* Clear out old subtitles */
	for (list<Subtitle>::iterator i = _subtitles.begin(); i != _subtitles.end(); ) {
		list<Subtitle>::iterator j = i;
		++j;
		
		if (i->ends_before (time)) {
			_subtitles.erase (i);
		}

		i = j;
	}

#ifdef DCPOMATIC_DEBUG
	_last_video = piece->content;
#endif

	Video (pi, same, time);

	_last_emit_was_black = false;
	_video_position = piece->video_position = (time + TIME_HZ / _film->video_frame_rate());

	if (frc.repeat > 1 && !piece->repeating ()) {
		piece->set_repeat (_last_incoming_video, frc.repeat - 1);
	}
}

/** @param already_resampled true if this data has already been through the chain up to the resampler */
void
Player::process_audio (weak_ptr<Piece> weak_piece, shared_ptr<const AudioBuffers> audio, AudioContent::Frame frame, bool already_resampled)
{
	shared_ptr<Piece> piece = weak_piece.lock ();
	if (!piece) {
		return;
	}

	shared_ptr<AudioContent> content = dynamic_pointer_cast<AudioContent> (piece->content);
	assert (content);

	if (!already_resampled) {
		/* Gain */
		if (content->audio_gain() != 0) {
			shared_ptr<AudioBuffers> gain (new AudioBuffers (audio));
			gain->apply_gain (content->audio_gain ());
			audio = gain;
		}
		
		/* Resample */
		if (content->content_audio_frame_rate() != content->output_audio_frame_rate()) {
			shared_ptr<Resampler> r = resampler (content, true);
			pair<shared_ptr<const AudioBuffers>, AudioContent::Frame> ro = r->run (audio, frame);
			audio = ro.first;
			frame = ro.second;
		}
	}
	
	Time const relative_time = _film->audio_frames_to_time (frame);

	if (content->trimmed (relative_time)) {
		return;
	}

	Time time = content->position() + (content->audio_delay() * TIME_HZ / 1000) + relative_time - content->trim_start ();
	
	/* Remap channels */
	shared_ptr<AudioBuffers> dcp_mapped (new AudioBuffers (_film->audio_channels(), audio->frames()));
	dcp_mapped->make_silent ();

	AudioMapping map = content->audio_mapping ();
	for (int i = 0; i < map.content_channels(); ++i) {
		for (int j = 0; j < _film->audio_channels(); ++j) {
			if (map.get (i, static_cast<libdcp::Channel> (j)) > 0) {
				dcp_mapped->accumulate_channel (
					audio.get(),
					i,
					static_cast<libdcp::Channel> (j),
					map.get (i, static_cast<libdcp::Channel> (j))
					);
			}
		}
	}

	audio = dcp_mapped;

	/* We must cut off anything that comes before the start of all time */
	if (time < 0) {
		int const frames = - time * _film->audio_frame_rate() / TIME_HZ;
		if (frames >= audio->frames ()) {
			return;
		}

		shared_ptr<AudioBuffers> trimmed (new AudioBuffers (audio->channels(), audio->frames() - frames));
		trimmed->copy_from (audio.get(), audio->frames() - frames, frames, 0);

		audio = trimmed;
		time = 0;
	}

	_audio_merger.push (audio, time);
	piece->audio_position += _film->audio_frames_to_time (audio->frames ());
}

void
Player::flush ()
{
	TimedAudioBuffers<Time> tb = _audio_merger.flush ();
	if (_audio && tb.audio) {
		Audio (tb.audio, tb.time);
		_audio_position += _film->audio_frames_to_time (tb.audio->frames ());
	}

	while (_video && _video_position < _audio_position) {
		emit_black ();
	}

	while (_audio && _audio_position < _video_position) {
		emit_silence (_film->time_to_audio_frames (_video_position - _audio_position));
	}
	
}

/** Seek so that the next pass() will yield (approximately) the requested frame.
 *  Pass accurate = true to try harder to get close to the request.
 *  @return true on error
 */
void
Player::seek (Time t, bool accurate)
{
	if (!_have_valid_pieces) {
		setup_pieces ();
	}

	if (_pieces.empty ()) {
		return;
	}

	for (list<shared_ptr<Piece> >::iterator i = _pieces.begin(); i != _pieces.end(); ++i) {
		shared_ptr<VideoContent> vc = dynamic_pointer_cast<VideoContent> ((*i)->content);
		if (!vc) {
			continue;
		}

		/* s is the offset of t from the start position of this content */
		Time s = t - vc->position ();
		s = max (static_cast<Time> (0), s);
		s = min (vc->length_after_trim(), s);

		/* Hence set the piece positions to the `global' time */
		(*i)->video_position = (*i)->audio_position = vc->position() + s;

		/* And seek the decoder */
		dynamic_pointer_cast<VideoDecoder>((*i)->decoder)->seek (
			vc->time_to_content_video_frames (s + vc->trim_start ()), accurate
			);

		(*i)->reset_repeat ();
	}

	_video_position = _audio_position = t;

	/* XXX: don't seek audio because we don't need to... */
}

void
Player::setup_pieces ()
{
	list<shared_ptr<Piece> > old_pieces = _pieces;

	_pieces.clear ();

	ContentList content = _playlist->content ();
	sort (content.begin(), content.end(), ContentSorter ());

	for (ContentList::iterator i = content.begin(); i != content.end(); ++i) {

		if (!(*i)->paths_valid ()) {
			continue;
		}

		shared_ptr<Piece> piece (new Piece (*i));

		/* XXX: into content? */

		shared_ptr<const FFmpegContent> fc = dynamic_pointer_cast<const FFmpegContent> (*i);
		if (fc) {
			shared_ptr<FFmpegDecoder> fd (new FFmpegDecoder (_film, fc, _video, _audio));
			
			fd->Video.connect (bind (&Player::process_video, this, weak_ptr<Piece> (piece), _1, _2, _3, _4, _5, 0));
			fd->Audio.connect (bind (&Player::process_audio, this, weak_ptr<Piece> (piece), _1, _2, false));
			fd->Subtitle.connect (bind (&Player::process_subtitle, this, weak_ptr<Piece> (piece), _1, _2, _3, _4));

			fd->seek (fc->time_to_content_video_frames (fc->trim_start ()), true);
			piece->decoder = fd;
		}
		
		shared_ptr<const ImageContent> ic = dynamic_pointer_cast<const ImageContent> (*i);
		if (ic) {
			bool reusing = false;
			
			/* See if we can re-use an old ImageDecoder */
			for (list<shared_ptr<Piece> >::const_iterator j = old_pieces.begin(); j != old_pieces.end(); ++j) {
				shared_ptr<ImageDecoder> imd = dynamic_pointer_cast<ImageDecoder> ((*j)->decoder);
				if (imd && imd->content() == ic) {
					piece = *j;
					reusing = true;
				}
			}

			if (!reusing) {
				shared_ptr<ImageDecoder> id (new ImageDecoder (_film, ic));
				id->Video.connect (bind (&Player::process_video, this, weak_ptr<Piece> (piece), _1, _2, _3, _4, _5, 0));
				piece->decoder = id;
			}
		}

		shared_ptr<const SndfileContent> sc = dynamic_pointer_cast<const SndfileContent> (*i);
		if (sc) {
			shared_ptr<AudioDecoder> sd (new SndfileDecoder (_film, sc));
			sd->Audio.connect (bind (&Player::process_audio, this, weak_ptr<Piece> (piece), _1, _2, false));

			piece->decoder = sd;
		}

		_pieces.push_back (piece);
	}

	_have_valid_pieces = true;
}

void
Player::content_changed (weak_ptr<Content> w, int property, bool frequent)
{
	shared_ptr<Content> c = w.lock ();
	if (!c) {
		return;
	}

	if (
		property == ContentProperty::POSITION || property == ContentProperty::LENGTH ||
		property == ContentProperty::TRIM_START || property == ContentProperty::TRIM_END ||
		property == VideoContentProperty::VIDEO_FRAME_TYPE 
		) {
		
		_have_valid_pieces = false;
		Changed (frequent);

	} else if (
		property == SubtitleContentProperty::SUBTITLE_X_OFFSET ||
		property == SubtitleContentProperty::SUBTITLE_Y_OFFSET ||
		property == SubtitleContentProperty::SUBTITLE_SCALE
		) {

		for (list<Subtitle>::iterator i = _subtitles.begin(); i != _subtitles.end(); ++i) {
			i->update (_film, _video_container_size);
		}
		
		Changed (frequent);

	} else if (
		property == VideoContentProperty::VIDEO_CROP || property == VideoContentProperty::VIDEO_SCALE ||
		property == VideoContentProperty::VIDEO_FRAME_RATE
		) {
		
		Changed (frequent);

	} else if (property == ContentProperty::PATH) {

		_have_valid_pieces = false;
		Changed (frequent);
	}
}

void
Player::playlist_changed ()
{
	_have_valid_pieces = false;
	Changed (false);
}

void
Player::set_video_container_size (libdcp::Size s)
{
	_video_container_size = s;

	shared_ptr<Image> im (new Image (PIX_FMT_RGB24, _video_container_size, true));
	im->make_black ();
	
	_black_frame.reset (
		new PlayerVideoFrame (
			shared_ptr<ImageProxy> (new RawImageProxy (im, _film->log ())),
			Crop(),
			_video_container_size,
			_video_container_size,
			Scaler::from_id ("bicubic"),
			EYES_BOTH,
			PART_WHOLE,
			ColourConversion ()
			)
		);
}

shared_ptr<Resampler>
Player::resampler (shared_ptr<AudioContent> c, bool create)
{
	map<shared_ptr<AudioContent>, shared_ptr<Resampler> >::iterator i = _resamplers.find (c);
	if (i != _resamplers.end ()) {
		return i->second;
	}

	if (!create) {
		return shared_ptr<Resampler> ();
	}

	LOG_GENERAL (
		"Creating new resampler for %1 to %2 with %3 channels", c->content_audio_frame_rate(), c->output_audio_frame_rate(), c->audio_channels()
		);

	shared_ptr<Resampler> r (new Resampler (c->content_audio_frame_rate(), c->output_audio_frame_rate(), c->audio_channels()));
	_resamplers[c] = r;
	return r;
}

void
Player::emit_black ()
{
#ifdef DCPOMATIC_DEBUG
	_last_video.reset ();
#endif

	Video (_black_frame, _last_emit_was_black, _video_position);
	_video_position += _film->video_frames_to_time (1);
	_last_emit_was_black = true;
}

void
Player::emit_silence (OutputAudioFrame most)
{
	if (most == 0) {
		return;
	}
	
	OutputAudioFrame N = min (most, _film->audio_frame_rate() / 2);
	shared_ptr<AudioBuffers> silence (new AudioBuffers (_film->audio_channels(), N));
	silence->make_silent ();
	Audio (silence, _audio_position);
	_audio_position += _film->audio_frames_to_time (N);
}

void
Player::film_changed (Film::Property p)
{
	/* Here we should notice Film properties that affect our output, and
	   alert listeners that our output now would be different to how it was
	   last time we were run.
	*/

	if (p == Film::SCALER || p == Film::WITH_SUBTITLES || p == Film::CONTAINER || p == Film::VIDEO_FRAME_RATE) {
		Changed (false);
	}
}

void
Player::process_subtitle (weak_ptr<Piece> weak_piece, shared_ptr<Image> image, dcpomatic::Rect<double> rect, Time from, Time to)
{
	if (!image) {
		/* A null image means that we should stop any current subtitles at `from' */
		for (list<Subtitle>::iterator i = _subtitles.begin(); i != _subtitles.end(); ++i) {
			i->set_stop (from);
		}
	} else {
		_subtitles.push_back (Subtitle (_film, _video_container_size, weak_piece, image, rect, from, to));
	}
}

/** Re-emit the last frame that was emitted, using current settings for crop, ratio, scaler and subtitles.
 *  @return false if this could not be done.
 */
bool
Player::repeat_last_video ()
{
	if (!_last_incoming_video.image || !_have_valid_pieces) {
		return false;
	}

	process_video (
		_last_incoming_video.weak_piece,
		_last_incoming_video.image,
		_last_incoming_video.eyes,
		_last_incoming_video.part,
		_last_incoming_video.same,
		_last_incoming_video.frame,
		_last_incoming_video.extra
		);

	return true;
}
