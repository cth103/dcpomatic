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
#include <wx/collpane.h>
#include <boost/signals2.hpp>
#include "lib/film.h"

class wxNotebook;
class wxListCtrl;
class Film;
class AudioDialog;

/** @class FilmEditor
 *  @brief A wx widget to edit a film's metadata, and perform various functions.
 */
class FilmEditor : public wxPanel
{
public:
	FilmEditor (boost::shared_ptr<Film>, wxWindow *);

	void set_film (boost::shared_ptr<Film>);

	boost::signals2::signal<void (std::string)> FileChanged;

private:
	void make_film_panel ();
	void make_content_panel ();
	void make_video_panel ();
	void make_audio_panel ();
	void make_subtitle_panel ();
	void connect_to_widgets ();
	
	/* Handle changes to the view */
	void name_changed (wxCommandEvent &);
	void use_dci_name_toggled (wxCommandEvent &);
	void edit_dci_button_clicked (wxCommandEvent &);
	void left_crop_changed (wxCommandEvent &);
	void right_crop_changed (wxCommandEvent &);
	void top_crop_changed (wxCommandEvent &);
	void bottom_crop_changed (wxCommandEvent &);
	void trust_content_header_changed (wxCommandEvent &);
	void format_changed (wxCommandEvent &);
	void trim_start_changed (wxCommandEvent &);
	void trim_end_changed (wxCommandEvent &);
	void dcp_content_type_changed (wxCommandEvent &);
	void dcp_ab_toggled (wxCommandEvent &);
	void scaler_changed (wxCommandEvent &);
	void audio_gain_changed (wxCommandEvent &);
	void audio_gain_calculate_button_clicked (wxCommandEvent &);
	void show_audio_clicked (wxCommandEvent &);
	void audio_delay_changed (wxCommandEvent &);
	void with_subtitles_toggled (wxCommandEvent &);
	void subtitle_offset_changed (wxCommandEvent &);
	void subtitle_scale_changed (wxCommandEvent &);
	void colour_lut_changed (wxCommandEvent &);
	void j2k_bandwidth_changed (wxCommandEvent &);
	void ffmpeg_audio_stream_changed (wxCommandEvent &);
	void ffmpeg_subtitle_stream_changed (wxCommandEvent &);
	void dcp_frame_rate_changed (wxCommandEvent &);
	void best_dcp_frame_rate_clicked (wxCommandEvent &);

	/* Handle changes to the model */
	void film_changed (Film::Property);

	/* Button clicks */
	void edit_filters_clicked (wxCommandEvent &);

	void set_things_sensitive (bool);
	void setup_formats ();
	void setup_subtitle_control_sensitivity ();
	void setup_streams ();
	void setup_audio_details ();
	void setup_dcp_name ();
	void setup_show_audio_sensitivity ();
	void setup_content ();
	
	void active_jobs_changed (bool);

	wxNotebook* _notebook;
	wxPanel* _film_panel;
	wxSizer* _film_sizer;
	wxPanel* _content_panel;
	wxSizer* _content_sizer;
	wxPanel* _video_panel;
	wxSizer* _video_sizer;
	wxPanel* _audio_panel;
	wxSizer* _audio_sizer;
	wxPanel* _subtitle_panel;
	wxSizer* _subtitle_sizer;

	/** The film we are editing */
	boost::shared_ptr<Film> _film;
	/** The Film's name */
	wxTextCtrl* _name;
	wxStaticText* _dcp_name;
	wxCheckBox* _use_dci_name;
	wxListCtrl* _content;
	wxButton* _content_add;
	wxButton* _content_remove;
	wxButton* _content_earlier;
	wxButton* _content_later;
	wxButton* _edit_dci_button;
	wxChoice* _format;
	wxStaticText* _format_description;
	wxCheckBox* _trust_content_header;
	wxSpinCtrl* _left_crop;
	wxSpinCtrl* _right_crop;
	wxSpinCtrl* _top_crop;
	wxSpinCtrl* _bottom_crop;
	wxStaticText* _filters;
	wxButton* _filters_button;
	wxChoice* _scaler;
	wxSpinCtrl* _audio_gain;
	wxButton* _audio_gain_calculate_button;
	wxButton* _show_audio;
	wxSpinCtrl* _audio_delay;
	wxCheckBox* _with_subtitles;
	wxChoice* _ffmpeg_subtitle_stream;
	wxSpinCtrl* _subtitle_offset;
	wxSpinCtrl* _subtitle_scale;
	wxChoice* _colour_lut;
	wxSpinCtrl* _j2k_bandwidth;
	wxChoice* _dcp_content_type;
	wxStaticText* _source_frame_rate;
	wxChoice* _dcp_frame_rate;
	wxButton* _best_dcp_frame_rate;
	wxStaticText* _frame_rate_description;
	wxStaticText* _original_size;
	wxStaticText* _length;
	/** The Film's audio details */
	wxStaticText* _audio;

	wxSpinCtrl* _trim_start;
	wxSpinCtrl* _trim_end;
	/** Selector to generate an A/B comparison DCP */
	wxCheckBox* _dcp_ab;

	std::vector<Format const *> _formats;

	bool _generally_sensitive;
	AudioDialog* _audio_dialog;
};
