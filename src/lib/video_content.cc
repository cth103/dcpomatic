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

#include "video_content.h"
#include "video_examiner.h"
#include "compose.hpp"
#include "ratio.h"
#include "config.h"
#include "colour_conversion.h"
#include "util.h"
#include "film.h"
#include "exceptions.h"
#include "frame_rate_change.h"
#include "log.h"
#include "safe_stringstream.h"
#include <libcxml/cxml.h>
#include <dcp/colour_matrix.h>
#include <dcp/raw_convert.h>
#include <iomanip>

#include "i18n.h"

#define LOG_GENERAL(...) film->log()->log (String::compose (__VA_ARGS__), Log::TYPE_GENERAL);

int const VideoContentProperty::VIDEO_SIZE	  = 0;
int const VideoContentProperty::VIDEO_FRAME_RATE  = 1;
int const VideoContentProperty::VIDEO_FRAME_TYPE  = 2;
int const VideoContentProperty::VIDEO_CROP	  = 3;
int const VideoContentProperty::VIDEO_SCALE	  = 4;
int const VideoContentProperty::COLOUR_CONVERSION = 5;
int const VideoContentProperty::VIDEO_FADE_IN     = 6;
int const VideoContentProperty::VIDEO_FADE_OUT    = 7;

using std::string;
using std::setprecision;
using std::cout;
using std::vector;
using std::min;
using std::max;
using std::stringstream;
using std::fixed;
using std::setprecision;
using boost::shared_ptr;
using boost::optional;
using boost::dynamic_pointer_cast;
using dcp::raw_convert;

VideoContent::VideoContent (shared_ptr<const Film> f)
	: Content (f)
	, _video_length (0)
	, _video_frame_rate (0)
	, _video_frame_type (VIDEO_FRAME_TYPE_2D)
	, _scale (VideoContentScale (Ratio::from_id ("178")))
{
	set_default_colour_conversion (false);
}

VideoContent::VideoContent (shared_ptr<const Film> f, DCPTime s, ContentTime len)
	: Content (f, s)
	, _video_length (len)
	, _video_frame_rate (0)
	, _video_frame_type (VIDEO_FRAME_TYPE_2D)
	, _scale (VideoContentScale (Ratio::from_id ("178")))
{
	set_default_colour_conversion (false);
}

VideoContent::VideoContent (shared_ptr<const Film> f, boost::filesystem::path p)
	: Content (f, p)
	, _video_length (0)
	, _video_frame_rate (0)
	, _video_frame_type (VIDEO_FRAME_TYPE_2D)
	, _scale (VideoContentScale (Ratio::from_id ("178")))
{
	set_default_colour_conversion (false);
}

VideoContent::VideoContent (shared_ptr<const Film> f, cxml::ConstNodePtr node, int version)
	: Content (f, node)
{
	_video_size.width = node->number_child<int> ("VideoWidth");
	_video_size.height = node->number_child<int> ("VideoHeight");
	_video_frame_rate = node->number_child<float> ("VideoFrameRate");

	if (version < 32) {
		/* DCP-o-matic 1.0 branch */
		_video_length = ContentTime::from_frames (node->number_child<int64_t> ("VideoLength"), _video_frame_rate);
	} else {
		_video_length = ContentTime (node->number_child<ContentTime::Type> ("VideoLength"));
	}
	
	_video_frame_type = static_cast<VideoFrameType> (node->number_child<int> ("VideoFrameType"));
	_sample_aspect_ratio = node->optional_number_child<float> ("SampleAspectRatio");
	_crop.left = node->number_child<int> ("LeftCrop");
	_crop.right = node->number_child<int> ("RightCrop");
	_crop.top = node->number_child<int> ("TopCrop");
	_crop.bottom = node->number_child<int> ("BottomCrop");

	if (version <= 7) {
		optional<string> r = node->optional_string_child ("Ratio");
		if (r) {
			_scale = VideoContentScale (Ratio::from_id (r.get ()));
		}
	} else {
		_scale = VideoContentScale (node->node_child ("Scale"));
	}

	
	if (node->optional_node_child ("ColourConversion")) {
		_colour_conversion = ColourConversion (node->node_child ("ColourConversion"));
	}
	if (version >= 32) {
		_fade_in = ContentTime (node->number_child<int64_t> ("FadeIn"));
		_fade_out = ContentTime (node->number_child<int64_t> ("FadeOut"));
	}
}

