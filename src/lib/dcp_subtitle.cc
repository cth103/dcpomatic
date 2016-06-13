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

#include "dcp_subtitle.h"
#include "exceptions.h"
#include "compose.hpp"
#include <dcp/interop_subtitle_asset.h>
#include <dcp/smpte_subtitle_asset.h>

#include "i18n.h"

using std::string;
using std::exception;
using boost::shared_ptr;

shared_ptr<dcp::SubtitleAsset>
DCPSubtitle::load (boost::filesystem::path file) const
{
	shared_ptr<dcp::SubtitleAsset> sc;
	string interop_error;
	string smpte_error;

	try {
		sc.reset (new dcp::InteropSubtitleAsset (file));
	} catch (exception& e) {
		interop_error = e.what ();
	}

	if (!sc) {
		try {
			sc.reset (new dcp::SMPTESubtitleAsset (file));
		} catch (exception& e) {
			smpte_error = e.what();
		}
	}

	if (!sc) {
		throw FileError (String::compose (_("Could not read subtitles (%1 / %2)"), interop_error, smpte_error), file);
	}

	return sc;
}
