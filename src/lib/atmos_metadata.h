/*
    Copyright (C) 2012-2020 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_ATMOS_METADATA_H
#define DCPOMATIC_ATMOS_METADATA_H

#include <dcp/atmos_asset.h>

class AtmosMetadata
{
public:
	AtmosMetadata (std::shared_ptr<const dcp::AtmosAsset> asset);
	std::shared_ptr<dcp::AtmosAsset> create (dcp::Fraction edit_rate) const;

private:
	int _first_frame;
	int _max_channel_count;
	int _max_object_count;
	int _atmos_version;
};

#endif

