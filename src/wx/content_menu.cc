/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#include <wx/wx.h>
#include "lib/playlist.h"
#include "lib/film.h"
#include "content_menu.h"
#include "repeat_dialog.h"

using std::cout;
using boost::shared_ptr;

enum {
	ID_repeat,
	ID_remove
};

ContentMenu::ContentMenu (shared_ptr<Film> f, wxWindow* p)
	: _menu (new wxMenu)
	, _film (f)
	, _parent (p)
{
	_menu->Append (ID_repeat, _("Repeat..."));
	_menu->AppendSeparator ();
	_menu->Append (ID_remove, _("Remove"));

	_parent->Bind (wxEVT_COMMAND_MENU_SELECTED, &ContentMenu::repeat, this, ID_repeat);
	_parent->Bind (wxEVT_COMMAND_MENU_SELECTED, &ContentMenu::remove, this, ID_remove);
}

ContentMenu::~ContentMenu ()
{
	delete _menu;
}

void
ContentMenu::popup (ContentList c, wxPoint p)
{
	_content = c;
	_parent->PopupMenu (_menu, p);
}

void
ContentMenu::repeat (wxCommandEvent &)
{
	if (_content.empty ()) {
		return;
	}
		
	RepeatDialog d (_parent);
	d.ShowModal ();

	shared_ptr<const Film> film = _film.lock ();
	if (!film) {
		return;
	}

	film->playlist()->repeat (_content, d.number ());
	d.Destroy ();

	_content.clear ();
}

void
ContentMenu::remove (wxCommandEvent &)
{
	if (_content.empty ()) {
		return;
	}

	shared_ptr<const Film> film = _film.lock ();
	if (!film) {
		return;
	}

	film->playlist()->remove (_content);

	_content.clear ();
}

