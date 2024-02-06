/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  src/dcp_video_frame.cc
 *  @brief A single frame of video destined for a DCP.
 *
 *  Given an Image and some settings, this class knows how to encode
 *  the image to J2K either on the local host or on a remote server.
 *
 *  Objects of this class are used for the queue that we keep
 *  of images that require encoding.
 */


#include "compose.hpp"
#include "config.h"
#include "cross.h"
#include "dcp_video.h"
#include "dcpomatic_log.h"
#include "dcpomatic_socket.h"
#include "encode_server_description.h"
#include "exceptions.h"
#include "image.h"
#include "log.h"
#include "player_video.h"
#include "rng.h"
#include <libcxml/cxml.h>
#include <dcp/raw_convert.h>
#include <dcp/openjpeg_image.h>
#include <dcp/rgb_xyz.h>
#include <dcp/j2k_transcode.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <stdint.h>
#include <iomanip>
#include <iostream>

#include "i18n.h"


using std::cout;
using std::make_shared;
using std::shared_ptr;
using std::string;
using dcp::ArrayData;
using dcp::raw_convert;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


/** Construct a DCP video frame.
 *  @param frame Input frame.
 *  @param index Index of the frame within the DCP.
 *  @param bw J2K bandwidth to use (see Config::j2k_bandwidth ())
 */
DCPVideo::DCPVideo (
	shared_ptr<const PlayerVideo> frame, int index, int dcp_fps, int bw, Resolution r
	)
	: _frame (frame)
	, _index (index)
	, _frames_per_second (dcp_fps)
	, _j2k_bandwidth (bw)
	, _resolution (r)
{

}

DCPVideo::DCPVideo (shared_ptr<const PlayerVideo> frame, shared_ptr<const cxml::Node> node)
	: _frame (frame)
{
	_index = node->number_child<int> ("Index");
	_frames_per_second = node->number_child<int> ("FramesPerSecond");
	_j2k_bandwidth = node->number_child<int> ("J2KBandwidth");
	_resolution = Resolution (node->optional_number_child<int>("Resolution").get_value_or(static_cast<int>(Resolution::TWO_K)));
}

shared_ptr<dcp::OpenJPEGImage>
DCPVideo::convert_to_xyz (shared_ptr<const PlayerVideo> frame)
{
	shared_ptr<dcp::OpenJPEGImage> xyz;

	auto image = frame->image (bind(&PlayerVideo::keep_xyz_or_rgb, _1), VideoRange::FULL, false);
	if (frame->colour_conversion()) {
		xyz = dcp::rgb_to_xyz (
			image->data()[0],
			image->size(),
			image->stride()[0],
			frame->colour_conversion().get()
			);
	} else {
		xyz = make_shared<dcp::OpenJPEGImage>(image->data()[0], image->size(), image->stride()[0]);
	}

	return xyz;
}

dcp::Size
DCPVideo::get_size() const
{
	auto image = _frame->image(bind(&PlayerVideo::keep_xyz_or_rgb, _1), VideoRange::FULL, false);
	return image->size();
}


void
DCPVideo::convert_to_xyz(uint16_t* dst) const
{
	auto image = _frame->image(bind(&PlayerVideo::keep_xyz_or_rgb, _1), VideoRange::FULL, false);
	if (_frame->colour_conversion()) {
		dcp::rgb_to_xyz (
			image->data()[0],
			dst,
			image->size(),
			image->stride()[0],
			_frame->colour_conversion().get()
			);
	}
}


/** J2K-encode this frame on the local host.
 *  @return Encoded data.
 */
