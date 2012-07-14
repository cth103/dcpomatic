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

/** @file src/film_editor.h
 *  @brief A GTK widget to edit a film's metadata, and perform various functions.
 */

#include <gtkmm.h>

class Film;

/** @class FilmEditor
 *  @brief A GTK widget to edit a film's metadata, and perform various functions.
 */
class FilmEditor
{
public:
	FilmEditor (Film *);

	Gtk::Widget& widget ();

	void set_film (Film *);
	void setup_visibility ();

	sigc::signal1<void, std::string> FileChanged;

private:
	/* Handle changes to the view */
	void name_changed ();
	void left_crop_changed ();
	void right_crop_changed ();
	void top_crop_changed ();
	void bottom_crop_changed ();
	void content_changed ();
	void frames_per_second_changed ();
	void format_changed ();
	void dcp_range_changed (int, TrimAction);
	void dcp_content_type_changed ();
	void dcp_ab_toggled ();
	void scaler_changed ();
	void audio_gain_changed ();
	void audio_delay_changed ();
	void still_duration_changed ();

	/* Handle changes to the model */
	void film_changed (Film::Property);

	/* Button clicks */
	void edit_filters_clicked ();
	void change_dcp_range_clicked ();

	void set_things_sensitive (bool);

	Gtk::Widget & video_widget (Gtk::Widget &);
	Gtk::Widget & still_widget (Gtk::Widget &);

	/** The film we are editing */
	Film* _film;
	/** The overall VBox containing our widget */
	Gtk::VBox _vbox;
	/** The Film's name */
	Gtk::Entry _name;
	/** The Film's frames per second */
	Gtk::SpinButton _frames_per_second;
	/** The Film's format */
	Gtk::ComboBoxText _format;
	/** The Film's content file */
	Gtk::FileChooserButton _content;
	/** The Film's left crop */
	Gtk::SpinButton _left_crop;
	/** The Film's right crop */
	Gtk::SpinButton _right_crop;
	/** The Film's top crop */
	Gtk::SpinButton _top_crop;
	/** The Film's bottom crop */
	Gtk::SpinButton _bottom_crop;
	/** Currently-applied filters */
	Gtk::Label _filters;
	/** Button to open the filters dialogue */
	Gtk::Button _filters_button;
	/** The Film's scaler */
	Gtk::ComboBoxText _scaler;
	/** The Film's audio gain */
	Gtk::SpinButton _audio_gain;
	/** The Film's audio delay */
	Gtk::SpinButton _audio_delay;
	/** The Film's DCP content type */
	Gtk::ComboBoxText _dcp_content_type;
	/** The Film's original size */
	Gtk::Label _original_size;
	/** The Film's length */
	Gtk::Label _length;
	/** The Film's audio details */
	Gtk::Label _audio;
	/** The Film's duration for still sources */
	Gtk::SpinButton _still_duration;

	/** Button to start making a DCP from existing J2K and WAV files */
	Gtk::Button _make_dcp_from_existing_button;
	/** Display of the range of frames that will be used */
	Gtk::Label _dcp_range;
	/** Button to change the range */
	Gtk::Button _change_dcp_range_button;
	/** Selector to generate an A/B comparison DCP */
	Gtk::CheckButton _dcp_ab;

	std::list<Gtk::Widget*> _video_widgets;
	std::list<Gtk::Widget*> _still_widgets;
};
