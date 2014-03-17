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
#include <algorithm>
#include "player.h"
#include "film.h"
#include "ffmpeg_decoder.h"
#include "audio_buffers.h"
#include "ffmpeg_content.h"
#include "image_decoder.h"
#include "image_content.h"
#include "sndfile_decoder.h"
#include "sndfile_content.h"
#include "subtitle_content.h"
#include "subrip_decoder.h"
#include "subrip_content.h"
#include "playlist.h"
#include "job.h"
#include "image.h"
#include "ratio.h"
#include "log.h"
#include "scaler.h"
#include "render_subtitles.h"

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
using boost::optional;

class Piece
{
public:
	Piece (shared_ptr<Content> c, shared_ptr<Decoder> d, FrameRateChange f)
		: content (c)
		, decoder (d)
		, frc (f)
	{}

	shared_ptr<Content> content;
	shared_ptr<Decoder> decoder;
	FrameRateChange frc;
};

Player::Player (shared_ptr<const Film> f, shared_ptr<const Playlist> p)
	: _film (f)
	, _playlist (p)
	, _video (true)
	, _audio (true)
	, _have_valid_pieces (false)
	, _video_position (0)
	, _audio_position (0)
	, _audio_merger (f->audio_channels(), f->audio_frame_rate ())
	, _last_emit_was_black (false)
	, _just_did_inaccurate_seek (false)
	, _approximate_size (false)
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

	/* Interrogate all our pieces to find the one with the earliest decoded data */

	shared_ptr<Piece> earliest_piece;
	shared_ptr<Decoded> earliest_decoded;
	DCPTime earliest_time = DCPTime::max ();
	DCPTime earliest_audio = DCPTime::max ();

	for (list<shared_ptr<Piece> >::iterator i = _pieces.begin(); i != _pieces.end(); ++i) {

		DCPTime const offset = (*i)->content->position() - (*i)->content->trim_start();
		
		bool done = false;
		shared_ptr<Decoded> dec;
		while (!done) {
			dec = (*i)->decoder->peek ();
			if (!dec) {
				/* Decoder has nothing else to give us */
				break;
			}

			dec->set_dcp_times ((*i)->frc, offset);
			DCPTime const t = dec->dcp_time - offset;
			if (t >= ((*i)->content->full_length() - (*i)->content->trim_end ())) {
				/* In the end-trimmed part; decoder has nothing else to give us */
				dec.reset ();
				done = true;
			} else if (t >= (*i)->content->trim_start ()) {
				/* Within the un-trimmed part; everything's ok */
				done = true;
			} else {
				/* Within the start-trimmed part; get something else */
				(*i)->decoder->consume ();
			}
		}

		if (!dec) {
			continue;
		}

		if (dec->dcp_time < earliest_time) {
			earliest_piece = *i;
			earliest_decoded = dec;
			earliest_time = dec->dcp_time;
		}

		if (dynamic_pointer_cast<DecodedAudio> (dec) && dec->dcp_time < earliest_audio) {
			earliest_audio = dec->dcp_time;
		}
	}
		
	if (!earliest_piece) {
		flush ();
		return true;
	}

	if (earliest_audio != DCPTime::max ()) {
		if (earliest_audio.get() < 0) {
			earliest_audio = DCPTime ();
		}
		TimedAudioBuffers tb = _audio_merger.pull (earliest_audio);
		Audio (tb.audio, tb.time);
		/* This assumes that the audio-frames-to-time conversion is exact
		   so that there are no accumulated errors caused by rounding.
		*/
		_audio_position += DCPTime::from_frames (tb.audio->frames(), _film->audio_frame_rate ());
	}

	/* Emit the earliest thing */

	shared_ptr<DecodedVideo> dv = dynamic_pointer_cast<DecodedVideo> (earliest_decoded);
	shared_ptr<DecodedAudio> da = dynamic_pointer_cast<DecodedAudio> (earliest_decoded);
	shared_ptr<DecodedImageSubtitle> dis = dynamic_pointer_cast<DecodedImageSubtitle> (earliest_decoded);
	shared_ptr<DecodedTextSubtitle> dts = dynamic_pointer_cast<DecodedTextSubtitle> (earliest_decoded);

	/* Will be set to false if we shouldn't consume the peeked DecodedThing */
	bool consume = true;

	if (dv && _video) {

		if (_just_did_inaccurate_seek) {

			/* Just emit; no subtlety */
			emit_video (earliest_piece, dv);
			step_video_position (dv);
			
		} else if (dv->dcp_time > _video_position) {

			/* Too far ahead */

			list<shared_ptr<Piece> >::iterator i = _pieces.begin();
			while (i != _pieces.end() && ((*i)->content->position() >= _video_position || _video_position >= (*i)->content->end())) {
				++i;
			}

			if (i == _pieces.end() || !_last_incoming_video.video || !_have_valid_pieces) {
				/* We're outside all video content */
				emit_black ();
				_statistics.video.black++;
			} else {
				/* We're inside some video; repeat the frame */
				_last_incoming_video.video->dcp_time = _video_position;
				emit_video (_last_incoming_video.weak_piece, _last_incoming_video.video);
				step_video_position (_last_incoming_video.video);
				_statistics.video.repeat++;
			}

			consume = false;

		} else if (dv->dcp_time == _video_position) {
			/* We're ok */
			emit_video (earliest_piece, dv);
			step_video_position (dv);
			_statistics.video.good++;
		} else {
			/* Too far behind: skip */
			_statistics.video.skip++;
		}

		_just_did_inaccurate_seek = false;

	} else if (da && _audio) {

		if (da->dcp_time > _audio_position) {
			/* Too far ahead */
			emit_silence (da->dcp_time - _audio_position);
			consume = false;
			_statistics.audio.silence += (da->dcp_time - _audio_position);
		} else if (da->dcp_time == _audio_position) {
			/* We're ok */
			emit_audio (earliest_piece, da);
			_statistics.audio.good += da->data->frames();
		} else {
			/* Too far behind: skip */
			_statistics.audio.skip += da->data->frames();
		}
		
	} else if (dis && _video) {
		_image_subtitle.piece = earliest_piece;
		_image_subtitle.subtitle = dis;
		update_subtitle_from_image ();
	} else if (dts && _video) {
		_text_subtitle.piece = earliest_piece;
		_text_subtitle.subtitle = dts;
		update_subtitle_from_text ();
	}

	if (consume) {
		earliest_piece->decoder->consume ();
	}			
	
	return false;
}

