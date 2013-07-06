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
 *  @brief A dialogue to edit DVD-o-matic configuration.
 */

#include <iostream>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <wx/stdpaths.h>
#include <wx/notebook.h>
#include "lib/config.h"
#include "lib/server.h"
#include "lib/format.h"
#include "lib/scaler.h"
#include "lib/filter.h"
#include "lib/dcp_content_type.h"
#include "config_dialog.h"
#include "wx_util.h"
#include "filter_dialog.h"
#include "server_dialog.h"
#include "dir_picker_ctrl.h"
#include "dci_metadata_dialog.h"

using namespace std;
using boost::bind;

ConfigDialog::ConfigDialog (wxWindow* parent)
	: wxDialog (parent, wxID_ANY, _("DVD-o-matic Preferences"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	wxBoxSizer* s = new wxBoxSizer (wxVERTICAL);
	_notebook = new wxNotebook (this, wxID_ANY);
	s->Add (_notebook, 1);

	make_misc_panel ();
	_notebook->AddPage (_misc_panel, _("Miscellaneous"), true);
	make_servers_panel ();
	_notebook->AddPage (_servers_panel, _("Encoding servers"), false);
	make_metadata_panel ();
	_notebook->AddPage (_metadata_panel, _("Metadata"), false);
	make_tms_panel ();
	_notebook->AddPage (_tms_panel, _("TMS"), false);
	make_ab_panel ();
	_notebook->AddPage (_ab_panel, _("A/B mode"), false);

	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (s, 1, wxEXPAND | wxALL, 6);

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

	wxFlexGridSizer* table = new wxFlexGridSizer (3, DVDOMATIC_SIZER_X_GAP, DVDOMATIC_SIZER_Y_GAP);
	table->AddGrowableCol (1, 1);
	s->Add (table, 1, wxALL | wxEXPAND, 8);

	_set_language = new wxCheckBox (_misc_panel, wxID_ANY, _("Set language"));
	table->Add (_set_language, 1, wxEXPAND);
	_language = new wxChoice (_misc_panel, wxID_ANY);
	_language->Append (wxT ("English"));
	_language->Append (wxT ("Français"));
	_language->Append (wxT ("Italiano"));
	_language->Append (wxT ("Español"));
	_language->Append (wxT ("Svenska"));
	table->Add (_language, 1, wxEXPAND);
	table->AddSpacer (0);

	wxStaticText* restart = add_label_to_sizer (table, _misc_panel, _("(restart DVD-o-matic to see language changes)"), false);
	wxFont font = restart->GetFont();
	font.SetStyle (wxFONTSTYLE_ITALIC);
	font.SetPointSize (font.GetPointSize() - 1);
	restart->SetFont (font);
	table->AddSpacer (0);
	table->AddSpacer (0);

	add_label_to_sizer (table, _misc_panel, _("Threads to use for encoding on this host"), true);
	_num_local_encoding_threads = new wxSpinCtrl (_misc_panel);
	table->Add (_num_local_encoding_threads, 1);
	table->AddSpacer (0);

	add_label_to_sizer (table, _misc_panel, _("Default directory for new films"), true);
#ifdef DVDOMATIC_USE_OWN_DIR_PICKER
	_default_directory = new DirPickerCtrl (_misc_panel);
#else	
	_default_directory = new wxDirPickerCtrl (_misc_panel, wxDD_DIR_MUST_EXIST);
#endif
	table->Add (_default_directory, 1, wxEXPAND);
	table->AddSpacer (0);

	add_label_to_sizer (table, _misc_panel, _("Default DCI name details"), true);
	_default_dci_metadata_button = new wxButton (_misc_panel, wxID_ANY, _("Edit..."));
	table->Add (_default_dci_metadata_button);
	table->AddSpacer (1);

	add_label_to_sizer (table, _misc_panel, _("Default format"), true);
	_default_format = new wxChoice (_misc_panel, wxID_ANY);
	table->Add (_default_format);
	table->AddSpacer (1);

	add_label_to_sizer (table, _misc_panel, _("Default content type"), true);
	_default_dcp_content_type = new wxChoice (_misc_panel, wxID_ANY);
	table->Add (_default_dcp_content_type);
	table->AddSpacer (1);
	
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

	_set_language->Connect (wxID_ANY, wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler (ConfigDialog::set_language_changed), 0, this);
	_language->Connect (wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler (ConfigDialog::language_changed), 0, this);

	_num_local_encoding_threads->SetRange (1, 128);
	_num_local_encoding_threads->SetValue (config->num_local_encoding_threads ());
	_num_local_encoding_threads->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (ConfigDialog::num_local_encoding_threads_changed), 0, this);

	_default_directory->SetPath (std_to_wx (config->default_directory_or (wx_to_std (wxStandardPaths::Get().GetDocumentsDir()))));
	_default_directory->Connect (wxID_ANY, wxEVT_COMMAND_DIRPICKER_CHANGED, wxCommandEventHandler (ConfigDialog::default_directory_changed), 0, this);

	_default_dci_metadata_button->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (ConfigDialog::edit_default_dci_metadata_clicked), 0, this);

	vector<Format const *> fmt = Format::all ();
	int n = 0;
	for (vector<Format const *>::iterator i = fmt.begin(); i != fmt.end(); ++i) {
		_default_format->Append (std_to_wx ((*i)->name ()));
		if (*i == config->default_format ()) {
			_default_format->SetSelection (n);
		}
		++n;
	}

	_default_format->Connect (wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler (ConfigDialog::default_format_changed), 0, this);
	
	vector<DCPContentType const *> const ct = DCPContentType::all ();
	n = 0;
	for (vector<DCPContentType const *>::const_iterator i = ct.begin(); i != ct.end(); ++i) {
		_default_dcp_content_type->Append (std_to_wx ((*i)->pretty_name ()));
		if (*i == config->default_dcp_content_type ()) {
			_default_dcp_content_type->SetSelection (n);
		}
		++n;
	}

	_default_dcp_content_type->Connect (wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler (ConfigDialog::default_dcp_content_type_changed), 0, this);
}

