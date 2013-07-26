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

class DirPickerCtrl;
class wxNotebook;

class ServerDescription;

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
	void add_server_clicked ();
	void edit_server_clicked ();
	void remove_server_clicked ();
	void server_selection_changed ();
	void default_container_changed ();
	void default_dcp_content_type_changed ();
	void issuer_changed ();
	void creator_changed ();
	void default_j2k_bandwidth_changed ();

	void add_server_to_control (ServerDescription *);
	void setup_language_sensitivity ();

	void make_misc_panel ();
	void make_tms_panel ();
	void make_metadata_panel ();
	void make_servers_panel ();

	wxNotebook* _notebook;
	wxPanel* _misc_panel;
	wxPanel* _tms_panel;
	wxPanel* _servers_panel;
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
	wxSpinCtrl* _default_still_length;
#ifdef DCPOMATIC_USE_OWN_DIR_PICKER
	DirPickerCtrl* _default_directory;
#else
	wxDirPickerCtrl* _default_directory;
#endif
	wxButton* _default_dci_metadata_button;
	wxListCtrl* _servers;
	wxButton* _add_server;
	wxButton* _edit_server;
	wxButton* _remove_server;
	wxTextCtrl* _issuer;
	wxTextCtrl* _creator;
	wxSpinCtrl* _default_j2k_bandwidth;
};

