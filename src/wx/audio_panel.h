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

#include "film_editor_panel.h"

class wxSpinCtrl;
class wxButton;
class wxChoice;
class wxStaticText;
class AudioMappingView;
class AudioDialog;

class AudioPanel : public FilmEditorPanel
{
public:
	AudioPanel (FilmEditor *);

	void film_changed (Film::Property);
	void film_content_changed (
		boost::shared_ptr<Content>,
		boost::shared_ptr<AudioContent>,
		boost::shared_ptr<SubtitleContent>,
		boost::shared_ptr<FFmpegContent>,
		int);
	void content_selection_changed ();

	void setup_sensitivity ();
	
private:
	void gain_changed (wxCommandEvent &);
	void gain_calculate_button_clicked (wxCommandEvent &);
	void show_clicked (wxCommandEvent &);
	void delay_changed (wxCommandEvent &);
	void stream_changed (wxCommandEvent &);
	void mapping_changed (AudioMapping);

	wxSpinCtrl* _gain;
	wxButton* _gain_calculate_button;
	wxButton* _show;
	wxSpinCtrl* _delay;
	wxChoice* _stream;
	wxStaticText* _description;
	AudioMappingView* _mapping;
	AudioDialog* _audio_dialog;
};
