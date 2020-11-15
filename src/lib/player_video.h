/*
    Copyright (C) 2013-2020 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_PLAYER_VIDEO_H
#define DCPOMATIC_PLAYER_VIDEO_H

#include "types.h"
#include "position.h"
#include "dcpomatic_time.h"
#include "colour_conversion.h"
#include "position_image.h"
extern "C" {
#include <libavutil/pixfmt.h>
}
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/noncopyable.hpp>

class Image;
class ImageProxy;
class Film;
class Socket;

/** Everything needed to describe a video frame coming out of the player, but with the
 *  bits still their raw form.  We may want to combine the bits on a remote machine,
 *  or maybe not even bother to combine them at all.
 */
class PlayerVideo : public boost::noncopyable
{
public:
	PlayerVideo (
		boost::shared_ptr<const ImageProxy>,
		Crop,
		boost::optional<double>,
		dcp::Size,
		dcp::Size,
		Eyes,
		Part,
		boost::optional<ColourConversion>,
		VideoRange video_range,
		boost::weak_ptr<Content>,
		boost::optional<Frame>,
		bool error
		);

	PlayerVideo (boost::shared_ptr<cxml::Node>, boost::shared_ptr<Socket>);

	boost::shared_ptr<PlayerVideo> shallow_copy () const;

	void set_text (PositionImage);

	void prepare (boost::function<AVPixelFormat (AVPixelFormat)> pixel_format, VideoRange video_range, bool aligned, bool fast);
	boost::shared_ptr<Image> image (boost::function<AVPixelFormat (AVPixelFormat)> pixel_format, VideoRange video_range, bool aligned, bool fast) const;

	static AVPixelFormat force (AVPixelFormat, AVPixelFormat);
	static AVPixelFormat keep_xyz_or_rgb (AVPixelFormat);

	void add_metadata (xmlpp::Node* node) const;
	void write_to_socket (boost::shared_ptr<Socket> socket) const;

	bool reset_metadata (boost::shared_ptr<const Film> film, dcp::Size video_container_size, dcp::Size film_frame_size);

	bool has_j2k () const;
	boost::shared_ptr<const dcp::Data> j2k () const;

	Eyes eyes () const {
		return _eyes;
	}

	void set_eyes (Eyes e) {
		_eyes = e;
	}

	boost::optional<ColourConversion> colour_conversion () const {
		return _colour_conversion;
	}

	/** @return Position of the content within the overall image once it has been scaled up */
	Position<int> inter_position () const;

	/** @return Size of the content within the overall image once it has been scaled up */
	dcp::Size inter_size () const {
		return _inter_size;
	}

	bool same (boost::shared_ptr<const PlayerVideo> other) const;

	size_t memory_used () const;

	boost::weak_ptr<Content> content () const {
		return _content;
	}

	bool error () const {
		return _error;
	}

private:
	void make_image (boost::function<AVPixelFormat (AVPixelFormat)> pixel_format, VideoRange video_range, bool aligned, bool fast) const;

	boost::shared_ptr<const ImageProxy> _in;
	Crop _crop;
	boost::optional<double> _fade;
	dcp::Size _inter_size;
	dcp::Size _out_size;
	Eyes _eyes;
	Part _part;
	boost::optional<ColourConversion> _colour_conversion;
	VideoRange _video_range;
	boost::optional<PositionImage> _text;
	/** Content that we came from.  This is so that reset_metadata() can work, and also
	 *  for variant:swaroop's non-skippable ads.
	 */
	boost::weak_ptr<Content> _content;
	/** Video frame that we came from.  Again, this is for reset_metadata() */
	boost::optional<Frame> _video_frame;

	mutable boost::mutex _mutex;
	mutable boost::shared_ptr<Image> _image;
	/** _crop that was used to make _image */
	mutable Crop _image_crop;
	/** _inter_size that was used to make _image */
	mutable dcp::Size _image_inter_size;
	/** _out_size that was used to make _image */
	mutable dcp::Size _image_out_size;
	/** _fade that was used to make _image */
	mutable boost::optional<double> _image_fade;
	/** true if there was an error when decoding our image */
	mutable bool _error;
};

#endif
