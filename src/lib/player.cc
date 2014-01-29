/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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
#include "ratio.h"
#include "resampler.h"
#include "log.h"
#include "scaler.h"

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

class Piece
{
public:
	Piece (shared_ptr<Content> c)
		: content (c)
		, video_position (c->position ())
		, audio_position (c->position ())
		, repeat_to_do (0)
		, repeat_done (0)
	{}
	
	Piece (shared_ptr<Content> c, shared_ptr<Decoder> d)
		: content (c)
		, decoder (d)
		, video_position (c->position ())
		, audio_position (c->position ())
	{}

	/** Set this piece to repeat a video frame a given number of times */
	void set_repeat (IncomingVideo video, int num)
	{
		repeat_video = video;
		repeat_to_do = num;
		repeat_done = 0;
	}

	void reset_repeat ()
	{
		repeat_video.image.reset ();
		repeat_to_do = 0;
		repeat_done = 0;
	}

	bool repeating () const
	{
		return repeat_done != repeat_to_do;
	}

	void repeat (Player* player)
	{
		player->process_video (
			repeat_video.weak_piece,
			repeat_video.image,
			repeat_video.eyes,
			repeat_done > 0,
			repeat_video.frame,
			(repeat_done + 1) * (TIME_HZ / player->_film->video_frame_rate ())
			);

		++repeat_done;
	}
	
	shared_ptr<Content> content;
	shared_ptr<Decoder> decoder;
	/** Time of the last video we emitted relative to the start of the DCP */
	Time video_position;
	/** Time of the last audio we emitted relative to the start of the DCP */
	Time audio_position;

