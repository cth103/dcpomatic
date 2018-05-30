/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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

/** @file src/full_config_dialog.cc
 *  @brief A dialogue to edit all DCP-o-matic configuration.
 */

#include "full_config_dialog.h"
#include "wx_util.h"
#include "editable_list.h"
#include "filter_dialog.h"
#include "dir_picker_ctrl.h"
#include "file_picker_ctrl.h"
#include "isdcf_metadata_dialog.h"
#include "server_dialog.h"
#include "make_chain_dialog.h"
#include "email_dialog.h"
#include "name_format_editor.h"
#include "nag_dialog.h"
#include "config_move_dialog.h"
#include "config_dialog.h"
#include "lib/config.h"
#include "lib/ratio.h"
#include "lib/filter.h"
#include "lib/dcp_content_type.h"
#include "lib/log.h"
#include "lib/util.h"
#include "lib/cross.h"
#include "lib/exceptions.h"
#include <dcp/locale_convert.h>
#include <dcp/exceptions.h>
#include <dcp/certificate_chain.h>
#include <wx/stdpaths.h>
#include <wx/preferences.h>
#include <wx/spinctrl.h>
#include <wx/filepicker.h>
#include <RtAudio.h>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <iostream>

using std::vector;
using std::string;
using std::list;
using std::cout;
using std::pair;
using std::make_pair;
using std::map;
using boost::bind;
using boost::shared_ptr;
using boost::function;
using boost::optional;
using dcp::locale_convert;

class FullGeneralPage : public GeneralPage
{
public:
	FullGeneralPage (wxSize panel_size, int border)
		: GeneralPage (panel_size, border)
	{}

private:
	void setup ()
	{
		wxGridBagSizer* table = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		_panel->GetSizer()->Add (table, 1, wxALL | wxEXPAND, _border);

		int r = 0;
		add_language_controls (table, r);

		add_label_to_sizer (table, _panel, _("Number of threads DCP-o-matic should use"), true, wxGBPosition (r, 0));
		_master_encoding_threads = new wxSpinCtrl (_panel);
		table->Add (_master_encoding_threads, wxGBPosition (r, 1));
		++r;

		add_label_to_sizer (table, _panel, _("Number of threads DCP-o-matic encode server should use"), true, wxGBPosition (r, 0));
		_server_encoding_threads = new wxSpinCtrl (_panel);
		table->Add (_server_encoding_threads, wxGBPosition (r, 1));
		++r;

		add_label_to_sizer (table, _panel, _("Configuration file"), true, wxGBPosition (r, 0));
		_config_file = new FilePickerCtrl (_panel, _("Select configuration file"), "*.xml", true);
		table->Add (_config_file, wxGBPosition (r, 1));
		++r;

		add_label_to_sizer (table, _panel, _("Cinema and screen database file"), true, wxGBPosition (r, 0));
		_cinemas_file = new FilePickerCtrl (_panel, _("Select cinema and screen database file"), "*.xml", true);
		table->Add (_cinemas_file, wxGBPosition (r, 1));
		++r;

		add_play_sound_controls (table, r);

#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
		_analyse_ebur128 = new wxCheckBox (_panel, wxID_ANY, _("Find integrated loudness, true peak and loudness range when analysing audio"));
		table->Add (_analyse_ebur128, wxGBPosition (r, 0), wxGBSpan (1, 2));
		++r;
#endif

		_automatic_audio_analysis = new wxCheckBox (_panel, wxID_ANY, _("Automatically analyse content audio"));
		table->Add (_automatic_audio_analysis, wxGBPosition (r, 0), wxGBSpan (1, 2));
		++r;

		add_update_controls (table, r);

		wxFlexGridSizer* bottom_table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		bottom_table->AddGrowableCol (1, 1);

		add_label_to_sizer (bottom_table, _panel, _("Issuer"), true);
		_issuer = new wxTextCtrl (_panel, wxID_ANY);
		bottom_table->Add (_issuer, 1, wxALL | wxEXPAND);

		add_label_to_sizer (bottom_table, _panel, _("Creator"), true);
		_creator = new wxTextCtrl (_panel, wxID_ANY);
		bottom_table->Add (_creator, 1, wxALL | wxEXPAND);

		table->Add (bottom_table, wxGBPosition (r, 0), wxGBSpan (2, 2), wxEXPAND);
		++r;

		_config_file->Bind  (wxEVT_FILEPICKER_CHANGED, boost::bind (&FullGeneralPage::config_file_changed,   this));
		_cinemas_file->Bind (wxEVT_FILEPICKER_CHANGED, boost::bind (&FullGeneralPage::cinemas_file_changed,  this));

		_master_encoding_threads->SetRange (1, 128);
		_master_encoding_threads->Bind (wxEVT_SPINCTRL, boost::bind (&FullGeneralPage::master_encoding_threads_changed, this));
		_server_encoding_threads->SetRange (1, 128);
		_server_encoding_threads->Bind (wxEVT_SPINCTRL, boost::bind (&FullGeneralPage::server_encoding_threads_changed, this));

#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
		_analyse_ebur128->Bind (wxEVT_CHECKBOX, boost::bind (&FullGeneralPage::analyse_ebur128_changed, this));
#endif
		_automatic_audio_analysis->Bind (wxEVT_CHECKBOX, boost::bind (&FullGeneralPage::automatic_audio_analysis_changed, this));

		_issuer->Bind (wxEVT_TEXT, boost::bind (&FullGeneralPage::issuer_changed, this));
		_creator->Bind (wxEVT_TEXT, boost::bind (&FullGeneralPage::creator_changed, this));
	}

	void config_changed ()
	{
		Config* config = Config::instance ();

		checked_set (_master_encoding_threads, config->master_encoding_threads ());
		checked_set (_server_encoding_threads, config->server_encoding_threads ());
#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
		checked_set (_analyse_ebur128, config->analyse_ebur128 ());
#endif
		checked_set (_automatic_audio_analysis, config->automatic_audio_analysis ());
		checked_set (_issuer, config->dcp_issuer ());
		checked_set (_creator, config->dcp_creator ());
		checked_set (_config_file, config->config_file());
		checked_set (_cinemas_file, config->cinemas_file());

		GeneralPage::config_changed ();
	}


#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
	void analyse_ebur128_changed ()
	{
		Config::instance()->set_analyse_ebur128 (_analyse_ebur128->GetValue ());
	}
#endif

	void automatic_audio_analysis_changed ()
	{
		Config::instance()->set_automatic_audio_analysis (_automatic_audio_analysis->GetValue ());
	}