VideoContent::VideoContent (shared_ptr<const Film> f, vector<shared_ptr<Content> > c)
	: Content (f, c)
	, _video_length (0)
{
	shared_ptr<VideoContent> ref = dynamic_pointer_cast<VideoContent> (c[0]);
	DCPOMATIC_ASSERT (ref);

	for (size_t i = 0; i < c.size(); ++i) {
		shared_ptr<VideoContent> vc = dynamic_pointer_cast<VideoContent> (c[i]);

		if (vc->video_size() != ref->video_size()) {
			throw JoinError (_("Content to be joined must have the same picture size."));
		}

		if (vc->video_frame_rate() != ref->video_frame_rate()) {
			throw JoinError (_("Content to be joined must have the same video frame rate."));
		}

		if (vc->video_frame_type() != ref->video_frame_type()) {
			throw JoinError (_("Content to be joined must have the same video frame type."));
		}

		if (vc->crop() != ref->crop()) {
			throw JoinError (_("Content to be joined must have the same crop."));
		}

		if (vc->scale() != ref->scale()) {
			throw JoinError (_("Content to be joined must have the same scale setting."));
		}

		if (vc->colour_conversion() != ref->colour_conversion()) {
			throw JoinError (_("Content to be joined must have the same colour conversion."));
		}

		if (vc->fade_in() != ref->fade_in() || vc->fade_out() != ref->fade_out()) {
			throw JoinError (_("Content to be joined must have the same fades."));
		}
		
		_video_length += vc->video_length ();
	}

	_video_size = ref->video_size ();
	_video_frame_rate = ref->video_frame_rate ();
	_video_frame_type = ref->video_frame_type ();
	_crop = ref->crop ();
	_scale = ref->scale ();
	_colour_conversion = ref->colour_conversion ();
	_fade_in = ref->fade_in ();
	_fade_out = ref->fade_out ();
}

void
VideoContent::as_xml (xmlpp::Node* node) const
{
	boost::mutex::scoped_lock lm (_mutex);
	node->add_child("VideoLength")->add_child_text (raw_convert<string> (_video_length.get ()));
	node->add_child("VideoWidth")->add_child_text (raw_convert<string> (_video_size.width));
	node->add_child("VideoHeight")->add_child_text (raw_convert<string> (_video_size.height));
	node->add_child("VideoFrameRate")->add_child_text (raw_convert<string> (_video_frame_rate));
	node->add_child("VideoFrameType")->add_child_text (raw_convert<string> (static_cast<int> (_video_frame_type)));
	if (_sample_aspect_ratio) {
		node->add_child("SampleAspectRatio")->add_child_text (raw_convert<string> (_sample_aspect_ratio.get ()));
	}
	_crop.as_xml (node);
	_scale.as_xml (node->add_child("Scale"));
	if (_colour_conversion) {
		_colour_conversion.get().as_xml (node->add_child("ColourConversion"));
	}
	node->add_child("FadeIn")->add_child_text (raw_convert<string> (_fade_in.get ()));
	node->add_child("FadeOut")->add_child_text (raw_convert<string> (_fade_out.get ()));
}

void
VideoContent::set_default_colour_conversion (bool signal)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_colour_conversion = ColourConversion (dcp::ColourConversion::srgb_to_xyz ());
	}

	if (signal) {
		signal_changed (VideoContentProperty::COLOUR_CONVERSION);
	}
}

