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

/** @file src/film_editor.cc
 *  @brief A GTK widget to edit a film's metadata, and perform various functions.
 */

#include <iostream>
#include <gtkmm.h>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include "lib/format.h"
#include "lib/film.h"
#include "lib/transcode_job.h"
#include "lib/exceptions.h"
#include "lib/ab_transcode_job.h"
#include "lib/thumbs_job.h"
#include "lib/job_manager.h"
#include "lib/filter.h"
#include "lib/screen.h"
#include "lib/config.h"
#include "filter_dialog.h"
#include "gtk_util.h"
#include "film_editor.h"
#include "dcp_range_dialog.h"

using namespace std;
using namespace boost;
using namespace Gtk;

/** @param f Film to edit */
FilmEditor::FilmEditor (Film* f)
	: _film (f)
	, _filters_button ("Edit...")
	, _change_dcp_range_button ("Edit...")
	, _dcp_ab ("A/B")
{
	_vbox.set_border_width (12);
	_vbox.set_spacing (12);
	
	/* Set up our editing widgets */
	_left_crop.set_range (0, 1024);
	_left_crop.set_increments (1, 16);
	_top_crop.set_range (0, 1024);
	_top_crop.set_increments (1, 16);
	_right_crop.set_range (0, 1024);
	_right_crop.set_increments (1, 16);
	_bottom_crop.set_range (0, 1024);
	_bottom_crop.set_increments (1, 16);
	_filters.set_alignment (0, 0.5);
	_audio_gain.set_range (-60, 60);
	_audio_gain.set_increments (1, 3);
	_audio_delay.set_range (-1000, 1000);
	_audio_delay.set_increments (1, 20);
	_still_duration.set_range (0, 60 * 60);
	_still_duration.set_increments (1, 5);
	_dcp_range.set_alignment (0, 0.5);

	vector<Format const *> fmt = Format::all ();
	for (vector<Format const *>::iterator i = fmt.begin(); i != fmt.end(); ++i) {
		_format.append_text ((*i)->name ());
	}

	_frames_per_second.set_increments (1, 5);
	_frames_per_second.set_digits (2);
	_frames_per_second.set_range (0, 60);

	vector<DCPContentType const *> const ct = DCPContentType::all ();
	for (vector<DCPContentType const *>::const_iterator i = ct.begin(); i != ct.end(); ++i) {
		_dcp_content_type.append_text ((*i)->pretty_name ());
	}

	vector<Scaler const *> const sc = Scaler::all ();
	for (vector<Scaler const *>::const_iterator i = sc.begin(); i != sc.end(); ++i) {
		_scaler.append_text ((*i)->name ());
	}
	
	_original_size.set_alignment (0, 0.5);
	_length.set_alignment (0, 0.5);
	_audio.set_alignment (0, 0.5);

	/* And set their values from the Film */
	set_film (f);
	
	/* Now connect to them, since initial values are safely set */
	_name.signal_changed().connect (sigc::mem_fun (*this, &FilmEditor::name_changed));
	_frames_per_second.signal_changed().connect (sigc::mem_fun (*this, &FilmEditor::frames_per_second_changed));
	_format.signal_changed().connect (sigc::mem_fun (*this, &FilmEditor::format_changed));
	_content.signal_file_set().connect (sigc::mem_fun (*this, &FilmEditor::content_changed));
	_left_crop.signal_value_changed().connect (sigc::mem_fun (*this, &FilmEditor::left_crop_changed));
	_right_crop.signal_value_changed().connect (sigc::mem_fun (*this, &FilmEditor::right_crop_changed));
	_top_crop.signal_value_changed().connect (sigc::mem_fun (*this, &FilmEditor::top_crop_changed));
	_bottom_crop.signal_value_changed().connect (sigc::mem_fun (*this, &FilmEditor::bottom_crop_changed));
	_filters_button.signal_clicked().connect (sigc::mem_fun (*this, &FilmEditor::edit_filters_clicked));
	_scaler.signal_changed().connect (sigc::mem_fun (*this, &FilmEditor::scaler_changed));
	_dcp_content_type.signal_changed().connect (sigc::mem_fun (*this, &FilmEditor::dcp_content_type_changed));
	_dcp_ab.signal_toggled().connect (sigc::mem_fun (*this, &FilmEditor::dcp_ab_toggled));
	_audio_gain.signal_value_changed().connect (sigc::mem_fun (*this, &FilmEditor::audio_gain_changed));
	_audio_delay.signal_value_changed().connect (sigc::mem_fun (*this, &FilmEditor::audio_delay_changed));
	_still_duration.signal_value_changed().connect (sigc::mem_fun (*this, &FilmEditor::still_duration_changed));
	_change_dcp_range_button.signal_clicked().connect (sigc::mem_fun (*this, &FilmEditor::change_dcp_range_clicked));

	/* Set up the table */

	Table* t = manage (new Table);
	
	t->set_row_spacings (4);
	t->set_col_spacings (12);
	
	int n = 0;
	t->attach (left_aligned_label ("Name"), 0, 1, n, n + 1);
	t->attach (_name, 1, 2, n, n + 1);
	++n;
	t->attach (left_aligned_label ("Content"), 0, 1, n, n + 1);
	t->attach (_content, 1, 2, n, n + 1);
	++n;
	t->attach (left_aligned_label ("Content Type"), 0, 1, n, n + 1);
	t->attach (_dcp_content_type, 1, 2, n, n + 1);
	++n;
	t->attach (video_widget (left_aligned_label ("Frames Per Second")), 0, 1, n, n + 1);
	t->attach (video_widget (_frames_per_second), 1, 2, n, n + 1);
	++n;
	t->attach (left_aligned_label ("Format"), 0, 1, n, n + 1);
	t->attach (_format, 1, 2, n, n + 1);
	++n;
	t->attach (left_aligned_label ("Crop"), 0, 1, n, n + 1);
	HBox* c = manage (new HBox);
	c->set_spacing (4);
	c->pack_start (left_aligned_label ("L"), false, false);
	c->pack_start (_left_crop, true, true);
	c->pack_start (left_aligned_label ("R"), false, false);
	c->pack_start (_right_crop, true, true);
	c->pack_start (left_aligned_label ("T"), false, false);
	c->pack_start (_top_crop, true, true);
	c->pack_start (left_aligned_label ("B"), false, false);
	c->pack_start (_bottom_crop, true, true);
	t->attach (*c, 1, 2, n, n + 1);
	++n;

	int const special = n;
	
	/* VIDEO-only stuff */
	t->attach (video_widget (left_aligned_label ("Filters")), 0, 1, n, n + 1);
	HBox* fb = manage (new HBox);
	fb->set_spacing (4);
	fb->pack_start (video_widget (_filters), true, true);
	fb->pack_start (video_widget (_filters_button), false, false);
	t->attach (*fb, 1, 2, n, n + 1);
	++n;
	t->attach (video_widget (left_aligned_label ("Scaler")), 0, 1, n, n + 1);
	t->attach (video_widget (_scaler), 1, 2, n, n + 1);
	++n;
	t->attach (video_widget (left_aligned_label ("Audio Gain")), 0, 1, n, n + 1);
	t->attach (video_widget (_audio_gain), 1, 2, n, n + 1);
	t->attach (video_widget (left_aligned_label ("dB")), 2, 3, n, n + 1);
	++n;
	t->attach (video_widget (left_aligned_label ("Audio Delay")), 0, 1, n, n + 1);
	t->attach (video_widget (_audio_delay), 1, 2, n, n + 1);
	t->attach (video_widget (left_aligned_label ("ms")), 2, 3, n, n + 1);
	++n;
	t->attach (video_widget (left_aligned_label ("Original Size")), 0, 1, n, n + 1);
	t->attach (video_widget (_original_size), 1, 2, n, n + 1);
	++n;
	t->attach (video_widget (left_aligned_label ("Length")), 0, 1, n, n + 1);
	t->attach (video_widget (_length), 1, 2, n, n + 1);
	++n;
	t->attach (video_widget (left_aligned_label ("Audio")), 0, 1, n, n + 1);
	t->attach (video_widget (_audio), 1, 2, n, n + 1);
	++n;
	t->attach (video_widget (left_aligned_label ("Range")), 0, 1, n, n + 1);
	Gtk::HBox* db = manage (new Gtk::HBox);
	db->pack_start (_dcp_range, true, true);
	db->pack_start (_change_dcp_range_button, false, false);
	t->attach (*db, 1, 2, n, n + 1);
	++n;
	t->attach (_dcp_ab, 0, 3, n, n + 1);

	/* STILL-only stuff */
	n = special;
	t->attach (still_widget (left_aligned_label ("Duration")), 0, 1, n, n + 1);
	t->attach (still_widget (_still_duration), 1, 2, n, n + 1);
	t->attach (still_widget (left_aligned_label ("s")), 2, 3, n, n + 1);
	++n;

	t->show_all ();
	_vbox.pack_start (*t, false, false);

	setup_visibility ();
}

