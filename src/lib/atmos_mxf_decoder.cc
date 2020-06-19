/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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

#include "atmos_content.h"
#include "atmos_decoder.h"
#include "atmos_mxf_content.h"
#include "atmos_mxf_decoder.h"
#include "dcpomatic_time.h"
#include <dcp/atmos_asset.h>
#include <dcp/atmos_asset_reader.h>

using boost::shared_ptr;

AtmosMXFDecoder::AtmosMXFDecoder (boost::shared_ptr<const Film> film, boost::shared_ptr<const AtmosMXFContent> content)
	: Decoder (film)
	, _content (content)
{
	atmos.reset (new AtmosDecoder(this, content));

	shared_ptr<dcp::AtmosAsset> asset (new dcp::AtmosAsset(_content->path(0)));
	_reader = asset->start_read ();
	_metadata = AtmosMetadata (asset);
}


bool
AtmosMXFDecoder::pass ()
{
	double const vfr = _content->active_video_frame_rate (film());
	int64_t const frame = _next.frames_round (vfr);

	if (frame >= _content->atmos->length()) {
		return true;
	}

	atmos->emit (film(), _reader->get_frame(frame), frame, *_metadata);
	_next += dcpomatic::ContentTime::from_frames (1, vfr);
	return false;
}


void
AtmosMXFDecoder::seek (dcpomatic::ContentTime t, bool accurate)
{
	Decoder::seek (t, accurate);
	_next = t;
}

