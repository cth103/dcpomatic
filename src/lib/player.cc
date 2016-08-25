/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

static bool
has_video (Content* c)
{
	return static_cast<bool>(c->video);
}

static bool
has_audio (Content* c)
{
	return static_cast<bool>(c->audio);
}

static bool
has_subtitle (Content* c)
{
	return static_cast<bool>(c->subtitle);
}

Player::Player (shared_ptr<const Film> film, shared_ptr<const Playlist> playlist)
	: _film (film)
	, _playlist (playlist)
	, _have_valid_pieces (false)
	, _ignore_video (false)
	, _ignore_audio (false)
	, _always_burn_subtitles (false)
	, _fast (false)
	, _play_referenced (false)
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

		if (decoder->audio && _fast) {
			decoder->audio->set_fast ();
		}

		shared_ptr<DCPDecoder> dcp = dynamic_pointer_cast<DCPDecoder> (decoder);
		if (dcp && _play_referenced) {
			dcp->set_decode_referenced ();
		}

		_pieces.push_back (shared_ptr<Piece> (new Piece (i, decoder, frc)));
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
		property == VideoContentProperty::FRAME_TYPE ||
		property == DCPContentProperty::CAN_BE_PLAYED ||
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
		property == SubtitleContentProperty::Y_SCALE
		) {

		/* These changes just need the pieces' decoders to be reset.
		   It's quite possible that other changes could be handled by
		   this branch rather than the _have_valid_pieces = false branch
		   above.  This would make things a lot faster.
		*/

		reset_pieces ();
		Changed (frequent);

	} else if (
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
	_video_container_size = s;

	_black_image.reset (new Image (AV_PIX_FMT_RGB24, _video_container_size, true));
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

/** @return All PlayerVideos at the given time.  There may be none if the content
 *  at `time' is a DCP which we are passing through (i.e. referring to by reference)
 *  or 2 if we have 3D.
 */
list<shared_ptr<PlayerVideo> >
Player::get_video (DCPTime time, bool accurate)
{
	if (!_have_valid_pieces) {
		setup_pieces ();
	}

	/* Find subtitles for possible burn-in */

	PlayerSubtitles ps = get_subtitles (time, DCPTime::from_frames (1, _film->video_frame_rate ()), false, true, accurate);

	list<PositionImage> sub_images;

	/* Image subtitles */
	list<PositionImage> c = transform_image_subtitles (ps.image);
	copy (c.begin(), c.end(), back_inserter (sub_images));

	/* Text subtitles (rendered to an image) */
	if (!ps.text.empty ()) {
		list<PositionImage> s = render_subtitles (ps.text, ps.fonts, _video_container_size, time);
		copy (s.begin (), s.end (), back_inserter (sub_images));
	}

	optional<PositionImage> subtitles;
	if (!sub_images.empty ()) {
		subtitles = merge (sub_images);
	}

	/* Find pieces containing video which is happening now */

	list<shared_ptr<Piece> > ov = overlaps (
		time,
		time + DCPTime::from_frames (1, _film->video_frame_rate ()),
		&has_video
		);

	list<shared_ptr<PlayerVideo> > pvf;

	if (ov.empty ()) {
		/* No video content at this time */
		pvf.push_back (black_player_video_frame (time));
	} else {
		/* Some video content at this time */
		shared_ptr<Piece> last = *(ov.rbegin ());
		VideoFrameType const last_type = last->content->video->frame_type ();

		/* Get video from appropriate piece(s) */
		BOOST_FOREACH (shared_ptr<Piece> piece, ov) {

			shared_ptr<VideoDecoder> decoder = piece->decoder->video;
			DCPOMATIC_ASSERT (decoder);

			shared_ptr<DCPContent> dcp_content = dynamic_pointer_cast<DCPContent> (piece->content);
			if (dcp_content && dcp_content->reference_video () && !_play_referenced) {
				continue;
			}

			bool const use =
				/* always use the last video */
				piece == last ||
				/* with a corresponding L/R eye if appropriate */
				(last_type == VIDEO_FRAME_TYPE_3D_LEFT && piece->content->video->frame_type() == VIDEO_FRAME_TYPE_3D_RIGHT) ||
				(last_type == VIDEO_FRAME_TYPE_3D_RIGHT && piece->content->video->frame_type() == VIDEO_FRAME_TYPE_3D_LEFT);

			if (use) {
				/* We want to use this piece */
				list<ContentVideo> content_video = decoder->get (dcp_to_content_video (piece, time), accurate);
				if (content_video.empty ()) {
					pvf.push_back (black_player_video_frame (time));
				} else {
					dcp::Size image_size = piece->content->video->scale().size (
						piece->content->video, _video_container_size, _film->frame_size ()
						);

					for (list<ContentVideo>::const_iterator i = content_video.begin(); i != content_video.end(); ++i) {
						pvf.push_back (
							shared_ptr<PlayerVideo> (
								new PlayerVideo (
									i->image,
									time,
									piece->content->video->crop (),
									piece->content->video->fade (i->frame.index()),
									image_size,
									_video_container_size,
									i->frame.eyes(),
									i->part,
									piece->content->video->colour_conversion ()
									)
								)
							);
					}
				}
			} else {
				/* Discard unused video */
				decoder->get (dcp_to_content_video (piece, time), accurate);
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

/** @return Audio data or 0 if the only audio data here is referenced DCP data */
shared_ptr<AudioBuffers>
Player::get_audio (DCPTime time, DCPTime length, bool accurate)
{
	if (!_have_valid_pieces) {
		setup_pieces ();
	}

	Frame const length_frames = length.frames_round (_film->audio_frame_rate ());

	shared_ptr<AudioBuffers> audio (new AudioBuffers (_film->audio_channels(), length_frames));
	audio->make_silent ();

	list<shared_ptr<Piece> > ov = overlaps (time, time + length, has_audio);
	if (ov.empty ()) {
		return audio;
	}

	bool all_referenced = true;
	BOOST_FOREACH (shared_ptr<Piece> i, ov) {
		shared_ptr<DCPContent> dcp_content = dynamic_pointer_cast<DCPContent> (i->content);
		if (i->content->audio && (!dcp_content || !dcp_content->reference_audio ())) {
			/* There is audio content which is not from a DCP or not set to be referenced */
			all_referenced = false;
		}
	}

	if (all_referenced && !_play_referenced) {
		return shared_ptr<AudioBuffers> ();
	}

	BOOST_FOREACH (shared_ptr<Piece> i, ov) {

		DCPOMATIC_ASSERT (i->content->audio);
		shared_ptr<AudioDecoder> decoder = i->decoder->audio;
		DCPOMATIC_ASSERT (decoder);

		/* The time that we should request from the content */
		DCPTime request = time - DCPTime::from_seconds (i->content->audio->delay() / 1000.0);
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

		Frame const content_frame = dcp_to_resampled_audio (i, request);

		BOOST_FOREACH (AudioStreamPtr j, i->content->audio->streams ()) {

			if (j->channels() == 0) {
				/* Some content (e.g. DCPs) can have streams with no channels */
				continue;
			}

			/* Audio from this piece's decoder stream (which might be more or less than what we asked for) */
			ContentAudio all = decoder->get (j, content_frame, request_frames, accurate);

			/* Gain */
			if (i->content->audio->gain() != 0) {
				shared_ptr<AudioBuffers> gain (new AudioBuffers (all.audio));
				gain->apply_gain (i->content->audio->gain ());
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
				dcp_mapped = _audio_processor->run (dcp_mapped, _film->audio_channels ());
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

ContentTime
Player::dcp_to_content_subtitle (shared_ptr<const Piece> piece, DCPTime t) const
{
	DCPTime s = t - piece->content->position ();
	s = min (piece->content->length_after_trim(), s);
	return max (ContentTime (), ContentTime (s, piece->frc) + piece->content->trim_start());
}

DCPTime
Player::content_subtitle_to_dcp (shared_ptr<const Piece> piece, ContentTime t) const
{
	return max (DCPTime (), DCPTime (t - piece->content->trim_start(), piece->frc) + piece->content->position());
}

/** @param burnt true to return only subtitles to be burnt, false to return only
 *  subtitles that should not be burnt.  This parameter will be ignored if
 *  _always_burn_subtitles is true; in this case, all subtitles will be returned.
 */
PlayerSubtitles
Player::get_subtitles (DCPTime time, DCPTime length, bool starting, bool burnt, bool accurate)
{
	list<shared_ptr<Piece> > subs = overlaps (time, time + length, has_subtitle);

	PlayerSubtitles ps (time);

	for (list<shared_ptr<Piece> >::const_iterator j = subs.begin(); j != subs.end(); ++j) {
		if (!(*j)->content->subtitle->use () || (!_always_burn_subtitles && (burnt != (*j)->content->subtitle->burn ()))) {
			continue;
		}

		shared_ptr<DCPContent> dcp_content = dynamic_pointer_cast<DCPContent> ((*j)->content);
		if (dcp_content && dcp_content->reference_subtitle () && !_play_referenced) {
			continue;
		}

		shared_ptr<SubtitleDecoder> subtitle_decoder = (*j)->decoder->subtitle;
		ContentTime const from = dcp_to_content_subtitle (*j, time);
		/* XXX: this video_frame_rate() should be the rate that the subtitle content has been prepared for */
		ContentTime const to = from + ContentTime::from_frames (1, _film->video_frame_rate ());

		list<ContentImageSubtitle> image = subtitle_decoder->get_image (ContentTimePeriod (from, to), starting, accurate);
		for (list<ContentImageSubtitle>::iterator i = image.begin(); i != image.end(); ++i) {

			/* Apply content's subtitle offsets */
			i->sub.rectangle.x += (*j)->content->subtitle->x_offset ();
			i->sub.rectangle.y += (*j)->content->subtitle->y_offset ();

			/* Apply content's subtitle scale */
			i->sub.rectangle.width *= (*j)->content->subtitle->x_scale ();
			i->sub.rectangle.height *= (*j)->content->subtitle->y_scale ();

			/* Apply a corrective translation to keep the subtitle centred after that scale */
			i->sub.rectangle.x -= i->sub.rectangle.width * ((*j)->content->subtitle->x_scale() - 1);
			i->sub.rectangle.y -= i->sub.rectangle.height * ((*j)->content->subtitle->y_scale() - 1);

			ps.image.push_back (i->sub);
		}

		list<ContentTextSubtitle> text = subtitle_decoder->get_text (ContentTimePeriod (from, to), starting, accurate);
		BOOST_FOREACH (ContentTextSubtitle& ts, text) {
			BOOST_FOREACH (dcp::SubtitleString s, ts.subs) {
				s.set_h_position (s.h_position() + (*j)->content->subtitle->x_offset ());
				s.set_v_position (s.v_position() + (*j)->content->subtitle->y_offset ());
				float const xs = (*j)->content->subtitle->x_scale();
				float const ys = (*j)->content->subtitle->y_scale();
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
				s.set_in (dcp::Time(content_subtitle_to_dcp (*j, ts.period().from).seconds(), 1000));
				s.set_out (dcp::Time(content_subtitle_to_dcp (*j, ts.period().to).seconds(), 1000));
				ps.text.push_back (SubtitleString (s, (*j)->content->subtitle->outline_width()));
				ps.add_fonts ((*j)->content->subtitle->fonts ());
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
			DCPTime const from = i->position() + DCPTime::from_frames (offset, _film->video_frame_rate());
			if (j->reference_video ()) {
				a.push_back (
					ReferencedReelAsset (
						k->main_picture (),
						DCPTimePeriod (from, from + DCPTime::from_frames (k->main_picture()->duration(), _film->video_frame_rate()))
						)
					);
			}

			if (j->reference_audio ()) {
				a.push_back (
					ReferencedReelAsset (
						k->main_sound (),
						DCPTimePeriod (from, from + DCPTime::from_frames (k->main_sound()->duration(), _film->video_frame_rate()))
						)
					);
			}

			if (j->reference_subtitle ()) {
				DCPOMATIC_ASSERT (k->main_subtitle ());
				a.push_back (
					ReferencedReelAsset (
						k->main_subtitle (),
						DCPTimePeriod (from, from + DCPTime::from_frames (k->main_subtitle()->duration(), _film->video_frame_rate()))
						)
					);
			}

			/* Assume that main picture duration is the length of the reel */
			offset += k->main_picture()->duration ();
		}
	}

	return a;
}

list<shared_ptr<Piece> >
Player::overlaps (DCPTime from, DCPTime to, boost::function<bool (Content *)> valid)
{
	if (!_have_valid_pieces) {
		setup_pieces ();
	}

	list<shared_ptr<Piece> > overlaps;
	BOOST_FOREACH (shared_ptr<Piece> i, _pieces) {
		if (valid (i->content.get ()) && i->content->position() < to && i->content->end() > from) {
			overlaps.push_back (i);
		}
	}

	return overlaps;
}

void
Player::reset_pieces ()
{
	BOOST_FOREACH (shared_ptr<Piece> i, _pieces) {
		i->decoder->reset ();
	}
}
