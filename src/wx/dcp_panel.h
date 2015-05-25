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
	void burn_subtitles_toggled ();
	void encrypted_toggled ();
	void edit_key_clicked ();

	void setup_frame_rate_widget ();
	void setup_container ();
	void setup_dcp_name ();

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
	wxButton* _best_frame_rate;
	wxCheckBox* _three_d;
	wxChoice* _resolution;
	wxChoice* _standard;
	wxCheckBox* _signed;
	wxCheckBox* _burn_subtitles;
	wxCheckBox* _encrypted;
	wxStaticText* _key;
	wxButton* _edit_key;

	boost::shared_ptr<Film> _film;
	bool _generally_sensitive;
};
