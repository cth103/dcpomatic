/*
    Copyright (C) 2020-2021 Carl Hetherington <cth@carlh.net>

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


#include "atmos_metadata.h"
#include "content_atmos.h"
#include "decoder_part.h"
#include <boost/signals2.hpp>


class AtmosDecoder : public DecoderPart
{
public:
	AtmosDecoder (Decoder* parent, std::shared_ptr<const Content> content);

	boost::optional<dcpomatic::ContentTime> position (std::shared_ptr<const Film>) const override {
		return _position;
	}

	void seek () override;

	void emit (std::shared_ptr<const Film> film, std::shared_ptr<const dcp::AtmosFrame> data, Frame frame, AtmosMetadata metadata);

	boost::signals2::signal<void (ContentAtmos)> Data;

private:
	std::shared_ptr<const Content> _content;
	boost::optional<dcpomatic::ContentTime> _position;
};
