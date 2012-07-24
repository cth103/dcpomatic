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
#include "gtk_util.h"
#include "filter_dialog.h"

using namespace std;
using namespace boost;

ConfigDialog::ConfigDialog ()
	: Gtk::Dialog ("DVD-o-matic Configuration")
	, _reference_filters_button ("Edit...")
	, _add_server ("Add Server")
	, _remove_server ("Remove Server")
	, _add_screen ("Add Screen")
	, _remove_screen ("Remove Screen")
{
	Gtk::Table* t = manage (new Gtk::Table);
	t->set_row_spacings (6);
	t->set_col_spacings (6);
	t->set_border_width (6);

	Config* config = Config::instance ();

	_tms_ip.set_text (config->tms_ip ());
	_tms_ip.signal_changed().connect (sigc::mem_fun (*this, &ConfigDialog::tms_ip_changed));
	_tms_path.set_text (config->tms_path ());
	_tms_path.signal_changed().connect (sigc::mem_fun (*this, &ConfigDialog::tms_path_changed));
	_tms_user.set_text (config->tms_user ());
	_tms_user.signal_changed().connect (sigc::mem_fun (*this, &ConfigDialog::tms_user_changed));
	_tms_password.set_text (config->tms_password ());
	_tms_password.signal_changed().connect (sigc::mem_fun (*this, &ConfigDialog::tms_password_changed));

	_num_local_encoding_threads.set_range (1, 128);
	_num_local_encoding_threads.set_increments (1, 4);
	_num_local_encoding_threads.set_value (config->num_local_encoding_threads ());
	_num_local_encoding_threads.signal_changed().connect (sigc::mem_fun (*this, &ConfigDialog::num_local_encoding_threads_changed));

	_colour_lut.append_text ("sRGB");
	_colour_lut.append_text ("Rec 709");
	_colour_lut.set_active (config->colour_lut_index ());
	_colour_lut.signal_changed().connect (sigc::mem_fun (*this, &ConfigDialog::colour_lut_changed));
	
	_j2k_bandwidth.set_range (50, 250);
	_j2k_bandwidth.set_increments (10, 50);
	_j2k_bandwidth.set_value (config->j2k_bandwidth() / 1e6);
	_j2k_bandwidth.signal_changed().connect (sigc::mem_fun (*this, &ConfigDialog::j2k_bandwidth_changed));

	vector<Scaler const *> const sc = Scaler::all ();
	for (vector<Scaler const *>::const_iterator i = sc.begin(); i != sc.end(); ++i) {
		_reference_scaler.append_text ((*i)->name ());
	}
	_reference_scaler.set_active (Scaler::as_index (config->reference_scaler ()));
	_reference_scaler.signal_changed().connect (sigc::mem_fun (*this, &ConfigDialog::reference_scaler_changed));

	_reference_filters.set_alignment (0, 0.5);
	pair<string, string> p = Filter::ffmpeg_strings (config->reference_filters ());
	_reference_filters.set_text (p.first + " " + p.second);
	_reference_filters_button.signal_clicked().connect (sigc::mem_fun (*this, &ConfigDialog::edit_reference_filters_clicked));

	_servers_store = Gtk::ListStore::create (_servers_columns);
	vector<Server*> servers = config->servers ();
	for (vector<Server*>::iterator i = servers.begin(); i != servers.end(); ++i) {
		add_server_to_store (*i);
	}
	
	_servers_view.set_model (_servers_store);
	_servers_view.append_column_editable ("Host Name", _servers_columns._host_name);
	_servers_view.append_column_editable ("Threads", _servers_columns._threads);

	_add_server.signal_clicked().connect (sigc::mem_fun (*this, &ConfigDialog::add_server_clicked));
	_remove_server.signal_clicked().connect (sigc::mem_fun (*this, &ConfigDialog::remove_server_clicked));

	_servers_view.get_selection()->signal_changed().connect (sigc::mem_fun (*this, &ConfigDialog::server_selection_changed));
	server_selection_changed ();
	
	_screens_store = Gtk::TreeStore::create (_screens_columns);
	vector<shared_ptr<Screen> > screens = config->screens ();
	for (vector<shared_ptr<Screen> >::iterator i = screens.begin(); i != screens.end(); ++i) {
		add_screen_to_store (*i);
	}

	_screens_view.set_model (_screens_store);
	_screens_view.append_column_editable ("Screen", _screens_columns._name);
	_screens_view.append_column ("Format", _screens_columns._format_name);
	_screens_view.append_column_editable ("x", _screens_columns._x);
	_screens_view.append_column_editable ("y", _screens_columns._y);
	_screens_view.append_column_editable ("Width", _screens_columns._width);
	_screens_view.append_column_editable ("Height", _screens_columns._height);

	_add_screen.signal_clicked().connect (sigc::mem_fun (*this, &ConfigDialog::add_screen_clicked));
	_remove_screen.signal_clicked().connect (sigc::mem_fun (*this, &ConfigDialog::remove_screen_clicked));

	_screens_view.get_selection()->signal_changed().connect (sigc::mem_fun (*this, &ConfigDialog::screen_selection_changed));
	screen_selection_changed ();

	int n = 0;
	t->attach (left_aligned_label ("TMS IP address"), 0, 1, n, n + 1);
	t->attach (_tms_ip, 1, 2, n, n + 1);
	++n;
	t->attach (left_aligned_label ("TMS target path"), 0, 1, n, n + 1);
	t->attach (_tms_path, 1, 2, n, n + 1);
	++n;
	t->attach (left_aligned_label ("TMS user name"), 0, 1, n, n + 1);
	t->attach (_tms_user, 1, 2, n, n + 1);
	++n;
	t->attach (left_aligned_label ("TMS password"), 0, 1, n, n + 1);
	t->attach (_tms_password, 1, 2, n, n + 1);
	++n;
	t->attach (left_aligned_label ("Threads to use for encoding on this host"), 0, 1, n, n + 1);
	t->attach (_num_local_encoding_threads, 1, 2, n, n + 1);
	++n;
	t->attach (left_aligned_label ("Colour look-up table"), 0, 1, n, n + 1);
	t->attach (_colour_lut, 1, 2, n, n + 1);
	++n;
	t->attach (left_aligned_label ("JPEG2000 bandwidth"), 0, 1, n, n + 1);
	t->attach (_j2k_bandwidth, 1, 2, n, n + 1);
	t->attach (left_aligned_label ("MBps"), 2, 3, n, n + 1);
	++n;
	t->attach (left_aligned_label ("Reference scaler for A/B"), 0, 1, n, n + 1);
	t->attach (_reference_scaler, 1, 2, n, n + 1);
	++n;
	t->attach (left_aligned_label ("Reference filters for A/B"), 0, 1, n, n + 1);
	Gtk::HBox* fb = Gtk::manage (new Gtk::HBox);
	fb->set_spacing (4);
	fb->pack_start (_reference_filters, true, true);
	fb->pack_start (_reference_filters_button, false, false);
	t->attach (*fb, 1, 2, n, n + 1);
	++n;
	t->attach (left_aligned_label ("Encoding Servers"), 0, 1, n, n + 1);
	t->attach (_servers_view, 1, 2, n, n + 1);
	Gtk::VBox* b = manage (new Gtk::VBox);
	b->pack_start (_add_server, false, false);
	b->pack_start (_remove_server, false, false);
	t->attach (*b, 2, 3, n, n + 1);
	++n;
	t->attach (left_aligned_label ("Screens"), 0, 1, n, n + 1);
	t->attach (_screens_view, 1, 2, n, n + 1);
	b = manage (new Gtk::VBox);
	b->pack_start (_add_screen, false, false);
	b->pack_start (_remove_screen, false, false);
	t->attach (*b, 2, 3, n, n + 1);
	++n;

	t->show_all ();
	get_vbox()->pack_start (*t);

	get_vbox()->set_border_width (24);

	add_button ("Close", Gtk::RESPONSE_CLOSE);
}