/** @return Our main widget, which contains everything else */
Widget&
FilmEditor::widget ()
{
	return _vbox;
}

/** Called when the left crop widget has been changed */
void
FilmEditor::left_crop_changed ()
{
	if (_film) {
		_film->set_left_crop (_left_crop.get_value ());
	}
}

/** Called when the right crop widget has been changed */
void
FilmEditor::right_crop_changed ()
{
	if (_film) {
		_film->set_right_crop (_right_crop.get_value ());
	}
}

/** Called when the top crop widget has been changed */
void
FilmEditor::top_crop_changed ()
{
	if (_film) {
		_film->set_top_crop (_top_crop.get_value ());
	}
}

/** Called when the bottom crop value has been changed */
void
FilmEditor::bottom_crop_changed ()
{
	if (_film) {
		_film->set_bottom_crop (_bottom_crop.get_value ());
	}
}

/** Called when the content filename has been changed */
void
FilmEditor::content_changed ()
{
	if (!_film) {
		return;
	}

	try {
		_film->set_content (_content.get_filename ());
	} catch (std::exception& e) {
		_content.set_filename (_film->directory ());
		stringstream m;
		m << "Could not set content: " << e.what() << ".";
		Gtk::MessageDialog d (m.str(), false, MESSAGE_ERROR);
		d.set_title ("DVD-o-matic");
		d.run ();
	}
}

