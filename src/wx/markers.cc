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


using std::pair;
using std::vector;


vector<pair<wxString, dcp::Marker>>
all_editable_markers()
{
	return vector<pair<wxString, dcp::Marker>>{
		{ _("First frame of ratings band (FFOB)"), dcp::Marker::FFOB },
		{ _("Last frame of ratings band (LFOB)"), dcp::Marker::LFOB },
		{ _("First frame of title credits (FFTC)"), dcp::Marker::FFTC },
		{ _("Last frame of title credits (LFTC)"), dcp::Marker::LFTC },
		{ _("First frame of intermission (FFOI)"), dcp::Marker::FFOI },
		{ _("Last frame of intermission (LFOI)"), dcp::Marker::LFOI },
		{ _("First frame of end credits (FFEC)"), dcp::Marker::FFEC },
		{ _("Last frame of end credits (LFEC)"), dcp::Marker::LFEC },
		{ _("First frame of moving credits (FFMC)"), dcp::Marker::FFMC },
		{ _("Last frame of moving credits (LFMC)"), dcp::Marker::LFMC }
	};
}

