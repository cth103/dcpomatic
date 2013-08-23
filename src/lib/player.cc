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
#include "still_image_decoder.h"
#include "still_image_content.h"
#include "moving_image_decoder.h"
#include "moving_image_content.h"
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

//#define DEBUG_PLAYER 1

class Piece
{
public:
	Piece (shared_ptr<Content> c)
		: content (c)
		, video_position (c->position ())
		, audio_position (c->position ())
	{}
	
	Piece (shared_ptr<Content> c, shared_ptr<Decoder> d)
		: content (c)
		, decoder (d)
		, video_position (c->position ())
		, audio_position (c->position ())
	{}
	
	shared_ptr<Content> content;
	shared_ptr<Decoder> decoder;
	Time video_position;
	Time audio_position;
};

#ifdef DEBUG_PLAYER
std::ostream& operator<<(std::ostream& s, Piece const & p)
{
	if (dynamic_pointer_cast<FFmpegContent> (p.content)) {
		s << "\tffmpeg	   ";
	} else if (dynamic_pointer_cast<StillImageContent> (p.content)) {
		s << "\tstill image";
	} else if (dynamic_pointer_cast<SndfileContent> (p.content)) {
		s << "\tsndfile	   ";
	}
	
	s << " at " << p.content->position() << " until " << p.content->end();
	
	return s;
}
#endif	

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
	set_video_container_size (_film->container()->size (_film->full_frame ()));
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
		_have_valid_pieces = true;
	}

#ifdef DEBUG_PLAYER
	cout << "= PASS\n";
#endif	

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

		if (_video && dynamic_pointer_cast<VideoDecoder> ((*i)->decoder)) {
			if ((*i)->video_position < earliest_t) {
				earliest_t = (*i)->video_position;
				earliest = *i;
				type = VIDEO;
			}
		}

		if (_audio && dynamic_pointer_cast<AudioDecoder> ((*i)->decoder)) {
			if ((*i)->audio_position < earliest_t) {
				earliest_t = (*i)->audio_position;
				earliest = *i;
				type = AUDIO;
			}
		}
	}

	if (!earliest) {
#ifdef DEBUG_PLAYER
		cout << "no earliest piece.\n";
#endif		
		
		flush ();
		return true;
	}

	switch (type) {
	case VIDEO:
		if (earliest_t > _video_position) {
#ifdef DEBUG_PLAYER
			cout << "no video here; emitting black frame (earliest=" << earliest_t << ", video_position=" << _video_position << ").\n";
#endif
			emit_black ();
		} else {
#ifdef DEBUG_PLAYER
			cout << "Pass video " << *earliest << "\n";
#endif			
			earliest->decoder->pass ();
		}
		break;

	case AUDIO:
		if (earliest_t > _audio_position) {
#ifdef DEBUG_PLAYER
			cout << "no audio here (none until " << earliest_t << "); emitting silence.\n";
#endif
			emit_silence (_film->time_to_audio_frames (earliest_t - _audio_position));
		} else {
#ifdef DEBUG_PLAYER
			cout << "Pass audio " << *earliest << "\n";
#endif
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
		Time audio_done_up_to = TIME_MAX;
		for (list<shared_ptr<Piece> >::iterator i = _pieces.begin(); i != _pieces.end(); ++i) {
			if (dynamic_pointer_cast<AudioDecoder> ((*i)->decoder)) {
				audio_done_up_to = min (audio_done_up_to, (*i)->audio_position);
			}
		}

		TimedAudioBuffers<Time> tb = _audio_merger.pull (audio_done_up_to);
		Audio (tb.audio, tb.time);
		_audio_position += _film->audio_frames_to_time (tb.audio->frames ());
	}
		
#ifdef DEBUG_PLAYER
	cout << "\tpost pass _video_position=" << _video_position << " _audio_position=" << _audio_position << "\n";
#endif	

	return false;
}

