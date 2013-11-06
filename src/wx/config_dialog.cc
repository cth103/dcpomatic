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

/** @file src/config_dialog.cc
 *  @brief A dialogue to edit DCP-o-matic configuration.
 */

#include <iostream>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <wx/stdpaths.h>
#include <wx/notebook.h>
#include <libdcp/colour_matrix.h>
#include "lib/config.h"
#include "lib/ratio.h"
#include "lib/scaler.h"
#include "lib/filter.h"
#include "lib/dcp_content_type.h"
#include "lib/colour_conversion.h"
#include "config_dialog.h"
#include "wx_util.h"
#include "filter_dialog.h"
#include "dir_picker_ctrl.h"
#include "dci_metadata_dialog.h"
#include "preset_colour_conversion_dialog.h"

using std::vector;
using std::string;
using std::list;
using boost::bind;
using boost::shared_ptr;
using boost::lexical_cast;

ConfigDialog::ConfigDialog (wxWindow* parent)
	: wxDialog (parent, wxID_ANY, _("DCP-o-matic Preferences"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	wxBoxSizer* s = new wxBoxSizer (wxVERTICAL);
	_notebook = new wxNotebook (this, wxID_ANY);
	s->Add (_notebook, 1);

	make_misc_panel ();
	_notebook->AddPage (_misc_panel, _("Miscellaneous"), true);
	make_colour_conversions_panel ();
	_notebook->AddPage (_colour_conversions_panel, _("Colour conversions"), false);
	make_metadata_panel ();
	_notebook->AddPage (_metadata_panel, _("Metadata"), false);
	make_tms_panel ();
	_notebook->AddPage (_tms_panel, _("TMS"), false);
	make_kdm_email_panel ();
	_notebook->AddPage (_kdm_email_panel, _("KDM email"), false);

	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (s, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizer (overall_sizer);
	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);
}

void
ConfigDialog::make_misc_panel ()
{
	_misc_panel = new wxPanel (_notebook);
	wxBoxSizer* s = new wxBoxSizer (wxVERTICAL);
	_misc_panel->SetSizer (s);

	wxFlexGridSizer* table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	table->AddGrowableCol (1, 1);
	s->Add (table, 1, wxALL | wxEXPAND, 8);

	_set_language = new wxCheckBox (_misc_panel, wxID_ANY, _("Set language"));
	table->Add (_set_language, 1);
	_language = new wxChoice (_misc_panel, wxID_ANY);
	_language->Append (wxT ("English"));
	_language->Append (wxT ("Français"));
	_language->Append (wxT ("Italiano"));
	_language->Append (wxT ("Español"));
	_language->Append (wxT ("Svenska"));
	table->Add (_language);

	wxStaticText* restart = add_label_to_sizer (table, _misc_panel, _("(restart DCP-o-matic to see language changes)"), false);
	wxFont font = restart->GetFont();
	font.SetStyle (wxFONTSTYLE_ITALIC);
	font.SetPointSize (font.GetPointSize() - 1);
	restart->SetFont (font);
	table->AddSpacer (0);

	add_label_to_sizer (table, _misc_panel, _("Threads to use for encoding on this host"), true);
	_num_local_encoding_threads = new wxSpinCtrl (_misc_panel);
	table->Add (_num_local_encoding_threads, 1);

	add_label_to_sizer (table, _misc_panel, _("Outgoing mail server"), true);
	_mail_server = new wxTextCtrl (_misc_panel, wxID_ANY);
	table->Add (_mail_server, 1, wxEXPAND | wxALL);

	add_label_to_sizer (table, _misc_panel, _("From address for KDM emails"), true);
	_kdm_from = new wxTextCtrl (_misc_panel, wxID_ANY);
	table->Add (_kdm_from, 1, wxEXPAND | wxALL);
	
	{
		add_label_to_sizer (table, _misc_panel, _("Default duration of still images"), true);
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_default_still_length = new wxSpinCtrl (_misc_panel);
		s->Add (_default_still_length);
		add_label_to_sizer (s, _misc_panel, _("s"), false);
		table->Add (s, 1);
	}

	add_label_to_sizer (table, _misc_panel, _("Default directory for new films"), true);
#ifdef DCPOMATIC_USE_OWN_DIR_PICKER
	_default_directory = new DirPickerCtrl (_misc_panel);
#else	
	_default_directory = new wxDirPickerCtrl (_misc_panel, wxDD_DIR_MUST_EXIST);
#endif
	table->Add (_default_directory, 1, wxEXPAND);

	add_label_to_sizer (table, _misc_panel, _("Default DCI name details"), true);
	_default_dci_metadata_button = new wxButton (_misc_panel, wxID_ANY, _("Edit..."));
	table->Add (_default_dci_metadata_button);

	add_label_to_sizer (table, _misc_panel, _("Default container"), true);
	_default_container = new wxChoice (_misc_panel, wxID_ANY);
	table->Add (_default_container);

	add_label_to_sizer (table, _misc_panel, _("Default content type"), true);
	_default_dcp_content_type = new wxChoice (_misc_panel, wxID_ANY);
	table->Add (_default_dcp_content_type);

	{
		add_label_to_sizer (table, _misc_panel, _("Default JPEG2000 bandwidth"), true);
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_default_j2k_bandwidth = new wxSpinCtrl (_misc_panel);
		s->Add (_default_j2k_bandwidth);
		add_label_to_sizer (s, _misc_panel, _("MBps"), false);
		table->Add (s, 1);
	}
	
	Config* config = Config::instance ();

	_set_language->SetValue (config->language ());

	if (config->language().get_value_or ("") == "fr") {
		_language->SetSelection (1);
	} else if (config->language().get_value_or ("") == "it") {
		_language->SetSelection (2);
	} else if (config->language().get_value_or ("") == "es") {
		_language->SetSelection (3);
	} else if (config->language().get_value_or ("") == "sv") {
		_language->SetSelection (4);
	} else {
		_language->SetSelection (0);
	}

	setup_language_sensitivity ();

	_set_language->Bind (wxEVT_COMMAND_CHECKBOX_CLICKED, boost::bind (&ConfigDialog::set_language_changed, this));
	_language->Bind     (wxEVT_COMMAND_CHOICE_SELECTED,  boost::bind (&ConfigDialog::language_changed,     this));

	_num_local_encoding_threads->SetRange (1, 128);
	_num_local_encoding_threads->SetValue (config->num_local_encoding_threads ());
	_num_local_encoding_threads->Bind (wxEVT_COMMAND_SPINCTRL_UPDATED, boost::bind (&ConfigDialog::num_local_encoding_threads_changed, this));

	_mail_server->SetValue (std_to_wx (config->mail_server ()));
	_mail_server->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ConfigDialog::mail_server_changed, this));
	_kdm_from->SetValue (std_to_wx (config->kdm_from ()));
	_kdm_from->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ConfigDialog::kdm_from_changed, this));

	_default_still_length->SetRange (1, 3600);
	_default_still_length->SetValue (config->default_still_length ());
	_default_still_length->Bind (wxEVT_COMMAND_SPINCTRL_UPDATED, boost::bind (&ConfigDialog::default_still_length_changed, this));

	_default_directory->SetPath (std_to_wx (config->default_directory_or (wx_to_std (wxStandardPaths::Get().GetDocumentsDir())).string ()));
	_default_directory->Bind (wxEVT_COMMAND_DIRPICKER_CHANGED, boost::bind (&ConfigDialog::default_directory_changed, this));

	_default_dci_metadata_button->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&ConfigDialog::edit_default_dci_metadata_clicked, this));

	vector<Ratio const *> ratio = Ratio::all ();
	int n = 0;
	for (vector<Ratio const *>::iterator i = ratio.begin(); i != ratio.end(); ++i) {
		_default_container->Append (std_to_wx ((*i)->nickname ()));
		if (*i == config->default_container ()) {
			_default_container->SetSelection (n);
		}
		++n;
	}

	_default_container->Bind (wxEVT_COMMAND_CHOICE_SELECTED, boost::bind (&ConfigDialog::default_container_changed, this));
	
	vector<DCPContentType const *> const ct = DCPContentType::all ();
	n = 0;
	for (vector<DCPContentType const *>::const_iterator i = ct.begin(); i != ct.end(); ++i) {
		_default_dcp_content_type->Append (std_to_wx ((*i)->pretty_name ()));
		if (*i == config->default_dcp_content_type ()) {
			_default_dcp_content_type->SetSelection (n);
		}
		++n;
	}

	_default_dcp_content_type->Bind (wxEVT_COMMAND_CHOICE_SELECTED, boost::bind (&ConfigDialog::default_dcp_content_type_changed, this));

	_default_j2k_bandwidth->SetRange (50, 250);
	_default_j2k_bandwidth->SetValue (config->default_j2k_bandwidth() / 1000000);
	_default_j2k_bandwidth->Bind (wxEVT_COMMAND_SPINCTRL_UPDATED, boost::bind (&ConfigDialog::default_j2k_bandwidth_changed, this));
}

