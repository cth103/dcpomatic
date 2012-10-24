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
#include "lib/config.h"
#include "lib/server.h"
#include "lib/screen.h"
#include "lib/format.h"
#include "lib/scaler.h"
#include "lib/filter.h"
#include "config_dialog.h"
#include "wx_util.h"
#include "filter_dialog.h"
#include "server_dialog.h"

using namespace std;
using boost::bind;

ConfigDialog::ConfigDialog (wxWindow* parent)
	: wxDialog (parent, wxID_ANY, _("DVD-o-matic Configuration"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	wxFlexGridSizer* table = new wxFlexGridSizer (3, 6, 6);
	table->AddGrowableCol (1, 1);

	add_label_to_sizer (table, this, "TMS IP address");
	_tms_ip = new wxTextCtrl (this, wxID_ANY);
	table->Add (_tms_ip, 1, wxEXPAND);
	table->AddSpacer (0);

	add_label_to_sizer (table, this, "TMS target path");
	_tms_path = new wxTextCtrl (this, wxID_ANY);
	table->Add (_tms_path, 1, wxEXPAND);
	table->AddSpacer (0);

	add_label_to_sizer (table, this, "TMS user name");
	_tms_user = new wxTextCtrl (this, wxID_ANY);
	table->Add (_tms_user, 1, wxEXPAND);
	table->AddSpacer (0);

	add_label_to_sizer (table, this, "TMS password");
	_tms_password = new wxTextCtrl (this, wxID_ANY);
	table->Add (_tms_password, 1, wxEXPAND);
	table->AddSpacer (0);

	add_label_to_sizer (table, this, "Threads to use for encoding on this host");
	_num_local_encoding_threads = new wxSpinCtrl (this);
	table->Add (_num_local_encoding_threads, 1, wxEXPAND);
	table->AddSpacer (0);

	add_label_to_sizer (table, this, "Colour look-up table");
	_colour_lut = new wxComboBox (this, wxID_ANY);
	for (int i = 0; i < 2; ++i) {
		_colour_lut->Append (std_to_wx (colour_lut_index_to_name (i)));
	}
	_colour_lut->SetSelection (0);
	table->Add (_colour_lut, 1, wxEXPAND);
	table->AddSpacer (0);

	add_label_to_sizer (table, this, "JPEG2000 bandwidth");
	_j2k_bandwidth = new wxSpinCtrl (this, wxID_ANY);
	table->Add (_j2k_bandwidth, 1, wxEXPAND);
	add_label_to_sizer (table, this, "MBps");

	add_label_to_sizer (table, this, "Reference scaler for A/B");
	_reference_scaler = new wxComboBox (this, wxID_ANY);
	vector<Scaler const *> const sc = Scaler::all ();
	for (vector<Scaler const *>::const_iterator i = sc.begin(); i != sc.end(); ++i) {
		_reference_scaler->Append (std_to_wx ((*i)->name ()));
	}

	table->Add (_reference_scaler, 1, wxEXPAND);
	table->AddSpacer (0);

	{
		add_label_to_sizer (table, this, "Reference filters for A/B");
		wxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_reference_filters = new wxStaticText (this, wxID_ANY, wxT (""));
		s->Add (_reference_filters, 1, wxEXPAND);
		_reference_filters_button = new wxButton (this, wxID_ANY, _("Edit..."));
		s->Add (_reference_filters_button, 0);
		table->Add (s, 1, wxEXPAND);
		table->AddSpacer (0);
	}

	add_label_to_sizer (table, this, "Encoding Servers");
	_servers = new wxListCtrl (this, wxID_ANY, wxDefaultPosition, wxSize (220, 100), wxLC_REPORT | wxLC_SINGLE_SEL);
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
		_add_server = new wxButton (this, wxID_ANY, _("Add"));
		s->Add (_add_server);
		_edit_server = new wxButton (this, wxID_ANY, _("Edit"));
		s->Add (_edit_server);
		_remove_server = new wxButton (this, wxID_ANY, _("Remove"));
		s->Add (_remove_server);
		table->Add (s, 0);
	}
		
	Config* config = Config::instance ();

	_tms_ip->SetValue (std_to_wx (config->tms_ip ()));
	_tms_ip->Connect (wxID_ANY, wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler (ConfigDialog::tms_ip_changed), 0, this);
	_tms_path->SetValue (std_to_wx (config->tms_path ()));
	_tms_path->Connect (wxID_ANY, wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler (ConfigDialog::tms_path_changed), 0, this);
	_tms_user->SetValue (std_to_wx (config->tms_user ()));
	_tms_user->Connect (wxID_ANY, wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler (ConfigDialog::tms_user_changed), 0, this);
	_tms_password->SetValue (std_to_wx (config->tms_password ()));
	_tms_password->Connect (wxID_ANY, wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler (ConfigDialog::tms_password_changed), 0, this);

	_num_local_encoding_threads->SetRange (1, 128);
	_num_local_encoding_threads->SetValue (config->num_local_encoding_threads ());
	_num_local_encoding_threads->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (ConfigDialog::num_local_encoding_threads_changed), 0, this);

	_colour_lut->Connect (wxID_ANY, wxEVT_COMMAND_COMBOBOX_SELECTED, wxCommandEventHandler (ConfigDialog::colour_lut_changed), 0, this);
	
	_j2k_bandwidth->SetRange (50, 250);
	_j2k_bandwidth->SetValue (rint ((double) config->j2k_bandwidth() / 1e6));
	_j2k_bandwidth->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (ConfigDialog::j2k_bandwidth_changed), 0, this);

	_reference_scaler->SetSelection (Scaler::as_index (config->reference_scaler ()));
	_reference_scaler->Connect (wxID_ANY, wxEVT_COMMAND_COMBOBOX_SELECTED, wxCommandEventHandler (ConfigDialog::reference_scaler_changed), 0, this);

	pair<string, string> p = Filter::ffmpeg_strings (config->reference_filters ());
	_reference_filters->SetLabel (std_to_wx (p.first + " " + p.second));
	_reference_filters_button->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (ConfigDialog::edit_reference_filters_clicked), 0, this);

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
ConfigDialog::colour_lut_changed (wxCommandEvent &)
{
	Config::instance()->set_colour_lut_index (_colour_lut->GetSelection ());
}

void
ConfigDialog::j2k_bandwidth_changed (wxCommandEvent &)
{
	Config::instance()->set_j2k_bandwidth (_j2k_bandwidth->GetValue() * 1e6);
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
ConfigDialog::server_selection_changed (wxListEvent& ev)
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
	_reference_filters->SetLabel (std_to_wx (p.first + " " + p.second));
}