ArrayData
DCPVideo::encode_locally () const
{
	auto const comment = Config::instance()->dcp_j2k_comment();

	ArrayData enc = {};
	/* This was empirically derived by a user: see #1902 */
	int const minimum_size = 16384;
	LOG_DEBUG_ENCODE("Using minimum frame size %1", minimum_size);

	auto xyz = convert_to_xyz(_frame);
	int noise_amount = 2;
	int pixel_skip = 16;
	while (true) {
		enc = dcp::compress_j2k (
			xyz,
			_j2k_bandwidth,
			_frames_per_second,
			_frame->eyes() == Eyes::LEFT || _frame->eyes() == Eyes::RIGHT,
			_resolution == Resolution::FOUR_K,
			comment.empty() ? "libdcp" : comment
		);

		if (enc.size() >= minimum_size) {
			LOG_DEBUG_ENCODE(N_("Frame %1 encoded size was OK (%2)"), _index, enc.size());
			break;
		}

		LOG_GENERAL (N_("Frame %1 encoded size was small (%2); adding noise at level %3 with pixel skip %4"), _index, enc.size(), noise_amount, pixel_skip);

		/* The JPEG2000 is too low-bitrate for some decoders <cough>DSS200</cough> so add some noise
		 * and try again.  This is slow but hopefully won't happen too often.  We have to do
		 * convert_to_xyz() again because compress_j2k() corrupts its xyz parameter.
		 */

		xyz = convert_to_xyz(_frame);
		auto size = xyz->size ();
		auto pixels = size.width * size.height;
		dcpomatic::RNG rng(42);
		for (auto c = 0; c < 3; ++c) {
			auto p = xyz->data(c);
			auto e = xyz->data(c) + pixels;
			while (p < e) {
				*p = std::min(4095, std::max(0, *p + (rng.get() % noise_amount)));
				p += pixel_skip;
			}
		}

		if (pixel_skip > 1) {
			--pixel_skip;
		} else {
			++noise_amount;
		}
		/* Something's gone badly wrong if this much noise doesn't help */
		DCPOMATIC_ASSERT (noise_amount < 16);
	}

	switch (_frame->eyes()) {
	case Eyes::BOTH:
		LOG_DEBUG_ENCODE (N_("Finished locally-encoded frame %1 for mono"), _index);
		break;
	case Eyes::LEFT:
		LOG_DEBUG_ENCODE (N_("Finished locally-encoded frame %1 for L"), _index);
		break;
	case Eyes::RIGHT:
		LOG_DEBUG_ENCODE (N_("Finished locally-encoded frame %1 for R"), _index);
		break;
	default:
		break;
	}

	return enc;
}

/** Send this frame to a remote server for J2K encoding, then read the result.
 *  @param serv Server to send to.
 *  @param timeout timeout in seconds.
 *  @return Encoded data.
 */
ArrayData
DCPVideo::encode_remotely (EncodeServerDescription serv, int timeout) const
{
	boost::asio::io_service io_service;
	boost::asio::ip::tcp::resolver resolver (io_service);
	boost::asio::ip::tcp::resolver::query query (serv.host_name(), raw_convert<string> (ENCODE_FRAME_PORT));
	boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve (query);

	auto socket = make_shared<Socket>(timeout);
	socket->set_send_buffer_size (512 * 1024);

	socket->connect (*endpoint_iterator);

	/* Collect all XML metadata */
	xmlpp::Document doc;
	auto root = doc.create_root_node ("EncodingRequest");
	cxml::add_text_child(root, "Version", raw_convert<string>(SERVER_LINK_VERSION));
	add_metadata (root);

	LOG_DEBUG_ENCODE (N_("Sending frame %1 to remote"), _index);

	{
		Socket::WriteDigestScope ds (socket);

		/* Send XML metadata */
		auto xml = doc.write_to_string ("UTF-8");
		socket->write(xml.bytes() + 1);
		socket->write ((uint8_t *) xml.c_str(), xml.bytes() + 1);

		/* Send binary data */
		LOG_TIMING("start-remote-send thread=%1", thread_id ());
		_frame->write_to_socket (socket);
	}

	/* Read the response (JPEG2000-encoded data); this blocks until the data
	   is ready and sent back.
	*/
	Socket::ReadDigestScope ds (socket);
	LOG_TIMING("start-remote-encode thread=%1", thread_id ());
	ArrayData e (socket->read_uint32 ());
	LOG_TIMING("start-remote-receive thread=%1", thread_id ());
	socket->read (e.data(), e.size());
	LOG_TIMING("finish-remote-receive thread=%1", thread_id ());
	if (!ds.check()) {
		throw NetworkError ("Checksums do not match");
	}

	LOG_DEBUG_ENCODE (N_("Finished remotely-encoded frame %1"), _index);

	return e;
}

void
DCPVideo::add_metadata (xmlpp::Element* el) const
{
	cxml::add_text_child(el, "Index", raw_convert<string>(_index));
	cxml::add_text_child(el, "FramesPerSecond", raw_convert<string>(_frames_per_second));
	cxml::add_text_child(el, "J2KBandwidth", raw_convert<string>(_j2k_bandwidth));
	cxml::add_text_child(el, "Resolution", raw_convert<string>(int(_resolution)));
	_frame->add_metadata (el);
}

Eyes
DCPVideo::eyes () const
{
	return _frame->eyes ();
}

/** @return true if this DCPVideo is definitely the same as another;
 *  (apart from the frame index), false if it is probably not.
 */
bool
DCPVideo::same (shared_ptr<const DCPVideo> other) const
{
	if (_frames_per_second != other->_frames_per_second ||
	    _j2k_bandwidth != other->_j2k_bandwidth ||
	    _resolution != other->_resolution) {
		return false;
	}

	return _frame->same (other->_frame);
}
