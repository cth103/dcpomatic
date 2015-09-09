/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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
#include "dcp_content.h"
#include "dcp_decoder.h"
#include "dcp_subtitle_content.h"
#include "dcp_subtitle_decoder.h"
#include "audio_processor.h"
#include "playlist.h"
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
using std::copy;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;
using boost::optional;

Player::Player (shared_ptr<const Film> film, shared_ptr<const Playlist> playlist)
	: _film (film)
	, _playlist (playlist)
	, _have_valid_pieces (false)
	, _ignore_video (false)
	, _ignore_audio (false)
	, _always_burn_subtitles (false)
	, _fast (false)
{
	_film_changed_connection = _film->Changed.connect (bind (&Player::film_changed, this, _1));
	_playlist_changed_connection = _playlist->Changed.connect (bind (&Player::playlist_changed, this));
	_playlist_content_changed_connection = _playlist->ContentChanged.connect (bind (&Player::playlist_content_changed, this, _1, _2, _3));
	set_video_container_size (_film->frame_size ());

	film_changed (Film::AUDIO_PROCESSOR);
}

void
Player::setup_pieces ()
{
	list<shared_ptr<Piece> > old_pieces = _pieces;
	_pieces.clear ();

	BOOST_FOREACH (shared_ptr<Content> i, _playlist->content ()) {

		if (!i->paths_valid ()) {
			continue;
		}

		shared_ptr<Decoder> decoder;
		optional<FrameRateChange> frc;

		/* Work out a FrameRateChange for the best overlap video for this content, in case we need it below */
		DCPTime best_overlap_t;
		shared_ptr<VideoContent> best_overlap;
		BOOST_FOREACH (shared_ptr<Content> j, _playlist->content ()) {
			shared_ptr<VideoContent> vc = dynamic_pointer_cast<VideoContent> (j);
			if (!vc) {
				continue;
			}

			DCPTime const overlap = max (vc->position(), i->position()) - min (vc->end(), i->end());
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
		shared_ptr<const FFmpegContent> fc = dynamic_pointer_cast<const FFmpegContent> (i);
		if (fc) {
			decoder.reset (new FFmpegDecoder (fc, _film->log(), _fast));
			frc = FrameRateChange (fc->video_frame_rate(), _film->video_frame_rate());
		}

		shared_ptr<const DCPContent> dc = dynamic_pointer_cast<const DCPContent> (i);
		if (dc) {
			decoder.reset (new DCPDecoder (dc, _fast));
			frc = FrameRateChange (dc->video_frame_rate(), _film->video_frame_rate());
		}

		/* ImageContent */
		shared_ptr<const ImageContent> ic = dynamic_pointer_cast<const ImageContent> (i);
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
		shared_ptr<const SndfileContent> sc = dynamic_pointer_cast<const SndfileContent> (i);
		if (sc) {
			decoder.reset (new SndfileDecoder (sc, _fast));
			frc = best_overlap_frc;
		}

		/* SubRipContent */
		shared_ptr<const SubRipContent> rc = dynamic_pointer_cast<const SubRipContent> (i);
		if (rc) {
			decoder.reset (new SubRipDecoder (rc));
			frc = best_overlap_frc;
		}

		/* DCPSubtitleContent */
		shared_ptr<const DCPSubtitleContent> dsc = dynamic_pointer_cast<const DCPSubtitleContent> (i);
		if (dsc) {
			decoder.reset (new DCPSubtitleDecoder (dsc));
			frc = best_overlap_frc;
		}

		shared_ptr<VideoDecoder> vd = dynamic_pointer_cast<VideoDecoder> (decoder);
		if (vd && _ignore_video) {
			vd->set_ignore_video ();
		}

		shared_ptr<AudioDecoder> ad = dynamic_pointer_cast<AudioDecoder> (decoder);
		if (ad && _ignore_audio) {
			ad->set_ignore_audio ();
		}

		_pieces.push_back (shared_ptr<Piece> (new Piece (i, decoder, frc.get ())));
	}

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
		property == SubtitleContentProperty::FONTS ||
		property == VideoContentProperty::VIDEO_CROP ||
		property == VideoContentProperty::VIDEO_SCALE ||
		property == VideoContentProperty::VIDEO_FRAME_RATE ||
		property == VideoContentProperty::VIDEO_FADE_IN ||
		property == VideoContentProperty::VIDEO_FADE_OUT ||
		property == VideoContentProperty::COLOUR_CONVERSION
		) {

		Changed (frequent);
	}
}