void
VideoContent::take_from_video_examiner (shared_ptr<VideoExaminer> d)
{
	/* These examiner calls could call other content methods which take a lock on the mutex */
	dcp::Size const vs = d->video_size ();
	optional<float> const vfr = d->video_frame_rate ();
	ContentTime vl = d->video_length ();
	optional<float> const ar = d->sample_aspect_ratio ();

	{
		boost::mutex::scoped_lock lm (_mutex);
		_video_size = vs;
		/* Default video frame rate to 24fps if the examiner doesn't know */
		_video_frame_rate = vfr.get_value_or (24);
		_video_length = vl;
		_sample_aspect_ratio = ar;

		/* Guess correct scale from size and sample aspect ratio */
		_scale = VideoContentScale (
			Ratio::nearest_from_ratio (float (_video_size.width) * ar.get_value_or (1) / _video_size.height)
			);
	}

	shared_ptr<const Film> film = _film.lock ();
	DCPOMATIC_ASSERT (film);
	LOG_GENERAL ("Video length obtained from header as %1 frames", _video_length.frames (_video_frame_rate));
	
	signal_changed (VideoContentProperty::VIDEO_SIZE);
	signal_changed (VideoContentProperty::VIDEO_FRAME_RATE);
	signal_changed (VideoContentProperty::VIDEO_SCALE);
	signal_changed (ContentProperty::LENGTH);
}

void
VideoContent::set_left_crop (int c)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		
		if (_crop.left == c) {
			return;
		}
		
		_crop.left = c;
	}
	
	signal_changed (VideoContentProperty::VIDEO_CROP);
}

void
VideoContent::set_right_crop (int c)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_crop.right == c) {
			return;
		}
		
		_crop.right = c;
	}
	
	signal_changed (VideoContentProperty::VIDEO_CROP);
}

void
VideoContent::set_top_crop (int c)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_crop.top == c) {
			return;
		}
		
		_crop.top = c;
	}
	
	signal_changed (VideoContentProperty::VIDEO_CROP);
}

void
VideoContent::set_bottom_crop (int c)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_crop.bottom == c) {
			return;
		}
		
		_crop.bottom = c;
	}

	signal_changed (VideoContentProperty::VIDEO_CROP);
}

void
VideoContent::set_scale (VideoContentScale s)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_scale == s) {
			return;
		}

		_scale = s;
	}

	signal_changed (VideoContentProperty::VIDEO_SCALE);
}

/** @return string which includes everything about how this content looks */
string
VideoContent::identifier () const
{
	SafeStringStream s;
	s << Content::identifier()
	  << "_" << crop().left
	  << "_" << crop().right
	  << "_" << crop().top
	  << "_" << crop().bottom
	  << "_" << scale().id();

	if (colour_conversion()) {
		s << "_" << colour_conversion().get().identifier ();
	}

	return s.str ();
}

void
VideoContent::set_video_frame_type (VideoFrameType t)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_video_frame_type = t;
	}

	signal_changed (VideoContentProperty::VIDEO_FRAME_TYPE);
}

string
VideoContent::technical_summary () const
{
	string s = String::compose (
		"video: length %1, size %2x%3, rate %4",
		video_length_after_3d_combine().seconds(),
		video_size().width,
		video_size().height,
		video_frame_rate()
		);

	if (sample_aspect_ratio ()) {
		s += String::compose (_(", sample aspect ratio %1"), (sample_aspect_ratio().get ()));
	}

	return s;
}

dcp::Size
VideoContent::video_size_after_3d_split () const
{
	dcp::Size const s = video_size ();
	switch (video_frame_type ()) {
	case VIDEO_FRAME_TYPE_2D:
	case VIDEO_FRAME_TYPE_3D_ALTERNATE:
	case VIDEO_FRAME_TYPE_3D_LEFT:
	case VIDEO_FRAME_TYPE_3D_RIGHT:
		return s;
	case VIDEO_FRAME_TYPE_3D_LEFT_RIGHT:
		return dcp::Size (s.width / 2, s.height);
	case VIDEO_FRAME_TYPE_3D_TOP_BOTTOM:
		return dcp::Size (s.width, s.height / 2);
	}

	DCPOMATIC_ASSERT (false);
}

void
VideoContent::unset_colour_conversion ()
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_colour_conversion = boost::optional<ColourConversion> ();
	}

	signal_changed (VideoContentProperty::COLOUR_CONVERSION);
}

void
VideoContent::set_colour_conversion (ColourConversion c)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_colour_conversion = c;
	}

	signal_changed (VideoContentProperty::COLOUR_CONVERSION);
}

void
VideoContent::set_fade_in (ContentTime t)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_fade_in = t;
	}

	signal_changed (VideoContentProperty::VIDEO_FADE_IN);
}

