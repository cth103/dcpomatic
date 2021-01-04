/*
    Copyright (C) 2012-2020 Carl Hetherington <cth@carlh.net>

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
class TextView;
class FontsDialog;
class SubtitleAnalysis;

class TextPanel : public ContentSubPanel
{
public:
	TextPanel (ContentPanel *, TextType t);

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
	void dcp_track_changed ();
	void stream_changed ();
	void text_view_clicked ();
	void fonts_dialog_clicked ();
	void reference_clicked ();
	void appearance_dialog_clicked ();
	void outline_subtitles_changed ();
	TextType current_type () const;
	void update_dcp_tracks ();
	void update_dcp_track_selection ();
	void add_to_grid ();
	void try_to_load_analysis ();
	void analysis_finished ();

	void setup_sensitivity ();
	void setup_visibility ();

	void update_outline_subtitles_in_viewer ();
	void clear_outline_subtitles ();

	wxCheckBox* _reference;
	wxStaticText* _reference_note;
	wxCheckBox* _outline_subtitles;
	wxCheckBox* _use;
	wxChoice* _type;
	wxCheckBox* _burn;
	wxStaticText* _offset_label;
	wxStaticText* _x_offset_label;
	wxStaticText* _x_offset_pc_label;
	wxStaticText* _y_offset_label;
	wxStaticText* _y_offset_pc_label;
	wxSpinCtrl* _x_offset;
	wxSpinCtrl* _y_offset;
	wxStaticText* _scale_label;
	wxStaticText* _x_scale_label;
	wxStaticText* _x_scale_pc_label;
	wxStaticText* _y_scale_label;
	wxStaticText* _y_scale_pc_label;
	wxSpinCtrl* _x_scale;
	wxSpinCtrl* _y_scale;
	wxStaticText* _line_spacing_label;
	wxStaticText* _line_spacing_pc_label;
	wxSpinCtrl* _line_spacing;
	wxStaticText* _dcp_track_label;
	wxChoice* _dcp_track;
	wxStaticText* _stream_label;
	wxChoice* _stream;
	wxButton* _text_view_button;
	TextView* _text_view;
	wxButton* _fonts_dialog_button;
	FontsDialog* _fonts_dialog;
	wxButton* _appearance_dialog_button;
	TextType _original_type;

	int _outline_subtitles_row;
	int _ccap_track_row;

	std::weak_ptr<Content> _analysis_content;
	boost::signals2::scoped_connection _analysis_finished_connection;
	std::shared_ptr<SubtitleAnalysis> _analysis;
	bool _loading_analysis;
};
