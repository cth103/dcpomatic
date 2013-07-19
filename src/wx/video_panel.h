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

class wxChoice;
class wxStaticText;
class wxSpinCtrl;
class wxButton;

class VideoPanel : public FilmEditorPanel
{
public:
	VideoPanel (FilmEditor *);

	void film_content_changed (boost::shared_ptr<Content>, int);

	void setup_scaling_description ();
	
private:
	void left_crop_changed (wxCommandEvent &);
	void right_crop_changed (wxCommandEvent &);
	void top_crop_changed (wxCommandEvent &);
	void bottom_crop_changed (wxCommandEvent &);
	void edit_filters_clicked (wxCommandEvent &);
	void ratio_changed (wxCommandEvent &);

	wxChoice* _ratio;
	wxStaticText* _ratio_description;
	wxStaticText* _scaling_description;
	wxSpinCtrl* _left_crop;
	wxSpinCtrl* _right_crop;
	wxSpinCtrl* _top_crop;
	wxSpinCtrl* _bottom_crop;
	wxStaticText* _filters;
	wxButton* _filters_button;
};
