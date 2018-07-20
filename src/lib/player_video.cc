/*
    Copyright (C) 2013-2018 Carl Hetherington <cth@carlh.net>

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

#include "player_video.h"
#include "content.h"
#include "video_content.h"
#include "image.h"
#include "image_proxy.h"
#include "j2k_image_proxy.h"
#include "film.h"
#include <dcp/raw_convert.h>
extern "C" {
#include <libavutil/pixfmt.h>
}
#include <libxml++/libxml++.h>
#include <iostream>

using std::string;
using std::cout;
using std::pair;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;
using boost::optional;
using boost::function;
using dcp::Data;
using dcp::raw_convert;

PlayerVideo::PlayerVideo (
	shared_ptr<const ImageProxy> in,
	Crop crop,
	boost::optional<double> fade,
	dcp::Size inter_size,
	dcp::Size out_size,
	Eyes eyes,
	Part part,
	optional<ColourConversion> colour_conversion,
	weak_ptr<Content> content,
	optional<Frame> video_frame
	)
	: _in (in)
	, _crop (crop)
	, _fade (fade)
	, _inter_size (inter_size)
	, _out_size (out_size)
	, _eyes (eyes)
	, _part (part)
	, _colour_conversion (colour_conversion)
	, _content (content)
	, _video_frame (video_frame)
{

}

PlayerVideo::PlayerVideo (shared_ptr<cxml::Node> node, shared_ptr<Socket> socket)
{
	_crop = Crop (node);
	_fade = node->optional_number_child<double> ("Fade");

	_inter_size = dcp::Size (node->number_child<int> ("InterWidth"), node->number_child<int> ("InterHeight"));
	_out_size = dcp::Size (node->number_child<int> ("OutWidth"), node->number_child<int> ("OutHeight"));
	_eyes = (Eyes) node->number_child<int> ("Eyes");
	_part = (Part) node->number_child<int> ("Part");

	/* Assume that the ColourConversion uses the current state version */
	_colour_conversion = ColourConversion::from_xml (node, Film::current_state_version);

	_in = image_proxy_factory (node->node_child ("In"), socket);

	if (node->optional_number_child<int> ("SubtitleX")) {

		shared_ptr<Image> image (
			new Image (AV_PIX_FMT_BGRA, dcp::Size (node->number_child<int> ("SubtitleWidth"), node->number_child<int> ("SubtitleHeight")), true)
			);

		image->read_from_socket (socket);

		_caption = PositionImage (image, Position<int> (node->number_child<int> ("SubtitleX"), node->number_child<int> ("SubtitleY")));
	}
}

void
PlayerVideo::set_caption (PositionImage image)
{
	_caption = image;
}

/** Create an image for this frame.
 *  @param note Handler for any notes that are made during the process.
 *  @param pixel_format Function which is called to decide what pixel format the output image should be;
 *  it is passed the pixel format of the input image from the ImageProxy, and should return the desired
 *  output pixel format.  Two functions always_rgb and keep_xyz_or_rgb are provided for use here.
 *  @param aligned true if the output image should be aligned to 32-byte boundaries.
 *  @param fast true to be fast at the expense of quality.
 */
shared_ptr<Image>
PlayerVideo::image (dcp::NoteHandler note, function<AVPixelFormat (AVPixelFormat)> pixel_format, bool aligned, bool fast) const
{
	pair<shared_ptr<Image>, int> prox = _in->image (optional<dcp::NoteHandler> (note), _inter_size);
	shared_ptr<Image> im = prox.first;
	int const reduce = prox.second;

	Crop total_crop = _crop;
	switch (_part) {
	case PART_LEFT_HALF:
		total_crop.right += im->size().width / 2;
		break;
	case PART_RIGHT_HALF:
		total_crop.left += im->size().width / 2;
		break;
	case PART_TOP_HALF:
		total_crop.bottom += im->size().height / 2;
		break;
	case PART_BOTTOM_HALF:
		total_crop.top += im->size().height / 2;
		break;
	default:
		break;
	}

	if (reduce > 0) {
		/* Scale the crop down to account for the scaling that has already happened in ImageProxy::image */
		int const r = pow(2, reduce);
		total_crop.left /= r;
		total_crop.right /= r;
		total_crop.top /= r;
		total_crop.bottom /= r;
	}

	dcp::YUVToRGB yuv_to_rgb = dcp::YUV_TO_RGB_REC601;
	if (_colour_conversion) {
		yuv_to_rgb = _colour_conversion.get().yuv_to_rgb();
	}

	shared_ptr<Image> out = im->crop_scale_window (
		total_crop, _inter_size, _out_size, yuv_to_rgb, pixel_format (_in->pixel_format()), aligned, fast
		);

	if (_caption) {
		out->alpha_blend (Image::ensure_aligned (_caption->image), _caption->position);
	}

	if (_fade) {
		out->fade (_fade.get ());
	}

	return out;
}

