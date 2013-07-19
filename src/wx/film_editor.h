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
class AudioDialog;
class TimelineDialog;
class AudioMappingView;
class Ratio;
class Timecode;

/** @class FilmEditor
 *  @brief A wx widget to edit a film's metadata, and perform various functions.
 */
class FilmEditor : public wxPanel
{
public:
	FilmEditor (boost::shared_ptr<Film>, wxWindow *);

	void set_film (boost::shared_ptr<Film>);
	void set_selection (boost::weak_ptr<Content>);

	boost::signals2::signal<void (std::string)> FileChanged;

private:
	void make_dcp_panel ();
	void make_content_panel ();
	void make_video_panel ();
	void make_audio_panel ();
	void make_subtitle_panel ();
	void make_timing_panel ();
	void connect_to_widgets ();
	
	/* Handle changes to the view */
	void name_changed (wxCommandEvent &);
	void use_dci_name_toggled (wxCommandEvent &);
	void edit_dci_button_clicked (wxCommandEvent &);
	void left_crop_changed (wxCommandEvent &);
	void right_crop_changed (wxCommandEvent &);
	void top_crop_changed (wxCommandEvent &);
	void bottom_crop_changed (wxCommandEvent &);
	void trust_content_headers_changed (wxCommandEvent &);
	void content_selection_changed (wxListEvent &);
	void content_add_clicked (wxCommandEvent &);
	void content_remove_clicked (wxCommandEvent &);
	void imagemagick_video_length_changed (wxCommandEvent &);
	void container_changed (wxCommandEvent &);
	void dcp_content_type_changed (wxCommandEvent &);
	void scaler_changed (wxCommandEvent &);
	void audio_gain_changed (wxCommandEvent &);
	void audio_gain_calculate_button_clicked (wxCommandEvent &);
	void show_audio_clicked (wxCommandEvent &);
	void audio_delay_changed (wxCommandEvent &);
	void with_subtitles_toggled (wxCommandEvent &);
	void subtitle_offset_changed (wxCommandEvent &);
	void subtitle_scale_changed (wxCommandEvent &);
	void j2k_bandwidth_changed (wxCommandEvent &);
	void dcp_frame_rate_changed (wxCommandEvent &);
	void best_dcp_frame_rate_clicked (wxCommandEvent &);
	void edit_filters_clicked (wxCommandEvent &);
	void content_timeline_clicked (wxCommandEvent &);
	void audio_stream_changed (wxCommandEvent &);
	void subtitle_stream_changed (wxCommandEvent &);
	void audio_mapping_changed (AudioMapping);
	void start_changed ();
	void length_changed ();
	void ratio_changed (wxCommandEvent &);
	void dcp_audio_channels_changed (wxCommandEvent &);
	void dcp_resolution_changed (wxCommandEvent &);
	void sequence_video_changed (wxCommandEvent &);
	void content_right_click (wxListEvent &);

	/* Handle changes to the model */
	void film_changed (Film::Property);
	void film_content_changed (boost::weak_ptr<Content>, int);

	void set_things_sensitive (bool);
	void setup_ratios ();
	void setup_subtitle_control_sensitivity ();
	void setup_dcp_name ();
	void setup_show_audio_sensitivity ();
	void setup_scaling_description ();
	void setup_content ();
	void setup_container ();
	void setup_content_sensitivity ();
	
	void active_jobs_changed (bool);
	boost::shared_ptr<Content> selected_content ();
	boost::shared_ptr<VideoContent> selected_video_content ();
	boost::shared_ptr<AudioContent> selected_audio_content ();
	boost::shared_ptr<SubtitleContent> selected_subtitle_content ();

	wxNotebook* _main_notebook;
	wxNotebook* _content_notebook;
	wxPanel* _dcp_panel;
	wxSizer* _dcp_sizer;
	wxPanel* _content_panel;
	wxSizer* _content_sizer;
	wxPanel* _video_panel;
	wxPanel* _audio_panel;
	wxPanel* _subtitle_panel;
	wxPanel* _timing_panel;

	/** The film we are editing */
	boost::shared_ptr<Film> _film;
	wxTextCtrl* _name;
	wxStaticText* _dcp_name;
	wxCheckBox* _use_dci_name;
	wxChoice* _container;
	wxListCtrl* _content;
	wxButton* _content_add;
	wxButton* _content_remove;
	wxButton* _content_earlier;
	wxButton* _content_later;
	wxButton* _content_timeline;
	wxCheckBox* _sequence_video;
	wxButton* _edit_dci_button;
	wxChoice* _ratio;
	wxStaticText* _ratio_description;
	wxStaticText* _scaling_description;
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
	wxSpinCtrl* _subtitle_offset;
	wxSpinCtrl* _subtitle_scale;
	wxSpinCtrl* _j2k_bandwidth;
	wxChoice* _dcp_content_type;
	wxChoice* _dcp_frame_rate;
	wxSpinCtrl* _dcp_audio_channels;
	wxButton* _best_dcp_frame_rate;
	wxChoice* _audio_stream;
	wxStaticText* _audio_description;
	wxChoice* _subtitle_stream;
	AudioMappingView* _audio_mapping;
	Timecode* _start;
	Timecode* _length;
	wxChoice* _dcp_resolution;

	ContentMenu _menu;

	std::vector<Ratio const *> _ratios;

	bool _generally_sensitive;
	AudioDialog* _audio_dialog;
	TimelineDialog* _timeline_dialog;
};
