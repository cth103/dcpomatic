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

#include "content_sub_panel.h"

class wxCheckBox;
class wxSpinCtrl;
class SubtitleView;
class FontsDialog;

class SubtitlePanel : public ContentSubPanel
{
public:
	SubtitlePanel (ContentPanel *);

	void film_changed (Film::Property);
	void film_content_changed (int);
	void content_selection_changed ();
	
private:
	void use_toggled ();
	void x_offset_changed ();
	void y_offset_changed ();
	void x_scale_changed ();
	void y_scale_changed ();
	void language_changed ();
	void stream_changed ();
	void subtitle_view_clicked ();
	void fonts_dialog_clicked ();

	void setup_sensitivity ();
	
	wxCheckBox* _use;
	wxSpinCtrl* _x_offset;
	wxSpinCtrl* _y_offset;
	wxSpinCtrl* _x_scale;
	wxSpinCtrl* _y_scale;
	wxTextCtrl* _language;
	wxChoice* _stream;
	wxButton* _subtitle_view_button;
	SubtitleView* _subtitle_view;
	wxButton* _fonts_dialog_button;
	FontsDialog* _fonts_dialog;
};