void
ConfigDialog::make_tms_panel ()
{
	_tms_panel = new wxPanel (_notebook);
	wxBoxSizer* s = new wxBoxSizer (wxVERTICAL);
	_tms_panel->SetSizer (s);

	wxFlexGridSizer* table = new wxFlexGridSizer (2, DVDOMATIC_SIZER_X_GAP, DVDOMATIC_SIZER_Y_GAP);
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
	_tms_ip->Connect (wxID_ANY, wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler (ConfigDialog::tms_ip_changed), 0, this);
	_tms_path->SetValue (std_to_wx (config->tms_path ()));
	_tms_path->Connect (wxID_ANY, wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler (ConfigDialog::tms_path_changed), 0, this);
	_tms_user->SetValue (std_to_wx (config->tms_user ()));
	_tms_user->Connect (wxID_ANY, wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler (ConfigDialog::tms_user_changed), 0, this);
	_tms_password->SetValue (std_to_wx (config->tms_password ()));
	_tms_password->Connect (wxID_ANY, wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler (ConfigDialog::tms_password_changed), 0, this);
}

void
ConfigDialog::make_metadata_panel ()
{
	_metadata_panel = new wxPanel (_notebook);
	wxBoxSizer* s = new wxBoxSizer (wxVERTICAL);
	_metadata_panel->SetSizer (s);

	wxFlexGridSizer* table = new wxFlexGridSizer (2, DVDOMATIC_SIZER_X_GAP, DVDOMATIC_SIZER_Y_GAP);
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
	_issuer->Connect (wxID_ANY, wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler (ConfigDialog::issuer_changed), 0, this);
	_creator->SetValue (std_to_wx (config->dcp_metadata().creator));
	_creator->Connect (wxID_ANY, wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler (ConfigDialog::creator_changed), 0, this);
}

void
ConfigDialog::make_ab_panel ()
{
	_ab_panel = new wxPanel (_notebook);
	wxBoxSizer* s = new wxBoxSizer (wxVERTICAL);
	_ab_panel->SetSizer (s);

	wxFlexGridSizer* table = new wxFlexGridSizer (3, DVDOMATIC_SIZER_X_GAP, DVDOMATIC_SIZER_Y_GAP);
	table->AddGrowableCol (1, 1);
	s->Add (table, 1, wxALL, 8);
	
	add_label_to_sizer (table, _ab_panel, _("Reference scaler"), true);
	_reference_scaler = new wxChoice (_ab_panel, wxID_ANY);
	vector<Scaler const *> const sc = Scaler::all ();
	for (vector<Scaler const *>::const_iterator i = sc.begin(); i != sc.end(); ++i) {
		_reference_scaler->Append (std_to_wx ((*i)->name ()));
	}

	table->Add (_reference_scaler, 1, wxEXPAND);
	table->AddSpacer (0);

	{
		add_label_to_sizer (table, _ab_panel, _("Reference filters"), true);
		wxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_reference_filters = new wxStaticText (_ab_panel, wxID_ANY, wxT (""));
		s->Add (_reference_filters, 1, wxALIGN_CENTER_VERTICAL | wxEXPAND | wxALL, 6);
		_reference_filters_button = new wxButton (_ab_panel, wxID_ANY, _("Edit..."));
		s->Add (_reference_filters_button, 0);
		table->Add (s, 1, wxEXPAND);
		table->AddSpacer (0);
	}

	Config* config = Config::instance ();
	
	_reference_scaler->SetSelection (Scaler::as_index (config->reference_scaler ()));
	_reference_scaler->Connect (wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler (ConfigDialog::reference_scaler_changed), 0, this);

	pair<string, string> p = Filter::ffmpeg_strings (config->reference_filters ());
	_reference_filters->SetLabel (std_to_wx (p.first) + N_(" ") + std_to_wx (p.second));
	_reference_filters_button->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (ConfigDialog::edit_reference_filters_clicked), 0, this);
}