void
Player::emit_video (weak_ptr<Piece> weak_piece, shared_ptr<DecodedVideo> video)
{
	/* Keep a note of what came in so that we can repeat it if required */
	_last_incoming_video.weak_piece = weak_piece;
	_last_incoming_video.video = video;
	
	shared_ptr<Piece> piece = weak_piece.lock ();
	if (!piece) {
		return;
	}

	shared_ptr<VideoContent> content = dynamic_pointer_cast<VideoContent> (piece->content);
	assert (content);

	FrameRateChange frc (content->video_frame_rate(), _film->video_frame_rate());

	dcp::Size image_size = content->scale().size (content, _video_container_size);
	if (_approximate_size) {
		image_size.width &= ~3;
		image_size.height &= ~3;
	}

	shared_ptr<PlayerImage> pi (
		new PlayerImage (
			video->image,
			content->crop(),
			image_size,
			_video_container_size,
			_film->scaler()
			)
		);
	
	if (
		_film->with_subtitles () &&
		_out_subtitle.image &&
		video->dcp_time >= _out_subtitle.from && video->dcp_time <= _out_subtitle.to
		) {

		Position<int> const container_offset (
			(_video_container_size.width - image_size.width) / 2,
			(_video_container_size.height - image_size.height) / 2
			);

		pi->set_subtitle (_out_subtitle.image, _out_subtitle.position + container_offset);
	}
		
					    
#ifdef DCPOMATIC_DEBUG
	_last_video = piece->content;
#endif

	Video (pi, video->eyes, content->colour_conversion(), video->same, video->dcp_time);
	
	_last_emit_was_black = false;
}

