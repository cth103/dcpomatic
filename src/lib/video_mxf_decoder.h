/*
    Copyright (C) 2016-2021 Carl Hetherington <cth@carlh.net>

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
	VideoMXFDecoder (std::shared_ptr<const Film> film, std::shared_ptr<const VideoMXFContent>);

	bool pass () override;
	void seek (dcpomatic::ContentTime t, bool accurate) override;

private:

	std::shared_ptr<const VideoMXFContent> _content;
	/** Time of next thing to return from pass */
	dcpomatic::ContentTime _next;

	std::shared_ptr<dcp::MonoPictureAssetReader> _mono_reader;
	std::shared_ptr<dcp::StereoPictureAssetReader> _stereo_reader;
	dcp::Size _size;
};