void
ConfigDialog::make_servers_panel ()
{
	_servers_panel = new wxPanel (_notebook);
	wxBoxSizer* s = new wxBoxSizer (wxVERTICAL);
	_servers_panel->SetSizer (s);

	wxFlexGridSizer* table = new wxFlexGridSizer (2, DVDOMATIC_SIZER_X_GAP, DVDOMATIC_SIZER_Y_GAP);
	table->AddGrowableCol (0, 1);
	s->Add (table, 1, wxALL | wxEXPAND, 8);

	Config* config = Config::instance ();

	_servers = new wxListCtrl (_servers_panel, wxID_ANY, wxDefaultPosition, wxSize (220, 100), wxLC_REPORT | wxLC_SINGLE_SEL);
	wxListItem ip;
	ip.SetId (0);
	ip.SetText (_("IP address"));
	ip.SetWidth (120);
	_servers->InsertColumn (0, ip);
	ip.SetId (1);
	ip.SetText (_("Threads"));
	ip.SetWidth (80);
	_servers->InsertColumn (1, ip);
	table->Add (_servers, 1, wxEXPAND | wxALL);

	{
		wxSizer* s = new wxBoxSizer (wxVERTICAL);
		_add_server = new wxButton (_servers_panel, wxID_ANY, _("Add"));
		s->Add (_add_server, 0, wxTOP | wxBOTTOM, 2);
		_edit_server = new wxButton (_servers_panel, wxID_ANY, _("Edit"));
		s->Add (_edit_server, 0, wxTOP | wxBOTTOM, 2);
		_remove_server = new wxButton (_servers_panel, wxID_ANY, _("Remove"));
		s->Add (_remove_server, 0, wxTOP | wxBOTTOM, 2);
		table->Add (s, 0);
	}

	vector<ServerDescription*> servers = config->servers ();
	for (vector<ServerDescription*>::iterator i = servers.begin(); i != servers.end(); ++i) {
		add_server_to_control (*i);
	}
	
	_add_server->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (ConfigDialog::add_server_clicked), 0, this);
	_edit_server->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (ConfigDialog::edit_server_clicked), 0, this);
	_remove_server->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (ConfigDialog::remove_server_clicked), 0, this);

	_servers->Connect (wxID_ANY, wxEVT_COMMAND_LIST_ITEM_SELECTED, wxListEventHandler (ConfigDialog::server_selection_changed), 0, this);
	_servers->Connect (wxID_ANY, wxEVT_COMMAND_LIST_ITEM_DESELECTED, wxListEventHandler (ConfigDialog::server_selection_changed), 0, this);
	wxListEvent ev;
	server_selection_changed (ev);
}

void
ConfigDialog::language_changed (wxCommandEvent &)
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
ConfigDialog::tms_ip_changed (wxCommandEvent &)
{
	Config::instance()->set_tms_ip (wx_to_std (_tms_ip->GetValue ()));
}

void
ConfigDialog::tms_path_changed (wxCommandEvent &)
{
	Config::instance()->set_tms_path (wx_to_std (_tms_path->GetValue ()));
}

void
ConfigDialog::tms_user_changed (wxCommandEvent &)
{
	Config::instance()->set_tms_user (wx_to_std (_tms_user->GetValue ()));
}

void
ConfigDialog::tms_password_changed (wxCommandEvent &)
{
	Config::instance()->set_tms_password (wx_to_std (_tms_password->GetValue ()));
}

void
ConfigDialog::num_local_encoding_threads_changed (wxCommandEvent &)
{
	Config::instance()->set_num_local_encoding_threads (_num_local_encoding_threads->GetValue ());
}

void
ConfigDialog::default_directory_changed (wxCommandEvent &)
{
	Config::instance()->set_default_directory (wx_to_std (_default_directory->GetPath ()));
}