void
Player::step_video_position (shared_ptr<DecodedVideo> video)
{
	/* This is a bit of a hack; don't update _video_position if EYES_RIGHT is on its way */
	if (video->eyes != EYES_LEFT) {
		/* This assumes that the video-frames-to-time conversion is exact
		   so that there are no accumulated errors caused by rounding.
		*/
		_video_position += DCPTime::from_frames (1, _film->video_frame_rate ());
	}
}

void
Player::emit_audio (weak_ptr<Piece> weak_piece, shared_ptr<DecodedAudio> audio)
{
	shared_ptr<Piece> piece = weak_piece.lock ();
	if (!piece) {
		return;
	}

	shared_ptr<AudioContent> content = dynamic_pointer_cast<AudioContent> (piece->content);
	assert (content);

	/* Gain */
	if (content->audio_gain() != 0) {
		shared_ptr<AudioBuffers> gain (new AudioBuffers (audio->data));
		gain->apply_gain (content->audio_gain ());
		audio->data = gain;
	}

	/* Remap channels */
	shared_ptr<AudioBuffers> dcp_mapped (new AudioBuffers (_film->audio_channels(), audio->data->frames()));
	dcp_mapped->make_silent ();
	AudioMapping map = content->audio_mapping ();
	for (int i = 0; i < map.content_channels(); ++i) {
		for (int j = 0; j < _film->audio_channels(); ++j) {
			if (map.get (i, static_cast<dcp::Channel> (j)) > 0) {
				dcp_mapped->accumulate_channel (
					audio->data.get(),
					i,
					static_cast<dcp::Channel> (j),
					map.get (i, static_cast<dcp::Channel> (j))
					);
			}
		}
	}

	audio->data = dcp_mapped;

	/* Delay */
	audio->dcp_time += DCPTime::from_seconds (content->audio_delay() / 1000.0);
	if (audio->dcp_time < DCPTime (0)) {
		int const frames = - audio->dcp_time.frames (_film->audio_frame_rate());
		if (frames >= audio->data->frames ()) {
			return;
		}

		shared_ptr<AudioBuffers> trimmed (new AudioBuffers (audio->data->channels(), audio->data->frames() - frames));
		trimmed->copy_from (audio->data.get(), audio->data->frames() - frames, frames, 0);

		audio->data = trimmed;
		audio->dcp_time = DCPTime ();
	}

	_audio_merger.push (audio->data, audio->dcp_time);
}

void
Player::flush ()
{
	TimedAudioBuffers tb = _audio_merger.flush ();
	if (_audio && tb.audio) {
		Audio (tb.audio, tb.time);
		_audio_position += DCPTime::from_frames (tb.audio->frames (), _film->audio_frame_rate ());
	}

	while (_video && _video_position < _audio_position) {
		emit_black ();
	}

	while (_audio && _audio_position < _video_position) {
		emit_silence (_video_position - _audio_position);
	}
}

/** Seek so that the next pass() will yield (approximately) the requested frame.
 *  Pass accurate = true to try harder to get close to the request.
 *  @return true on error
 */
