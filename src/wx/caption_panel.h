/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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

class wxCheckBox;
class wxSpinCtrl;
class CaptionView;
class FontsDialog;

class CaptionPanel : public ContentSubPanel
{
public:
	explicit CaptionPanel (ContentPanel *);

	void film_changed (Film::Property);
	void film_content_changed (int);
	void content_selection_changed ();

private:
	void use_toggled ();
	void type_changed ();
	void burn_toggled ();
	void x_offset_changed ();
	void y_offset_changed ();
	void x_scale_changed ();
	void y_scale_changed ();
	void line_spacing_changed ();
	void language_changed ();
	void stream_changed ();
	void caption_view_clicked ();
	void fonts_dialog_clicked ();
	void reference_clicked ();
	void appearance_dialog_clicked ();

	void setup_sensitivity ();

	wxCheckBox* _reference;
	wxStaticText* _reference_note;
	wxCheckBox* _use;
	wxChoice* _type;
	wxCheckBox* _burn;
	wxSpinCtrl* _x_offset;
	wxSpinCtrl* _y_offset;
	wxSpinCtrl* _x_scale;
	wxSpinCtrl* _y_scale;
	wxSpinCtrl* _line_spacing;
	wxTextCtrl* _language;
	wxChoice* _stream;
	wxButton* _caption_view_button;
	CaptionView* _caption_view;
	wxButton* _fonts_dialog_button;
	FontsDialog* _fonts_dialog;
	wxButton* _appearance_dialog_button;
	CaptionType _original_type;
};
