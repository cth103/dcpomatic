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
#include "dcpomatic_time.h"
#include "decoder.h"
#include <dcp/atmos_asset_reader.h>


class AtmosMXFContent;


class AtmosMXFDecoder : public Decoder
{
public:
	AtmosMXFDecoder (std::shared_ptr<const Film> film, std::shared_ptr<const AtmosMXFContent>);

	bool pass () override;
	void seek (dcpomatic::ContentTime t, bool accurate) override;

private:
	std::shared_ptr<const AtmosMXFContent> _content;
	dcpomatic::ContentTime _next;
	std::shared_ptr<dcp::AtmosAssetReader> _reader;
	boost::optional<AtmosMetadata> _metadata;
};