void
Player::seek (DCPTime t, bool accurate)
{
	if (!_have_valid_pieces) {
		setup_pieces ();
	}

	if (_pieces.empty ()) {
		return;
	}

	for (list<shared_ptr<Piece> >::iterator i = _pieces.begin(); i != _pieces.end(); ++i) {
		/* s is the offset of t from the start position of this content */
		DCPTime s = t - (*i)->content->position ();
		s = max (static_cast<DCPTime> (0), s);
		s = min ((*i)->content->length_after_trim(), s);

		/* Convert this to the content time */
		ContentTime ct (s + (*i)->content->trim_start(), (*i)->frc);

		/* And seek the decoder */
		(*i)->decoder->seek (ct, accurate);
	}

	_video_position = t.round_up (_film->video_frame_rate());
	_audio_position = t.round_up (_film->audio_frame_rate());

	_audio_merger.clear (_audio_position);

	if (!accurate) {
		/* We just did an inaccurate seek, so it's likely that the next thing seen
		   out of pass() will be a fair distance from _{video,audio}_position.  Setting
		   this flag stops pass() from trying to fix that: we assume that if it
		   was an inaccurate seek then the caller does not care too much about
		   inserting black/silence to keep the time tidy.
		*/
		_just_did_inaccurate_seek = true;
	}
}

