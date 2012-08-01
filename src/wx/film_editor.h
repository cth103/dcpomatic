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
 *  @brief A wx widget to edit a film's metadata, and perform various functions.
 */

#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <wx/filepicker.h>
#include "lib/trim_action.h"
#include "lib/film.h"

class Film;

/** @class FilmEditor
 *  @brief A wx widget to edit a film's metadata, and perform various functions.
 */
class FilmEditor : public wxPanel
{
public:
	FilmEditor (Film *, wxWindow *);

	void set_film (Film *);
	void setup_visibility ();

	sigc::signal1<void, std::string> FileChanged;

private:
	/* Handle changes to the view */
	void name_changed (wxCommandEvent &);
	void left_crop_changed (wxCommandEvent &);
	void right_crop_changed (wxCommandEvent &);
	void top_crop_changed (wxCommandEvent &);
	void bottom_crop_changed (wxCommandEvent &);
	void content_changed (wxCommandEvent &);
	void format_changed (wxCommandEvent &);
	void dcp_range_changed (int, TrimAction);
	void dcp_content_type_changed (wxCommandEvent &);
	void dcp_ab_toggled (wxCommandEvent &);
	void scaler_changed (wxCommandEvent &);
	void audio_gain_changed (wxCommandEvent &);
	void audio_gain_calculate_button_clicked (wxCommandEvent &);
	void audio_delay_changed (wxCommandEvent &);
	void still_duration_changed (wxCommandEvent &);

	/* Handle changes to the model */
	void film_changed (Film::Property);

	/* Button clicks */
	void edit_filters_clicked (wxCommandEvent &);
	void change_dcp_range_clicked (wxCommandEvent &);

	void set_things_sensitive (bool);

	wxControl* video_control (wxControl *);
	wxControl* still_control (wxControl *);

	Film::Property _ignore_changes;

	/** The film we are editing */
	Film* _film;
	/** The Film's name */
	wxTextCtrl* _name;
	/** The Film's format */
	wxComboBox* _format;
	/** The Film's content file */
	wxFilePickerCtrl* _content;
	/** The Film's left crop */
	wxSpinCtrl* _left_crop;
	/** The Film's right crop */
	wxSpinCtrl* _right_crop;
	/** The Film's top crop */
	wxSpinCtrl* _top_crop;
	/** The Film's bottom crop */
	wxSpinCtrl* _bottom_crop;
	/** Currently-applied filters */
	wxStaticText* _filters;
	/** Button to open the filters dialogue */
	wxButton* _filters_button;
	/** The Film's scaler */
	wxComboBox* _scaler;
	/** The Film's audio gain */
	wxSpinCtrl* _audio_gain;
	/** A button to open the gain calculation dialogue */
	wxButton* _audio_gain_calculate_button;
	/** The Film's audio delay */
	wxSpinCtrl* _audio_delay;
	/** The Film's DCP content type */
	wxComboBox* _dcp_content_type;
	/** The Film's frames per second */
	wxStaticText* _frames_per_second;
	/** The Film's original size */
	wxStaticText* _original_size;
	/** The Film's length */
	wxStaticText* _length;
	/** The Film's audio details */
	wxStaticText* _audio;
	/** The Film's duration for still sources */
	wxSpinCtrl* _still_duration;

	/** Display of the range of frames that will be used */
	wxStaticText* _dcp_range;
	/** Button to change the range */
	wxButton* _change_dcp_range_button;
	/** Selector to generate an A/B comparison DCP */
	wxCheckBox* _dcp_ab;

	std::list<wxControl*> _video_controls;
	std::list<wxControl*> _still_controls;

	wxSizer* _sizer;
};
