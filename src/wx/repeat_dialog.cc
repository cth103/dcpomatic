/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

#include "repeat_dialog.h"
#include "wx_util.h"

RepeatDialog::RepeatDialog (wxWindow* parent)
	: TableDialog (parent, _("Repeat Content"), 3, true)
{
	add (_("Repeat"), true);
	_number = add (new wxSpinCtrl (this, wxID_ANY));
	add (_("times"), false);

	_number->SetRange (1, 1024);

	layout ();
}

int
RepeatDialog::number () const
{
	return _number->GetValue ();
}
