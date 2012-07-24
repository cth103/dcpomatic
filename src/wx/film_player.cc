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

#include "lib/screen.h"
#include "lib/config.h"
#include "lib/player_manager.h"
#include "lib/film.h"
#include "film_player.h"
#include "gtk_util.h"

using namespace std;
using namespace boost;

FilmPlayer::FilmPlayer (Film const * f)
	: _play ("Play")
	, _pause ("Pause")
	, _stop ("Stop")
	, _ab ("A/B")
	, _ignore_position_changed (false)
{
	set_film (f);

	vector<shared_ptr<Screen> > const scr = Config::instance()->screens ();
	for (vector<shared_ptr<Screen> >::const_iterator i = scr.begin(); i != scr.end(); ++i) {
		_screen.append_text ((*i)->name ());
	}
	
	if (!scr.empty ()) {
		_screen.set_active (0);
	}

	_status.set_use_markup (true);

	_position.set_digits (0);

	_main_vbox.set_spacing (12);

	Gtk::HBox* l = manage (new Gtk::HBox);
	l->pack_start (_play);
	l->pack_start (_pause);
	l->pack_start (_stop);

	Gtk::VBox* r = manage (new Gtk::VBox);
	r->pack_start (_screen, false, false);
	r->pack_start (_ab, false, false);
	r->pack_start (*manage (new Gtk::Label ("")), true, true);

	Gtk::HBox* t = manage (new Gtk::HBox);
	t->pack_start (*l, true, true);
	t->pack_start (*r, false, false);

	_main_vbox.pack_start (*t, true, true);
	_main_vbox.pack_start (_position, false, false);
	_main_vbox.pack_start (_status, false, false);

	_play.signal_clicked().connect (sigc::mem_fun (*this, &FilmPlayer::play_clicked));
	_pause.signal_clicked().connect (sigc::mem_fun (*this, &FilmPlayer::pause_clicked));
	_stop.signal_clicked().connect (sigc::mem_fun (*this, &FilmPlayer::stop_clicked));
	_position.signal_value_changed().connect (sigc::mem_fun (*this, &FilmPlayer::position_changed));
	_position.signal_format_value().connect (sigc::mem_fun (*this, &FilmPlayer::format_position));

	set_button_states ();
	Glib::signal_timeout().connect (sigc::bind_return (sigc::mem_fun (*this, &FilmPlayer::update), true), 1000);

	Config::instance()->Changed.connect (sigc::mem_fun (*this, &FilmPlayer::update_screens));
}

void
FilmPlayer::set_film (Film const * f)
{
	_film = f;

	if (_film && _film->length() != 0 && _film->frames_per_second() != 0) {
		_position.set_range (0, _film->length() / _film->frames_per_second());
	}

	if (_film) {
		_film->Changed.connect (sigc::mem_fun (*this, &FilmPlayer::film_changed));
	}
}

Gtk::Widget &
FilmPlayer::widget ()
{
	return _main_vbox;
}

void
FilmPlayer::set_button_states ()
{
	if (_film == 0) {
		_play.set_sensitive (false);
		_pause.set_sensitive (false);
		_stop.set_sensitive (false);
		_screen.set_sensitive (false);
		_position.set_sensitive (false);
		_ab.set_sensitive (false);
		return;
	}
	
	PlayerManager::State s = PlayerManager::instance()->state ();

	switch (s) {
	case PlayerManager::QUIESCENT:
		_play.set_sensitive (true);
		_pause.set_sensitive (false);
		_stop.set_sensitive (false);
		_screen.set_sensitive (true);
		_position.set_sensitive (false);
		_ab.set_sensitive (true);
		break;
	case PlayerManager::PLAYING:
		_play.set_sensitive (false);
		_pause.set_sensitive (true);
		_stop.set_sensitive (true);
		_screen.set_sensitive (false);
		_position.set_sensitive (true);
		_ab.set_sensitive (false);
		break;
	case PlayerManager::PAUSED:
		_play.set_sensitive (true);
		_pause.set_sensitive (false);
		_stop.set_sensitive (true);
		_screen.set_sensitive (false);
		_position.set_sensitive (false);
		_ab.set_sensitive (false);
		break;
	}
}

