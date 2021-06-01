/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/


#include "lib/config.h"
#include "lib/film.h"


class Button;
class wxNotebook;
class wxPanel;
class wxBoxSizer;
class wxTextCtrl;
class wxStaticText;
class wxCheckBox;
class wxChoice;
class wxButton;
class wxSpinCtrl;
class wxSizer;
class wxGridBagSizer;

class AudioDialog;
class MarkersDialog;
class InteropMetadataDialog;
class SMPTEMetadataDialog;
class Film;
class FilmViewer;
class Ratio;


class DCPPanel
{
public:
	DCPPanel (wxNotebook *, std::shared_ptr<Film>, std::weak_ptr<FilmViewer> viewer);

	DCPPanel (DCPPanel const&) = delete;
	DCPPanel& operator= (DCPPanel const&) = delete;

	void set_film (std::shared_ptr<Film>);
	void set_general_sensitivity (bool);

	void film_changed (Film::Property);
	void film_content_changed (int);

	wxPanel* panel () const {
		return _panel;
	}

private:
	void name_changed ();
	void use_isdcf_name_toggled ();
	void copy_isdcf_name_button_clicked ();
	void container_changed ();
	void dcp_content_type_changed ();
	void j2k_bandwidth_changed ();
	void frame_rate_choice_changed ();
	void frame_rate_spin_changed ();
	void best_frame_rate_clicked ();
	void content_timeline_clicked ();
	void audio_channels_changed ();
	void resolution_changed ();
	void three_d_changed ();
	void standard_changed ();
	void encrypted_toggled ();
	void audio_processor_changed ();
	void show_audio_clicked ();
	void reel_type_changed ();
	void reel_length_changed ();
	void markers_clicked ();
	void metadata_clicked ();
	void reencode_j2k_changed ();
	void enable_audio_language_toggled ();
	void edit_audio_language_clicked ();

	void setup_frame_rate_widget ();
	void setup_container ();
	void setup_dcp_name ();
	void add_to_grid ();
	void add_video_panel_to_grid ();
	void add_audio_panel_to_grid ();
	void add_audio_processors ();

	int minimum_allowed_audio_channels () const;

	wxPanel* make_general_panel ();
	wxPanel* make_video_panel ();
	wxPanel* make_audio_panel ();

	void config_changed (Config::Property p);

	void setup_sensitivity ();

	wxPanel* _panel;
	wxNotebook* _notebook;
	wxBoxSizer* _sizer;
	wxGridBagSizer* _grid;
	wxGridBagSizer* _video_grid;
	wxGridBagSizer* _audio_grid;

	wxStaticText* _name_label;
	wxTextCtrl* _name;
	wxStaticText* _dcp_name;
	wxCheckBox* _use_isdcf_name;
	wxCheckBox* _enable_audio_language = nullptr;
	wxStaticText* _audio_language = nullptr;
	Button* _edit_audio_language = nullptr;
	wxStaticText* _container_label;
	wxChoice* _container;
	wxStaticText* _container_size;
	wxButton* _copy_isdcf_name_button;
	wxStaticText* _j2k_bandwidth_label;
	wxStaticText* _mbits_label;
 	wxSpinCtrl* _j2k_bandwidth;
	wxStaticText* _dcp_content_type_label;
	wxChoice* _dcp_content_type;
	wxStaticText* _frame_rate_label;
	wxChoice* _frame_rate_choice;
	wxSpinCtrl* _frame_rate_spin;
	wxSizer* _frame_rate_sizer;
	wxStaticText* _channels_label;
	wxChoice* _audio_channels;
	wxStaticText* _processor_label;
	wxChoice* _audio_processor;
	wxButton* _show_audio;
	wxButton* _best_frame_rate;
	wxCheckBox* _three_d;
	wxCheckBox* _reencode_j2k;
	wxStaticText* _resolution_label;
	wxChoice* _resolution;
	wxStaticText* _standard_label;
	wxChoice* _standard;
	wxCheckBox* _encrypted;
	wxStaticText* _reels_label;
	wxChoice* _reel_type;
	wxStaticText* _reel_length_label;
	wxStaticText* _reel_length_gb_label;
	wxSpinCtrl* _reel_length;
	wxButton* _markers;
	wxButton* _metadata;
	wxSizer* _audio_panel_sizer;

	AudioDialog* _audio_dialog = nullptr;
	MarkersDialog* _markers_dialog = nullptr;
	InteropMetadataDialog* _interop_metadata_dialog = nullptr;
	SMPTEMetadataDialog* _smpte_metadata_dialog = nullptr;

	std::shared_ptr<Film> _film;
	std::weak_ptr<FilmViewer> _viewer;
	bool _generally_sensitive;
};
