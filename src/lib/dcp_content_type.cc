/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/** @file src/content_type.cc
 *  @brief A description of the type of content for a DCP (e.g. feature, trailer etc.)
 */

#include "dcp_content_type.h"
#include <cassert>

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
	_dcp_content_types.push_back (new DCPContentType (_("Feature"), dcp::FEATURE, N_("FTR")));
	_dcp_content_types.push_back (new DCPContentType (_("Short"), dcp::SHORT, N_("SHR")));
	_dcp_content_types.push_back (new DCPContentType (_("Trailer"), dcp::TRAILER, N_("TLR")));
	_dcp_content_types.push_back (new DCPContentType (_("Test"), dcp::TEST, N_("TST")));
	_dcp_content_types.push_back (new DCPContentType (_("Transitional"), dcp::TRANSITIONAL, N_("XSN")));
	_dcp_content_types.push_back (new DCPContentType (_("Rating"), dcp::RATING, N_("RTG")));
	_dcp_content_types.push_back (new DCPContentType (_("Teaser"), dcp::TEASER, N_("TSR")));
	_dcp_content_types.push_back (new DCPContentType (_("Policy"), dcp::POLICY, N_("POL")));
	_dcp_content_types.push_back (new DCPContentType (_("Public Service Announcement"), dcp::PUBLIC_SERVICE_ANNOUNCEMENT, N_("PSA")));
	_dcp_content_types.push_back (new DCPContentType (_("Advertisement"), dcp::ADVERTISEMENT, N_("ADV")));
}

DCPContentType const *
DCPContentType::from_pretty_name (string n)
{
	for (vector<DCPContentType const *>::const_iterator i = _dcp_content_types.begin(); i != _dcp_content_types.end(); ++i) {
		if ((*i)->pretty_name() == n) {
			return *i;
		}
	}

	return 0;
}

DCPContentType const *
DCPContentType::from_isdcf_name (string n)
{
	for (vector<DCPContentType const *>::const_iterator i = _dcp_content_types.begin(); i != _dcp_content_types.end(); ++i) {
		if ((*i)->isdcf_name() == n) {
			return *i;
		}
	}

	return 0;
}

DCPContentType const *
DCPContentType::from_index (int n)
{
	assert (n >= 0 && n < int (_dcp_content_types.size ()));
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