/** Called when the DCP A/B switch has been toggled */
void
FilmEditor::dcp_ab_toggled ()
{
	if (_film) {
		_film->set_dcp_ab (_dcp_ab.get_active ());
	}
}

/** Called when the name widget has been changed */
void
FilmEditor::name_changed ()
{
	if (_film) {
		_film->set_name (_name.get_text ());
	}
}

/** Called when the metadata stored in the Film object has changed;
 *  so that we can update the GUI.
 *  @param p Property of the Film that has changed.
 */
void
FilmEditor::film_changed (Film::Property p)
{
	if (!_film) {
		return;
	}
	
	stringstream s;
		
	switch (p) {
	case Film::CONTENT:
		_content.set_filename (_film->content ());
		setup_visibility ();
		break;
	case Film::FORMAT:
		_format.set_active (Format::as_index (_film->format ()));
		break;
	case Film::LEFT_CROP:
		_left_crop.set_value (_film->left_crop ());
		break;
	case Film::RIGHT_CROP:
		_right_crop.set_value (_film->right_crop ());
		break;
	case Film::TOP_CROP:
		_top_crop.set_value (_film->top_crop ());
		break;
	case Film::BOTTOM_CROP:
		_bottom_crop.set_value (_film->bottom_crop ());
		break;
	case Film::FILTERS:
	{
		pair<string, string> p = Filter::ffmpeg_strings (_film->filters ());
		_filters.set_text (p.first + " " + p.second);
		break;
	}
	case Film::NAME:
		_name.set_text (_film->name ());
		break;
	case Film::FRAMES_PER_SECOND:
		_frames_per_second.set_value (_film->frames_per_second ());
		break;
	case Film::AUDIO_CHANNELS:
	case Film::AUDIO_SAMPLE_RATE:
		if (_film->audio_channels() == 0 && _film->audio_sample_rate() == 0) {
			_audio.set_text ("");
		} else {
			s << _film->audio_channels () << " channels, " << _film->audio_sample_rate() << "Hz";
			_audio.set_text (s.str ());
		}
		break;
	case Film::SIZE:
		if (_film->size().width == 0 && _film->size().height == 0) {
			_original_size.set_text ("");
		} else {
			s << _film->size().width << " x " << _film->size().height;
			_original_size.set_text (s.str ());
		}
		break;
	case Film::LENGTH:
		if (_film->frames_per_second() > 0 && _film->length() > 0) {
			s << _film->length() << " frames; " << seconds_to_hms (_film->length() / _film->frames_per_second());
		} else if (_film->length() > 0) {
			s << _film->length() << " frames";
		} 
		_length.set_text (s.str ());
		break;
	case Film::DCP_CONTENT_TYPE:
		_dcp_content_type.set_active (DCPContentType::as_index (_film->dcp_content_type ()));
		break;
	case Film::THUMBS:
		break;
	case Film::DCP_FRAMES:
		if (_film->dcp_frames() == 0) {
			_dcp_range.set_text ("Whole film");
		} else {
			stringstream s;
			s << "First " << _film->dcp_frames() << " frames";
			_dcp_range.set_text (s.str ());
		}
		break;
	case Film::DCP_TRIM_ACTION:
		break;
	case Film::DCP_AB:
		_dcp_ab.set_active (_film->dcp_ab ());
		break;
	case Film::SCALER:
		_scaler.set_active (Scaler::as_index (_film->scaler ()));
		break;
	case Film::AUDIO_GAIN:
		_audio_gain.set_value (_film->audio_gain ());
		break;
	case Film::AUDIO_DELAY:
		_audio_delay.set_value (_film->audio_delay ());
		break;
	case Film::STILL_DURATION:
		_still_duration.set_value (_film->still_duration ());
		break;
	}
}