	void master_encoding_threads_changed ()
	{
		Config::instance()->set_master_encoding_threads (_master_encoding_threads->GetValue ());
	}

	void server_encoding_threads_changed ()
	{
		Config::instance()->set_server_encoding_threads (_server_encoding_threads->GetValue ());
	}

	void issuer_changed ()
	{
		Config::instance()->set_dcp_issuer (wx_to_std (_issuer->GetValue ()));
	}

	void creator_changed ()
	{
		Config::instance()->set_dcp_creator (wx_to_std (_creator->GetValue ()));
	}

	void config_file_changed ()
	{
		Config* config = Config::instance();
		boost::filesystem::path new_file = wx_to_std(_config_file->GetPath());
		if (new_file == config->config_file()) {
			return;
		}
		bool copy_and_link = true;
		if (boost::filesystem::exists(new_file)) {
			ConfigMoveDialog* d = new ConfigMoveDialog (_panel, new_file);
			if (d->ShowModal() == wxID_OK) {
				copy_and_link = false;
			}
			d->Destroy ();
		}

		if (copy_and_link) {
			config->write ();
			if (new_file != config->config_file()) {
				config->copy_and_link (new_file);
			}
		} else {
			config->link (new_file);
		}
	}

	void cinemas_file_changed ()
	{
		Config::instance()->set_cinemas_file (wx_to_std (_cinemas_file->GetPath ()));
	}

	wxSpinCtrl* _master_encoding_threads;
	wxSpinCtrl* _server_encoding_threads;
	FilePickerCtrl* _config_file;
	FilePickerCtrl* _cinemas_file;
#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
	wxCheckBox* _analyse_ebur128;
#endif
	wxCheckBox* _automatic_audio_analysis;
	wxTextCtrl* _issuer;
	wxTextCtrl* _creator;
};

class DefaultsPage : public StandardPage
{
public:
	DefaultsPage (wxSize panel_size, int border)
		: StandardPage (panel_size, border)
	{}

	wxString GetName () const
	{
		return _("Defaults");
	}

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon () const
	{
		return wxBitmap ("defaults", wxBITMAP_TYPE_PNG_RESOURCE);
	}
#endif

private:
	void setup ()
	{
		wxFlexGridSizer* table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		table->AddGrowableCol (1, 1);
		_panel->GetSizer()->Add (table, 1, wxALL | wxEXPAND, _border);

		{
			add_label_to_sizer (table, _panel, _("Default duration of still images"), true);
			wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
			_still_length = new wxSpinCtrl (_panel);
			s->Add (_still_length);
			add_label_to_sizer (s, _panel, _("s"), false);
			table->Add (s, 1);
		}

		add_label_to_sizer (table, _panel, _("Default directory for new films"), true);
#ifdef DCPOMATIC_USE_OWN_PICKER
		_directory = new DirPickerCtrl (_panel);
#else
		_directory = new wxDirPickerCtrl (_panel, wxDD_DIR_MUST_EXIST);
#endif
		table->Add (_directory, 1, wxEXPAND);

		add_label_to_sizer (table, _panel, _("Default ISDCF name details"), true);
		_isdcf_metadata_button = new wxButton (_panel, wxID_ANY, _("Edit..."));
		table->Add (_isdcf_metadata_button);

		add_label_to_sizer (table, _panel, _("Default container"), true);
		_container = new wxChoice (_panel, wxID_ANY);
		table->Add (_container);

		add_label_to_sizer (table, _panel, _("Default scale-to"), true);
		_scale_to = new wxChoice (_panel, wxID_ANY);
		table->Add (_scale_to);

		add_label_to_sizer (table, _panel, _("Default content type"), true);
		_dcp_content_type = new wxChoice (_panel, wxID_ANY);
		table->Add (_dcp_content_type);

		add_label_to_sizer (table, _panel, _("Default DCP audio channels"), true);
		_dcp_audio_channels = new wxChoice (_panel, wxID_ANY);
		table->Add (_dcp_audio_channels);

		{
			add_label_to_sizer (table, _panel, _("Default JPEG2000 bandwidth"), true);
			wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
			_j2k_bandwidth = new wxSpinCtrl (_panel);
			s->Add (_j2k_bandwidth);
			add_label_to_sizer (s, _panel, _("Mbit/s"), false);
			table->Add (s, 1);
		}

		{
			add_label_to_sizer (table, _panel, _("Default audio delay"), true);
			wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
			_audio_delay = new wxSpinCtrl (_panel);
			s->Add (_audio_delay);
			add_label_to_sizer (s, _panel, _("ms"), false);
			table->Add (s, 1);
		}

		add_label_to_sizer (table, _panel, _("Default standard"), true);
		_standard = new wxChoice (_panel, wxID_ANY);
		table->Add (_standard);

		add_label_to_sizer (table, _panel, _("Default KDM directory"), true);
#ifdef DCPOMATIC_USE_OWN_PICKER
		_kdm_directory = new DirPickerCtrl (_panel);
#else
		_kdm_directory = new wxDirPickerCtrl (_panel, wxDD_DIR_MUST_EXIST);
#endif

		table->Add (_kdm_directory, 1, wxEXPAND);

		_upload_after_make_dcp = new wxCheckBox (_panel, wxID_ANY, _("Default to enabling upload of DCP to TMS"));
		table->Add (_upload_after_make_dcp, 1, wxEXPAND);

		_still_length->SetRange (1, 3600);
		_still_length->Bind (wxEVT_SPINCTRL, boost::bind (&DefaultsPage::still_length_changed, this));

		_directory->Bind (wxEVT_DIRPICKER_CHANGED, boost::bind (&DefaultsPage::directory_changed, this));
		_kdm_directory->Bind (wxEVT_DIRPICKER_CHANGED, boost::bind (&DefaultsPage::kdm_directory_changed, this));

		_isdcf_metadata_button->Bind (wxEVT_BUTTON, boost::bind (&DefaultsPage::edit_isdcf_metadata_clicked, this));

		BOOST_FOREACH (Ratio const * i, Ratio::containers()) {
			_container->Append (std_to_wx(i->container_nickname()));
		}

		_container->Bind (wxEVT_CHOICE, boost::bind (&DefaultsPage::container_changed, this));

		_scale_to->Append (_("Guess from content"));

		BOOST_FOREACH (Ratio const * i, Ratio::all()) {
			_scale_to->Append (std_to_wx(i->image_nickname()));
		}

		_scale_to->Bind (wxEVT_CHOICE, boost::bind (&DefaultsPage::scale_to_changed, this));

		BOOST_FOREACH (DCPContentType const * i, DCPContentType::all()) {
			_dcp_content_type->Append (std_to_wx (i->pretty_name ()));
		}

		setup_audio_channels_choice (_dcp_audio_channels, 2);

		_dcp_content_type->Bind (wxEVT_CHOICE, boost::bind (&DefaultsPage::dcp_content_type_changed, this));
		_dcp_audio_channels->Bind (wxEVT_CHOICE, boost::bind (&DefaultsPage::dcp_audio_channels_changed, this));

		_j2k_bandwidth->SetRange (50, 250);
		_j2k_bandwidth->Bind (wxEVT_SPINCTRL, boost::bind (&DefaultsPage::j2k_bandwidth_changed, this));

		_audio_delay->SetRange (-1000, 1000);
		_audio_delay->Bind (wxEVT_SPINCTRL, boost::bind (&DefaultsPage::audio_delay_changed, this));

		_standard->Append (_("SMPTE"));
		_standard->Append (_("Interop"));
		_standard->Bind (wxEVT_CHOICE, boost::bind (&DefaultsPage::standard_changed, this));

		_upload_after_make_dcp->Bind (wxEVT_CHECKBOX, boost::bind (&DefaultsPage::upload_after_make_dcp_changed, this));
	}

