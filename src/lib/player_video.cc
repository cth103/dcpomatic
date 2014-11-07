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

#include <dcp/raw_convert.h>
#include "player_video.h"
#include "image.h"
#include "image_proxy.h"
#include "j2k_image_proxy.h"
#include "scaler.h"

using std::string;
using std::cout;
using dcp::raw_convert;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::optional;

PlayerVideo::PlayerVideo (
	shared_ptr<const ImageProxy> in,
	DCPTime time,
	Crop crop,
	boost::optional<float> fade,
	dcp::Size inter_size,
	dcp::Size out_size,
	Scaler const * scaler,
	Eyes eyes,
	Part part,
	optional<ColourConversion> colour_conversion
	)
	: _in (in)
	, _time (time)
	, _crop (crop)
	, _fade (fade)
	, _inter_size (inter_size)
	, _out_size (out_size)
	, _scaler (scaler)
	, _eyes (eyes)
	, _part (part)
	, _colour_conversion (colour_conversion)
{

}

PlayerVideo::PlayerVideo (shared_ptr<cxml::Node> node, shared_ptr<Socket> socket)
{
	_time = DCPTime (node->number_child<DCPTime::Type> ("Time"));
	_crop = Crop (node);
	_fade = node->optional_number_child<float> ("Fade");

	_inter_size = dcp::Size (node->number_child<int> ("InterWidth"), node->number_child<int> ("InterHeight"));
	_out_size = dcp::Size (node->number_child<int> ("OutWidth"), node->number_child<int> ("OutHeight"));
	_scaler = Scaler::from_id (node->string_child ("Scaler"));
	_eyes = (Eyes) node->number_child<int> ("Eyes");
	_part = (Part) node->number_child<int> ("Part");
	_colour_conversion = ColourConversion::from_xml (node);

	_in = image_proxy_factory (node->node_child ("In"), socket);

	if (node->optional_number_child<int> ("SubtitleX")) {
		
		_subtitle.position = Position<int> (node->number_child<int> ("SubtitleX"), node->number_child<int> ("SubtitleY"));

		_subtitle.image.reset (
			new Image (PIX_FMT_RGBA, dcp::Size (node->number_child<int> ("SubtitleWidth"), node->number_child<int> ("SubtitleHeight")), true)
			);
		
		_subtitle.image->read_from_socket (socket);
	}
}

void
PlayerVideo::set_subtitle (PositionImage image)
{
	_subtitle = image;
}

shared_ptr<Image>
PlayerVideo::image (AVPixelFormat pixel_format, bool burn_subtitle) const
{
	shared_ptr<Image> im = _in->image ();
	
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
		
	shared_ptr<Image> out = im->crop_scale_window (total_crop, _inter_size, _out_size, _scaler, pixel_format, true);

	if (burn_subtitle && _subtitle.image) {
		out->alpha_blend (_subtitle.image, _subtitle.position);
	}

	if (_fade) {
		out->fade (_fade.get ());
	}

	return out;
}

void
PlayerVideo::add_metadata (xmlpp::Node* node, bool send_subtitles) const
{
	node->add_child("Time")->add_child_text (raw_convert<string> (_time.get ()));
	_crop.as_xml (node);
	if (_fade) {
		node->add_child("Fade")->add_child_text (raw_convert<string> (_fade.get ()));
	}
	_in->add_metadata (node->add_child ("In"));
	node->add_child("InterWidth")->add_child_text (raw_convert<string> (_inter_size.width));
	node->add_child("InterHeight")->add_child_text (raw_convert<string> (_inter_size.height));
	node->add_child("OutWidth")->add_child_text (raw_convert<string> (_out_size.width));
	node->add_child("OutHeight")->add_child_text (raw_convert<string> (_out_size.height));
	node->add_child("Scaler")->add_child_text (_scaler->id ());
	node->add_child("Eyes")->add_child_text (raw_convert<string> (_eyes));
	node->add_child("Part")->add_child_text (raw_convert<string> (_part));
	if (_colour_conversion) {
		_colour_conversion.get().as_xml (node);
	}
	if (send_subtitles && _subtitle.image) {
		node->add_child ("SubtitleWidth")->add_child_text (raw_convert<string> (_subtitle.image->size().width));
		node->add_child ("SubtitleHeight")->add_child_text (raw_convert<string> (_subtitle.image->size().height));
		node->add_child ("SubtitleX")->add_child_text (raw_convert<string> (_subtitle.position.x));
		node->add_child ("SubtitleY")->add_child_text (raw_convert<string> (_subtitle.position.y));
	}
}

void
PlayerVideo::send_binary (shared_ptr<Socket> socket, bool send_subtitles) const
{
	_in->send_binary (socket);
	if (send_subtitles && _subtitle.image) {
		_subtitle.image->write_to_socket (socket);
	}
}

bool
PlayerVideo::has_j2k () const
{
	/* XXX: burnt-in subtitle; maybe other things */
	
	shared_ptr<const J2KImageProxy> j2k = dynamic_pointer_cast<const J2KImageProxy> (_in);
	if (!j2k) {
		return false;
	}
	
	return _crop == Crop () && _inter_size == j2k->size();
}

shared_ptr<EncodedData>
PlayerVideo::j2k () const
{
	shared_ptr<const J2KImageProxy> j2k = dynamic_pointer_cast<const J2KImageProxy> (_in);
	assert (j2k);
	return j2k->j2k ();
}

Position<int>
PlayerVideo::inter_position () const
{
	return Position<int> ((_out_size.width - _inter_size.width) / 2, (_out_size.height - _inter_size.height) / 2);
}

/** @return true if this PlayerVideo is definitely the same as another
 * (apart from _time), false if it is probably not
 */
bool
PlayerVideo::same (shared_ptr<const PlayerVideo> other) const
{
	if (_in != other->_in ||
	    _crop != other->_crop ||
	    _fade.get_value_or(0) != other->_fade.get_value_or(0) ||
	    _inter_size != other->_inter_size ||
	    _out_size != other->_out_size ||
	    _scaler != other->_scaler ||
	    _eyes != other->_eyes ||
	    _part != other->_part ||
	    _colour_conversion != other->_colour_conversion ||
	    !_subtitle.same (other->_subtitle)) {
		return false;
	}

	return _in->same (other->_in);
}
