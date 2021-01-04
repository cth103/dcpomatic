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


#include "atmos_metadata.h"
#include <dcp/atmos_asset.h>


using std::shared_ptr;


AtmosMetadata::AtmosMetadata (shared_ptr<const dcp::AtmosAsset> asset)
	: _first_frame (asset->first_frame())
	, _max_channel_count (asset->max_channel_count())
	, _max_object_count (asset->max_object_count())
	, _atmos_version (asset->atmos_version())
{

}


shared_ptr<dcp::AtmosAsset>
AtmosMetadata::create (dcp::Fraction edit_rate) const
{
	return shared_ptr<dcp::AtmosAsset> (new dcp::AtmosAsset(edit_rate, _first_frame, _max_channel_count, _max_object_count, _atmos_version));
}
