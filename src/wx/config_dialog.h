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

/** @file src/config_dialog.h
 *  @brief A dialogue to edit DCP-o-matic configuration.
 */

#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <wx/listctrl.h>
#include <wx/filepicker.h>
#include "wx_util.h"
#include "editable_list.h"

class DirPickerCtrl;
class wxNotebook;
class PresetColourConversion;
class PresetColourConversionDialog;
class ServerDialog;

/** @class ConfigDialog
 *  @brief A dialogue to edit DCP-o-matic configuration.
 */
class ConfigDialog : public wxDialog
{
public:
	ConfigDialog (wxWindow *);

private:
	void set_language_changed ();
	void language_changed ();
	void tms_ip_changed ();
	void tms_path_changed ();
	void tms_user_changed ();
	void tms_password_changed ();
	void num_local_encoding_threads_changed ();
	void default_still_length_changed ();
	void default_directory_changed ();
	void edit_default_dci_metadata_clicked ();
	void default_container_changed ();
	void default_dcp_content_type_changed ();
	void issuer_changed ();
	void creator_changed ();
	void default_j2k_bandwidth_changed ();
	void default_audio_delay_changed ();
	void mail_server_changed ();
	void mail_user_changed ();
	void mail_password_changed ();
	void kdm_from_changed ();
	void kdm_email_changed ();
	void use_any_servers_changed ();

	void setup_language_sensitivity ();

	void make_misc_panel ();
	void make_defaults_panel ();
	void make_servers_panel ();
	void make_tms_panel ();
	void make_metadata_panel ();
	void make_colour_conversions_panel ();
	void make_kdm_email_panel ();

	void check_for_updates_changed ();
	void check_for_test_updates_changed ();

	wxNotebook* _notebook;
	wxPanel* _misc_panel;
	wxPanel* _defaults_panel;
	wxPanel* _servers_panel;
	wxPanel* _tms_panel;
	EditableList<PresetColourConversion, PresetColourConversionDialog>* _colour_conversions_panel;
	wxPanel* _metadata_panel;
	wxCheckBox* _set_language;
	wxChoice* _language;
	wxChoice* _default_container;
	wxChoice* _default_dcp_content_type;
	wxTextCtrl* _tms_ip;
	wxTextCtrl* _tms_path;
	wxTextCtrl* _tms_user;
	wxTextCtrl* _tms_password;
	wxSpinCtrl* _num_local_encoding_threads;
	wxTextCtrl* _mail_server;
	wxTextCtrl* _mail_user;
	wxTextCtrl* _mail_password;
	wxTextCtrl* _kdm_from;
	wxSpinCtrl* _default_still_length;
#ifdef DCPOMATIC_USE_OWN_DIR_PICKER
	DirPickerCtrl* _default_directory;
#else
	wxDirPickerCtrl* _default_directory;
#endif
	wxButton* _default_dci_metadata_button;
	wxTextCtrl* _issuer;
	wxTextCtrl* _creator;
	wxSpinCtrl* _default_j2k_bandwidth;
	wxSpinCtrl* _default_audio_delay;
	wxPanel* _kdm_email_panel;
	wxTextCtrl* _kdm_email;
	wxCheckBox* _use_any_servers;
	wxCheckBox* _check_for_updates;
	wxCheckBox* _check_for_test_updates;
	EditableList<std::string, ServerDialog>* _servers_list;
};