void
Player::set_video_container_size (dcp::Size s)
{
	_video_container_size = s;

	_black_image.reset (new Image (PIX_FMT_RGB24, _video_container_size, true));
	_black_image->make_black ();
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

	if (p == Film::CONTAINER || p == Film::VIDEO_FRAME_RATE) {
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
					true
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
Player::black_player_video_frame (DCPTime time) const
{
	return shared_ptr<PlayerVideo> (
		new PlayerVideo (
			shared_ptr<const ImageProxy> (new RawImageProxy (_black_image)),
			time,
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

/** @return All PlayerVideos at the given time (there may be two frames for 3D) */
list<shared_ptr<PlayerVideo> >
Player::get_video (DCPTime time, bool accurate)
{
	if (!_have_valid_pieces) {
		setup_pieces ();
	}

	/* Find subtitles for possible burn-in */

	PlayerSubtitles ps = get_subtitles (time, DCPTime::from_frames (1, _film->video_frame_rate ()), false, true);

	list<PositionImage> sub_images;

	/* Image subtitles */
	list<PositionImage> c = transform_image_subtitles (ps.image);
	copy (c.begin(), c.end(), back_inserter (sub_images));

	/* Text subtitles (rendered to an image) */
	if (!ps.text.empty ()) {
		list<PositionImage> s = render_subtitles (ps.text, ps.fonts, _video_container_size);
		copy (s.begin (), s.end (), back_inserter (sub_images));
	}

	optional<PositionImage> subtitles;
	if (!sub_images.empty ()) {
		subtitles = merge (sub_images);
	}

	/* Find pieces containing video which is happening now */

	list<shared_ptr<Piece> > ov = overlaps<VideoContent> (
		time,
		time + DCPTime::from_frames (1, _film->video_frame_rate ()) - DCPTime::delta()
		);

	list<shared_ptr<PlayerVideo> > pvf;

	if (ov.empty ()) {
		/* No video content at this time */
		pvf.push_back (black_player_video_frame (time));
	} else {
		/* Some video content at this time */
		shared_ptr<Piece> last = *(ov.rbegin ());
		VideoFrameType const last_type = dynamic_pointer_cast<VideoContent> (last->content)->video_frame_type ();

		/* Get video from appropriate piece(s) */
		BOOST_FOREACH (shared_ptr<Piece> piece, ov) {

			shared_ptr<VideoDecoder> decoder = dynamic_pointer_cast<VideoDecoder> (piece->decoder);
			DCPOMATIC_ASSERT (decoder);
			shared_ptr<VideoContent> video_content = dynamic_pointer_cast<VideoContent> (piece->content);
			DCPOMATIC_ASSERT (video_content);

			bool const use =
				/* always use the last video */
				piece == last ||
				/* with a corresponding L/R eye if appropriate */
				(last_type == VIDEO_FRAME_TYPE_3D_LEFT && video_content->video_frame_type() == VIDEO_FRAME_TYPE_3D_RIGHT) ||
				(last_type == VIDEO_FRAME_TYPE_3D_RIGHT && video_content->video_frame_type() == VIDEO_FRAME_TYPE_3D_LEFT);

			if (use) {
				/* We want to use this piece */
				list<ContentVideo> content_video = decoder->get_video (dcp_to_content_video (piece, time), accurate);
				if (content_video.empty ()) {
					pvf.push_back (black_player_video_frame (time));
				} else {
					dcp::Size image_size = video_content->scale().size (video_content, _video_container_size, _film->frame_size ());

					for (list<ContentVideo>::const_iterator i = content_video.begin(); i != content_video.end(); ++i) {
						pvf.push_back (
							shared_ptr<PlayerVideo> (
								new PlayerVideo (
									i->image,
									content_video_to_dcp (piece, i->frame),
									video_content->crop (),
									video_content->fade (i->frame),
									image_size,
									_video_container_size,
									i->eyes,
									i->part,
									video_content->colour_conversion ()
									)
								)
							);
					}
				}
			} else {
				/* Discard unused video */
				decoder->get_video (dcp_to_content_video (piece, time), accurate);
			}
		}
	}

	if (subtitles) {
		BOOST_FOREACH (shared_ptr<PlayerVideo> p, pvf) {
			p->set_subtitle (subtitles.get ());
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

	Frame const length_frames = length.frames_round (_film->audio_frame_rate ());

	shared_ptr<AudioBuffers> audio (new AudioBuffers (_film->audio_channels(), length_frames));
	audio->make_silent ();

	list<shared_ptr<Piece> > ov = overlaps<AudioContent> (time, time + length);
	if (ov.empty ()) {
		return audio;
	}

	for (list<shared_ptr<Piece> >::iterator i = ov.begin(); i != ov.end(); ++i) {

		shared_ptr<AudioContent> content = dynamic_pointer_cast<AudioContent> ((*i)->content);
		DCPOMATIC_ASSERT (content);
		shared_ptr<AudioDecoder> decoder = dynamic_pointer_cast<AudioDecoder> ((*i)->decoder);
		DCPOMATIC_ASSERT (decoder);

		/* The time that we should request from the content */
		DCPTime request = time - DCPTime::from_seconds (content->audio_delay() / 1000.0);
		Frame request_frames = length_frames;
		DCPTime offset;
		if (request < DCPTime ()) {
			/* We went off the start of the content, so we will need to offset
			   the stuff we get back.
			*/
			offset = -request;
			request_frames += request.frames_round (_film->audio_frame_rate ());
			if (request_frames < 0) {
				request_frames = 0;
			}
			request = DCPTime ();
		}

		Frame const content_frame = dcp_to_resampled_audio (*i, request);

		BOOST_FOREACH (AudioStreamPtr j, content->audio_streams ()) {

			if (j->channels() == 0) {
				/* Some content (e.g. DCPs) can have streams with no channels */
				continue;
			}

			/* Audio from this piece's decoder stream (which might be more or less than what we asked for) */
			ContentAudio all = decoder->get_audio (j, content_frame, request_frames, accurate);

			/* Gain */
			if (content->audio_gain() != 0) {
				shared_ptr<AudioBuffers> gain (new AudioBuffers (all.audio));
				gain->apply_gain (content->audio_gain ());
				all.audio = gain;
			}

			/* Remap channels */
			shared_ptr<AudioBuffers> dcp_mapped (new AudioBuffers (_film->audio_channels(), all.audio->frames()));
			dcp_mapped->make_silent ();
			AudioMapping map = j->mapping ();
			for (int i = 0; i < map.input_channels(); ++i) {
				for (int j = 0; j < _film->audio_channels(); ++j) {
					if (map.get (i, j) > 0) {
						dcp_mapped->accumulate_channel (
							all.audio.get(),
							i,
							j,
							map.get (i, j)
							);
					}
				}
			}

			if (_audio_processor) {
				dcp_mapped = _audio_processor->run (dcp_mapped);
			}

			all.audio = dcp_mapped;

			audio->accumulate_frames (
				all.audio.get(),
				content_frame - all.frame,
				offset.frames_round (_film->audio_frame_rate()),
				min (Frame (all.audio->frames()), request_frames)
				);
		}
	}

	return audio;
}

Frame
Player::dcp_to_content_video (shared_ptr<const Piece> piece, DCPTime t) const
{
	shared_ptr<const VideoContent> vc = dynamic_pointer_cast<const VideoContent> (piece->content);
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
	shared_ptr<const VideoContent> vc = dynamic_pointer_cast<const VideoContent> (piece->content);
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

ContentTime
Player::dcp_to_content_subtitle (shared_ptr<const Piece> piece, DCPTime t) const
{
	DCPTime s = t - piece->content->position ();
	s = min (piece->content->length_after_trim(), s);
	return max (ContentTime (), ContentTime (s, piece->frc) + piece->content->trim_start());
}

/** @param burnt true to return only subtitles to be burnt, false to return only
 *  subtitles that should not be burnt.  This parameter will be ignored if
 *  _always_burn_subtitles is true; in this case, all subtitles will be returned.
 */
PlayerSubtitles
Player::get_subtitles (DCPTime time, DCPTime length, bool starting, bool burnt)
{
	list<shared_ptr<Piece> > subs = overlaps<SubtitleContent> (time, time + length);

	PlayerSubtitles ps (time, length);

	for (list<shared_ptr<Piece> >::const_iterator j = subs.begin(); j != subs.end(); ++j) {
		shared_ptr<SubtitleContent> subtitle_content = dynamic_pointer_cast<SubtitleContent> ((*j)->content);
		if (!subtitle_content->use_subtitles () || (!_always_burn_subtitles && (burnt != subtitle_content->burn_subtitles ()))) {
			continue;
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
				s.set_h_position (s.h_position() + subtitle_content->subtitle_x_offset ());
				s.set_v_position (s.v_position() + subtitle_content->subtitle_y_offset ());
				float const xs = subtitle_content->subtitle_x_scale();
				float const ys = subtitle_content->subtitle_y_scale();
				float const average = s.size() * (xs + ys) / 2;
				s.set_size (average);
				if (fabs (1.0 - xs / ys) > dcp::ASPECT_ADJUST_EPSILON) {
					s.set_aspect_adjust (xs / ys);
				}
				ps.text.push_back (s);
				ps.add_fonts (subtitle_content->fonts ());
			}
		}
	}

	return ps;
}

list<shared_ptr<Font> >
Player::get_subtitle_fonts ()
{
	if (!_have_valid_pieces) {
		setup_pieces ();
	}

	list<shared_ptr<Font> > fonts;
	BOOST_FOREACH (shared_ptr<Piece>& p, _pieces) {
		shared_ptr<SubtitleContent> sc = dynamic_pointer_cast<SubtitleContent> (p->content);
		if (sc) {
			/* XXX: things may go wrong if there are duplicate font IDs
			   with different font files.
			*/
			list<shared_ptr<Font> > f = sc->fonts ();
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

/** Set this player never to produce any audio data */
void
Player::set_ignore_audio ()
{
	_ignore_audio = true;
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
