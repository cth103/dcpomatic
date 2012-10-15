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

#include <cassert>
#include "dcp_content_type.h"

using namespace std;

vector<DCPContentType const *> DCPContentType::_dcp_content_types;

DCPContentType::DCPContentType (string p, libdcp::ContentKind k, string d)
	: _pretty_name (p)
	, _libdcp_kind (k)
	, _dci_name (d)
{

}

void
DCPContentType::setup_dcp_content_types ()
{
	_dcp_content_types.push_back (new DCPContentType ("Feature", libdcp::FEATURE, "FTR"));
	_dcp_content_types.push_back (new DCPContentType ("Short", libdcp::SHORT, "SHR"));
	_dcp_content_types.push_back (new DCPContentType ("Trailer", libdcp::TRAILER, "TLR"));
	_dcp_content_types.push_back (new DCPContentType ("Test", libdcp::TEST, "TST"));
	_dcp_content_types.push_back (new DCPContentType ("Transitional", libdcp::TRANSITIONAL, "XSN"));
	_dcp_content_types.push_back (new DCPContentType ("Rating", libdcp::RATING, "RTG"));
	_dcp_content_types.push_back (new DCPContentType ("Teaser", libdcp::TEASER, "TSR"));
	_dcp_content_types.push_back (new DCPContentType ("Policy", libdcp::POLICY, "POL"));
	_dcp_content_types.push_back (new DCPContentType ("Public Service Announcement", libdcp::PUBLIC_SERVICE_ANNOUNCEMENT, "PSA"));
	_dcp_content_types.push_back (new DCPContentType ("Advertisement", libdcp::ADVERTISEMENT, "ADV"));
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
