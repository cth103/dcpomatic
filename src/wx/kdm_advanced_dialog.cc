/*
    Copyright (C) 2018-2019 Carl Hetherington <cth@carlh.net>

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
#include "kdm_advanced_dialog.h"
#include "wx_util.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/spinctrl.h>
LIBDCP_ENABLE_WARNINGS


using boost::optional;


KDMAdvancedDialog::KDMAdvancedDialog (wxWindow* parent, bool forensic_mark_video, bool forensic_mark_audio, optional<int> forensic_mark_audio_up_to)
	: TableDialog (parent, _("Advanced KDM options"), 2, 1, false)
{
	_forensic_mark_video = new CheckBox (this, _("Forensically mark video"));
	_forensic_mark_video->SetValue (forensic_mark_video);
	add (_forensic_mark_video);
	add_spacer ();
	_forensic_mark_audio = new CheckBox (this, _("Forensically mark audio"));
	_forensic_mark_audio->SetValue (forensic_mark_audio);
	add (_forensic_mark_audio);
	add_spacer ();

	_forensic_mark_all_audio = new wxRadioButton (this, wxID_ANY, _("Mark all audio channels"));
	_table->Add (_forensic_mark_all_audio, 1, wxEXPAND | wxLEFT, DCPOMATIC_SIZER_GAP);
	add_spacer ();
	wxBoxSizer* hbox = new wxBoxSizer (wxHORIZONTAL);
	_forensic_mark_some_audio = new wxRadioButton (this, wxID_ANY, _("Mark audio channels up to (and including)"));
	hbox->Add (_forensic_mark_some_audio, 1, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_X_GAP);
	_forensic_mark_audio_up_to = new wxSpinCtrl (this, wxID_ANY);
	hbox->Add (_forensic_mark_audio_up_to, 0, wxRIGHT, DCPOMATIC_SIZER_X_GAP);
	_table->Add (hbox, 0, wxLEFT, DCPOMATIC_SIZER_GAP);
	add_spacer ();

	if (forensic_mark_audio_up_to) {
		_forensic_mark_audio_up_to->SetValue (*forensic_mark_audio_up_to);
		_forensic_mark_some_audio->SetValue (true);
	}

	layout ();
	setup_sensitivity ();

	_forensic_mark_audio_up_to->SetRange (1, 15);
	_forensic_mark_audio->bind(&KDMAdvancedDialog::setup_sensitivity, this);
	_forensic_mark_all_audio->Bind (wxEVT_RADIOBUTTON, boost::bind(&KDMAdvancedDialog::setup_sensitivity, this));
	_forensic_mark_some_audio->Bind (wxEVT_RADIOBUTTON, boost::bind(&KDMAdvancedDialog::setup_sensitivity, this));
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

optional<int>
KDMAdvancedDialog::forensic_mark_audio_up_to () const
{
	if (!_forensic_mark_some_audio->GetValue()) {
		return optional<int>();
	}

	return _forensic_mark_audio_up_to->GetValue();
}

void
KDMAdvancedDialog::setup_sensitivity ()
{
	_forensic_mark_all_audio->Enable (_forensic_mark_audio->GetValue());
	_forensic_mark_some_audio->Enable (_forensic_mark_audio->GetValue());
	_forensic_mark_audio_up_to->Enable (_forensic_mark_audio->GetValue() && _forensic_mark_some_audio->GetValue());
}
