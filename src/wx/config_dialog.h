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

#include <gtkmm.h>

class Screen;
class Server;

/** @class ConfigDialog
 *  @brief A dialogue to edit DVD-o-matic configuration.
 */
class ConfigDialog : public Gtk::Dialog
{
public:
	ConfigDialog ();

private:
	void on_response (int);

	void tms_ip_changed ();
	void tms_path_changed ();
	void tms_user_changed ();
	void tms_password_changed ();
	void num_local_encoding_threads_changed ();
	void colour_lut_changed ();
	void j2k_bandwidth_changed ();
	void add_server_clicked ();
	void remove_server_clicked ();
	void server_selection_changed ();
	void add_screen_clicked ();
	void remove_screen_clicked ();
	void screen_selection_changed ();
	void reference_scaler_changed ();
	void edit_reference_filters_clicked ();
	void reference_filters_changed (std::vector<Filter const *>);

	void add_screen_to_store (boost::shared_ptr<Screen>);
	void add_server_to_store (Server *);

	struct ServersModelColumns : public Gtk::TreeModelColumnRecord
	{
		ServersModelColumns () {
			add (_host_name);
			add (_threads);
		}

		Gtk::TreeModelColumn<std::string> _host_name;
		Gtk::TreeModelColumn<int> _threads;
	};

	struct ScreensModelColumns : public Gtk::TreeModelColumnRecord
	{
		ScreensModelColumns () {
			add (_name);
			add (_format_name);
			add (_format_nickname);
			add (_x);
			add (_y);
			add (_width);
			add (_height);
		}

		Gtk::TreeModelColumn<std::string> _name;
		Gtk::TreeModelColumn<std::string> _format_name;
		Gtk::TreeModelColumn<std::string> _format_nickname;
		Gtk::TreeModelColumn<std::string> _x;
		Gtk::TreeModelColumn<std::string> _y;
		Gtk::TreeModelColumn<std::string> _width;
		Gtk::TreeModelColumn<std::string> _height;
	};

	Gtk::Entry _tms_ip;
	Gtk::Entry _tms_path;
	Gtk::Entry _tms_user;
	Gtk::Entry _tms_password;
	Gtk::SpinButton _num_local_encoding_threads;
	Gtk::ComboBoxText _colour_lut;
	Gtk::SpinButton _j2k_bandwidth;
	Gtk::ComboBoxText _reference_scaler;
	Gtk::Label _reference_filters;
	Gtk::Button _reference_filters_button;
	ServersModelColumns _servers_columns;
	Glib::RefPtr<Gtk::ListStore> _servers_store;
	Gtk::TreeView _servers_view;
	Gtk::Button _add_server;
	Gtk::Button _remove_server;
	ScreensModelColumns _screens_columns;
	Glib::RefPtr<Gtk::TreeStore> _screens_store;
	Gtk::TreeView _screens_view;
	Gtk::Button _add_screen;
	Gtk::Button _remove_screen;
};

