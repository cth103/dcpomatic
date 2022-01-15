/*
    Copyright (C) 2022 Carl Hetherington <cth@carlh.net>

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


#include "markers.h"


using std::make_pair;
using std::pair;
using std::vector;


vector<pair<wxString, dcp::Marker>>
all_markers ()
{
	vector<pair<wxString, dcp::Marker>> out;
	out.push_back (make_pair(_("First frame of composition"), dcp::Marker::FFOC));
	out.push_back (make_pair(_("Last frame of composition"), dcp::Marker::LFOC));
	out.push_back (make_pair(_("First frame of title credits"), dcp::Marker::FFTC));
	out.push_back (make_pair(_("Last frame of title credits"), dcp::Marker::LFTC));
	out.push_back (make_pair(_("First frame of intermission"), dcp::Marker::FFOI));
	out.push_back (make_pair(_("Last frame of intermission"), dcp::Marker::LFOI));
	out.push_back (make_pair(_("First frame of end credits"), dcp::Marker::FFEC));
	out.push_back (make_pair(_("Last frame of end credits"), dcp::Marker::LFEC));
	out.push_back (make_pair(_("First frame of moving credits"), dcp::Marker::FFMC));
	out.push_back (make_pair(_("Last frame of moving credits"), dcp::Marker::LFMC));
	return out;
}

