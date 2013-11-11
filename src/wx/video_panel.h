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

#include "lib/film.h"
#include "film_editor_panel.h"
#include "content_widget.h"

class wxChoice;
class wxStaticText;
class wxSpinCtrl;
class wxButton;

class VideoPanel : public FilmEditorPanel
{
public:
	VideoPanel (FilmEditor *);

	void film_changed (Film::Property);
	void film_content_changed (int);
	void content_selection_changed ();

private:
	void edit_filters_clicked ();
	void ratio_changed ();
	void frame_type_changed ();
	void edit_colour_conversion_clicked ();

	void setup_description ();

	wxChoice* _frame_type;
	ContentWidget<VideoContent, wxSpinCtrl>* _left_crop;
	ContentWidget<VideoContent, wxSpinCtrl>* _right_crop;
	ContentWidget<VideoContent, wxSpinCtrl>* _top_crop;
	ContentWidget<VideoContent, wxSpinCtrl>* _bottom_crop;
	wxChoice* _ratio;
	wxStaticText* _ratio_description;
	wxStaticText* _description;
	wxStaticText* _filters;
	wxButton* _filters_button;
	wxStaticText* _colour_conversion;
	wxButton* _colour_conversion_button;
};
