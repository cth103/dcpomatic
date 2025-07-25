/*
    Copyright (C) 2013-2022 Carl Hetherington <cth@carlh.net>

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


#include "colour_conversion.h"
#include "config.h"
#include "content.h"
#include "dcpomatic_log.h"
#include "exceptions.h"
#include "film.h"
#include "frame_rate_change.h"
#include "log.h"
#include "ratio.h"
#include "util.h"
#include "video_content.h"
#include "video_examiner.h"
#include <libcxml/cxml.h>
#include <libxml++/libxml++.h>
#include <fmt/format.h>
#include <iomanip>
#include <iostream>

#include "i18n.h"


using std::cout;
using std::dynamic_pointer_cast;
using std::list;
using std::make_shared;
using std::max;
using std::min;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
using namespace dcpomatic;


VideoContent::VideoContent(Content* parent)
	: ContentPart(parent)
	, _use(true)
	, _length(0)
	, _frame_type(VideoFrameType::TWO_D)
	, _yuv(true)
	, _fade_in(0)
	, _fade_out(0)
	, _range(VideoRange::FULL)
{

}


/** @param video_range_hint Video range to use if none is given in the XML */
shared_ptr<VideoContent>
VideoContent::from_xml(Content* parent, cxml::ConstNodePtr node, int version, VideoRange video_range_hint)
{
	if (!node->optional_number_child<int>("VideoWidth")) {
		return {};
	}

	return make_shared<VideoContent>(parent, node, version, video_range_hint);
}


/** @param video_range_hint Video range to use if none is given in the XML */
VideoContent::VideoContent(Content* parent, cxml::ConstNodePtr node, int version, VideoRange video_range_hint)
	: ContentPart(parent)
{
	_size = dcp::Size(node->number_child<int>("VideoWidth"), node->number_child<int>("VideoHeight"));

	_use = node->optional_bool_child("Use").get_value_or(true);
	_length = node->number_child<Frame>("VideoLength");

	if (version <= 34) {
		/* Snapshot of the VideoFrameType enum at version 34 */
		switch (node->number_child<int>("VideoFrameType")) {
		case 0:
			_frame_type = VideoFrameType::TWO_D;
			break;
		case 1:
			_frame_type = VideoFrameType::THREE_D_LEFT_RIGHT;
			break;
		case 2:
			_frame_type = VideoFrameType::THREE_D_TOP_BOTTOM;
			break;
		case 3:
			_frame_type = VideoFrameType::THREE_D_ALTERNATE;
			break;
		case 4:
			_frame_type = VideoFrameType::THREE_D_LEFT;
			break;
		case 5:
			_frame_type = VideoFrameType::THREE_D_RIGHT;
			break;
		}
	} else {
		_frame_type = string_to_video_frame_type(node->string_child("VideoFrameType"));
	}

	_sample_aspect_ratio = node->optional_number_child<double>("SampleAspectRatio");
	_crop.left = node->number_child<int>("LeftCrop");
	_crop.right = node->number_child<int>("RightCrop");
	_crop.top = node->number_child<int>("TopCrop");
	_crop.bottom = node->number_child<int>("BottomCrop");

	if (version <= 7) {
		if (auto r = node->optional_string_child("Ratio")) {
			if (auto ratio = Ratio::from_id_if_exists(r.get())) {
				_legacy_ratio = ratio->ratio();
			}
		}
	} else if (version <= 37) {
		if (auto id = node->node_child("Scale")->optional_string_child("Ratio")) {
			if (auto ratio = Ratio::from_id_if_exists(*id)) {
				_legacy_ratio = ratio->ratio();
			}
		}
		if (auto scale = node->node_child("Scale")->optional_bool_child("Scale")) {
			if (*scale) {
				/* This is what we used to call "no stretch" */
				DCPOMATIC_ASSERT(_size);
				_legacy_ratio = _size->ratio();
			} else {
				/* This is what we used to call "no scale" */
				DCPOMATIC_ASSERT(_size);
				_custom_size = *_size;
			}
		}

	} else {
		_custom_ratio = node->optional_number_child<float>("CustomRatio");
		if (node->optional_number_child<int>("CustomWidth")) {
			_custom_size = dcp::Size(node->number_child<int>("CustomWidth"), node->number_child<int>("CustomHeight"));
		}
	}

	if (node->optional_node_child("ColourConversion")) {
		_colour_conversion = ColourConversion(node->node_child("ColourConversion"), version);
	}

	_yuv = node->optional_bool_child("YUV").get_value_or(true);

	if (version >= 39) {
		_fade_in = node->number_child<Frame>("VideoFadeIn");
		_fade_out = node->number_child<Frame>("VideoFadeOut");
	} else if (version >= 32) {
		_fade_in = node->number_child<Frame>("FadeIn");
		_fade_out = node->number_child<Frame>("FadeOut");
	} else {
		_fade_in = _fade_out = 0;
	}

	auto video_range = node->optional_string_child("Range");
	if (!video_range) {
		_range = video_range_hint;
	} else if (*video_range == "full") {
		_range = VideoRange::FULL;
	} else {
		_range = VideoRange::VIDEO;
	}

	if (auto pixel_quanta = node->optional_node_child("PixelQuanta")) {
		_pixel_quanta = PixelQuanta(pixel_quanta);
	}

	if (auto burnt = node->optional_string_child("BurntSubtitleLanguage")) {
		_burnt_subtitle_language = dcp::LanguageTag(*burnt);
	}

	_has_alpha = node->optional_bool_child("HasAlpha").get_value_or(false);
}


