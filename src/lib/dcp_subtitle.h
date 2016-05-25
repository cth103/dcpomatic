/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_DCP_SUBTITLE_H
#define DCPOMATIC_DCP_SUBTITLE_H

#include <boost/shared_ptr.hpp>
#include <boost/filesystem.hpp>

namespace dcp {
	class SubtitleAsset;
}

class DCPSubtitle
{
protected:
	boost::shared_ptr<dcp::SubtitleAsset> load (boost::filesystem::path) const;
};

#endif
