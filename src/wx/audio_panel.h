/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#include "lib/audio_mapping.h"
#include "content_sub_panel.h"
#include "content_widget.h"

class wxSpinCtrlDouble;
class wxButton;
class wxChoice;
class wxStaticText;
class AudioMappingView;
class AudioDialog;

class AudioPanel : public ContentSubPanel
{
public:
	AudioPanel (ContentPanel *);

	void film_changed (Film::Property);
	void film_content_changed (int);
	void content_selection_changed ();
	
private:
	void gain_calculate_button_clicked ();
	void show_clicked ();
	void stream_changed ();
	void mapping_changed (AudioMapping);
	void processor_changed ();
	void setup_processors ();
	void setup_description ();

	ContentSpinCtrlDouble<AudioContent>* _gain;
	wxButton* _gain_calculate_button;
	wxButton* _show;
	ContentSpinCtrl<AudioContent>* _delay;
	wxChoice* _stream;
	wxChoice* _processor;
	AudioMappingView* _mapping;
	wxStaticText* _description;
	AudioDialog* _audio_dialog;

	boost::signals2::scoped_connection _mapping_connection;
};
