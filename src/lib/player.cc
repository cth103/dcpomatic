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
#include "dcp_content.h"
#include "playlist.h"
#include "job.h"
#include "image.h"
#include "raw_image_proxy.h"
#include "ratio.h"
#include "log.h"
#include "scaler.h"
#include "render_subtitles.h"
#include "config.h"
#include "content_video.h"
#include "player_video.h"
#include "frame_rate_change.h"
#include "dcp_content.h"
#include "dcp_decoder.h"
#include "dcp_subtitle_content.h"
#include "dcp_subtitle_decoder.h"
#include <boost/foreach.hpp>
#include <stdint.h>
#include <algorithm>

#include "i18n.h"

#define LOG_GENERAL(...) _film->log()->log (String::compose (__VA_ARGS__), Log::TYPE_GENERAL);

using std::list;
using std::cout;
using std::min;
using std::max;
using std::min;
using std::vector;
using std::pair;
using std::map;
using std::make_pair;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;
using boost::optional;

Player::Player (shared_ptr<const Film> f, shared_ptr<const Playlist> p)
	: _film (f)
	, _playlist (p)
	, _have_valid_pieces (false)
	, _approximate_size (false)
{
	_playlist_changed_connection = _playlist->Changed.connect (bind (&Player::playlist_changed, this));
	_playlist_content_changed_connection = _playlist->ContentChanged.connect (bind (&Player::content_changed, this, _1, _2, _3));
	_film_changed_connection = _film->Changed.connect (bind (&Player::film_changed, this, _1));
	set_video_container_size (_film->frame_size ());
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
			decoder.reset (new FFmpegDecoder (fc, _film->log()));
			frc = FrameRateChange (fc->video_frame_rate(), _film->video_frame_rate());
		}

		shared_ptr<const DCPContent> dc = dynamic_pointer_cast<const DCPContent> (*i);
		if (dc) {
			decoder.reset (new DCPDecoder (dc));
			frc = FrameRateChange (dc->video_frame_rate(), _film->video_frame_rate());
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

		/* DCPSubtitleContent */
		shared_ptr<const DCPSubtitleContent> dsc = dynamic_pointer_cast<const DCPSubtitleContent> (*i);
		if (dsc) {
			decoder.reset (new DCPSubtitleDecoder (dsc));
			frc = best_overlap_frc;
		}

		_pieces.push_back (shared_ptr<Piece> (new Piece (*i, decoder, frc.get ())));
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
		property == ContentProperty::POSITION ||
		property == ContentProperty::LENGTH ||
		property == ContentProperty::TRIM_START ||
		property == ContentProperty::TRIM_END ||
		property == ContentProperty::PATH ||
		property == VideoContentProperty::VIDEO_FRAME_TYPE ||
		property == DCPContentProperty::CAN_BE_PLAYED
		) {
		
		_have_valid_pieces = false;
		Changed (frequent);

	} else if (
		property == SubtitleContentProperty::USE_SUBTITLES ||
		property == SubtitleContentProperty::SUBTITLE_X_OFFSET ||
		property == SubtitleContentProperty::SUBTITLE_Y_OFFSET ||
		property == SubtitleContentProperty::SUBTITLE_X_SCALE ||
		property == SubtitleContentProperty::SUBTITLE_Y_SCALE ||
		property == VideoContentProperty::VIDEO_CROP ||
		property == VideoContentProperty::VIDEO_SCALE ||
		property == VideoContentProperty::VIDEO_FRAME_RATE
		) {
		
		Changed (frequent);
	}
}

/** @param already_resampled true if this data has already been through the chain up to the resampler */
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

	_black_image.reset (new Image (PIX_FMT_RGB24, _video_container_size, true));
	_black_image->make_black ();
}