/** Called when the format widget has been changed */
void
FilmEditor::format_changed ()
{
	if (_film) {
		int const n = _format.get_active_row_number ();
		if (n >= 0) {
			_film->set_format (Format::from_index (n));
		}
	}
}

/** Called when the DCP content type widget has been changed */
void
FilmEditor::dcp_content_type_changed ()
{
	if (_film) {
		int const n = _dcp_content_type.get_active_row_number ();
		if (n >= 0) {
			_film->set_dcp_content_type (DCPContentType::from_index (n));
		}
	}
}

/** Sets the Film that we are editing */
void
FilmEditor::set_film (Film* f)
{
	_film = f;

	set_things_sensitive (_film != 0);

	if (_film) {
		_film->Changed.connect (sigc::mem_fun (*this, &FilmEditor::film_changed));
	}

	if (_film) {
		FileChanged (_film->directory ());
	} else {
		FileChanged ("");
	}
	
	film_changed (Film::NAME);
	film_changed (Film::CONTENT);
	film_changed (Film::DCP_CONTENT_TYPE);
	film_changed (Film::FORMAT);
	film_changed (Film::LEFT_CROP);
	film_changed (Film::RIGHT_CROP);
	film_changed (Film::TOP_CROP);
	film_changed (Film::BOTTOM_CROP);
	film_changed (Film::FILTERS);
	film_changed (Film::DCP_FRAMES);
	film_changed (Film::DCP_TRIM_ACTION);
	film_changed (Film::DCP_AB);
	film_changed (Film::SIZE);
	film_changed (Film::LENGTH);
	film_changed (Film::FRAMES_PER_SECOND);
	film_changed (Film::AUDIO_CHANNELS);
	film_changed (Film::AUDIO_SAMPLE_RATE);
	film_changed (Film::SCALER);
	film_changed (Film::AUDIO_GAIN);
	film_changed (Film::AUDIO_DELAY);
	film_changed (Film::STILL_DURATION);
}