void
PlayerVideo::add_metadata (xmlpp::Node* node) const
{
	_crop.as_xml (node);
	if (_fade) {
		node->add_child("Fade")->add_child_text (raw_convert<string> (_fade.get ()));
	}
	_in->add_metadata (node->add_child ("In"));
	node->add_child("InterWidth")->add_child_text (raw_convert<string> (_inter_size.width));
	node->add_child("InterHeight")->add_child_text (raw_convert<string> (_inter_size.height));
	node->add_child("OutWidth")->add_child_text (raw_convert<string> (_out_size.width));
	node->add_child("OutHeight")->add_child_text (raw_convert<string> (_out_size.height));
	node->add_child("Eyes")->add_child_text (raw_convert<string> (static_cast<int> (_eyes)));
	node->add_child("Part")->add_child_text (raw_convert<string> (static_cast<int> (_part)));
	if (_colour_conversion) {
		_colour_conversion.get().as_xml (node);
	}
	if (_caption) {
		node->add_child ("SubtitleWidth")->add_child_text (raw_convert<string> (_caption->image->size().width));
		node->add_child ("SubtitleHeight")->add_child_text (raw_convert<string> (_caption->image->size().height));
		node->add_child ("SubtitleX")->add_child_text (raw_convert<string> (_caption->position.x));
		node->add_child ("SubtitleY")->add_child_text (raw_convert<string> (_caption->position.y));
	}
}

void
PlayerVideo::send_binary (shared_ptr<Socket> socket) const
{
	_in->send_binary (socket);
	if (_caption) {
		_caption->image->write_to_socket (socket);
	}
}

bool
PlayerVideo::has_j2k () const
{
	/* XXX: maybe other things */

	shared_ptr<const J2KImageProxy> j2k = dynamic_pointer_cast<const J2KImageProxy> (_in);
	if (!j2k) {
		return false;
	}

	return _crop == Crop () && _out_size == j2k->size() && !_caption && !_fade && !_colour_conversion;
}

Data
PlayerVideo::j2k () const
{
	shared_ptr<const J2KImageProxy> j2k = dynamic_pointer_cast<const J2KImageProxy> (_in);
	DCPOMATIC_ASSERT (j2k);
	return j2k->j2k ();
}

Position<int>
PlayerVideo::inter_position () const
{
	return Position<int> ((_out_size.width - _inter_size.width) / 2, (_out_size.height - _inter_size.height) / 2);
}

/** @return true if this PlayerVideo is definitely the same as another, false if it is probably not */
bool
PlayerVideo::same (shared_ptr<const PlayerVideo> other) const
{
	if (_crop != other->_crop ||
	    _fade.get_value_or(0) != other->_fade.get_value_or(0) ||
	    _inter_size != other->_inter_size ||
	    _out_size != other->_out_size ||
	    _eyes != other->_eyes ||
	    _part != other->_part ||
	    _colour_conversion != other->_colour_conversion) {
		return false;
	}

	if ((!_caption && other->_caption) || (_caption && !other->_caption)) {
		/* One has a caption and the other doesn't */
		return false;
	}

	if (_caption && other->_caption && !_caption->same (other->_caption.get ())) {
		/* They both have captions but they are different */
		return false;
	}

	/* Now neither has subtitles */

	return _in->same (other->_in);
}

AVPixelFormat
PlayerVideo::always_rgb (AVPixelFormat)
{
	return AV_PIX_FMT_RGB24;
}

AVPixelFormat
PlayerVideo::keep_xyz_or_rgb (AVPixelFormat p)
{
	return p == AV_PIX_FMT_XYZ12LE ? AV_PIX_FMT_XYZ12LE : AV_PIX_FMT_RGB48LE;
}

void
PlayerVideo::prepare ()
{
	_in->prepare (_inter_size);
}

size_t
PlayerVideo::memory_used () const
{
	return _in->memory_used();
}

/** @return Shallow copy of this; _in and _caption are shared between the original and the copy */
shared_ptr<PlayerVideo>
PlayerVideo::shallow_copy () const
{
	return shared_ptr<PlayerVideo>(
		new PlayerVideo(
			_in,
			_crop,
			_fade,
			_inter_size,
			_out_size,
			_eyes,
			_part,
			_colour_conversion,
			_content,
			_video_frame
			)
		);
}

/** Re-read crop, fade, inter/out size and colour conversion from our content.
 *  @return true if this was possible, false if not.
 */
bool
PlayerVideo::reset_metadata (dcp::Size video_container_size, dcp::Size film_frame_size)
{
	shared_ptr<Content> content = _content.lock();
	if (!content || !_video_frame) {
		return false;
	}

	_crop = content->video->crop();
	_fade = content->video->fade(_video_frame.get());
	_inter_size = content->video->scale().size(content->video, video_container_size, film_frame_size);
	_out_size = video_container_size;
	_colour_conversion = content->video->colour_conversion();

	return true;
}