void
FilmPlayer::play_clicked ()
{
	PlayerManager* p = PlayerManager::instance ();

	switch (p->state ()) {
	case PlayerManager::QUIESCENT:
		_last_play_fs = _film->state_copy ();
		if (_ab.get_active ()) {
			shared_ptr<FilmState> fs_a = _film->state_copy ();
			fs_a->filters.clear ();
			/* This is somewhat arbitrary, but hey ho */
			fs_a->scaler = Scaler::from_id ("bicubic");
			p->setup (fs_a, _last_play_fs, screen ());
		} else {
			p->setup (_last_play_fs, screen ());
		}
		p->pause_or_unpause ();
		break;
	case PlayerManager::PLAYING:
		break;
	case PlayerManager::PAUSED:
		p->pause_or_unpause ();
		break;
	}
}

void
FilmPlayer::pause_clicked ()
{
	PlayerManager* p = PlayerManager::instance ();

	switch (p->state ()) {
	case PlayerManager::QUIESCENT:
		break;
	case PlayerManager::PLAYING:
		p->pause_or_unpause ();
		break;
	case PlayerManager::PAUSED:
		break;
	}
}

void
FilmPlayer::stop_clicked ()
{
	PlayerManager::instance()->stop ();
}

shared_ptr<Screen>
FilmPlayer::screen () const
{
	vector<shared_ptr<Screen> > const s = Config::instance()->screens ();
	if (s.empty ()) {
		return shared_ptr<Screen> ();
	}
			 
	int const r = _screen.get_active_row_number ();
	if (r >= int (s.size ())) {
		return s[0];
	}
	
	return s[r];
}

void
FilmPlayer::update ()
{
	set_button_states ();
	set_status ();
}

void
FilmPlayer::set_status ()
{
	PlayerManager::State s = PlayerManager::instance()->state ();

	stringstream m;
	switch (s) {
	case PlayerManager::QUIESCENT:
		m << "Idle";
		break;
	case PlayerManager::PLAYING:
		m << "<span foreground=\"red\" weight=\"bold\">PLAYING</span>";
		break;
	case PlayerManager::PAUSED:
		m << "<b>Paused</b>";
		break;
	}

	_ignore_position_changed = true;
	
	if (s != PlayerManager::QUIESCENT) {
		float const p = PlayerManager::instance()->position ();
		if (_last_play_fs->frames_per_second != 0 && _last_play_fs->length != 0) {
			m << " <i>(" << seconds_to_hms (_last_play_fs->length / _last_play_fs->frames_per_second - p) << " remaining)</i>";
		}

		_position.set_value (p);
	} else {
		_position.set_value (0);
	}

	_ignore_position_changed = false;
	
	_status.set_markup (m.str ());
}

void
FilmPlayer::position_changed ()
{
	if (_ignore_position_changed) {
		return;
	}

	PlayerManager::instance()->set_position (_position.get_value ());
}

string
FilmPlayer::format_position (double v)
{
	return seconds_to_hms (v);
}

void
FilmPlayer::update_screens ()
{
	string const c = _screen.get_active_text ();
	
	_screen.clear ();
	
	vector<shared_ptr<Screen> > const scr = Config::instance()->screens ();
	bool have_last_active_text = false;
	for (vector<shared_ptr<Screen> >::const_iterator i = scr.begin(); i != scr.end(); ++i) {
		_screen.append_text ((*i)->name ());
		if ((*i)->name() == c) {
			have_last_active_text = true;
		}
	}

	if (have_last_active_text) {
		_screen.set_active_text (c);
	} else if (!scr.empty ()) {
		_screen.set_active (0);
	}
}

void
FilmPlayer::film_changed (Film::Property p)
{
	if (p == Film::CONTENT) {
		setup_visibility ();
	}
}

void
FilmPlayer::setup_visibility ()
{
	if (!_film) {
		return;
	}
	
	widget().property_visible() = (_film->content_type() == VIDEO);
}
