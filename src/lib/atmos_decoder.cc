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


#include "atmos_decoder.h"
#include "content.h"
#include "dcpomatic_assert.h"
#include "dcpomatic_time.h"
#include "decoder.h"
#include "film.h"


using std::shared_ptr;


AtmosDecoder::AtmosDecoder (Decoder* parent, shared_ptr<const Content> content)
	: DecoderPart (parent)
	, _content (content)
{

}


void
AtmosDecoder::seek ()
{
	_position = boost::none;
}


void
AtmosDecoder::emit (shared_ptr<const Film> film, shared_ptr<const dcp::AtmosFrame> data, Frame frame, AtmosMetadata metadata)
{
	Data (ContentAtmos(data, frame, metadata));
	/* There's no fiddling with frame rates when we are using Atmos; the DCP rate must be the same as the Atmos one */
	_position = dcpomatic::ContentTime::from_frames (frame, film->video_frame_rate());
}