void
VideoContent::set_fade_out (ContentTime t)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_fade_out = t;
	}

	signal_changed (VideoContentProperty::VIDEO_FADE_OUT);
}

/** @return Video size after 3D split and crop */
dcp::Size
VideoContent::video_size_after_crop () const
{
	return crop().apply (video_size_after_3d_split ());
}

/** @param t A time offset from the start of this piece of content.
 *  @return Corresponding time with respect to the content.
 */
ContentTime
VideoContent::dcp_time_to_content_time (DCPTime t) const
{
	shared_ptr<const Film> film = _film.lock ();
	DCPOMATIC_ASSERT (film);
	return ContentTime (t, FrameRateChange (video_frame_rate(), film->video_frame_rate()));
}

void
VideoContent::scale_and_crop_to_fit_width ()
{
	shared_ptr<const Film> film = _film.lock ();
	DCPOMATIC_ASSERT (film);

	set_scale (VideoContentScale (film->container ()));

	int const crop = max (0, int (video_size().height - double (film->frame_size().height) * video_size().width / film->frame_size().width));
	set_top_crop (crop / 2);
	set_bottom_crop (crop / 2);
}

void
VideoContent::scale_and_crop_to_fit_height ()
{
	shared_ptr<const Film> film = _film.lock ();
	DCPOMATIC_ASSERT (film);

	set_scale (VideoContentScale (film->container ()));

	int const crop = max (0, int (video_size().width - double (film->frame_size().width) * video_size().height / film->frame_size().height));
	set_left_crop (crop / 2);
	set_right_crop (crop / 2);
}

void
VideoContent::set_video_frame_rate (float r)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_video_frame_rate == r) {
			return;
		}
		
		_video_frame_rate = r;
	}
	
	signal_changed (VideoContentProperty::VIDEO_FRAME_RATE);
}

optional<float>
VideoContent::fade (VideoFrame f) const
{
	DCPOMATIC_ASSERT (f >= 0);
	
	if (f < fade_in().frames (video_frame_rate ())) {
		return float (f) / _fade_in.frames (video_frame_rate ());
	}

	VideoFrame fade_out_start = ContentTime (video_length() - fade_out()).frames (video_frame_rate ());
	if (f >= fade_out_start) {
		return 1 - float (f - fade_out_start) / fade_out().frames (video_frame_rate ());
	}

	return optional<float> ();
}

string
VideoContent::processing_description () const
{
	/* stringstream is OK here as this string is just for presentation to the user */
	stringstream d;

	if (video_size().width && video_size().height) {
		d << String::compose (
			_("Content video is %1x%2"),
			video_size_after_3d_split().width,
			video_size_after_3d_split().height
			);

		d << " (" << fixed << setprecision(2) << video_size_after_3d_split().ratio() << ":1)\n";
	}

	if ((crop().left || crop().right || crop().top || crop().bottom) && video_size() != dcp::Size (0, 0)) {
		dcp::Size cropped = video_size_after_crop ();
		d << String::compose (
			_("Cropped to %1x%2"),
			cropped.width, cropped.height
			);

		d << " (" << fixed << setprecision(2) << cropped.ratio () << ":1)\n";
	}

	shared_ptr<const Film> film = _film.lock ();
	DCPOMATIC_ASSERT (film);

	dcp::Size const container_size = film->frame_size ();
	dcp::Size const scaled = scale().size (dynamic_pointer_cast<const VideoContent> (shared_from_this ()), container_size, container_size, 1);

	if (scaled != video_size_after_crop ()) {
		d << String::compose (
			_("Scaled to %1x%2"),
			scaled.width, scaled.height
			);

		d << " (" << fixed << setprecision(2) << scaled.ratio() << ":1)\n";
	}
	
	if (scaled != container_size) {
		d << String::compose (
			_("Padded with black to %1x%2"),
			container_size.width, container_size.height
			);

		d << " (" << fixed << setprecision(2) << container_size.ratio () << ":1)\n";
	}

	d << _("Content frame rate");
	d << " " << fixed << setprecision(4) << video_frame_rate() << "\n";
	
	FrameRateChange frc (video_frame_rate(), film->video_frame_rate ());
	d << frc.description () << "\n";

	return d.str ();
}
