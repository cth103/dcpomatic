/*
    Copyright (C) 2018-2021 Carl Hetherington <cth@carlh.net>

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


#include "check_box.h"
#include "paste_dialog.h"


PasteDialog::PasteDialog (wxWindow* parent, bool video, bool audio, bool text)
	: TableDialog (parent, _("Paste"), 1, 0, true)
{
	_video = new CheckBox (this, _("Paste video settings"));
	_video->Enable (video);
	add (_video);
	_audio = new CheckBox (this, _("Paste audio settings"));
	_audio->Enable (audio);
	add (_audio);
	_text = new CheckBox (this, _("Paste subtitle and caption settings"));
	_text->Enable (text);
	add (_text);

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
PasteDialog::text () const
{
	return _text->GetValue ();
}