VideoContent::VideoContent(Content* parent, vector<shared_ptr<Content> > c)
	: ContentPart(parent)
	, _length(0)
	, _yuv(false)
{
	auto ref = c[0]->video;
	DCPOMATIC_ASSERT(ref);

	for (size_t i = 1; i < c.size(); ++i) {

		if (c[i]->video->use() != ref->use()) {
			throw JoinError(_("Content to be joined must have all its video used or not used."));
		}

		if (c[i]->video->size() != ref->size()) {
			throw JoinError(_("Content to be joined must have the same picture size."));
		}

		if (c[i]->video->frame_type() != ref->frame_type()) {
			throw JoinError(_("Content to be joined must have the same video frame type."));
		}

		if (c[i]->video->requested_crop() != ref->requested_crop()) {
			throw JoinError(_("Content to be joined must have the same crop."));
		}

		if (c[i]->video->custom_ratio() != ref->custom_ratio()) {
			throw JoinError(_("Content to be joined must have the same custom ratio setting."));
		}

		if (c[i]->video->custom_size() != ref->custom_size()) {
			throw JoinError(_("Content to be joined must have the same custom size setting."));
		}

		if (c[i]->video->colour_conversion() != ref->colour_conversion()) {
			throw JoinError(_("Content to be joined must have the same colour conversion."));
		}

		if (c[i]->video->fade_in() != ref->fade_in() || c[i]->video->fade_out() != ref->fade_out()) {
			throw JoinError(_("Content to be joined must have the same fades."));
		}

		if (c[i]->video->burnt_subtitle_language() != ref->burnt_subtitle_language()) {
			throw JoinError(_("Content to be joined must have the same burnt subtitle language."));
		}

		_length += c[i]->video->length();

		if (c[i]->video->yuv()) {
			_yuv = true;
		}

		_pixel_quanta = max(_pixel_quanta, c[i]->video->_pixel_quanta);
		_has_alpha = _has_alpha | c[i]->video->_has_alpha;
	}

	_use = ref->use();
	_size = ref->size();
	_frame_type = ref->frame_type();
	_crop = ref->requested_crop();
	_custom_ratio = ref->custom_ratio();
	_colour_conversion = ref->colour_conversion();
	_fade_in = ref->fade_in();
	_fade_out = ref->fade_out();
	_range = ref->range();
	_burnt_subtitle_language = ref->burnt_subtitle_language();
}