	void config_changed ()
	{
		Config* config = Config::instance ();

		vector<Ratio const *> containers = Ratio::containers ();
		for (size_t i = 0; i < containers.size(); ++i) {
			if (containers[i] == config->default_container ()) {
				_container->SetSelection (i);
			}
		}

		vector<Ratio const *> ratios = Ratio::all ();
		for (size_t i = 0; i < ratios.size(); ++i) {
			if (ratios[i] == config->default_scale_to ()) {
				_scale_to->SetSelection (i + 1);
			}
		}

		if (!config->default_scale_to()) {
			_scale_to->SetSelection (0);
		}

		vector<DCPContentType const *> const ct = DCPContentType::all ();
		for (size_t i = 0; i < ct.size(); ++i) {
			if (ct[i] == config->default_dcp_content_type ()) {
				_dcp_content_type->SetSelection (i);
			}
		}

		checked_set (_still_length, config->default_still_length ());
		_directory->SetPath (std_to_wx (config->default_directory_or (wx_to_std (wxStandardPaths::Get().GetDocumentsDir())).string ()));
		_kdm_directory->SetPath (std_to_wx (config->default_kdm_directory_or (wx_to_std (wxStandardPaths::Get().GetDocumentsDir())).string ()));
		checked_set (_j2k_bandwidth, config->default_j2k_bandwidth() / 1000000);
		_j2k_bandwidth->SetRange (50, config->maximum_j2k_bandwidth() / 1000000);
		checked_set (_dcp_audio_channels, locale_convert<string> (config->default_dcp_audio_channels()));
		checked_set (_audio_delay, config->default_audio_delay ());
		checked_set (_standard, config->default_interop() ? 1 : 0);
		checked_set (_upload_after_make_dcp, config->default_upload_after_make_dcp());
	}

	void j2k_bandwidth_changed ()
	{
		Config::instance()->set_default_j2k_bandwidth (_j2k_bandwidth->GetValue() * 1000000);
	}

	void audio_delay_changed ()
	{
		Config::instance()->set_default_audio_delay (_audio_delay->GetValue());
	}

	void dcp_audio_channels_changed ()
	{
		int const s = _dcp_audio_channels->GetSelection ();
		if (s != wxNOT_FOUND) {
			Config::instance()->set_default_dcp_audio_channels (
				locale_convert<int> (string_client_data (_dcp_audio_channels->GetClientObject (s)))
				);
		}
	}

	void directory_changed ()
	{
		Config::instance()->set_default_directory (wx_to_std (_directory->GetPath ()));
	}

	void kdm_directory_changed ()
	{
		Config::instance()->set_default_kdm_directory (wx_to_std (_kdm_directory->GetPath ()));
	}

	void edit_isdcf_metadata_clicked ()
	{
		ISDCFMetadataDialog* d = new ISDCFMetadataDialog (_panel, Config::instance()->default_isdcf_metadata (), false);
		d->ShowModal ();
		Config::instance()->set_default_isdcf_metadata (d->isdcf_metadata ());
		d->Destroy ();
	}

	void still_length_changed ()
	{
		Config::instance()->set_default_still_length (_still_length->GetValue ());
	}

	void container_changed ()
	{
		vector<Ratio const *> ratio = Ratio::containers ();
		Config::instance()->set_default_container (ratio[_container->GetSelection()]);
	}

	void scale_to_changed ()
	{
		int const s = _scale_to->GetSelection ();
		if (s == 0) {
			Config::instance()->set_default_scale_to (0);
		} else {
			vector<Ratio const *> ratio = Ratio::all ();
			Config::instance()->set_default_scale_to (ratio[s - 1]);
		}
	}

	void dcp_content_type_changed ()
	{
		vector<DCPContentType const *> ct = DCPContentType::all ();
		Config::instance()->set_default_dcp_content_type (ct[_dcp_content_type->GetSelection()]);
	}

	void standard_changed ()
	{
		Config::instance()->set_default_interop (_standard->GetSelection() == 1);
	}

	void upload_after_make_dcp_changed ()
	{
		Config::instance()->set_default_upload_after_make_dcp (_upload_after_make_dcp->GetValue ());
	}

	wxSpinCtrl* _j2k_bandwidth;
	wxSpinCtrl* _audio_delay;
	wxButton* _isdcf_metadata_button;
	wxSpinCtrl* _still_length;
#ifdef DCPOMATIC_USE_OWN_PICKER
	DirPickerCtrl* _directory;
	DirPickerCtrl* _kdm_directory;
#else
	wxDirPickerCtrl* _directory;
	wxDirPickerCtrl* _kdm_directory;
#endif
	wxChoice* _container;
	wxChoice* _scale_to;
	wxChoice* _dcp_content_type;
	wxChoice* _dcp_audio_channels;
	wxChoice* _standard;
	wxCheckBox* _upload_after_make_dcp;
};

class EncodingServersPage : public StandardPage
{
public:
	EncodingServersPage (wxSize panel_size, int border)
		: StandardPage (panel_size, border)
	{}

