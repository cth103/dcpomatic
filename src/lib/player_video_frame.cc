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

#include <libdcp/raw_convert.h>
#include "player_video_frame.h"
#include "image.h"
#include "image_proxy.h"
#include "scaler.h"

using std::string;
using std::cout;
using boost::shared_ptr;
using libdcp::raw_convert;

PlayerVideoFrame::PlayerVideoFrame (
	shared_ptr<const ImageProxy> in,
	Crop crop,
	libdcp::Size inter_size,
	libdcp::Size out_size,
	Scaler const * scaler,
	Eyes eyes,
	Part part,
	ColourConversion colour_conversion
	)
	: _in (in)
	, _crop (crop)
	, _inter_size (inter_size)
	, _out_size (out_size)
	, _scaler (scaler)
	, _eyes (eyes)
	, _part (part)
	, _colour_conversion (colour_conversion)
{

}

PlayerVideoFrame::PlayerVideoFrame (shared_ptr<cxml::Node> node, shared_ptr<Socket> socket, shared_ptr<Log> log)
{
	_crop = Crop (node);

	_inter_size = libdcp::Size (node->number_child<int> ("InterWidth"), node->number_child<int> ("InterHeight"));
	_out_size = libdcp::Size (node->number_child<int> ("OutWidth"), node->number_child<int> ("OutHeight"));
	_scaler = Scaler::from_id (node->string_child ("Scaler"));
	_eyes = (Eyes) node->number_child<int> ("Eyes");
	_part = (Part) node->number_child<int> ("Part");
	_colour_conversion = ColourConversion (node);

	_in = image_proxy_factory (node->node_child ("In"), socket, log);

	if (node->optional_number_child<int> ("SubtitleX")) {
		
		_subtitle_position = Position<int> (node->number_child<int> ("SubtitleX"), node->number_child<int> ("SubtitleY"));

		shared_ptr<Image> image (
			new Image (PIX_FMT_RGBA, libdcp::Size (node->number_child<int> ("SubtitleWidth"), node->number_child<int> ("SubtitleHeight")), true)
			);
		
		image->read_from_socket (socket);
		_subtitle_image = image;
	}
}

void
PlayerVideoFrame::set_subtitle (shared_ptr<const Image> image, Position<int> pos)
{
	_subtitle_image = image;
	_subtitle_position = pos;
}

shared_ptr<Image>
PlayerVideoFrame::image () const
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
		
	shared_ptr<Image> out = im->crop_scale_window (total_crop, _inter_size, _out_size, _scaler, PIX_FMT_RGB24, false);

	Position<int> const container_offset ((_out_size.width - _inter_size.width) / 2, (_out_size.height - _inter_size.width) / 2);

	if (_subtitle_image) {
		out->alpha_blend (_subtitle_image, _subtitle_position);
	}

	return out;
}

void
PlayerVideoFrame::add_metadata (xmlpp::Node* node) const
{
	_crop.as_xml (node);
	_in->add_metadata (node->add_child ("In"));
	node->add_child("InterWidth")->add_child_text (raw_convert<string> (_inter_size.width));
	node->add_child("InterHeight")->add_child_text (raw_convert<string> (_inter_size.height));
	node->add_child("OutWidth")->add_child_text (raw_convert<string> (_out_size.width));
	node->add_child("OutHeight")->add_child_text (raw_convert<string> (_out_size.height));
	node->add_child("Scaler")->add_child_text (_scaler->id ());
	node->add_child("Eyes")->add_child_text (raw_convert<string> (_eyes));
	node->add_child("Part")->add_child_text (raw_convert<string> (_part));
	_colour_conversion.as_xml (node);
	if (_subtitle_image) {
		node->add_child ("SubtitleWidth")->add_child_text (raw_convert<string> (_subtitle_image->size().width));
		node->add_child ("SubtitleHeight")->add_child_text (raw_convert<string> (_subtitle_image->size().height));
		node->add_child ("SubtitleX")->add_child_text (raw_convert<string> (_subtitle_position.x));
		node->add_child ("SubtitleY")->add_child_text (raw_convert<string> (_subtitle_position.y));
	}
}

void
PlayerVideoFrame::send_binary (shared_ptr<Socket> socket) const
{
	_in->send_binary (socket);
	if (_subtitle_image) {
		_subtitle_image->write_to_socket (socket);
	}
}