void
VideoContent::as_xml(xmlpp::Element* element) const
{
	boost::mutex::scoped_lock lm(_mutex);
	cxml::add_text_child(element, "Use", _use ? "1" : "0");
	cxml::add_text_child(element, "VideoLength", fmt::to_string(_length));
	if (_size) {
		cxml::add_text_child(element, "VideoWidth", fmt::to_string(_size->width));
		cxml::add_text_child(element, "VideoHeight", fmt::to_string(_size->height));
	}
	cxml::add_text_child(element, "VideoFrameType", video_frame_type_to_string(_frame_type));
	if (_sample_aspect_ratio) {
		cxml::add_text_child(element, "SampleAspectRatio", fmt::to_string(_sample_aspect_ratio.get()));
	}
	_crop.as_xml(element);
	if (_custom_ratio) {
		cxml::add_text_child(element, "CustomRatio", fmt::to_string(*_custom_ratio));
	}
	if (_custom_size) {
		cxml::add_text_child(element, "CustomWidth", fmt::to_string(_custom_size->width));
		cxml::add_text_child(element, "CustomHeight", fmt::to_string(_custom_size->height));
	}
	if (_colour_conversion) {
		_colour_conversion.get().as_xml(cxml::add_child(element, "ColourConversion"));
	}
	cxml::add_text_child(element, "YUV", _yuv ? "1" : "0");
	cxml::add_text_child(element, "VideoFadeIn", fmt::to_string(_fade_in));
	cxml::add_text_child(element, "VideoFadeOut", fmt::to_string(_fade_out));
	cxml::add_text_child(element, "Range", _range == VideoRange::FULL ? "full" : "video");
	_pixel_quanta.as_xml(cxml::add_child(element, "PixelQuanta"));
	if (_burnt_subtitle_language) {
		cxml::add_text_child(element, "BurntSubtitleLanguage", _burnt_subtitle_language->as_string());
	}
	cxml::add_text_child(element, "HasAlpha", _has_alpha ? "1" : "0");
}

void
VideoContent::take_from_examiner(shared_ptr<const Film> film, shared_ptr<VideoExaminer> d)
{
	/* These examiner calls could call other content methods which take a lock on the mutex */
	auto const vs = d->video_size();
	auto vl = d->video_length();
	auto const ar = d->sample_aspect_ratio();
	auto const yuv = d->yuv();
	auto const range = d->range();
	auto const pixel_quanta = d->pixel_quanta();
	auto const has_alpha = d->has_alpha();

	ContentChangeSignaller cc1(_parent, VideoContentProperty::SIZE);
	ContentChangeSignaller cc2(_parent, ContentProperty::LENGTH);
	ContentChangeSignaller cc3(_parent, VideoContentProperty::RANGE);

	{
		boost::mutex::scoped_lock lm(_mutex);
		_size = vs;
		_length = vl;
		_sample_aspect_ratio = ar;
		_yuv = yuv;
		_range = range;
		_pixel_quanta = pixel_quanta;
		_has_alpha = has_alpha;
	}

	LOG_GENERAL("Video length obtained from header as {} frames", _length);

	if (d->video_frame_rate()) {
		_parent->set_video_frame_rate(film, d->video_frame_rate().get());
	}
}

/** @return string which includes everything about how this content looks */
string
VideoContent::identifier() const
{
	char buffer[256];
	auto const crop = actual_crop();
	snprintf(
		buffer, sizeof(buffer), "%d_%d_%d_%d_%d_%f_%d_%d%" PRId64 "_%" PRId64 "_%d",
		(_use ? 1 : 0),
		crop.left,
		crop.right,
		crop.top,
		crop.bottom,
		_custom_ratio.get_value_or(0),
		_custom_size ? _custom_size->width : 0,
		_custom_size ? _custom_size->height : 0,
		_fade_in,
		_fade_out,
		_range == VideoRange::FULL ? 0 : 1
		);

	string s(buffer);

	if (colour_conversion()) {
		s += "_" + colour_conversion().get().identifier();
	}

	return s;
}

string
VideoContent::technical_summary() const
{
	string const size_string = size() ? fmt::format("{}x{}", size()->width, size()->height) : _("unknown");

	string s = fmt::format(
		N_("video: length {} frames, size {}"),
		length_after_3d_combine(),
		size_string
		);

	if (sample_aspect_ratio()) {
		s += fmt::format(N_(", sample aspect ratio {}"), sample_aspect_ratio().get());
	}

	return s;
}

optional<dcp::Size>
VideoContent::size_after_3d_split() const
{
	auto const s = size();
	if (!s) {
		return {};
	}

	switch (frame_type()) {
	case VideoFrameType::TWO_D:
	case VideoFrameType::THREE_D:
	case VideoFrameType::THREE_D_ALTERNATE:
	case VideoFrameType::THREE_D_LEFT:
	case VideoFrameType::THREE_D_RIGHT:
		return *s;
	case VideoFrameType::THREE_D_LEFT_RIGHT:
		return dcp::Size(s->width / 2, s->height);
	case VideoFrameType::THREE_D_TOP_BOTTOM:
		return dcp::Size(s->width, s->height / 2);
	}

	DCPOMATIC_ASSERT(false);
	return {};
}

/** @return Video size after 3D split and crop */
optional<dcp::Size>
VideoContent::size_after_crop() const
{
	auto const after_3d = size_after_3d_split();
	if (!after_3d) {
		return {};
	}

	return actual_crop().apply(*after_3d);
}