void
ConfigDialog::make_tms_panel ()
{
	_tms_panel = new wxPanel (_notebook);
	wxBoxSizer* s = new wxBoxSizer (wxVERTICAL);
	_tms_panel->SetSizer (s);

	wxFlexGridSizer* table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	table->AddGrowableCol (1, 1);
	s->Add (table, 1, wxALL | wxEXPAND, 8);

	add_label_to_sizer (table, _tms_panel, _("IP address"), true);
	_tms_ip = new wxTextCtrl (_tms_panel, wxID_ANY);
	table->Add (_tms_ip, 1, wxEXPAND);

	add_label_to_sizer (table, _tms_panel, _("Target path"), true);
	_tms_path = new wxTextCtrl (_tms_panel, wxID_ANY);
	table->Add (_tms_path, 1, wxEXPAND);

	add_label_to_sizer (table, _tms_panel, _("User name"), true);
	_tms_user = new wxTextCtrl (_tms_panel, wxID_ANY);
	table->Add (_tms_user, 1, wxEXPAND);

	add_label_to_sizer (table, _tms_panel, _("Password"), true);
	_tms_password = new wxTextCtrl (_tms_panel, wxID_ANY);
	table->Add (_tms_password, 1, wxEXPAND);

	Config* config = Config::instance ();
	
	_tms_ip->SetValue (std_to_wx (config->tms_ip ()));
	_tms_ip->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ConfigDialog::tms_ip_changed, this));
	_tms_path->SetValue (std_to_wx (config->tms_path ()));
	_tms_path->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ConfigDialog::tms_path_changed, this));
	_tms_user->SetValue (std_to_wx (config->tms_user ()));
	_tms_user->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ConfigDialog::tms_user_changed, this));
	_tms_password->SetValue (std_to_wx (config->tms_password ()));
	_tms_password->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ConfigDialog::tms_password_changed, this));
}