void
Player::film_changed (Film::Property p)
{
	/* Here we should notice Film properties that affect our output, and
	   alert listeners that our output now would be different to how it was
	   last time we were run.
	*/

	if (p == Film::SCALER || p == Film::CONTAINER || p == Film::VIDEO_FRAME_RATE) {
		Changed (false);
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
					Scaler::from_id ("bicubic"),
					i->image->pixel_format (),
					true
					),
				Position<int> (
					rint (_video_container_size.width * i->rectangle.x),
					rint (_video_container_size.height * i->rectangle.y)
					)
				)
			);
	}

	return all;
}

void
Player::set_approximate_size ()
{
	_approximate_size = true;
}

shared_ptr<PlayerVideo>
Player::black_player_video_frame (DCPTime time) const
{
	return shared_ptr<PlayerVideo> (
		new PlayerVideo (
			shared_ptr<const ImageProxy> (new RawImageProxy (_black_image)),
			time,
			Crop (),
			optional<float> (),
			_video_container_size,
			_video_container_size,
			Scaler::from_id ("bicubic"),
			EYES_BOTH,
			PART_WHOLE,
			Config::instance()->colour_conversions().front().conversion
		)
	);
}

/** @return All PlayerVideos at the given time (there may be two frames for 3D) */
list<shared_ptr<PlayerVideo> >
Player::get_video (DCPTime time, bool accurate)
{
	if (!_have_valid_pieces) {
		setup_pieces ();
	}
	
	list<shared_ptr<Piece> > ov = overlaps<VideoContent> (
		time,
		time + DCPTime::from_frames (1, _film->video_frame_rate ())
		);

	list<shared_ptr<PlayerVideo> > pvf;

	if (ov.empty ()) {
		/* No video content at this time */
		pvf.push_back (black_player_video_frame (time));
	} else {
		/* Create a PlayerVideo from the content's video at this time */

		shared_ptr<Piece> piece = ov.back ();
		shared_ptr<VideoDecoder> decoder = dynamic_pointer_cast<VideoDecoder> (piece->decoder);
		assert (decoder);
		shared_ptr<VideoContent> content = dynamic_pointer_cast<VideoContent> (piece->content);
		assert (content);

		list<ContentVideo> content_video = decoder->get_video (dcp_to_content_video (piece, time), accurate);
		if (content_video.empty ()) {
			pvf.push_back (black_player_video_frame (time));
			return pvf;
		}
		
		dcp::Size image_size = content->scale().size (content, _video_container_size, _film->frame_size (), _approximate_size ? 4 : 1);
		if (_approximate_size) {
			image_size.width &= ~3;
			image_size.height &= ~3;
		}
		
		for (list<ContentVideo>::const_iterator i = content_video.begin(); i != content_video.end(); ++i) {
			pvf.push_back (
				shared_ptr<PlayerVideo> (
					new PlayerVideo (
						i->image,
						content_video_to_dcp (piece, i->frame),
						content->crop (),
						content->fade (i->frame),
						image_size,
						_video_container_size,
						_film->scaler(),
						i->eyes,
						i->part,
						content->colour_conversion ()
						)
					)
				);
		}
	}

	/* Add subtitles (for possible burn-in) to whatever PlayerVideos we got */

	PlayerSubtitles ps = get_subtitles (time, DCPTime::from_frames (1, _film->video_frame_rate ()), false);

	list<PositionImage> sub_images;

	/* Image subtitles */
	list<PositionImage> c = transform_image_subtitles (ps.image);
	copy (c.begin(), c.end(), back_inserter (sub_images));

	/* Text subtitles (rendered to images) */
	sub_images.push_back (render_subtitles (ps.text, _video_container_size));
	
	if (!sub_images.empty ()) {
		for (list<shared_ptr<PlayerVideo> >::const_iterator i = pvf.begin(); i != pvf.end(); ++i) {
			(*i)->set_subtitle (merge (sub_images));
		}
	}	
		
	return pvf;
}