/** @param time Time within the whole (untrimmed) content.
 *  @return Fade factor (between 0 and 1) or unset if there is no fade.
 */
optional<double>
VideoContent::fade(shared_ptr<const Film> film, ContentTime time) const
{
	DCPOMATIC_ASSERT(time.get() >= 0);

	double const vfr = _parent->active_video_frame_rate(film);

	auto const trim_start = _parent->trim_start();
	auto const fade_in_time = ContentTime::from_frames(fade_in(), vfr);
	/* time after the trimmed start of the content */
	auto const time_after_start = time - trim_start;
	if (fade_in_time.get() && time_after_start < fade_in_time) {
		return std::max(0.0, static_cast<double>(time_after_start.get()) / fade_in_time.get());
	}

	auto const fade_out_time = ContentTime::from_frames(fade_out(), vfr);
	auto const end = ContentTime::from_frames(length(), vfr) - _parent->trim_end();
	auto const time_after_end_fade_start = time - (end - fade_out_time);
	if (time_after_end_fade_start > ContentTime()) {
		return std::max(0.0, 1 - static_cast<double>(time_after_end_fade_start.get()) / fade_out_time.get());
	}

	return {};
}

string
VideoContent::processing_description(shared_ptr<const Film> film)
{
	string d;
	char buffer[256];

	if (size() && size()->width && size()->height) {
		d += fmt::format(
			_("Content video is {}x{}"),
			size_after_3d_split()->width,
			size_after_3d_split()->height
			);


		auto ratio = size_after_3d_split()->ratio();

		if (sample_aspect_ratio()) {
			snprintf(buffer, sizeof(buffer), _(", pixel aspect ratio %.2f:1"), sample_aspect_ratio().get());
			d += buffer;
			ratio *= sample_aspect_ratio().get();
		}

		snprintf(buffer, sizeof(buffer), _("\nDisplay aspect ratio %.2f:1"), ratio);
		d += buffer;
	}

	auto const crop = actual_crop();

	if ((crop.left || crop.right || crop.top || crop.bottom) && size() != dcp::Size(0, 0)) {
		auto const cropped = size_after_crop();
		if (cropped) {
			d += fmt::format(
				_("\nCropped to {}x{}"),
				cropped->width, cropped->height
				);

			snprintf(buffer, sizeof(buffer), " (%.2f:1)", cropped->ratio());
			d += buffer;
		}
	}

	auto const container_size = film->frame_size();
	auto const scaled = scaled_size(container_size);

	if (scaled && *scaled != size_after_crop()) {
		d += fmt::format(
			_("\nScaled to {}x{}"),
			scaled->width, scaled->height
			);

		snprintf(buffer, sizeof(buffer), _(" (%.2f:1)"), scaled->ratio());
		d += buffer;
	}

	if (scaled && *scaled != container_size) {
		d += fmt::format(
			_("\nPadded with black to fit container {} ({}x{})"),
			film->container().container_nickname(),
			container_size.width, container_size.height
			);

		snprintf(buffer, sizeof(buffer), _(" (%.2f:1)"), container_size.ratio());
		d += buffer;
	}

	if (_parent->video_frame_rate()) {
		double const vfr = _parent->video_frame_rate().get();

		snprintf(buffer, sizeof(buffer), _("\nContent frame rate %.4f\n"), vfr);
		d += buffer;

		FrameRateChange frc(vfr, film->video_frame_rate());
		d += frc.description();
	}

	return d;
}

void
VideoContent::add_properties(list<UserProperty>& p) const
{
	p.push_back(UserProperty(UserProperty::VIDEO, _("Length"), length(), _("video frames")));
	if (auto s = size()) {
		p.push_back(UserProperty(UserProperty::VIDEO, _("Size"), fmt::format("{}x{}", s->width, s->height)));
	}
}

void
VideoContent::set_length(Frame len)
{
	maybe_set(_length, len, ContentProperty::LENGTH);
}

void
VideoContent::set_crop(Crop c)
{
	maybe_set(_crop, c, VideoContentProperty::CROP);
}

void
VideoContent::set_left_crop(int c)
{
	maybe_set(_crop.left, c, VideoContentProperty::CROP);
}

void
VideoContent::set_right_crop(int c)
{
	maybe_set(_crop.right, c, VideoContentProperty::CROP);
}

void
VideoContent::set_top_crop(int c)
{
	maybe_set(_crop.top, c, VideoContentProperty::CROP);
}