void
Player::setup_pieces ()
{
	list<shared_ptr<Piece> > old_pieces = _pieces;
	_pieces.clear ();

	ContentList content = _playlist->content ();

	for (ContentList::iterator i = content.begin(); i != content.end(); ++i) {

		if (!(*i)->paths_valid ()) {
			continue;
		}
		
		shared_ptr<Decoder> decoder;
		optional<FrameRateChange> frc;

		/* Work out a FrameRateChange for the best overlap video for this content, in case we need it below */
		DCPTime best_overlap_t;
		shared_ptr<VideoContent> best_overlap;
		for (ContentList::iterator j = content.begin(); j != content.end(); ++j) {
			shared_ptr<VideoContent> vc = dynamic_pointer_cast<VideoContent> (*j);
			if (!vc) {
				continue;
			}
			
			DCPTime const overlap = max (vc->position(), (*i)->position()) - min (vc->end(), (*i)->end());
			if (overlap > best_overlap_t) {
				best_overlap = vc;
				best_overlap_t = overlap;
			}
		}

		optional<FrameRateChange> best_overlap_frc;
		if (best_overlap) {
			best_overlap_frc = FrameRateChange (best_overlap->video_frame_rate(), _film->video_frame_rate ());
		} else {
			/* No video overlap; e.g. if the DCP is just audio */
			best_overlap_frc = FrameRateChange (_film->video_frame_rate(), _film->video_frame_rate ());
		}

		/* FFmpeg */
		shared_ptr<const FFmpegContent> fc = dynamic_pointer_cast<const FFmpegContent> (*i);
		if (fc) {
			decoder.reset (new FFmpegDecoder (fc, _film->log(), _video, _audio, _film->with_subtitles ()));
			frc = FrameRateChange (fc->video_frame_rate(), _film->video_frame_rate());
		}

		/* ImageContent */
		shared_ptr<const ImageContent> ic = dynamic_pointer_cast<const ImageContent> (*i);
		if (ic) {
			/* See if we can re-use an old ImageDecoder */
			for (list<shared_ptr<Piece> >::const_iterator j = old_pieces.begin(); j != old_pieces.end(); ++j) {
				shared_ptr<ImageDecoder> imd = dynamic_pointer_cast<ImageDecoder> ((*j)->decoder);
				if (imd && imd->content() == ic) {
					decoder = imd;
				}
			}

			if (!decoder) {
				decoder.reset (new ImageDecoder (ic));
			}

			frc = FrameRateChange (ic->video_frame_rate(), _film->video_frame_rate());
		}

		/* SndfileContent */
		shared_ptr<const SndfileContent> sc = dynamic_pointer_cast<const SndfileContent> (*i);
		if (sc) {
			decoder.reset (new SndfileDecoder (sc));
			frc = best_overlap_frc;
		}

		/* SubRipContent */
		shared_ptr<const SubRipContent> rc = dynamic_pointer_cast<const SubRipContent> (*i);
		if (rc) {
			decoder.reset (new SubRipDecoder (rc));
			frc = best_overlap_frc;
		}

		ContentTime st ((*i)->trim_start(), frc.get ());
		decoder->seek (st, true);
		
		_pieces.push_back (shared_ptr<Piece> (new Piece (*i, decoder, frc.get ())));
	}

	_have_valid_pieces = true;

	/* The Piece for the _last_incoming_video will no longer be valid */
	_last_incoming_video.video.reset ();

	_video_position = DCPTime ();
	_audio_position = DCPTime ();
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

		update_subtitle_from_image ();
		update_subtitle_from_text ();
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
Player::set_video_container_size (dcp::Size s)
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

void
Player::emit_black ()
{
#ifdef DCPOMATIC_DEBUG
	_last_video.reset ();
#endif

	Video (_black_frame, EYES_BOTH, ColourConversion(), _last_emit_was_black, _video_position);
	_video_position += DCPTime::from_frames (1, _film->video_frame_rate ());
	_last_emit_was_black = true;
}

void
Player::emit_silence (DCPTime most)
{
	if (most == DCPTime ()) {
		return;
	}
	
	DCPTime t = min (most, DCPTime::from_seconds (0.5));
	shared_ptr<AudioBuffers> silence (new AudioBuffers (_film->audio_channels(), t.frames (_film->audio_frame_rate())));
	silence->make_silent ();
	Audio (silence, _audio_position);
	
	_audio_position += t;
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
Player::update_subtitle_from_image ()
{
	shared_ptr<Piece> piece = _image_subtitle.piece.lock ();
	if (!piece) {
		return;
	}

	if (!_image_subtitle.subtitle->image) {
		_out_subtitle.image.reset ();
		return;
	}

	shared_ptr<SubtitleContent> sc = dynamic_pointer_cast<SubtitleContent> (piece->content);
	assert (sc);

	dcpomatic::Rect<double> in_rect = _image_subtitle.subtitle->rect;
	dcp::Size scaled_size;

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
	
	_out_subtitle.image = _image_subtitle.subtitle->image->scale (
		scaled_size,
		Scaler::from_id ("bicubic"),
		_image_subtitle.subtitle->image->pixel_format (),
		true
		);
	
	_out_subtitle.from = _image_subtitle.subtitle->dcp_time + piece->content->position ();
	_out_subtitle.to = _image_subtitle.subtitle->dcp_time_to + piece->content->position ();
}

/** Re-emit the last frame that was emitted, using current settings for crop, ratio, scaler and subtitles.
 *  @return false if this could not be done.
 */
bool
Player::repeat_last_video ()
{
	if (!_last_incoming_video.video || !_have_valid_pieces) {
		return false;
	}

	emit_video (
		_last_incoming_video.weak_piece,
		_last_incoming_video.video
		);

	return true;
}

void
Player::update_subtitle_from_text ()
{
	if (_text_subtitle.subtitle->subs.empty ()) {
		_out_subtitle.image.reset ();
		return;
	}

	render_subtitles (_text_subtitle.subtitle->subs, _video_container_size, _out_subtitle.image, _out_subtitle.position);
}

void
Player::set_approximate_size ()
{
	_approximate_size = true;
}
			      
PlayerImage::PlayerImage (
	shared_ptr<const Image> in,
	Crop crop,
	dcp::Size inter_size,
	dcp::Size out_size,
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
PlayerImage::image (AVPixelFormat format, bool aligned)
{
	shared_ptr<Image> out = _in->crop_scale_window (_crop, _inter_size, _out_size, _scaler, format, aligned);
	
	Position<int> const container_offset ((_out_size.width - _inter_size.width) / 2, (_out_size.height - _inter_size.width) / 2);

	if (_subtitle_image) {
		out->alpha_blend (_subtitle_image, _subtitle_position);
	}

	return out;
}

void
PlayerStatistics::dump (shared_ptr<Log> log) const
{
	log->log (String::compose ("Video: %1 good %2 skipped %3 black %4 repeat", video.good, video.skip, video.black, video.repeat));
	log->log (String::compose ("Audio: %1 good %2 skipped %3 silence", audio.good, audio.skip, audio.silence.seconds()));
}

PlayerStatistics const &
Player::statistics () const
{
	return _statistics;
}
