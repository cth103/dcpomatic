/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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

#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>

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

class AudioDialog;

class Film;

class DCPPanel : public boost::noncopyable
{
public:
	DCPPanel (wxNotebook *, boost::shared_ptr<Film>);

	void set_film (boost::shared_ptr<Film>);
	void set_general_sensitivity (bool);

	void film_changed (int);
	void film_content_changed (int);

	wxPanel* panel () const {
		return _panel;
	}

private:
	void name_changed ();
	void use_isdcf_name_toggled ();
	void edit_isdcf_button_clicked ();
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
	void signed_toggled ();
	void encrypted_toggled ();
	void edit_key_clicked ();
	void audio_processor_changed ();
	void show_audio_clicked ();
	void reel_type_changed ();
	void reel_length_changed ();
	void upload_after_make_dcp_changed ();

	void setup_frame_rate_widget ();
	void setup_container ();
	void setup_dcp_name ();
	void setup_audio_channels_choice ();

	int minimum_allowed_audio_channels () const;

	wxPanel* make_general_panel ();
	wxPanel* make_video_panel ();
	wxPanel* make_audio_panel ();

	void config_changed ();

	wxPanel* _panel;
	wxNotebook* _notebook;
	wxBoxSizer* _sizer;

	wxTextCtrl* _name;
	wxStaticText* _dcp_name;
	wxCheckBox* _use_isdcf_name;
	wxChoice* _container;
	wxStaticText* _container_size;
	wxButton* _edit_isdcf_button;
	wxButton* _copy_isdcf_name_button;
 	wxSpinCtrl* _j2k_bandwidth;
	wxChoice* _dcp_content_type;
	wxChoice* _frame_rate_choice;
	wxSpinCtrl* _frame_rate_spin;
	wxSizer* _frame_rate_sizer;
	wxChoice* _audio_channels;
	wxChoice* _audio_processor;
	wxButton* _show_audio;
	wxButton* _best_frame_rate;
	wxCheckBox* _three_d;
	wxChoice* _resolution;
	wxChoice* _standard;
	wxCheckBox* _signed;
	wxCheckBox* _encrypted;
	wxStaticText* _key;
	wxButton* _edit_key;
	wxChoice* _reel_type;
	wxSpinCtrl* _reel_length;
	wxCheckBox* _upload_after_make_dcp;

	AudioDialog* _audio_dialog;

	boost::shared_ptr<Film> _film;
	bool _generally_sensitive;
};