	wxString GetName () const
	{
		return _("Servers");
	}

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon () const
	{
		return wxBitmap ("servers", wxBITMAP_TYPE_PNG_RESOURCE);
	}
#endif

private:
	void setup ()
	{
		_use_any_servers = new wxCheckBox (_panel, wxID_ANY, _("Search network for servers"));
		_panel->GetSizer()->Add (_use_any_servers, 0, wxALL, _border);

		vector<string> columns;
		columns.push_back (wx_to_std (_("IP address / host name")));
		_servers_list = new EditableList<string, ServerDialog> (
			_panel,
			columns,
			boost::bind (&Config::servers, Config::instance()),
			boost::bind (&Config::set_servers, Config::instance(), _1),
			boost::bind (&EncodingServersPage::server_column, this, _1)
			);

		_panel->GetSizer()->Add (_servers_list, 1, wxEXPAND | wxALL, _border);

		_use_any_servers->Bind (wxEVT_CHECKBOX, boost::bind (&EncodingServersPage::use_any_servers_changed, this));
	}

	void config_changed ()
	{
		checked_set (_use_any_servers, Config::instance()->use_any_servers ());
		_servers_list->refresh ();
	}

	void use_any_servers_changed ()
	{
		Config::instance()->set_use_any_servers (_use_any_servers->GetValue ());
	}

	string server_column (string s)
	{
		return s;
	}

	wxCheckBox* _use_any_servers;
	EditableList<string, ServerDialog>* _servers_list;
};

class TMSPage : public StandardPage
{
public:
	TMSPage (wxSize panel_size, int border)
		: StandardPage (panel_size, border)
	{}

	wxString GetName () const
	{
		return _("TMS");
	}

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon () const
	{
		return wxBitmap ("tms", wxBITMAP_TYPE_PNG_RESOURCE);
	}
#endif

private:
	void setup ()
	{
		wxFlexGridSizer* table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		table->AddGrowableCol (1, 1);
		_panel->GetSizer()->Add (table, 1, wxALL | wxEXPAND, _border);

		add_label_to_sizer (table, _panel, _("Protocol"), true);
		_tms_protocol = new wxChoice (_panel, wxID_ANY);
		table->Add (_tms_protocol, 1, wxEXPAND);

		add_label_to_sizer (table, _panel, _("IP address"), true);
		_tms_ip = new wxTextCtrl (_panel, wxID_ANY);
		table->Add (_tms_ip, 1, wxEXPAND);

		add_label_to_sizer (table, _panel, _("Target path"), true);
		_tms_path = new wxTextCtrl (_panel, wxID_ANY);
		table->Add (_tms_path, 1, wxEXPAND);

		add_label_to_sizer (table, _panel, _("User name"), true);
		_tms_user = new wxTextCtrl (_panel, wxID_ANY);
		table->Add (_tms_user, 1, wxEXPAND);

		add_label_to_sizer (table, _panel, _("Password"), true);
		_tms_password = new wxTextCtrl (_panel, wxID_ANY);
		table->Add (_tms_password, 1, wxEXPAND);

		_tms_protocol->Append (_("SCP (for AAM and Doremi)"));
		_tms_protocol->Append (_("FTP (for Dolby)"));

		_tms_protocol->Bind (wxEVT_CHOICE, boost::bind (&TMSPage::tms_protocol_changed, this));
		_tms_ip->Bind (wxEVT_TEXT, boost::bind (&TMSPage::tms_ip_changed, this));
		_tms_path->Bind (wxEVT_TEXT, boost::bind (&TMSPage::tms_path_changed, this));
		_tms_user->Bind (wxEVT_TEXT, boost::bind (&TMSPage::tms_user_changed, this));
		_tms_password->Bind (wxEVT_TEXT, boost::bind (&TMSPage::tms_password_changed, this));
	}

	void config_changed ()
	{
		Config* config = Config::instance ();

		checked_set (_tms_protocol, config->tms_protocol ());
		checked_set (_tms_ip, config->tms_ip ());
		checked_set (_tms_path, config->tms_path ());
		checked_set (_tms_user, config->tms_user ());
		checked_set (_tms_password, config->tms_password ());
	}

	void tms_protocol_changed ()
	{
		Config::instance()->set_tms_protocol (static_cast<Protocol> (_tms_protocol->GetSelection ()));
	}

	void tms_ip_changed ()
	{
		Config::instance()->set_tms_ip (wx_to_std (_tms_ip->GetValue ()));
	}

	void tms_path_changed ()
	{
		Config::instance()->set_tms_path (wx_to_std (_tms_path->GetValue ()));
	}

	void tms_user_changed ()
	{
		Config::instance()->set_tms_user (wx_to_std (_tms_user->GetValue ()));
	}

	void tms_password_changed ()
	{
		Config::instance()->set_tms_password (wx_to_std (_tms_password->GetValue ()));
	}

	wxChoice* _tms_protocol;
	wxTextCtrl* _tms_ip;
	wxTextCtrl* _tms_path;
	wxTextCtrl* _tms_user;
	wxTextCtrl* _tms_password;
};

static string
column (string s)
{
	return s;
}

class EmailPage : public StandardPage
{
public:
	EmailPage (wxSize panel_size, int border)
		: StandardPage (panel_size, border)
	{}

	wxString GetName () const
	{
		return _("Email");
	}

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon () const
	{
		return wxBitmap ("email", wxBITMAP_TYPE_PNG_RESOURCE);
	}
#endif

private:
	void setup ()
	{
		wxFlexGridSizer* table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		table->AddGrowableCol (1, 1);
		_panel->GetSizer()->Add (table, 1, wxEXPAND | wxALL, _border);

		add_label_to_sizer (table, _panel, _("Outgoing mail server"), true);
		{
			wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
			_server = new wxTextCtrl (_panel, wxID_ANY);
			s->Add (_server, 1, wxEXPAND | wxALL);
			add_label_to_sizer (s, _panel, _("port"), false);
			_port = new wxSpinCtrl (_panel, wxID_ANY);
			_port->SetRange (0, 65535);
			s->Add (_port);
			table->Add (s, 1, wxEXPAND | wxALL);
		}

		add_label_to_sizer (table, _panel, _("Mail user name"), true);
		_user = new wxTextCtrl (_panel, wxID_ANY);
		table->Add (_user, 1, wxEXPAND | wxALL);

		add_label_to_sizer (table, _panel, _("Mail password"), true);
		_password = new wxTextCtrl (_panel, wxID_ANY);
		table->Add (_password, 1, wxEXPAND | wxALL);

		_server->Bind (wxEVT_TEXT, boost::bind (&EmailPage::server_changed, this));
		_port->Bind (wxEVT_SPINCTRL, boost::bind (&EmailPage::port_changed, this));
		_user->Bind (wxEVT_TEXT, boost::bind (&EmailPage::user_changed, this));
		_password->Bind (wxEVT_TEXT, boost::bind (&EmailPage::password_changed, this));
	}