void
ConfigDialog::make_metadata_panel ()
{
	_metadata_panel = new wxPanel (_notebook);
	wxBoxSizer* s = new wxBoxSizer (wxVERTICAL);
	_metadata_panel->SetSizer (s);

	wxFlexGridSizer* table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	table->AddGrowableCol (1, 1);
	s->Add (table, 1, wxALL | wxEXPAND, 8);

	add_label_to_sizer (table, _metadata_panel, _("Issuer"), true);
	_issuer = new wxTextCtrl (_metadata_panel, wxID_ANY);
	table->Add (_issuer, 1, wxEXPAND);

	add_label_to_sizer (table, _metadata_panel, _("Creator"), true);
	_creator = new wxTextCtrl (_metadata_panel, wxID_ANY);
	table->Add (_creator, 1, wxEXPAND);

	Config* config = Config::instance ();

	_issuer->SetValue (std_to_wx (config->dcp_metadata().issuer));
	_issuer->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ConfigDialog::issuer_changed, this));
	_creator->SetValue (std_to_wx (config->dcp_metadata().creator));
	_creator->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ConfigDialog::creator_changed, this));
}

void
ConfigDialog::language_changed ()
{
	switch (_language->GetSelection ()) {
	case 0:
		Config::instance()->set_language ("en");
		break;
	case 1:
		Config::instance()->set_language ("fr");
		break;
	case 2:
		Config::instance()->set_language ("it");
		break;
	case 3:
		Config::instance()->set_language ("es");
		break;
	case 4:
		Config::instance()->set_language ("sv");
		break;
	}
}

void
ConfigDialog::tms_ip_changed ()
{
	Config::instance()->set_tms_ip (wx_to_std (_tms_ip->GetValue ()));
}