shared_ptr<AudioBuffers>
Player::get_audio (DCPTime time, DCPTime length, bool accurate)
{
	if (!_have_valid_pieces) {
		setup_pieces ();
	}

	AudioFrame const length_frames = length.frames (_film->audio_frame_rate ());

	shared_ptr<AudioBuffers> audio (new AudioBuffers (_film->audio_channels(), length_frames));
	audio->make_silent ();
	
	list<shared_ptr<Piece> > ov = overlaps<AudioContent> (time, time + length);
	if (ov.empty ()) {
		return audio;
	}

	for (list<shared_ptr<Piece> >::iterator i = ov.begin(); i != ov.end(); ++i) {

		shared_ptr<AudioContent> content = dynamic_pointer_cast<AudioContent> ((*i)->content);
		assert (content);
		shared_ptr<AudioDecoder> decoder = dynamic_pointer_cast<AudioDecoder> ((*i)->decoder);
		assert (decoder);

		if (content->audio_frame_rate() == 0) {
			/* This AudioContent has no audio (e.g. if it is an FFmpegContent with no
			 * audio stream).
			 */
			continue;
		}

		/* The time that we should request from the content */
		DCPTime request = time - DCPTime::from_seconds (content->audio_delay() / 1000.0);
		DCPTime offset;
		if (request < DCPTime ()) {
			/* We went off the start of the content, so we will need to offset
			   the stuff we get back.
			*/
			offset = -request;
			request = DCPTime ();
		}

		AudioFrame const content_frame = dcp_to_content_audio (*i, request);

		/* Audio from this piece's decoder (which might be more or less than what we asked for) */
		shared_ptr<ContentAudio> all = decoder->get_audio (content_frame, length_frames, accurate);

		/* Gain */
		if (content->audio_gain() != 0) {
			shared_ptr<AudioBuffers> gain (new AudioBuffers (all->audio));
			gain->apply_gain (content->audio_gain ());
			all->audio = gain;
		}

		/* Remap channels */
		shared_ptr<AudioBuffers> dcp_mapped (new AudioBuffers (_film->audio_channels(), all->audio->frames()));
		dcp_mapped->make_silent ();
		AudioMapping map = content->audio_mapping ();
		for (int i = 0; i < map.content_channels(); ++i) {
			for (int j = 0; j < _film->audio_channels(); ++j) {
				if (map.get (i, static_cast<dcp::Channel> (j)) > 0) {
					dcp_mapped->accumulate_channel (
						all->audio.get(),
						i,
						j,
						map.get (i, static_cast<dcp::Channel> (j))
						);
				}
			}
		}
		
		all->audio = dcp_mapped;

		audio->accumulate_frames (
			all->audio.get(),
			content_frame - all->frame,
			offset.frames (_film->audio_frame_rate()),
			min (AudioFrame (all->audio->frames()), length_frames) - offset.frames (_film->audio_frame_rate ())
			);
	}

	return audio;
}

VideoFrame
Player::dcp_to_content_video (shared_ptr<const Piece> piece, DCPTime t) const
{
	/* s is the offset of t from the start position of this content */
	DCPTime s = t - piece->content->position ();
	s = DCPTime (max (DCPTime::Type (0), s.get ()));
	s = DCPTime (min (piece->content->length_after_trim().get(), s.get()));

	/* Convert this to the content frame */
	return DCPTime (s + piece->content->trim_start()).frames (_film->video_frame_rate()) * piece->frc.factor ();
}

DCPTime
Player::content_video_to_dcp (shared_ptr<const Piece> piece, VideoFrame f) const
{
	DCPTime t = DCPTime::from_frames (f / piece->frc.factor (), _film->video_frame_rate()) - piece->content->trim_start () + piece->content->position ();
	if (t < DCPTime ()) {
		t = DCPTime ();
	}

	return t;
}

AudioFrame
Player::dcp_to_content_audio (shared_ptr<const Piece> piece, DCPTime t) const
{
	/* s is the offset of t from the start position of this content */
	DCPTime s = t - piece->content->position ();
	s = DCPTime (max (DCPTime::Type (0), s.get ()));
	s = DCPTime (min (piece->content->length_after_trim().get(), s.get()));

	/* Convert this to the content frame */
	return DCPTime (s + piece->content->trim_start()).frames (_film->audio_frame_rate());
}