	void config_changed ()
	{
		Config* config = Config::instance ();

		checked_set (_server, config->mail_server ());
		checked_set (_port, config->mail_port ());
		checked_set (_user, config->mail_user ());
		checked_set (_password, config->mail_password ());
	}

	void server_changed ()
	{
		Config::instance()->set_mail_server (wx_to_std (_server->GetValue ()));
	}

	void port_changed ()
	{
		Config::instance()->set_mail_port (_port->GetValue ());
	}

	void user_changed ()
	{
		Config::instance()->set_mail_user (wx_to_std (_user->GetValue ()));
	}

	void password_changed ()
	{
		Config::instance()->set_mail_password (wx_to_std (_password->GetValue ()));
	}

	wxTextCtrl* _server;
	wxSpinCtrl* _port;
	wxTextCtrl* _user;
	wxTextCtrl* _password;
};

class KDMEmailPage : public StandardPage
{
public:

	KDMEmailPage (wxSize panel_size, int border)
#ifdef DCPOMATIC_OSX
		/* We have to force both width and height of this one */
		: StandardPage (wxSize (panel_size.GetWidth(), 128), border)
#else
		: StandardPage (panel_size, border)
#endif
	{}

	wxString GetName () const
	{
		return _("KDM Email");
	}

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon () const
	{
		return wxBitmap ("kdm_email", wxBITMAP_TYPE_PNG_RESOURCE);
	}
#endif

private:
	void setup ()
	{
		wxFlexGridSizer* table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		table->AddGrowableCol (1, 1);
		_panel->GetSizer()->Add (table, 1, wxEXPAND | wxALL, _border);

		add_label_to_sizer (table, _panel, _("Subject"), true);
		_subject = new wxTextCtrl (_panel, wxID_ANY);
		table->Add (_subject, 1, wxEXPAND | wxALL);

		add_label_to_sizer (table, _panel, _("From address"), true);
		_from = new wxTextCtrl (_panel, wxID_ANY);
		table->Add (_from, 1, wxEXPAND | wxALL);

		vector<string> columns;
		columns.push_back (wx_to_std (_("Address")));
		add_label_to_sizer (table, _panel, _("CC addresses"), true);
		_cc = new EditableList<string, EmailDialog> (
			_panel,
			columns,
			bind (&Config::kdm_cc, Config::instance()),
			bind (&Config::set_kdm_cc, Config::instance(), _1),
			bind (&column, _1)
			);
		table->Add (_cc, 1, wxEXPAND | wxALL);

		add_label_to_sizer (table, _panel, _("BCC address"), true);
		_bcc = new wxTextCtrl (_panel, wxID_ANY);
		table->Add (_bcc, 1, wxEXPAND | wxALL);

		_email = new wxTextCtrl (_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (-1, 200), wxTE_MULTILINE);
		_panel->GetSizer()->Add (_email, 0, wxEXPAND | wxALL, _border);

		_reset_email = new wxButton (_panel, wxID_ANY, _("Reset to default subject and text"));
		_panel->GetSizer()->Add (_reset_email, 0, wxEXPAND | wxALL, _border);

		_cc->layout ();

		_subject->Bind (wxEVT_TEXT, boost::bind (&KDMEmailPage::kdm_subject_changed, this));
		_from->Bind (wxEVT_TEXT, boost::bind (&KDMEmailPage::kdm_from_changed, this));
		_bcc->Bind (wxEVT_TEXT, boost::bind (&KDMEmailPage::kdm_bcc_changed, this));
		_email->Bind (wxEVT_TEXT, boost::bind (&KDMEmailPage::kdm_email_changed, this));
		_reset_email->Bind (wxEVT_BUTTON, boost::bind (&KDMEmailPage::reset_email, this));
	}

	void config_changed ()
	{
		Config* config = Config::instance ();

		checked_set (_subject, config->kdm_subject ());
		checked_set (_from, config->kdm_from ());
		checked_set (_bcc, config->kdm_bcc ());
		checked_set (_email, Config::instance()->kdm_email ());
	}

	void kdm_subject_changed ()
	{
		Config::instance()->set_kdm_subject (wx_to_std (_subject->GetValue ()));
	}

	void kdm_from_changed ()
	{
		Config::instance()->set_kdm_from (wx_to_std (_from->GetValue ()));
	}

	void kdm_bcc_changed ()
	{
		Config::instance()->set_kdm_bcc (wx_to_std (_bcc->GetValue ()));
	}

	void kdm_email_changed ()
	{
		if (_email->GetValue().IsEmpty ()) {
			/* Sometimes we get sent an erroneous notification that the email
			   is empty; I don't know why.
			*/
			return;
		}
		Config::instance()->set_kdm_email (wx_to_std (_email->GetValue ()));
	}

	void reset_email ()
	{
		Config::instance()->reset_kdm_email ();
		checked_set (_email, Config::instance()->kdm_email ());
	}

	wxTextCtrl* _subject;
	wxTextCtrl* _from;
	EditableList<string, EmailDialog>* _cc;
	wxTextCtrl* _bcc;
	wxTextCtrl* _email;
	wxButton* _reset_email;
};

class NotificationsPage : public StandardPage
{
public:
	NotificationsPage (wxSize panel_size, int border)
#ifdef DCPOMATIC_OSX
		/* We have to force both width and height of this one */
		: StandardPage (wxSize (panel_size.GetWidth(), 128), border)
#else
		: StandardPage (panel_size, border)
#endif
	{}

