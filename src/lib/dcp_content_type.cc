/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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

/** @file src/content_type.cc
 *  @brief A description of the type of content for a DCP (e.g. feature, trailer etc.)
 */

#include "dcp_content_type.h"
#include "dcpomatic_assert.h"

#include "i18n.h"


using std::string;
using std::vector;
using boost::optional;


vector<DCPContentType> DCPContentType::_dcp_content_types;


DCPContentType::DCPContentType (string p, dcp::ContentKind k, string d)
	: _pretty_name (p)
	, _libdcp_kind (k)
	, _isdcf_name (d)
{

}


void
DCPContentType::setup_dcp_content_types ()
{
	/// TRANSLATORS: these are the types that a DCP can have, explained in some
	/// more detail here: https://registry-page.isdcf.com/contenttypes/
	_dcp_content_types = {
		DCPContentType(_("Feature"), dcp::ContentKind::FEATURE, N_("FTR")),
		DCPContentType(_("Short"), dcp::ContentKind::SHORT, N_("SHR")),
		DCPContentType(_("Trailer"), dcp::ContentKind::TRAILER, N_("TLR")),
		DCPContentType(_("Test"), dcp::ContentKind::TEST, N_("TST")),
		DCPContentType(_("Transitional"), dcp::ContentKind::TRANSITIONAL, N_("XSN")),
		DCPContentType(_("Rating"), dcp::ContentKind::RATING, N_("RTG")),
		DCPContentType(_("Teaser"), dcp::ContentKind::TEASER, N_("TSR")),
		DCPContentType(_("Policy"), dcp::ContentKind::POLICY, N_("POL")),
		DCPContentType(_("Public Service Announcement"), dcp::ContentKind::PUBLIC_SERVICE_ANNOUNCEMENT, N_("PSA")),
		DCPContentType(_("Advertisement"), dcp::ContentKind::ADVERTISEMENT, N_("ADV")),
		DCPContentType(_("Clip"), dcp::ContentKind::CLIP, N_("CLP")),
		DCPContentType(_("Promo"), dcp::ContentKind::PROMO, N_("PRO")),
		DCPContentType(_("Stereo card"), dcp::ContentKind::STEREOCARD, N_("STR")),
		DCPContentType(_("Episode"), dcp::ContentKind::EPISODE, N_("EPS")),
		DCPContentType(_("Highlights"), dcp::ContentKind::HIGHLIGHTS, N_("HLT")),
		DCPContentType(_("Event"), dcp::ContentKind::EVENT, N_("EVT")),
	};
}


DCPContentType const *
DCPContentType::from_isdcf_name (string n)
{
	for (auto& i: _dcp_content_types) {
		if (i.isdcf_name() == n) {
			return &i;
		}
	}

	return nullptr;
}


DCPContentType const *
DCPContentType::from_libdcp_kind (dcp::ContentKind kind)
{
	for (auto& i: _dcp_content_types) {
		if (i.libdcp_kind() == kind) {
			return &i;
		}
	}

	DCPOMATIC_ASSERT (false);
	return nullptr;
}


DCPContentType const *
DCPContentType::from_index (int n)
{
	DCPOMATIC_ASSERT (n >= 0 && n < int(_dcp_content_types.size()));
	return &_dcp_content_types[n];
}


optional<int>
DCPContentType::as_index (DCPContentType const * c)
{
	vector<DCPContentType>::size_type i = 0;
	while (i < _dcp_content_types.size() && &_dcp_content_types[i] != c) {
		++i;
	}

	if (i == _dcp_content_types.size()) {
		return {};
	}

	return i;
}


vector<DCPContentType const *>
DCPContentType::all ()
{
	vector<DCPContentType const *> raw;
	for (auto& type: _dcp_content_types) {
		raw.push_back (&type);
	}
	return raw;
}