void
ConfigDialog::tms_ip_changed ()
{
	Config::instance()->set_tms_ip (_tms_ip.get_text ());
}

void
ConfigDialog::tms_path_changed ()
{
	Config::instance()->set_tms_path (_tms_path.get_text ());
}

void
ConfigDialog::tms_user_changed ()
{
	Config::instance()->set_tms_user (_tms_user.get_text ());
}

void
ConfigDialog::tms_password_changed ()
{
	Config::instance()->set_tms_password (_tms_password.get_text ());
}


void
ConfigDialog::num_local_encoding_threads_changed ()
{
	Config::instance()->set_num_local_encoding_threads (_num_local_encoding_threads.get_value ());
}

void
ConfigDialog::colour_lut_changed ()
{
	Config::instance()->set_colour_lut_index (_colour_lut.get_active_row_number ());
}

void
ConfigDialog::j2k_bandwidth_changed ()
{
	Config::instance()->set_j2k_bandwidth (_j2k_bandwidth.get_value() * 1e6);
}

void
ConfigDialog::on_response (int r)
{
	vector<Server*> servers;
	
	Gtk::TreeModel::Children c = _servers_store->children ();
	for (Gtk::TreeModel::Children::iterator i = c.begin(); i != c.end(); ++i) {
		Gtk::TreeModel::Row r = *i;
		Server* s = new Server (r[_servers_columns._host_name], r[_servers_columns._threads]);
		servers.push_back (s);
	}

	Config::instance()->set_servers (servers);

	vector<shared_ptr<Screen> > screens;

	c = _screens_store->children ();
	for (Gtk::TreeModel::Children::iterator i = c.begin(); i != c.end(); ++i) {

		Gtk::TreeModel::Row r = *i;
		shared_ptr<Screen> s (new Screen (r[_screens_columns._name]));

		Gtk::TreeModel::Children cc = r.children ();
		for (Gtk::TreeModel::Children::iterator j = cc.begin(); j != cc.end(); ++j) {
			Gtk::TreeModel::Row r = *j;
			string const x_ = r[_screens_columns._x];
			string const y_ = r[_screens_columns._y];
			string const width_ = r[_screens_columns._width];
			string const height_ = r[_screens_columns._height];
			s->set_geometry (
				Format::from_nickname (r[_screens_columns._format_nickname]),
				Position (lexical_cast<int> (x_), lexical_cast<int> (y_)),
				Size (lexical_cast<int> (width_), lexical_cast<int> (height_))
				);
		}

		screens.push_back (s);
	}
	
	Config::instance()->set_screens (screens);
	
	Gtk::Dialog::on_response (r);
}