	wxString GetName () const
	{
		return _("Notifications");
	}

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon () const
	{
		return wxBitmap ("notifications", wxBITMAP_TYPE_PNG_RESOURCE);
	}
#endif

private:
	void setup ()
	{
		wxFlexGridSizer* table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		table->AddGrowableCol (1, 1);
		_panel->GetSizer()->Add (table, 1, wxEXPAND | wxALL, _border);

		_enable_message_box = new wxCheckBox (_panel, wxID_ANY, _("Message box"));
		table->Add (_enable_message_box, 1, wxEXPAND | wxALL);
		table->AddSpacer (0);

		_enable_email = new wxCheckBox (_panel, wxID_ANY, _("Email"));
		table->Add (_enable_email, 1, wxEXPAND | wxALL);
		table->AddSpacer (0);

		add_label_to_sizer (table, _panel, _("Subject"), true);
		_subject = new wxTextCtrl (_panel, wxID_ANY);
		table->Add (_subject, 1, wxEXPAND | wxALL);

		add_label_to_sizer (table, _panel, _("From address"), true);
		_from = new wxTextCtrl (_panel, wxID_ANY);
		table->Add (_from, 1, wxEXPAND | wxALL);

		add_label_to_sizer (table, _panel, _("To address"), true);
		_to = new wxTextCtrl (_panel, wxID_ANY);
		table->Add (_to, 1, wxEXPAND | wxALL);

		vector<string> columns;
		columns.push_back (wx_to_std (_("Address")));
		add_label_to_sizer (table, _panel, _("CC addresses"), true);
		_cc = new EditableList<string, EmailDialog> (
			_panel,
			columns,
			bind (&Config::notification_cc, Config::instance()),
			bind (&Config::set_notification_cc, Config::instance(), _1),
			bind (&column, _1)
			);
		table->Add (_cc, 1, wxEXPAND | wxALL);

		add_label_to_sizer (table, _panel, _("BCC address"), true);
		_bcc = new wxTextCtrl (_panel, wxID_ANY);
		table->Add (_bcc, 1, wxEXPAND | wxALL);

		_email = new wxTextCtrl (_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (-1, 200), wxTE_MULTILINE);
		_panel->GetSizer()->Add (_email, 0, wxEXPAND | wxALL, _border);

		_reset_email = new wxButton (_panel, wxID_ANY, _("Reset to default subject and text"));
		_panel->GetSizer()->Add (_reset_email, 0, wxEXPAND | wxALL, _border);

		_cc->layout ();

		_enable_message_box->Bind (wxEVT_CHECKBOX, boost::bind (&NotificationsPage::type_changed, this, _enable_message_box, Config::MESSAGE_BOX));
		_enable_email->Bind (wxEVT_CHECKBOX, boost::bind (&NotificationsPage::type_changed, this, _enable_email, Config::EMAIL));

		_subject->Bind (wxEVT_TEXT, boost::bind (&NotificationsPage::notification_subject_changed, this));
		_from->Bind (wxEVT_TEXT, boost::bind (&NotificationsPage::notification_from_changed, this));
		_to->Bind (wxEVT_TEXT, boost::bind (&NotificationsPage::notification_to_changed, this));
		_bcc->Bind (wxEVT_TEXT, boost::bind (&NotificationsPage::notification_bcc_changed, this));
		_email->Bind (wxEVT_TEXT, boost::bind (&NotificationsPage::notification_email_changed, this));
		_reset_email->Bind (wxEVT_BUTTON, boost::bind (&NotificationsPage::reset_email, this));

		update_sensitivity ();
	}

	void update_sensitivity ()
	{
		bool const s = _enable_email->GetValue();
		_subject->Enable(s);
		_from->Enable(s);
		_to->Enable(s);
		_cc->Enable(s);
		_bcc->Enable(s);
		_email->Enable(s);
		_reset_email->Enable(s);
	}

	void config_changed ()
	{
		Config* config = Config::instance ();

		checked_set (_enable_message_box, config->notification(Config::MESSAGE_BOX));
		checked_set (_enable_email, config->notification(Config::EMAIL));
		checked_set (_subject, config->notification_subject ());
		checked_set (_from, config->notification_from ());
		checked_set (_to, config->notification_to ());
		checked_set (_bcc, config->notification_bcc ());
		checked_set (_email, Config::instance()->notification_email ());

		update_sensitivity ();
	}

	void notification_subject_changed ()
	{
		Config::instance()->set_notification_subject (wx_to_std (_subject->GetValue ()));
	}

	void notification_from_changed ()
	{
		Config::instance()->set_notification_from (wx_to_std (_from->GetValue ()));
	}

	void notification_to_changed ()
	{
		Config::instance()->set_notification_to (wx_to_std (_to->GetValue ()));
	}

	void notification_bcc_changed ()
	{
		Config::instance()->set_notification_bcc (wx_to_std (_bcc->GetValue ()));
	}

	void notification_email_changed ()
	{
		if (_email->GetValue().IsEmpty ()) {
			/* Sometimes we get sent an erroneous notification that the email
			   is empty; I don't know why.
			*/
			return;
		}
		Config::instance()->set_notification_email (wx_to_std (_email->GetValue ()));
	}

	void reset_email ()
	{
		Config::instance()->reset_notification_email ();
		checked_set (_email, Config::instance()->notification_email ());
	}

	void type_changed (wxCheckBox* b, Config::Notification n)
	{
		Config::instance()->set_notification(n, b->GetValue());
		update_sensitivity ();
	}

	wxCheckBox* _enable_message_box;
	wxCheckBox* _enable_email;

	wxTextCtrl* _subject;
	wxTextCtrl* _from;
	wxTextCtrl* _to;
	EditableList<string, EmailDialog>* _cc;
	wxTextCtrl* _bcc;
	wxTextCtrl* _email;
	wxButton* _reset_email;
};

class CoverSheetPage : public StandardPage
{
public:

	CoverSheetPage (wxSize panel_size, int border)
#ifdef DCPOMATIC_OSX
		/* We have to force both width and height of this one */
		: StandardPage (wxSize (panel_size.GetWidth(), 128), border)
#else
		: StandardPage (panel_size, border)
#endif
	{}

	wxString GetName () const
	{
		return _("Cover Sheet");
	}

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon () const
	{
		return wxBitmap ("cover_sheet", wxBITMAP_TYPE_PNG_RESOURCE);
	}
#endif

private:
	void setup ()
	{
		_cover_sheet = new wxTextCtrl (_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (-1, 200), wxTE_MULTILINE);
		_panel->GetSizer()->Add (_cover_sheet, 0, wxEXPAND | wxALL, _border);

		_reset_cover_sheet = new wxButton (_panel, wxID_ANY, _("Reset to default text"));
		_panel->GetSizer()->Add (_reset_cover_sheet, 0, wxEXPAND | wxALL, _border);

		_cover_sheet->Bind (wxEVT_TEXT, boost::bind (&CoverSheetPage::cover_sheet_changed, this));
		_reset_cover_sheet->Bind (wxEVT_BUTTON, boost::bind (&CoverSheetPage::reset_cover_sheet, this));
	}

	void config_changed ()
	{
		checked_set (_cover_sheet, Config::instance()->cover_sheet ());
	}

	void cover_sheet_changed ()
	{
		if (_cover_sheet->GetValue().IsEmpty ()) {
			/* Sometimes we get sent an erroneous notification that the cover sheet
			   is empty; I don't know why.
			*/
			return;
		}
		Config::instance()->set_cover_sheet (wx_to_std (_cover_sheet->GetValue ()));
	}