/** Updates the sensitivity of lots of widgets to a given value.
 *  @param s true to make sensitive, false to make insensitive.
 */
void
FilmEditor::set_things_sensitive (bool s)
{
	_name.set_sensitive (s);
	_frames_per_second.set_sensitive (s);
	_format.set_sensitive (s);
	_content.set_sensitive (s);
	_left_crop.set_sensitive (s);
	_right_crop.set_sensitive (s);
	_top_crop.set_sensitive (s);
	_bottom_crop.set_sensitive (s);
	_filters_button.set_sensitive (s);
	_scaler.set_sensitive (s);
	_dcp_content_type.set_sensitive (s);
	_dcp_range.set_sensitive (s);
	_change_dcp_range_button.set_sensitive (s);
	_dcp_ab.set_sensitive (s);
	_audio_gain.set_sensitive (s);
	_audio_delay.set_sensitive (s);
	_still_duration.set_sensitive (s);
}

/** Called when the `Edit filters' button has been clicked */
void
FilmEditor::edit_filters_clicked ()
{
	FilterDialog d (_film->filters ());
	d.ActiveChanged.connect (sigc::mem_fun (*_film, &Film::set_filters));
	d.run ();
}

/** Called when the scaler widget has been changed */
void
FilmEditor::scaler_changed ()
{
	if (_film) {
		int const n = _scaler.get_active_row_number ();
		if (n >= 0) {
			_film->set_scaler (Scaler::from_index (n));
		}
	}
}

/** Called when the frames per second widget has been changed */
void
FilmEditor::frames_per_second_changed ()
{
	if (_film) {
		_film->set_frames_per_second (_frames_per_second.get_value ());
	}
}

void
FilmEditor::audio_gain_changed ()
{
	if (_film) {
		_film->set_audio_gain (_audio_gain.get_value ());
	}
}

void
FilmEditor::audio_delay_changed ()
{
	if (_film) {
		_film->set_audio_delay (_audio_delay.get_value ());
	}
}

Widget&
FilmEditor::video_widget (Widget& w)
{
	_video_widgets.push_back (&w);
	return w;
}

Widget&
FilmEditor::still_widget (Widget& w)
{
	_still_widgets.push_back (&w);
	return w;
}

void
FilmEditor::setup_visibility ()
{
	ContentType c = VIDEO;

	if (_film) {
		c = _film->content_type ();
	}

	for (list<Widget *>::iterator i = _video_widgets.begin(); i != _video_widgets.end(); ++i) {
		(*i)->property_visible() = (c == VIDEO);
	}

	for (list<Widget *>::iterator i = _still_widgets.begin(); i != _still_widgets.end(); ++i) {
		(*i)->property_visible() = (c == STILL);
	}
}

void
FilmEditor::still_duration_changed ()
{
	if (_film) {
		_film->set_still_duration (_still_duration.get_value ());
	}
}

void
FilmEditor::change_dcp_range_clicked ()
{
	DCPRangeDialog d (_film);
	d.Changed.connect (sigc::mem_fun (*this, &FilmEditor::dcp_range_changed));
	d.run ();
}

void
FilmEditor::dcp_range_changed (int frames, TrimAction action)
{
	_film->set_dcp_frames (frames);
	_film->set_dcp_trim_action (action);
}
