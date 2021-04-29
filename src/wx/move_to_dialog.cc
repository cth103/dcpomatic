/*
    Copyright (C) 2016-2021 Carl Hetherington <cth@carlh.net>

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


#include "move_to_dialog.h"
#include "lib/film.h"
#include <wx/spinctrl.h>


using std::list;
using std::shared_ptr;
using boost::optional;
using namespace dcpomatic;


MoveToDialog::MoveToDialog (wxWindow* parent, optional<DCPTime> position, shared_ptr<const Film> film)
	: TableDialog (parent, _("Move content"), 2, 0, true)
	, _film (film)
{
	add (_("Start of reel"), true);
	_reel = new wxSpinCtrl (this, wxID_ANY);
	_reel->SetRange (1, film->reels().size());
	add (_reel);

	layout ();

	if (position) {
		int j = 0;
		for (auto i: film->reels()) {
			if (i.from == position.get()) {
				_reel->SetValue (j + 1);
			}
			++j;
		}
	}
}


DCPTime
MoveToDialog::position () const
{
	auto film = _film.lock ();
	DCPOMATIC_ASSERT (film);
	auto reels = film->reels ();
	auto i = reels.begin ();
	for (int j = 0; j < _reel->GetValue() - 1; ++j) {
		DCPOMATIC_ASSERT (i != reels.end());
		++i;
	}

	DCPOMATIC_ASSERT (i != reels.end());
	return i->from;
}
