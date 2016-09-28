/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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

#include "decoder.h"
#include <dcp/mono_picture_asset_reader.h>
#include <dcp/stereo_picture_asset_reader.h>

class VideoMXFContent;
class Log;

class VideoMXFDecoder : public Decoder
{
public:
	VideoMXFDecoder (boost::shared_ptr<const VideoMXFContent>, boost::shared_ptr<Log> log);

private:
	bool pass (PassReason, bool accurate);
	void seek (ContentTime t, bool accurate);

	boost::shared_ptr<const VideoMXFContent> _content;
	/** Time of next thing to return from pass */
	ContentTime _next;

	boost::shared_ptr<dcp::MonoPictureAssetReader> _mono_reader;
	boost::shared_ptr<dcp::StereoPictureAssetReader> _stereo_reader;
	dcp::Size _size;
};