void
VideoContent::set_bottom_crop(int c)
{
	maybe_set(_crop.bottom, c, VideoContentProperty::CROP);
}


void
VideoContent::set_frame_type(VideoFrameType t)
{
	maybe_set(_frame_type, t, VideoContentProperty::FRAME_TYPE);
}

void
VideoContent::unset_colour_conversion()
{
	maybe_set(_colour_conversion, boost::optional<ColourConversion>(), VideoContentProperty::COLOUR_CONVERSION);
}

void
VideoContent::set_colour_conversion(ColourConversion c)
{
	maybe_set(_colour_conversion, c, VideoContentProperty::COLOUR_CONVERSION);
}

void
VideoContent::set_fade_in(Frame t)
{
	maybe_set(_fade_in, t, VideoContentProperty::FADE_IN);
}

void
VideoContent::set_fade_out(Frame t)
{
	maybe_set(_fade_out, t, VideoContentProperty::FADE_OUT);
}

void
VideoContent::set_range(VideoRange r)
{
	maybe_set(_range, r, VideoContentProperty::RANGE);
}

void
VideoContent::set_use(bool u)
{
	maybe_set(_use, u, VideoContentProperty::USE);
}


void
VideoContent::set_burnt_subtitle_language(boost::optional<dcp::LanguageTag> language)
{
	maybe_set(_burnt_subtitle_language, language, VideoContentProperty::BURNT_SUBTITLE_LANGUAGE);
}


void
VideoContent::take_settings_from(shared_ptr<const VideoContent> c)
{
	if (c->_colour_conversion) {
		set_colour_conversion(c->_colour_conversion.get());
	} else {
		unset_colour_conversion();
	}
	set_use(c->_use);
	set_frame_type(c->_frame_type);
	set_left_crop(c->_crop.left);
	set_right_crop(c->_crop.right);
	set_top_crop(c->_crop.top);
	set_bottom_crop(c->_crop.bottom);
	set_custom_ratio(c->_custom_ratio);
	set_custom_size(c->_custom_size);
	set_fade_in(c->_fade_in);
	set_fade_out(c->_fade_out);
	set_burnt_subtitle_language(c->_burnt_subtitle_language);
	set_range(c->_range);
}


void
VideoContent::modify_position(shared_ptr<const Film> film, DCPTime& pos) const
{
	pos = pos.round(film->video_frame_rate());
}

void
VideoContent::modify_trim_start(ContentTime& trim) const
{
	if (_parent->video_frame_rate()) {
		trim = trim.round(_parent->video_frame_rate().get());
	}
}


/** @param film_container The size of the container for the DCP that we are working on */
optional<dcp::Size>
VideoContent::scaled_size(dcp::Size film_container)
{
	if (_custom_ratio) {
		return fit_ratio_within(*_custom_ratio, film_container);
	}

	if (_custom_size) {
		if (_custom_size->width <= film_container.width && _custom_size->height <= film_container.height) {
			return *_custom_size;
		}
		return fit_ratio_within(_custom_size->ratio(), film_container);
	}

	auto size = size_after_crop();
	if (!size) {
		return {};
	}

	size->width = std::lrint(size->width * _sample_aspect_ratio.get_value_or(1));

	/* This is what we will return unless there is any legacy stuff to take into account */
	auto auto_size = fit_ratio_within(size->ratio(), film_container);

	if (_legacy_ratio) {
		if (fit_ratio_within(*_legacy_ratio, film_container) != auto_size) {
			_custom_ratio = *_legacy_ratio;
			_legacy_ratio = {};
			return fit_ratio_within(*_custom_ratio, film_container);
		}
		_legacy_ratio = {};
	}

	return _pixel_quanta.round(auto_size);
}


void
VideoContent::set_custom_ratio(optional<float> ratio)
{
	maybe_set(_custom_ratio, ratio, VideoContentProperty::CUSTOM_RATIO);
}


void
VideoContent::set_custom_size(optional<dcp::Size> size)
{
	maybe_set(_custom_size, size, VideoContentProperty::CUSTOM_SIZE);
}


Crop
VideoContent::actual_crop() const
{
	return Crop(
		_pixel_quanta.round_x(_crop.left),
		_pixel_quanta.round_x(_crop.right),
		_pixel_quanta.round_y(_crop.top),
		_pixel_quanta.round_y(_crop.bottom)
	);
}


void
VideoContent::rotate_size()
{
	if (_size) {
		std::swap(_size->width, _size->height);
	}
}

