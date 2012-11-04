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
 *  @brief A dialogue to edit DVD-o-matic configuration.
 */

#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <wx/listctrl.h>
#include <wx/filepicker.h>

class DirPickerCtrl;

class Screen;
class ServerDescription;

/** @class ConfigDialog
 *  @brief A dialogue to edit DVD-o-matic configuration.
 */
class ConfigDialog : public wxDialog
{
public:
	ConfigDialog (wxWindow *);

private:
	void tms_ip_changed (wxCommandEvent &);
	void tms_path_changed (wxCommandEvent &);
	void tms_user_changed (wxCommandEvent &);
	void tms_password_changed (wxCommandEvent &);
	void num_local_encoding_threads_changed (wxCommandEvent &);
	void default_directory_changed (wxCommandEvent &);
	void colour_lut_changed (wxCommandEvent &);
	void j2k_bandwidth_changed (wxCommandEvent &);
	void reference_scaler_changed (wxCommandEvent &);
	void edit_reference_filters_clicked (wxCommandEvent &);
	void reference_filters_changed (std::vector<Filter const *>);
	void add_server_clicked (wxCommandEvent &);
	void edit_server_clicked (wxCommandEvent &);
	void remove_server_clicked (wxCommandEvent &);
	void server_selection_changed (wxListEvent &);

	void add_server_to_control (ServerDescription *);
	
	wxTextCtrl* _tms_ip;
	wxTextCtrl* _tms_path;
	wxTextCtrl* _tms_user;
	wxTextCtrl* _tms_password;
	wxSpinCtrl* _num_local_encoding_threads;
#ifdef __WXMSW__	
	DirPickerCtrl* _default_directory;
#else
	wxDirPickerCtrl* _default_directory;
#endif	
	wxComboBox* _colour_lut;
	wxSpinCtrl* _j2k_bandwidth;
	wxComboBox* _reference_scaler;
	wxStaticText* _reference_filters;
	wxButton* _reference_filters_button;
	wxListCtrl* _servers;
	wxButton* _add_server;
	wxButton* _edit_server;
	wxButton* _remove_server;
};