void
Player::process_video (weak_ptr<Piece> weak_piece, shared_ptr<const Image> image, Eyes eyes, bool same, VideoContent::Frame frame)
{
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

	/* Convert to RGB first, as FFmpeg doesn't seem to like handling YUV images with odd widths */
	shared_ptr<Image> work_image = image->scale (image->size (), _film->scaler(), PIX_FMT_RGB24, true);

	shared_ptr<Image> work_image = work_image->crop (content->crop(), true);

	libdcp::Size const image_size = content->ratio()->size (_video_container_size);
	
	work_image = work_image->scale (image_size, _film->scaler(), PIX_FMT_RGB24, true);

	Time time = content->position() + relative_time - content->trim_start ();
	    
	if (_film->with_subtitles () && _out_subtitle.image && time >= _out_subtitle.from && time <= _out_subtitle.to) {
		work_image->alpha_blend (_out_subtitle.image, _out_subtitle.position);
	}

	if (image_size != _video_container_size) {
		assert (image_size.width <= _video_container_size.width);
		assert (image_size.height <= _video_container_size.height);
		shared_ptr<Image> im (new Image (PIX_FMT_RGB24, _video_container_size, true));
		im->make_black ();
		im->copy (work_image, Position<int> ((_video_container_size.width - image_size.width) / 2, (_video_container_size.height - image_size.height) / 2));
		work_image = im;
	}

#ifdef DCPOMATIC_DEBUG
	_last_video = piece->content;
#endif

	Video (work_image, eyes, content->colour_conversion(), same, time);
	time += TIME_HZ / _film->video_frame_rate();

	if (frc.repeat) {
		Video (work_image, eyes, content->colour_conversion(), true, time);
		time += TIME_HZ / _film->video_frame_rate();
	}

	_last_emit_was_black = false;

	_video_position = piece->video_position = time;
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

	Time time = content->position() + (content->audio_delay() * TIME_HZ / 1000) + relative_time;
	
	/* Remap channels */
	shared_ptr<AudioBuffers> dcp_mapped (new AudioBuffers (_film->audio_channels(), audio->frames()));
	dcp_mapped->make_silent ();
	list<pair<int, libdcp::Channel> > map = content->audio_mapping().content_to_dcp ();
	for (list<pair<int, libdcp::Channel> >::iterator i = map.begin(); i != map.end(); ++i) {
		if (i->first < audio->channels() && i->second < dcp_mapped->channels()) {
			dcp_mapped->accumulate_channel (audio.get(), i->first, i->second);
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
	if (tb.audio) {
		Audio (tb.audio, tb.time);
		_audio_position += _film->audio_frames_to_time (tb.audio->frames ());
	}

	while (_video_position < _audio_position) {
		emit_black ();
	}

	while (_audio_position < _video_position) {
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
		_have_valid_pieces = true;
	}

	if (_pieces.empty ()) {
		return;
	}

	for (list<shared_ptr<Piece> >::iterator i = _pieces.begin(); i != _pieces.end(); ++i) {
		shared_ptr<VideoContent> vc = dynamic_pointer_cast<VideoContent> ((*i)->content);
		if (!vc) {
			continue;
		}
		
		Time s = t - vc->position ();
		s = max (static_cast<Time> (0), s);
		s = min (vc->length_after_trim(), s);

		(*i)->video_position = (*i)->audio_position = vc->position() + s;

		FrameRateConversion frc (vc->video_frame_rate(), _film->video_frame_rate());
		/* Here we are converting from time (in the DCP) to a frame number in the content.
		   Hence we need to use the DCP's frame rate and the double/skip correction, not
		   the source's rate.
		*/
		VideoContent::Frame f = (s + vc->trim_start ()) * _film->video_frame_rate() / (frc.factor() * TIME_HZ);
		dynamic_pointer_cast<VideoDecoder>((*i)->decoder)->seek (f, accurate);
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
			
			fd->Video.connect (bind (&Player::process_video, this, piece, _1, _2, _3, _4));
			fd->Audio.connect (bind (&Player::process_audio, this, piece, _1, _2));
			fd->Subtitle.connect (bind (&Player::process_subtitle, this, piece, _1, _2, _3, _4));

			piece->decoder = fd;
		}
		
		shared_ptr<const StillImageContent> ic = dynamic_pointer_cast<const StillImageContent> (*i);
		if (ic) {
			shared_ptr<StillImageDecoder> id;
			
			/* See if we can re-use an old StillImageDecoder */
			for (list<shared_ptr<Piece> >::const_iterator j = old_pieces.begin(); j != old_pieces.end(); ++j) {
				shared_ptr<StillImageDecoder> imd = dynamic_pointer_cast<StillImageDecoder> ((*j)->decoder);
				if (imd && imd->content() == ic) {
					id = imd;
				}
			}

			if (!id) {
				id.reset (new StillImageDecoder (_film, ic));
				id->Video.connect (bind (&Player::process_video, this, piece, _1, _2, _3, _4));
			}

			piece->decoder = id;
		}

		shared_ptr<const MovingImageContent> mc = dynamic_pointer_cast<const MovingImageContent> (*i);
		if (mc) {
			shared_ptr<MovingImageDecoder> md;

			if (!md) {
				md.reset (new MovingImageDecoder (_film, mc));
				md->Video.connect (bind (&Player::process_video, this, piece, _1, _2, _3, _4));
			}

			piece->decoder = md;
		}

		shared_ptr<const SndfileContent> sc = dynamic_pointer_cast<const SndfileContent> (*i);
		if (sc) {
			shared_ptr<AudioDecoder> sd (new SndfileDecoder (_film, sc));
			sd->Audio.connect (bind (&Player::process_audio, this, piece, _1, _2));

			piece->decoder = sd;
		}

		_pieces.push_back (piece);
	}

#ifdef DEBUG_PLAYER
	cout << "=== Player setup:\n";
	for (list<shared_ptr<Piece> >::iterator i = _pieces.begin(); i != _pieces.end(); ++i) {
		cout << *(i->get()) << "\n";
	}
#endif	
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
		property == VideoContentProperty::VIDEO_CROP || property == VideoContentProperty::VIDEO_RATIO
		) {
		
		_have_valid_pieces = false;
		Changed (frequent);

	} else if (property == SubtitleContentProperty::SUBTITLE_OFFSET || property == SubtitleContentProperty::SUBTITLE_SCALE) {
		update_subtitle ();
		Changed (frequent);
	} else if (property == VideoContentProperty::VIDEO_FRAME_TYPE) {
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
	_black_frame.reset (new Image (PIX_FMT_RGB24, _video_container_size, true));
	_black_frame->make_black ();
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

	if (p == Film::SCALER || p == Film::WITH_SUBTITLES || p == Film::CONTAINER) {
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

	in_rect.y += sc->subtitle_offset ();

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
	_out_subtitle.from = _in_subtitle.from + piece->content->position ();
	_out_subtitle.to = _in_subtitle.to + piece->content->position ();
}
