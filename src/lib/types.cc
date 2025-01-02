/*
    Copyright (C) 2013-2019 Carl Hetherington <cth@carlh.net>

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

#include "types.h"
#include "compose.hpp"
#include "dcpomatic_assert.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/filesystem.h>
#include <dcp/reel_asset.h>
#include <dcp/reel_file_asset.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
LIBDCP_ENABLE_WARNINGS
#include <libcxml/cxml.h>

#include "i18n.h"

using std::max;
using std::min;
using std::string;
using std::list;
using std::shared_ptr;
using std::vector;


CPLSummary::CPLSummary (boost::filesystem::path p)
	: dcp_directory(p.filename().string())
{
	dcp::DCP dcp (p);

	vector<dcp::VerificationNote> notes;
	dcp.read (&notes);
	for (auto i: notes) {
		if (i.code() != dcp::VerificationNote::Code::EXTERNAL_ASSET) {
			/* It's not just a warning about this DCP being a VF */
			throw dcp::ReadError(dcp::note_to_string(i));
		}
	}

	cpl_id = dcp.cpls().front()->id();
	cpl_annotation_text = dcp.cpls().front()->annotation_text();
	cpl_file = dcp.cpls().front()->file().get();

	encrypted = false;
	for (auto j: dcp.cpls()) {
		for (auto k: j->reel_file_assets()) {
			if (k->encrypted()) {
				encrypted = true;
			}
		}
	}

	boost::system::error_code ec;
	auto last_write = dcp::filesystem::last_write_time(p, ec);
	last_write_time = ec ? 0 : last_write;
}


