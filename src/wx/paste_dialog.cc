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

#include "paste_dialog.h"

PasteDialog::PasteDialog (wxWindow* parent, bool video, bool audio, bool subtitle)
	: TableDialog (parent, _("Paste"), 1, 0, true)
{
	_video = new wxCheckBox (this, wxID_ANY, _("Paste video settings"));
	_video->Enable (video);
	add (_video);
	_audio = new wxCheckBox (this, wxID_ANY, _("Paste audio settings"));
	_audio->Enable (audio);
	add (_audio);
	_subtitle = new wxCheckBox (this, wxID_ANY, _("Paste subtitle settings"));
	_subtitle->Enable (subtitle);
	add (_subtitle);

	layout ();
}

bool
PasteDialog::video () const
{
	return _video->GetValue ();
}

bool
PasteDialog::audio () const
{
	return _audio->GetValue ();
}

bool
PasteDialog::subtitle () const
{
	return _subtitle->GetValue ();
}
