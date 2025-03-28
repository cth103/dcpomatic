/*
    Copyright (C) 2012-2022 Carl Hetherington <cth@carlh.net>

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


#include "content_sub_panel.h"
#include "content_widget.h"
#include "timecode.h"
#include "wx_ptr.h"
#include "lib/audio_mapping.h"


class AudioDialog;
class AudioMappingView;
class CheckBox;
class LanguageTagWidget;
class wxButton;
class wxChoice;
class wxSpinCtrlDouble;
class wxStaticText;


class AudioPanel : public ContentSubPanel
{
public:
	explicit AudioPanel (ContentPanel *);

	void create () override;
	void film_changed(FilmProperty) override;
	void film_content_changed (int) override;
	void content_selection_changed () override;
	void set_film (std::shared_ptr<Film>);

private:
	void show_clicked ();
	void gain_calculate_button_clicked ();
	void mapping_changed (AudioMapping);
	void setup_description ();
	void setup_peak ();
	void active_jobs_changed (boost::optional<std::string>, boost::optional<std::string>);
	void setup_sensitivity ();
	void add_to_grid () override;
	boost::optional<float> peak () const;
	void fade_in_changed ();
	void fade_out_changed ();
	void use_same_fades_as_video_changed ();

	wxButton* _show;
	wxStaticText* _gain_label;
	wxStaticText* _gain_db_label;
	ContentSpinCtrlDouble<AudioContent>* _gain;
	wxButton* _gain_calculate_button;
	wxStaticText* _peak;
	wxStaticText* _delay_label;
	wxStaticText* _delay_ms_label;
	ContentSpinCtrl<AudioContent>* _delay;
	wxStaticText* _fade_in_label;
	Timecode<dcpomatic::ContentTime>* _fade_in;
	wxStaticText* _fade_out_label;
	Timecode<dcpomatic::ContentTime>* _fade_out;
	CheckBox* _use_same_fades_as_video;
	AudioMappingView* _mapping;
	wxStaticText* _description;
	wx_ptr<AudioDialog> _audio_dialog;

	boost::signals2::scoped_connection _mapping_connection;
	boost::signals2::scoped_connection _active_jobs_connection;

	static std::map<boost::filesystem::path, float> _peak_cache;
};