ContentTime
Player::dcp_to_content_subtitle (shared_ptr<const Piece> piece, DCPTime t) const
{
	/* s is the offset of t from the start position of this content */
	DCPTime s = t - piece->content->position ();
	s = DCPTime (max (DCPTime::Type (0), s.get ()));
	s = DCPTime (min (piece->content->length_after_trim().get(), s.get()));

	return ContentTime (s + piece->content->trim_start(), piece->frc);
}

void
PlayerStatistics::dump (shared_ptr<Log> log) const
{
	log->log (String::compose ("Video: %1 good %2 skipped %3 black %4 repeat", video.good, video.skip, video.black, video.repeat), Log::TYPE_GENERAL);
	log->log (String::compose ("Audio: %1 good %2 skipped %3 silence", audio.good, audio.skip, audio.silence.seconds()), Log::TYPE_GENERAL);
}

PlayerStatistics const &
Player::statistics () const
{
	return _statistics;
}

PlayerSubtitles
Player::get_subtitles (DCPTime time, DCPTime length, bool starting)
{
	list<shared_ptr<Piece> > subs = overlaps<SubtitleContent> (time, time + length);

	PlayerSubtitles ps (time, length);

	for (list<shared_ptr<Piece> >::const_iterator j = subs.begin(); j != subs.end(); ++j) {
		shared_ptr<SubtitleContent> subtitle_content = dynamic_pointer_cast<SubtitleContent> ((*j)->content);
		if (!subtitle_content->use_subtitles ()) {
			continue;
		}

		/* XXX: this will break down if we have multiple subtitle content */
		ps.language = subtitle_content->subtitle_language();
		if (ps.language.empty ()) {
			ps.language = _("Unknown");
		}

		shared_ptr<SubtitleDecoder> subtitle_decoder = dynamic_pointer_cast<SubtitleDecoder> ((*j)->decoder);
		ContentTime const from = dcp_to_content_subtitle (*j, time);
		/* XXX: this video_frame_rate() should be the rate that the subtitle content has been prepared for */
		ContentTime const to = from + ContentTime::from_frames (1, _film->video_frame_rate ());

		list<ContentImageSubtitle> image = subtitle_decoder->get_image_subtitles (ContentTimePeriod (from, to), starting);
		for (list<ContentImageSubtitle>::iterator i = image.begin(); i != image.end(); ++i) {
			
			/* Apply content's subtitle offsets */
			i->sub.rectangle.x += subtitle_content->subtitle_x_offset ();
			i->sub.rectangle.y += subtitle_content->subtitle_y_offset ();

			/* Apply content's subtitle scale */
			i->sub.rectangle.width *= subtitle_content->subtitle_x_scale ();
			i->sub.rectangle.height *= subtitle_content->subtitle_y_scale ();

			/* Apply a corrective translation to keep the subtitle centred after that scale */
			i->sub.rectangle.x -= i->sub.rectangle.width * (subtitle_content->subtitle_x_scale() - 1);
			i->sub.rectangle.y -= i->sub.rectangle.height * (subtitle_content->subtitle_y_scale() - 1);
			
			ps.image.push_back (i->sub);
		}

		list<ContentTextSubtitle> text = subtitle_decoder->get_text_subtitles (ContentTimePeriod (from, to), starting);
		BOOST_FOREACH (ContentTextSubtitle& ts, text) {
			BOOST_FOREACH (dcp::SubtitleString& s, ts.subs) {
				s.set_v_position (s.v_position() + subtitle_content->subtitle_y_offset ());
				s.set_size (s.size() * max (subtitle_content->subtitle_x_scale(), subtitle_content->subtitle_y_scale()));
				ps.text.push_back (s);
			}
		}
	}

	return ps;
}
