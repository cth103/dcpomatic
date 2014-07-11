/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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
#include <wx/collpane.h>
#include <boost/signals2.hpp>
#include "lib/film.h"
#include "content_menu.h"
#include "content_panel.h"

class wxNotebook;
class wxListCtrl;
class wxListEvent;
class Film;
class TimelineDialog;
class Ratio;
class Timecode;
class FilmEditorPanel;
class SubtitleContent;

/** @class FilmEditor
 *  @brief A wx widget to edit a film's metadata, and perform various functions.
 */
class FilmEditor : public wxPanel
{
public:
	FilmEditor (boost::shared_ptr<Film>, wxWindow *);

	void set_film (boost::shared_ptr<Film>);

	boost::signals2::signal<void (boost::filesystem::path)> FileChanged;

	/* Stuff for panels */

	ContentPanel* content_panel () const {
		return _content_panel;
	}
	
	boost::shared_ptr<Film> film () const {
		return _film;
	}

private:
	void make_dcp_panel ();
	void connect_to_widgets ();
	
	/* Handle changes to the view */
	void name_changed ();
	void use_isdcf_name_toggled ();
	void edit_isdcf_button_clicked ();
	void container_changed ();
	void dcp_content_type_changed ();
	void scaler_changed ();
	void j2k_bandwidth_changed ();
	void frame_rate_choice_changed ();
	void frame_rate_spin_changed ();
	void best_frame_rate_clicked ();
	void content_timeline_clicked ();
	void audio_channels_changed ();
	void resolution_changed ();
	void three_d_changed ();
	void standard_changed ();
	void signed_toggled ();
	void burn_subtitles_toggled ();
	void encrypted_toggled ();

	/* Handle changes to the model */
	void film_changed (Film::Property);
	void film_content_changed (int);

	void set_general_sensitivity (bool);
	void setup_dcp_name ();
	void setup_container ();
	void setup_frame_rate_widget ();
	
	void active_jobs_changed (bool);
	void config_changed ();

	wxNotebook* _main_notebook;
	wxPanel* _dcp_panel;
	wxSizer* _dcp_sizer;
	ContentPanel* _content_panel;

	/** The film we are editing */
	boost::shared_ptr<Film> _film;
	wxTextCtrl* _name;
	wxStaticText* _dcp_name;
	wxCheckBox* _use_isdcf_name;
	wxChoice* _container;
	wxButton* _edit_isdcf_button;
	wxChoice* _scaler;
 	wxSpinCtrl* _j2k_bandwidth;
	wxChoice* _dcp_content_type;
	wxChoice* _frame_rate_choice;
	wxSpinCtrl* _frame_rate_spin;
	wxSizer* _frame_rate_sizer;
	wxSpinCtrl* _audio_channels;
	wxButton* _best_frame_rate;
	wxCheckBox* _three_d;
	wxChoice* _resolution;
	wxChoice* _standard;
	wxCheckBox* _signed;
	wxCheckBox* _burn_subtitles;
	wxCheckBox* _encrypted;

	std::vector<Ratio const *> _ratios;

	bool _generally_sensitive;
};