	IncomingVideo repeat_video;
	int repeat_to_do;
	int repeat_done;
};

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
	set_video_container_size (fit_ratio_within (_film->container()->ratio (), _film->full_frame ()));
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
		if ((*i)->decoder->done ()) {
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
						process_audio (earliest, b, ac->audio_length ());
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
Player::process_video (weak_ptr<Piece> weak_piece, shared_ptr<const Image> image, Eyes eyes, bool same, VideoContent::Frame frame, Time extra)
{
	/* Keep a note of what came in so that we can repeat it if required */
	_last_incoming_video.weak_piece = weak_piece;
	_last_incoming_video.image = image;
	_last_incoming_video.eyes = eyes;
	_last_incoming_video.same = same;
	_last_incoming_video.frame = frame;
	_last_incoming_video.extra = extra;
	
	shared_ptr<Piece> piece = weak_piece.lock ();
	if (!piece) {
		return;
	}

	shared_ptr<VideoContent> content = dynamic_pointer_cast<VideoContent> (piece->content);
	assert (content);

	FrameRateConversion frc (content->video_frame_rate(), _film->video_frame_rate());
	if (frc.skip && (frame % 2) == 1) {
		return;
	}

	Time const relative_time = (frame * frc.factor() * TIME_HZ / _film->video_frame_rate());
	if (content->trimmed (relative_time)) {
		return;
	}

	Time const time = content->position() + relative_time + extra - content->trim_start ();
	float const ratio = content->ratio() ? content->ratio()->ratio() : content->video_size_after_crop().ratio();
	libdcp::Size const image_size = fit_ratio_within (ratio, _video_container_size);

	shared_ptr<PlayerImage> pi (
		new PlayerImage (
			image,
			content->crop(),
			image_size,
			_video_container_size,
			_film->scaler()
			)
		);
	
	if (_film->with_subtitles () && _out_subtitle.image && time >= _out_subtitle.from && time <= _out_subtitle.to) {

		Position<int> const container_offset (
			(_video_container_size.width - image_size.width) / 2,
			(_video_container_size.height - image_size.width) / 2
			);

		pi->set_subtitle (_out_subtitle.image, _out_subtitle.position + container_offset);
	}
		
					    
#ifdef DCPOMATIC_DEBUG
	_last_video = piece->content;
#endif

	Video (pi, eyes, content->colour_conversion(), same, time);

	_last_emit_was_black = false;
	_video_position = piece->video_position = (time + TIME_HZ / _film->video_frame_rate());

	if (frc.repeat > 1 && !piece->repeating ()) {
		piece->set_repeat (_last_incoming_video, frc.repeat - 1);
	}
}

void
Player::process_audio (weak_ptr<Piece> weak_piece, shared_ptr<const AudioBuffers> audio, AudioContent::Frame frame)
{
	shared_ptr<Piece> piece = weak_piece.lock ();
	if (!piece) {
		return;
	}

	shared_ptr<AudioContent> content = dynamic_pointer_cast<AudioContent> (piece->content);
	assert (content);

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

		shared_ptr<Piece> piece (new Piece (*i));

		/* XXX: into content? */

		shared_ptr<const FFmpegContent> fc = dynamic_pointer_cast<const FFmpegContent> (*i);
		if (fc) {
			shared_ptr<FFmpegDecoder> fd (new FFmpegDecoder (_film, fc, _video, _audio));
			
			fd->Video.connect (bind (&Player::process_video, this, weak_ptr<Piece> (piece), _1, _2, _3, _4, 0));
			fd->Audio.connect (bind (&Player::process_audio, this, weak_ptr<Piece> (piece), _1, _2));
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
				id->Video.connect (bind (&Player::process_video, this, weak_ptr<Piece> (piece), _1, _2, _3, _4, 0));
				piece->decoder = id;
			}
		}

		shared_ptr<const SndfileContent> sc = dynamic_pointer_cast<const SndfileContent> (*i);
		if (sc) {
			shared_ptr<AudioDecoder> sd (new SndfileDecoder (_film, sc));
			sd->Audio.connect (bind (&Player::process_audio, this, weak_ptr<Piece> (piece), _1, _2));

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

		update_subtitle ();
		Changed (frequent);

	} else if (
		property == VideoContentProperty::VIDEO_CROP || property == VideoContentProperty::VIDEO_RATIO ||
		property == VideoContentProperty::VIDEO_FRAME_RATE
		) {
		
		Changed (frequent);

	} else if (property == ContentProperty::PATH) {

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
		new PlayerImage (
			im,
			Crop(),
			_video_container_size,
			_video_container_size,
			Scaler::from_id ("bicubic")
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

	_film->log()->log (
		String::compose (
			"Creating new resampler for %1 to %2 with %3 channels", c->content_audio_frame_rate(), c->output_audio_frame_rate(), c->audio_channels()
			)
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

	Video (_black_frame, EYES_BOTH, ColourConversion(), _last_emit_was_black, _video_position);
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
	_in_subtitle.piece = weak_piece;
	_in_subtitle.image = image;
	_in_subtitle.rect = rect;
	_in_subtitle.from = from;
	_in_subtitle.to = to;

	update_subtitle ();
}

void
Player::update_subtitle ()
{
	shared_ptr<Piece> piece = _in_subtitle.piece.lock ();
	if (!piece) {
		return;
	}

	if (!_in_subtitle.image) {
		_out_subtitle.image.reset ();
		return;
	}

	shared_ptr<SubtitleContent> sc = dynamic_pointer_cast<SubtitleContent> (piece->content);
	assert (sc);

	dcpomatic::Rect<double> in_rect = _in_subtitle.rect;
	libdcp::Size scaled_size;

	in_rect.x += sc->subtitle_x_offset ();
	in_rect.y += sc->subtitle_y_offset ();

	/* We will scale the subtitle up to fit _video_container_size, and also by the additional subtitle_scale */
	scaled_size.width = in_rect.width * _video_container_size.width * sc->subtitle_scale ();
	scaled_size.height = in_rect.height * _video_container_size.height * sc->subtitle_scale ();

	/* Then we need a corrective translation, consisting of two parts:
	 *
	 * 1.  that which is the result of the scaling of the subtitle by _video_container_size; this will be
	 *     rect.x * _video_container_size.width and rect.y * _video_container_size.height.
	 *
	 * 2.  that to shift the origin of the scale by subtitle_scale to the centre of the subtitle; this will be
	 *     (width_before_subtitle_scale * (1 - subtitle_scale) / 2) and
	 *     (height_before_subtitle_scale * (1 - subtitle_scale) / 2).
	 *
	 * Combining these two translations gives these expressions.
	 */
	
	_out_subtitle.position.x = rint (_video_container_size.width * (in_rect.x + (in_rect.width * (1 - sc->subtitle_scale ()) / 2)));
	_out_subtitle.position.y = rint (_video_container_size.height * (in_rect.y + (in_rect.height * (1 - sc->subtitle_scale ()) / 2)));
	
	_out_subtitle.image = _in_subtitle.image->scale (
		scaled_size,
		Scaler::from_id ("bicubic"),
		_in_subtitle.image->pixel_format (),
		true
		);

	/* XXX: hack */
	Time from = _in_subtitle.from;
	Time to = _in_subtitle.to;
	shared_ptr<VideoContent> vc = dynamic_pointer_cast<VideoContent> (piece->content);
	if (vc) {
		from = rint (from * vc->video_frame_rate() / _film->video_frame_rate());
		to = rint (to * vc->video_frame_rate() / _film->video_frame_rate());
	}
	
	_out_subtitle.from = from + piece->content->position ();
	_out_subtitle.to = to + piece->content->position ();
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
		_last_incoming_video.same,
		_last_incoming_video.frame,
		_last_incoming_video.extra
		);

	return true;
}

PlayerImage::PlayerImage (
	shared_ptr<const Image> in,
	Crop crop,
	libdcp::Size inter_size,
	libdcp::Size out_size,
	Scaler const * scaler
	)
	: _in (in)
	, _crop (crop)
	, _inter_size (inter_size)
	, _out_size (out_size)
	, _scaler (scaler)
{

}

void
PlayerImage::set_subtitle (shared_ptr<const Image> image, Position<int> pos)
{
	_subtitle_image = image;
	_subtitle_position = pos;
}

shared_ptr<Image>
PlayerImage::image ()
{
	shared_ptr<Image> out = _in->crop_scale_window (_crop, _inter_size, _out_size, _scaler, PIX_FMT_RGB24, false);

	Position<int> const container_offset ((_out_size.width - _inter_size.width) / 2, (_out_size.height - _inter_size.width) / 2);

	if (_subtitle_image) {
		out->alpha_blend (_subtitle_image, _subtitle_position);
	}

	return out;
}