void
ConfigDialog::tms_path_changed ()
{
	Config::instance()->set_tms_path (wx_to_std (_tms_path->GetValue ()));
}

void
ConfigDialog::tms_user_changed ()
{
	Config::instance()->set_tms_user (wx_to_std (_tms_user->GetValue ()));
}

void
ConfigDialog::tms_password_changed ()
{
	Config::instance()->set_tms_password (wx_to_std (_tms_password->GetValue ()));
}

void
ConfigDialog::num_local_encoding_threads_changed ()
{
	Config::instance()->set_num_local_encoding_threads (_num_local_encoding_threads->GetValue ());
}

void
ConfigDialog::default_directory_changed ()
{
	Config::instance()->set_default_directory (wx_to_std (_default_directory->GetPath ()));
}

void
ConfigDialog::edit_default_dci_metadata_clicked ()
{
	DCIMetadataDialog* d = new DCIMetadataDialog (this, Config::instance()->default_dci_metadata ());
	d->ShowModal ();
	Config::instance()->set_default_dci_metadata (d->dci_metadata ());
	d->Destroy ();
}

void
ConfigDialog::set_language_changed ()
{
	setup_language_sensitivity ();
	if (_set_language->GetValue ()) {
		language_changed ();
	} else {
		Config::instance()->unset_language ();
	}
}

void
ConfigDialog::setup_language_sensitivity ()
{
	_language->Enable (_set_language->GetValue ());
}

void
ConfigDialog::default_still_length_changed ()
{
	Config::instance()->set_default_still_length (_default_still_length->GetValue ());
}

void
ConfigDialog::default_container_changed ()
{
	vector<Ratio const *> ratio = Ratio::all ();
	Config::instance()->set_default_container (ratio[_default_container->GetSelection()]);
}

void
ConfigDialog::default_dcp_content_type_changed ()
{
	vector<DCPContentType const *> ct = DCPContentType::all ();
	Config::instance()->set_default_dcp_content_type (ct[_default_dcp_content_type->GetSelection()]);
}

void
ConfigDialog::issuer_changed ()
{
	libdcp::XMLMetadata m = Config::instance()->dcp_metadata ();
	m.issuer = wx_to_std (_issuer->GetValue ());
	Config::instance()->set_dcp_metadata (m);
}

void
ConfigDialog::creator_changed ()
{
	libdcp::XMLMetadata m = Config::instance()->dcp_metadata ();
	m.creator = wx_to_std (_creator->GetValue ());
	Config::instance()->set_dcp_metadata (m);
}

void
ConfigDialog::default_j2k_bandwidth_changed ()
{
	Config::instance()->set_default_j2k_bandwidth (_default_j2k_bandwidth->GetValue() * 1000000);
}

static std::string
colour_conversion_column (PresetColourConversion c)
{
	return c.name;
}

void
ConfigDialog::make_colour_conversions_panel ()
{
	vector<string> columns;
	columns.push_back (wx_to_std (_("Name")));
	_colour_conversions_panel = new EditableList<PresetColourConversion, PresetColourConversionDialog> (
		_notebook,
		columns,
		boost::bind (&Config::colour_conversions, Config::instance()),
		boost::bind (&Config::set_colour_conversions, Config::instance(), _1),
		boost::bind (&colour_conversion_column, _1)
		);
}

void
ConfigDialog::mail_server_changed ()
{
	Config::instance()->set_mail_server (wx_to_std (_mail_server->GetValue ()));
}


void
ConfigDialog::kdm_from_changed ()
{
	Config::instance()->set_kdm_from (wx_to_std (_kdm_from->GetValue ()));
}

void
ConfigDialog::make_kdm_email_panel ()
{
	_kdm_email_panel = new wxPanel (_notebook);
	wxBoxSizer* s = new wxBoxSizer (wxVERTICAL);
	_kdm_email_panel->SetSizer (s);

	_kdm_email = new wxTextCtrl (_kdm_email_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
	s->Add (_kdm_email, 1, wxEXPAND | wxALL);

	_kdm_email->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ConfigDialog::kdm_email_changed, this));
	_kdm_email->SetValue (wx_to_std (Config::instance()->kdm_email ()));
}

void
ConfigDialog::kdm_email_changed ()
{
	Config::instance()->set_kdm_email (wx_to_std (_kdm_email->GetValue ()));
}