void
ConfigDialog::add_server_to_store (Server* s)
{
	Gtk::TreeModel::iterator i = _servers_store->append ();
	Gtk::TreeModel::Row r = *i;
	r[_servers_columns._host_name] = s->host_name ();
	r[_servers_columns._threads] = s->threads ();
}

void
ConfigDialog::add_server_clicked ()
{
	Server s ("localhost", 1);
	add_server_to_store (&s);
}

void
ConfigDialog::remove_server_clicked ()
{
	Gtk::TreeModel::iterator i = _servers_view.get_selection()->get_selected ();
	if (i) {
		_servers_store->erase (i);
	}
}

void
ConfigDialog::server_selection_changed ()
{
	Gtk::TreeModel::iterator i = _servers_view.get_selection()->get_selected ();
	_remove_server.set_sensitive (i);
}
	

void
ConfigDialog::add_screen_to_store (shared_ptr<Screen> s)
{
	Gtk::TreeModel::iterator i = _screens_store->append ();
	Gtk::TreeModel::Row r = *i;
	r[_screens_columns._name] = s->name ();

	Screen::GeometryMap geoms = s->geometries ();
	for (Screen::GeometryMap::const_iterator j = geoms.begin(); j != geoms.end(); ++j) {
		i = _screens_store->append (r.children ());
		Gtk::TreeModel::Row c = *i;
		c[_screens_columns._format_name] = j->first->name ();
		c[_screens_columns._format_nickname] = j->first->nickname ();
		c[_screens_columns._x] = lexical_cast<string> (j->second.position.x);
		c[_screens_columns._y] = lexical_cast<string> (j->second.position.y);
		c[_screens_columns._width] = lexical_cast<string> (j->second.size.width);
		c[_screens_columns._height] = lexical_cast<string> (j->second.size.height);
	}
}

void
ConfigDialog::add_screen_clicked ()
{
	shared_ptr<Screen> s (new Screen ("New Screen"));
	add_screen_to_store (s);
}

void
ConfigDialog::remove_screen_clicked ()
{
	Gtk::TreeModel::iterator i = _screens_view.get_selection()->get_selected ();
	if (i) {
		_screens_store->erase (i);
	}
}

void
ConfigDialog::screen_selection_changed ()
{
	Gtk::TreeModel::iterator i = _screens_view.get_selection()->get_selected ();
	_remove_screen.set_sensitive (i);
}
	

void
ConfigDialog::reference_scaler_changed ()
{
	int const n = _reference_scaler.get_active_row_number ();
	if (n >= 0) {
		Config::instance()->set_reference_scaler (Scaler::from_index (n));
	}
}

void
ConfigDialog::edit_reference_filters_clicked ()
{
	FilterDialog d (Config::instance()->reference_filters ());
	d.ActiveChanged.connect (sigc::mem_fun (*this, &ConfigDialog::reference_filters_changed));
	d.run ();
}

void
ConfigDialog::reference_filters_changed (vector<Filter const *> f)
{
	Config::instance()->set_reference_filters (f);
	pair<string, string> p = Filter::ffmpeg_strings (Config::instance()->reference_filters ());
	_reference_filters.set_text (p.first + " " + p.second);
}
