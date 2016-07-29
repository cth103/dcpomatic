/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "lib/config.h"
#include "kdm_output_panel.h"
#include "wx_util.h"
#include "name_format_editor.h"
#include <dcp/types.h>
#ifdef DCPOMATIC_USE_OWN_PICKER
#include "dir_picker_ctrl.h"
#else
#include <wx/filepicker.h>
#endif
#include <wx/stdpaths.h>

KDMOutputPanel::KDMOutputPanel (wxWindow* parent, bool interop)
	: wxPanel (parent, wxID_ANY)
{
	wxFlexGridSizer* table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, 0);

	add_label_to_sizer (table, this, _("KDM type"), true);
	_type = new wxChoice (this, wxID_ANY);
	_type->Append ("Modified Transitional 1", ((void *) dcp::MODIFIED_TRANSITIONAL_1));
	if (!interop) {
		_type->Append ("DCI Any", ((void *) dcp::DCI_ANY));
		_type->Append ("DCI Specific", ((void *) dcp::DCI_SPECIFIC));
	}
	table->Add (_type, 1, wxEXPAND);
	_type->SetSelection (0);

	{
		int flags = wxALIGN_TOP | wxTOP;
		wxString t = _("Filename format");
#ifdef __WXOSX__
		flags |= wxALIGN_RIGHT;
		t += wxT (":");
#endif
		wxStaticText* m = new wxStaticText (this, wxID_ANY, t);
		table->Add (m, 0, flags, DCPOMATIC_SIZER_Y_GAP);
	}

	_filename_format = new NameFormatEditor<KDMNameFormat> (this, Config::instance()->kdm_filename_format());
	NameFormat::Map ex;
	ex["film_name"] = "Bambi";
	ex["cinema"] = "Lumière";
	ex["screen"] = "Screen 1";
	ex["from"] = "2012/03/15 12:30";
	ex["to"] = "2012/03/22 02:30";
	_filename_format->set_example (ex);
	table->Add (_filename_format->panel(), 1, wxEXPAND);

	_write_to = new wxRadioButton (this, wxID_ANY, _("Write to"));
	table->Add (_write_to, 1, wxEXPAND);

#ifdef DCPOMATIC_USE_OWN_PICKER
	_folder = new DirPickerCtrl (this);
#else
	_folder = new wxDirPickerCtrl (this, wxID_ANY, wxEmptyString, wxDirSelectorPromptStr, wxDefaultPosition, wxSize (300, -1));
#endif

	_folder->SetPath (wxStandardPaths::Get().GetDocumentsDir());

	table->Add (_folder, 1, wxEXPAND);

	_email = new wxRadioButton (this, wxID_ANY, _("Send by email"));
	table->Add (_email, 1, wxEXPAND);
	table->AddSpacer (0);

	_write_to->SetValue (true);

	_write_to->Bind (wxEVT_COMMAND_RADIOBUTTON_SELECTED, boost::bind (&KDMOutputPanel::setup_sensitivity, this));
	_email->Bind    (wxEVT_COMMAND_RADIOBUTTON_SELECTED, boost::bind (&KDMOutputPanel::setup_sensitivity, this));

	SetSizer (table);
}

void
KDMOutputPanel::setup_sensitivity ()
{
	_folder->Enable (_write_to->GetValue ());
}

boost::filesystem::path
KDMOutputPanel::directory () const
{
	return wx_to_std (_folder->GetPath ());
}

bool
KDMOutputPanel::write_to () const
{
	return _write_to->GetValue ();
}

dcp::Formulation
KDMOutputPanel::formulation () const
{
	return (dcp::Formulation) reinterpret_cast<intptr_t> (_type->GetClientData (_type->GetSelection()));
}

void
KDMOutputPanel::save_kdm_name_format () const
{
	Config::instance()->set_kdm_filename_format (name_format ());
}

KDMNameFormat
KDMOutputPanel::name_format () const
{
	return _filename_format->get ();
}