	void reset_cover_sheet ()
	{
		Config::instance()->reset_cover_sheet ();
		checked_set (_cover_sheet, Config::instance()->cover_sheet ());
	}

	wxTextCtrl* _cover_sheet;
	wxButton* _reset_cover_sheet;
};


/** @class AdvancedPage
 *  @brief Advanced page of the preferences dialog.
 */
class AdvancedPage : public StockPage
{
public:
	AdvancedPage (wxSize panel_size, int border)
		: StockPage (Kind_Advanced, panel_size, border)
		, _maximum_j2k_bandwidth (0)
		, _allow_any_dcp_frame_rate (0)
		, _only_servers_encode (0)
		, _log_general (0)
		, _log_warning (0)
		, _log_error (0)
		, _log_timing (0)
		, _log_debug_decode (0)
		, _log_debug_encode (0)
		, _log_debug_email (0)
	{}

private:
	void add_top_aligned_label_to_sizer (wxSizer* table, wxWindow* parent, wxString text)
	{
		int flags = wxALIGN_TOP | wxTOP | wxLEFT;
#ifdef __WXOSX__
		flags |= wxALIGN_RIGHT;
		text += wxT (":");
#endif
		wxStaticText* m = new wxStaticText (parent, wxID_ANY, text);
		table->Add (m, 0, flags, DCPOMATIC_SIZER_Y_GAP);
	}

	void setup ()
	{
		wxFlexGridSizer* table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		table->AddGrowableCol (1, 1);
		_panel->GetSizer()->Add (table, 1, wxALL | wxEXPAND, _border);

		{
			add_label_to_sizer (table, _panel, _("Maximum JPEG2000 bandwidth"), true);
			wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
			_maximum_j2k_bandwidth = new wxSpinCtrl (_panel);
			s->Add (_maximum_j2k_bandwidth, 1);
			add_label_to_sizer (s, _panel, _("Mbit/s"), false);
			table->Add (s, 1);
		}

		_allow_any_dcp_frame_rate = new wxCheckBox (_panel, wxID_ANY, _("Allow any DCP frame rate"));
		table->Add (_allow_any_dcp_frame_rate, 1, wxEXPAND | wxALL);
		table->AddSpacer (0);

		_only_servers_encode = new wxCheckBox (_panel, wxID_ANY, _("Only servers encode"));
		table->Add (_only_servers_encode, 1, wxEXPAND | wxALL);
		table->AddSpacer (0);

		{
			add_label_to_sizer (table, _panel, _("Maximum number of frames to store per thread"), true);
			wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
			_frames_in_memory_multiplier = new wxSpinCtrl (_panel);
			s->Add (_frames_in_memory_multiplier, 1);
			table->Add (s, 1);
		}

		{
			add_top_aligned_label_to_sizer (table, _panel, _("DCP metadata filename format"));
			dcp::NameFormat::Map titles;
			titles['t'] = "type (cpl/pkl)";
			dcp::NameFormat::Map examples;
			examples['t'] = "cpl";
			_dcp_metadata_filename_format = new NameFormatEditor (
				_panel, Config::instance()->dcp_metadata_filename_format(), titles, examples, "_eb1c112c-ca3c-4ae6-9263-c6714ff05d64.xml"
				);
			table->Add (_dcp_metadata_filename_format->panel(), 1, wxEXPAND | wxALL);
		}

		{
			add_top_aligned_label_to_sizer (table, _panel, _("DCP asset filename format"));
			dcp::NameFormat::Map titles;
			titles['t'] = "type (j2c/pcm/sub)";
			titles['r'] = "reel number";
			titles['n'] = "number of reels";
			titles['c'] = "content filename";
			dcp::NameFormat::Map examples;
			examples['t'] = "j2c";
			examples['r'] = "1";
			examples['n'] = "4";
			examples['c'] = "myfile.mp4";
			_dcp_asset_filename_format = new NameFormatEditor (
				_panel, Config::instance()->dcp_asset_filename_format(), titles, examples, "_eb1c112c-ca3c-4ae6-9263-c6714ff05d64.mxf"
				);
			table->Add (_dcp_asset_filename_format->panel(), 1, wxEXPAND | wxALL);
		}

		{
			add_top_aligned_label_to_sizer (table, _panel, _("Log"));
			wxBoxSizer* t = new wxBoxSizer (wxVERTICAL);
			_log_general = new wxCheckBox (_panel, wxID_ANY, _("General"));
			t->Add (_log_general, 1, wxEXPAND | wxALL);
			_log_warning = new wxCheckBox (_panel, wxID_ANY, _("Warnings"));
			t->Add (_log_warning, 1, wxEXPAND | wxALL);
			_log_error = new wxCheckBox (_panel, wxID_ANY, _("Errors"));
			t->Add (_log_error, 1, wxEXPAND | wxALL);
			/// TRANSLATORS: translate the word "Timing" here; do not include the "Config|" prefix
			_log_timing = new wxCheckBox (_panel, wxID_ANY, S_("Config|Timing"));
			t->Add (_log_timing, 1, wxEXPAND | wxALL);
			_log_debug_decode = new wxCheckBox (_panel, wxID_ANY, _("Debug: decode"));
			t->Add (_log_debug_decode, 1, wxEXPAND | wxALL);
			_log_debug_encode = new wxCheckBox (_panel, wxID_ANY, _("Debug: encode"));
			t->Add (_log_debug_encode, 1, wxEXPAND | wxALL);
			_log_debug_email = new wxCheckBox (_panel, wxID_ANY, _("Debug: email sending"));
			t->Add (_log_debug_email, 1, wxEXPAND | wxALL);
			table->Add (t, 0, wxALL, 6);
		}

#ifdef DCPOMATIC_WINDOWS
		_win32_console = new wxCheckBox (_panel, wxID_ANY, _("Open console window"));
		table->Add (_win32_console, 1, wxEXPAND | wxALL);
		table->AddSpacer (0);
#endif

		_maximum_j2k_bandwidth->SetRange (1, 1000);
		_maximum_j2k_bandwidth->Bind (wxEVT_SPINCTRL, boost::bind (&AdvancedPage::maximum_j2k_bandwidth_changed, this));
		_allow_any_dcp_frame_rate->Bind (wxEVT_CHECKBOX, boost::bind (&AdvancedPage::allow_any_dcp_frame_rate_changed, this));
		_only_servers_encode->Bind (wxEVT_CHECKBOX, boost::bind (&AdvancedPage::only_servers_encode_changed, this));
		_frames_in_memory_multiplier->Bind (wxEVT_SPINCTRL, boost::bind(&AdvancedPage::frames_in_memory_multiplier_changed, this));
		_dcp_metadata_filename_format->Changed.connect (boost::bind (&AdvancedPage::dcp_metadata_filename_format_changed, this));
		_dcp_asset_filename_format->Changed.connect (boost::bind (&AdvancedPage::dcp_asset_filename_format_changed, this));
		_log_general->Bind (wxEVT_CHECKBOX, boost::bind (&AdvancedPage::log_changed, this));
		_log_warning->Bind (wxEVT_CHECKBOX, boost::bind (&AdvancedPage::log_changed, this));
		_log_error->Bind (wxEVT_CHECKBOX, boost::bind (&AdvancedPage::log_changed, this));
		_log_timing->Bind (wxEVT_CHECKBOX, boost::bind (&AdvancedPage::log_changed, this));
		_log_debug_decode->Bind (wxEVT_CHECKBOX, boost::bind (&AdvancedPage::log_changed, this));
		_log_debug_encode->Bind (wxEVT_CHECKBOX, boost::bind (&AdvancedPage::log_changed, this));
		_log_debug_email->Bind (wxEVT_CHECKBOX, boost::bind (&AdvancedPage::log_changed, this));
#ifdef DCPOMATIC_WINDOWS
		_win32_console->Bind (wxEVT_CHECKBOX, boost::bind (&AdvancedPage::win32_console_changed, this));
#endif
	}

