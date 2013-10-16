/*
    Copyright (C) 2012-2013 Carl Hetherington <cth@carlh.net>

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
	void set_selection (boost::weak_ptr<Content>);

	boost::signals2::signal<void (boost::filesystem::path)> FileChanged;

	/* Stuff for panels */
	
	wxNotebook* content_notebook () const {
		return _content_notebook;
	}

	boost::shared_ptr<Film> film () const {
		return _film;
	}

	boost::shared_ptr<Content> selected_content ();
	boost::shared_ptr<VideoContent> selected_video_content ();
	boost::shared_ptr<AudioContent> selected_audio_content ();
	boost::shared_ptr<SubtitleContent> selected_subtitle_content ();
	
private:
	void make_dcp_panel ();
	void make_content_panel ();
	void connect_to_widgets ();
	
	/* Handle changes to the view */
	void name_changed ();
	void use_dci_name_toggled ();
	void edit_dci_button_clicked ();
	void content_selection_changed ();
	void content_add_file_clicked ();
	void content_add_folder_clicked ();
	void content_remove_clicked ();
	void content_earlier_clicked ();
	void content_later_clicked ();
	void container_changed ();
	void dcp_content_type_changed ();
	void scaler_changed ();
	void j2k_bandwidth_changed ();
	void frame_rate_changed ();
	void best_frame_rate_clicked ();
	void content_timeline_clicked ();
	void audio_channels_changed ();
	void resolution_changed ();
	void sequence_video_changed ();
	void content_right_click (wxListEvent &);
	void three_d_changed ();
	void standard_changed ();
	void encrypted_toggled ();

	/* Handle changes to the model */
	void film_changed (Film::Property);
	void film_content_changed (boost::weak_ptr<Content>, int);

	void set_general_sensitivity (bool);
	void setup_dcp_name ();
	void setup_content ();
	void setup_container ();
	void setup_content_sensitivity ();
	
	void active_jobs_changed (bool);

	FilmEditorPanel* _video_panel;
	FilmEditorPanel* _audio_panel;
	FilmEditorPanel* _subtitle_panel;
	FilmEditorPanel* _timing_panel;
	std::list<FilmEditorPanel *> _panels;

	wxNotebook* _main_notebook;
	wxNotebook* _content_notebook;
	wxPanel* _dcp_panel;
	wxSizer* _dcp_sizer;
	wxPanel* _content_panel;
	wxSizer* _content_sizer;

	/** The film we are editing */
	boost::shared_ptr<Film> _film;
	wxTextCtrl* _name;
	wxStaticText* _dcp_name;
	wxCheckBox* _use_dci_name;
	wxChoice* _container;
	wxListCtrl* _content;
	wxButton* _content_add_file;
	wxButton* _content_add_folder;
	wxButton* _content_remove;
	wxButton* _content_earlier;
	wxButton* _content_later;
	wxButton* _content_timeline;
	wxCheckBox* _sequence_video;
	wxButton* _edit_dci_button;
	wxChoice* _scaler;
 	wxSpinCtrl* _j2k_bandwidth;
	wxChoice* _dcp_content_type;
	wxChoice* _frame_rate;
	wxSpinCtrl* _audio_channels;
	wxButton* _best_frame_rate;
	wxCheckBox* _three_d;
	wxChoice* _resolution;
	wxChoice* _standard;
	wxCheckBox* _encrypted;

	ContentMenu _menu;

	std::vector<Ratio const *> _ratios;

	bool _generally_sensitive;
	TimelineDialog* _timeline_dialog;
};