void
ConfigDialog::add_server_to_control (ServerDescription* s)
{
	wxListItem item;
	int const n = _servers->GetItemCount ();
	item.SetId (n);
	_servers->InsertItem (item);
	_servers->SetItem (n, 0, std_to_wx (s->host_name ()));
	_servers->SetItem (n, 1, std_to_wx (boost::lexical_cast<string> (s->threads ())));
}

void
ConfigDialog::add_server_clicked (wxCommandEvent &)
{
	ServerDialog* d = new ServerDialog (this, 0);
	d->ShowModal ();
	ServerDescription* s = d->server ();
	d->Destroy ();
	
	add_server_to_control (s);
	vector<ServerDescription*> o = Config::instance()->servers ();
	o.push_back (s);
	Config::instance()->set_servers (o);
}

void
ConfigDialog::edit_server_clicked (wxCommandEvent &)
{
	int i = _servers->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (i == -1) {
		return;
	}

	wxListItem item;
	item.SetId (i);
	item.SetColumn (0);
	_servers->GetItem (item);

	vector<ServerDescription*> servers = Config::instance()->servers ();
	assert (i >= 0 && i < int (servers.size ()));

	ServerDialog* d = new ServerDialog (this, servers[i]);
	d->ShowModal ();
	d->Destroy ();

	_servers->SetItem (i, 0, std_to_wx (servers[i]->host_name ()));
	_servers->SetItem (i, 1, std_to_wx (boost::lexical_cast<string> (servers[i]->threads ())));
}

void
ConfigDialog::remove_server_clicked (wxCommandEvent &)
{
	int i = _servers->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (i >= 0) {
		_servers->DeleteItem (i);
	}

	vector<ServerDescription*> o = Config::instance()->servers ();
	o.erase (o.begin() + i);
	Config::instance()->set_servers (o);
}

void
ConfigDialog::server_selection_changed (wxListEvent &)
{
	int const i = _servers->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	_edit_server->Enable (i >= 0);
	_remove_server->Enable (i >= 0);
}

void
ConfigDialog::reference_scaler_changed (wxCommandEvent &)
{
	int const n = _reference_scaler->GetSelection ();
	if (n >= 0) {
		Config::instance()->set_reference_scaler (Scaler::from_index (n));
	}
}

void
ConfigDialog::edit_reference_filters_clicked (wxCommandEvent &)
{
	FilterDialog* d = new FilterDialog (this, Config::instance()->reference_filters ());
	d->ActiveChanged.connect (boost::bind (&ConfigDialog::reference_filters_changed, this, _1));
	d->ShowModal ();
	d->Destroy ();
}

void
ConfigDialog::reference_filters_changed (vector<Filter const *> f)
{
	Config::instance()->set_reference_filters (f);
	pair<string, string> p = Filter::ffmpeg_strings (Config::instance()->reference_filters ());
	_reference_filters->SetLabel (std_to_wx (p.first) + N_(" ") + std_to_wx (p.second));
}

void
ConfigDialog::edit_default_dci_metadata_clicked (wxCommandEvent &)
{
	DCIMetadataDialog* d = new DCIMetadataDialog (this, Config::instance()->default_dci_metadata ());
	d->ShowModal ();
	Config::instance()->set_default_dci_metadata (d->dci_metadata ());
	d->Destroy ();
}

void
ConfigDialog::set_language_changed (wxCommandEvent& ev)
{
	setup_language_sensitivity ();
	if (_set_language->GetValue ()) {
		language_changed (ev);
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
ConfigDialog::default_format_changed (wxCommandEvent &)
{
	vector<Format const *> fmt = Format::all ();
	Config::instance()->set_default_format (fmt[_default_format->GetSelection()]);
}

void
ConfigDialog::default_dcp_content_type_changed (wxCommandEvent &)
{
	vector<DCPContentType const *> ct = DCPContentType::all ();
	Config::instance()->set_default_dcp_content_type (ct[_default_dcp_content_type->GetSelection()]);
}

void
ConfigDialog::issuer_changed (wxCommandEvent &)
{
	libdcp::XMLMetadata m = Config::instance()->dcp_metadata ();
	m.issuer = wx_to_std (_issuer->GetValue ());
	Config::instance()->set_dcp_metadata (m);
}

void
ConfigDialog::creator_changed (wxCommandEvent &)
{
	libdcp::XMLMetadata m = Config::instance()->dcp_metadata ();
	m.creator = wx_to_std (_creator->GetValue ());
	Config::instance()->set_dcp_metadata (m);
}