	void config_changed ()
	{
		Config* config = Config::instance ();

		checked_set (_maximum_j2k_bandwidth, config->maximum_j2k_bandwidth() / 1000000);
		checked_set (_allow_any_dcp_frame_rate, config->allow_any_dcp_frame_rate ());
		checked_set (_only_servers_encode, config->only_servers_encode ());
		checked_set (_log_general, config->log_types() & LogEntry::TYPE_GENERAL);
		checked_set (_log_warning, config->log_types() & LogEntry::TYPE_WARNING);
		checked_set (_log_error, config->log_types() & LogEntry::TYPE_ERROR);
		checked_set (_log_timing, config->log_types() & LogEntry::TYPE_TIMING);
		checked_set (_log_debug_decode, config->log_types() & LogEntry::TYPE_DEBUG_DECODE);
		checked_set (_log_debug_encode, config->log_types() & LogEntry::TYPE_DEBUG_ENCODE);
		checked_set (_log_debug_email, config->log_types() & LogEntry::TYPE_DEBUG_EMAIL);
		checked_set (_frames_in_memory_multiplier, config->frames_in_memory_multiplier());
#ifdef DCPOMATIC_WINDOWS
		checked_set (_win32_console, config->win32_console());
#endif
	}

	void maximum_j2k_bandwidth_changed ()
	{
		Config::instance()->set_maximum_j2k_bandwidth (_maximum_j2k_bandwidth->GetValue() * 1000000);
	}

	void frames_in_memory_multiplier_changed ()
	{
		Config::instance()->set_frames_in_memory_multiplier (_frames_in_memory_multiplier->GetValue());
	}

	void allow_any_dcp_frame_rate_changed ()
	{
		Config::instance()->set_allow_any_dcp_frame_rate (_allow_any_dcp_frame_rate->GetValue ());
	}

	void only_servers_encode_changed ()
	{
		Config::instance()->set_only_servers_encode (_only_servers_encode->GetValue ());
	}

	void dcp_metadata_filename_format_changed ()
	{
		Config::instance()->set_dcp_metadata_filename_format (_dcp_metadata_filename_format->get ());
	}

	void dcp_asset_filename_format_changed ()
	{
		Config::instance()->set_dcp_asset_filename_format (_dcp_asset_filename_format->get ());
	}

	void log_changed ()
	{
		int types = 0;
		if (_log_general->GetValue ()) {
			types |= LogEntry::TYPE_GENERAL;
		}
		if (_log_warning->GetValue ()) {
			types |= LogEntry::TYPE_WARNING;
		}
		if (_log_error->GetValue ())  {
			types |= LogEntry::TYPE_ERROR;
		}
		if (_log_timing->GetValue ()) {
			types |= LogEntry::TYPE_TIMING;
		}
		if (_log_debug_decode->GetValue ()) {
			types |= LogEntry::TYPE_DEBUG_DECODE;
		}
		if (_log_debug_encode->GetValue ()) {
			types |= LogEntry::TYPE_DEBUG_ENCODE;
		}
		if (_log_debug_email->GetValue ()) {
			types |= LogEntry::TYPE_DEBUG_EMAIL;
		}
		Config::instance()->set_log_types (types);
	}

#ifdef DCPOMATIC_WINDOWS
	void win32_console_changed ()
	{
		Config::instance()->set_win32_console (_win32_console->GetValue ());
	}
#endif

	wxSpinCtrl* _maximum_j2k_bandwidth;
	wxSpinCtrl* _frames_in_memory_multiplier;
	wxCheckBox* _allow_any_dcp_frame_rate;
	wxCheckBox* _only_servers_encode;
	NameFormatEditor* _dcp_metadata_filename_format;
	NameFormatEditor* _dcp_asset_filename_format;
	wxCheckBox* _log_general;
	wxCheckBox* _log_warning;
	wxCheckBox* _log_error;
	wxCheckBox* _log_timing;
	wxCheckBox* _log_debug_decode;
	wxCheckBox* _log_debug_encode;
	wxCheckBox* _log_debug_email;
#ifdef DCPOMATIC_WINDOWS
	wxCheckBox* _win32_console;
#endif
};

wxPreferencesEditor*
create_full_config_dialog ()
{
	wxPreferencesEditor* e = new wxPreferencesEditor ();

#ifdef DCPOMATIC_OSX
	/* Width that we force some of the config panels to be on OSX so that
	   the containing window doesn't shrink too much when we select those panels.
	   This is obviously an unpleasant hack.
	*/
	wxSize ps = wxSize (600, -1);
	int const border = 16;
#else
	wxSize ps = wxSize (-1, -1);
	int const border = 8;
#endif

	e->AddPage (new FullGeneralPage (ps, border));
	e->AddPage (new DefaultsPage (ps, border));
	e->AddPage (new EncodingServersPage (ps, border));
	e->AddPage (new KeysPage (ps, border));
	e->AddPage (new TMSPage (ps, border));
	e->AddPage (new EmailPage (ps, border));
	e->AddPage (new KDMEmailPage (ps, border));
	e->AddPage (new NotificationsPage (ps, border));
	e->AddPage (new CoverSheetPage (ps, border));
	e->AddPage (new AdvancedPage (ps, border));
	return e;
}
