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

/** @file src/content_type.cc
 *  @brief A description of the type of content for a DCP (e.g. feature, trailer etc.)
 */

#include "dcp_content_type.h"
#include "dcpomatic_assert.h"

#include "i18n.h"

using namespace std;

vector<DCPContentType const *> DCPContentType::_dcp_content_types;

DCPContentType::DCPContentType (string p, dcp::ContentKind k, string d)
	: _pretty_name (p)
	, _libdcp_kind (k)
	, _isdcf_name (d)
{

}

void
DCPContentType::setup_dcp_content_types ()
{
	_dcp_content_types.push_back (new DCPContentType(_("Feature"), dcp::ContentKind::FEATURE, N_("FTR")));
	_dcp_content_types.push_back (new DCPContentType(_("Short"), dcp::ContentKind::SHORT, N_("SHR")));
	_dcp_content_types.push_back (new DCPContentType(_("Trailer"), dcp::ContentKind::TRAILER, N_("TLR")));
	_dcp_content_types.push_back (new DCPContentType(_("Test"), dcp::ContentKind::TEST, N_("TST")));
	_dcp_content_types.push_back (new DCPContentType(_("Transitional"), dcp::ContentKind::TRANSITIONAL, N_("XSN")));
	_dcp_content_types.push_back (new DCPContentType(_("Rating"), dcp::ContentKind::RATING, N_("RTG")));
	_dcp_content_types.push_back (new DCPContentType(_("Teaser"), dcp::ContentKind::TEASER, N_("TSR")));
	_dcp_content_types.push_back (new DCPContentType(_("Policy"), dcp::ContentKind::POLICY, N_("POL")));
	_dcp_content_types.push_back (new DCPContentType(_("Public Service Announcement"), dcp::ContentKind::PUBLIC_SERVICE_ANNOUNCEMENT, N_("PSA")));
	_dcp_content_types.push_back (new DCPContentType(_("Advertisement"), dcp::ContentKind::ADVERTISEMENT, N_("ADV")));
	_dcp_content_types.push_back (new DCPContentType(_("Episode"), dcp::ContentKind::EPISODE, N_("EPS")));
	_dcp_content_types.push_back (new DCPContentType(_("Promo"), dcp::ContentKind::PROMO, N_("PRO")));
}

DCPContentType const *
DCPContentType::from_isdcf_name (string n)
{
	for (auto i: _dcp_content_types) {
		if (i->isdcf_name() == n) {
			return i;
		}
	}

	return 0;
}

DCPContentType const *
DCPContentType::from_libdcp_kind (dcp::ContentKind kind)
{
	for (auto i: _dcp_content_types) {
		if (i->libdcp_kind() == kind) {
			return i;
		}
	}

	DCPOMATIC_ASSERT (false);
	return 0;
}


DCPContentType const *
DCPContentType::from_index (int n)
{
	DCPOMATIC_ASSERT (n >= 0 && n < int (_dcp_content_types.size ()));
	return _dcp_content_types[n];
}

int
DCPContentType::as_index (DCPContentType const * c)
{
	vector<DCPContentType*>::size_type i = 0;
	while (i < _dcp_content_types.size() && _dcp_content_types[i] != c) {
		++i;
	}

	if (i == _dcp_content_types.size ()) {
		return -1;
	}

	return i;
}

vector<DCPContentType const *>
DCPContentType::all ()
{
	return _dcp_content_types;
}
