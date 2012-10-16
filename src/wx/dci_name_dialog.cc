/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

#include <wx/sizer.h>
#include "dci_name_dialog.h"
#include "wx_util.h"
#include "film.h"

DCINameDialog::DCINameDialog (wxWindow* parent, Film* film)
	: wxDialog (parent, wxID_ANY, _("DCI name"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
	, _film (film)
{
	wxFlexGridSizer* table = new wxFlexGridSizer (2, 6, 6);
	table->AddGrowableCol (1, 1);

	add_label_to_sizer (table, this, "Audio Language (e.g. EN)");
	_audio_language = new wxTextCtrl (this, wxID_ANY);
	table->Add (_audio_language, 1, wxEXPAND);

	add_label_to_sizer (table, this, "Subtitle Language (e.g. FR)");
	_subtitle_language = new wxTextCtrl (this, wxID_ANY);
	table->Add (_subtitle_language, 1, wxEXPAND);
	
	add_label_to_sizer (table, this, "Territory (e.g. UK)");
	_territory = new wxTextCtrl (this, wxID_ANY);
	table->Add (_territory, 1, wxEXPAND);

	add_label_to_sizer (table, this, "Rating (e.g. 15");
	_rating = new wxTextCtrl (this, wxID_ANY);
	table->Add (_rating, 1, wxEXPAND);

	add_label_to_sizer (table, this, "Studio (e.g. TCF)");
	_studio = new wxTextCtrl (this, wxID_ANY);
	table->Add (_studio, 1, wxEXPAND);

	add_label_to_sizer (table, this, "Facility (e.g. DLA)");
	_facility = new wxTextCtrl (this, wxID_ANY);
	table->Add (_facility, 1, wxEXPAND);

	add_label_to_sizer (table, this, "Package Type (e.g. OV)");
	_package_type = new wxTextCtrl (this, wxID_ANY);
	table->Add (_package_type, 1, wxEXPAND);

	_audio_language->SetValue (std_to_wx (_film->audio_language ()));
	_subtitle_language->SetValue (std_to_wx (_film->subtitle_language ()));
	_territory->SetValue (std_to_wx (_film->territory ()));
	_rating->SetValue (std_to_wx (_film->rating ()));
	_studio->SetValue (std_to_wx (_film->studio ()));
	_facility->SetValue (std_to_wx (_film->facility ()));
	_package_type->SetValue (std_to_wx (_film->package_type ()));
	
	_audio_language->Connect (wxID_ANY, wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler (DCINameDialog::audio_language_changed), 0, this);
	_subtitle_language->Connect (wxID_ANY, wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler (DCINameDialog::subtitle_language_changed), 0, this);
	_territory->Connect (wxID_ANY, wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler (DCINameDialog::territory_changed), 0, this);
	_rating->Connect (wxID_ANY, wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler (DCINameDialog::rating_changed), 0, this);
	_studio->Connect (wxID_ANY, wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler (DCINameDialog::studio_changed), 0, this);
	_facility->Connect (wxID_ANY, wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler (DCINameDialog::facility_changed), 0, this);
	_package_type->Connect (wxID_ANY, wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler (DCINameDialog::package_type_changed), 0, this);

	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (table, 1, wxEXPAND | wxALL, 6);
	
	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}
	
	SetSizer (overall_sizer);
	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);
}

void
DCINameDialog::audio_language_changed (wxCommandEvent &)
{
	_film->set_audio_language (wx_to_std (_audio_language->GetValue ()));
}

void
DCINameDialog::subtitle_language_changed (wxCommandEvent &)
{
	_film->set_subtitle_language (wx_to_std (_subtitle_language->GetValue ()));
}

void
DCINameDialog::territory_changed (wxCommandEvent &)
{
	_film->set_territory (wx_to_std (_territory->GetValue ()));
}

void
DCINameDialog::rating_changed (wxCommandEvent &)
{
	_film->set_rating (wx_to_std (_rating->GetValue ()));
}

void
DCINameDialog::studio_changed (wxCommandEvent &)
{
	_film->set_studio (wx_to_std (_studio->GetValue ()));
}

void
DCINameDialog::facility_changed (wxCommandEvent &)
{
	_film->set_facility (wx_to_std (_facility->GetValue ()));
}

void
DCINameDialog::package_type_changed (wxCommandEvent &)
{
	_film->set_package_type (wx_to_std (_package_type->GetValue ()));
}
