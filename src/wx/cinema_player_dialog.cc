/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#include "cinema_player_dialog.h"
#include "controls.h"
#include "player_information.h"
#include <boost/shared_ptr.hpp>

using boost::shared_ptr;

CinemaPlayerDialog::CinemaPlayerDialog (wxWindow* parent, shared_ptr<FilmViewer> viewer)
	: wxDialog (parent, wxID_ANY, _("DCP-o-matic Player"))
{
	_controls = new Controls (this, viewer, false, false, false);
	_info = new PlayerInformation (this, viewer);

	wxBoxSizer* s = new wxBoxSizer (wxVERTICAL);
	s->Add (_controls, 0, wxEXPAND | wxALL, 6);
	s->Add (_info, 0, wxEXPAND | wxALL, 6);

	SetSize (640, -1);

	SetSizer (s);
}

void
CinemaPlayerDialog::triggered_update ()
{
	_info->triggered_update ();
}
