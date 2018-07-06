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

#include "kdm_advanced_dialog.h"

KDMAdvancedDialog::KDMAdvancedDialog (wxWindow* parent, bool forensic_mark_video, bool forensic_mark_audio)
	: TableDialog (parent, _("Advanced KDM options"), 2, 1, false)
{
	_forensic_mark_video = new wxCheckBox (this, wxID_ANY, _("Forensically mark video"));
	_forensic_mark_video->SetValue (forensic_mark_video);
	add (_forensic_mark_video);
	add_spacer ();
	_forensic_mark_audio = new wxCheckBox (this, wxID_ANY, _("Forensically mark audio"));
	_forensic_mark_audio->SetValue (forensic_mark_audio);
	add (_forensic_mark_audio);
	add_spacer ();

	layout ();
}

bool
KDMAdvancedDialog::forensic_mark_video () const
{
	return _forensic_mark_video->GetValue ();
}

bool
KDMAdvancedDialog::forensic_mark_audio () const
{
	return _forensic_mark_audio->GetValue ();
}
