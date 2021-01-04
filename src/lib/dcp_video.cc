/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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

#include "dcp_video.h"
#include "config.h"
#include "exceptions.h"
#include "encode_server_description.h"
#include "dcpomatic_socket.h"
#include "image.h"
#include "log.h"
#include "dcpomatic_log.h"
#include "cross.h"
#include "player_video.h"
#include "compose.hpp"
#include "warnings.h"
#include <libcxml/cxml.h>
#include <dcp/raw_convert.h>
#include <dcp/openjpeg_image.h>
#include <dcp/rgb_xyz.h>
#include <dcp/j2k.h>
DCPOMATIC_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
DCPOMATIC_ENABLE_WARNINGS
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <stdint.h>
#include <iomanip>
#include <iostream>

#include "i18n.h"

using std::string;
using std::cout;
using std::shared_ptr;
using dcp::Size;
using dcp::ArrayData;
using dcp::raw_convert;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif

#define DCI_COEFFICENT (48.0 / 52.37)

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
	_resolution = Resolution (node->optional_number_child<int>("Resolution").get_value_or (RESOLUTION_2K));
}

shared_ptr<dcp::OpenJPEGImage>
DCPVideo::convert_to_xyz (shared_ptr<const PlayerVideo> frame, dcp::NoteHandler note)
{
	shared_ptr<dcp::OpenJPEGImage> xyz;

	shared_ptr<Image> image = frame->image (bind (&PlayerVideo::keep_xyz_or_rgb, _1), VIDEO_RANGE_FULL, true, false);
	if (frame->colour_conversion()) {
		xyz = dcp::rgb_to_xyz (
			image->data()[0],
			image->size(),
			image->stride()[0],
			frame->colour_conversion().get(),
			note
			);
	} else {
		xyz.reset (new dcp::OpenJPEGImage (image->data()[0], image->size(), image->stride()[0]));
	}

	return xyz;
}

/** J2K-encode this frame on the local host.
 *  @return Encoded data.
 */
ArrayData
DCPVideo::encode_locally ()
{
	string const comment = Config::instance()->dcp_j2k_comment();

	ArrayData enc = dcp::compress_j2k (
		convert_to_xyz (_frame, boost::bind(&Log::dcp_log, dcpomatic_log.get(), _1, _2)),
		_j2k_bandwidth,
		_frames_per_second,
		_frame->eyes() == EYES_LEFT || _frame->eyes() == EYES_RIGHT,
		_resolution == RESOLUTION_4K,
		comment.empty() ? "libdcp" : comment
		);

	switch (_frame->eyes()) {
	case EYES_BOTH:
		LOG_DEBUG_ENCODE (N_("Finished locally-encoded frame %1 for mono"), _index);
		break;
	case EYES_LEFT:
		LOG_DEBUG_ENCODE (N_("Finished locally-encoded frame %1 for L"), _index);
		break;
	case EYES_RIGHT:
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
DCPVideo::encode_remotely (EncodeServerDescription serv, int timeout)
{
	boost::asio::io_service io_service;
	boost::asio::ip::tcp::resolver resolver (io_service);
	boost::asio::ip::tcp::resolver::query query (serv.host_name(), raw_convert<string> (ENCODE_FRAME_PORT));
	boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve (query);

	shared_ptr<Socket> socket (new Socket (timeout));

	socket->connect (*endpoint_iterator);

	/* Collect all XML metadata */
	xmlpp::Document doc;
	xmlpp::Element* root = doc.create_root_node ("EncodingRequest");
	root->add_child("Version")->add_child_text (raw_convert<string> (SERVER_LINK_VERSION));
	add_metadata (root);

	LOG_DEBUG_ENCODE (N_("Sending frame %1 to remote"), _index);

	{
		Socket::WriteDigestScope ds (socket);

		/* Send XML metadata */
		string xml = doc.write_to_string ("UTF-8");
		socket->write (xml.length() + 1);
		socket->write ((uint8_t *) xml.c_str(), xml.length() + 1);

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
	el->add_child("Index")->add_child_text (raw_convert<string> (_index));
	el->add_child("FramesPerSecond")->add_child_text (raw_convert<string> (_frames_per_second));
	el->add_child("J2KBandwidth")->add_child_text (raw_convert<string> (_j2k_bandwidth));
	el->add_child("Resolution")->add_child_text (raw_convert<string> (int (_resolution)));
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
