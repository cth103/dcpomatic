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

/** @file  src/lib/video_panel.h
 *  @brief VideoPanel class.
 */

#include "content_sub_panel.h"
#include "content_widget.h"
#include "timecode.h"
#include "lib/film.h"

class wxChoice;
class wxStaticText;
class wxSpinCtrl;
class wxButton;
class wxToggleButton;

/** @class VideoPanel
 *  @brief The video tab of the film editor.
 */
class VideoPanel : public ContentSubPanel
{
public:
	explicit VideoPanel (ContentPanel *);

	void film_changed (Film::Property);
	void film_content_changed (int);
	void content_selection_changed ();

private:
	void reference_clicked ();
	void colour_conversion_changed ();
	void edit_colour_conversion_clicked ();
	void range_changed ();
	void fade_in_changed ();
	void fade_out_changed ();
	void add_to_grid ();
	void scale_fit_clicked ();
	void scale_custom_clicked ();
	bool scale_custom_edit_clicked ();
	void left_right_link_clicked ();
	void top_bottom_link_clicked ();
	void left_crop_changed ();
	void right_crop_changed ();
	void top_crop_changed ();
	void bottom_crop_changed ();

	void setup_description ();
	void setup_sensitivity ();

	wxCheckBox* _reference;
	wxStaticText* _reference_note;
	wxStaticText* _type_label;
	ContentChoice<VideoContent, VideoFrameType>* _frame_type;
	wxStaticText* _crop_label;
	wxStaticText* _left_crop_label;
	ContentSpinCtrl<VideoContent>* _left_crop;
	wxToggleButton* _left_right_link;
	wxStaticText* _right_crop_label;
	ContentSpinCtrl<VideoContent>* _right_crop;
	wxStaticText* _top_crop_label;
	ContentSpinCtrl<VideoContent>* _top_crop;
	wxToggleButton* _top_bottom_link;
	wxStaticText* _bottom_crop_label;
	ContentSpinCtrl<VideoContent>* _bottom_crop;
	wxStaticText* _fade_in_label;
	Timecode<dcpomatic::ContentTime>* _fade_in;
	wxStaticText* _fade_out_label;
	Timecode<dcpomatic::ContentTime>* _fade_out;
	wxStaticText* _scale_label;
	wxRadioButton* _scale_fit;
	wxRadioButton* _scale_custom;
	wxButton* _scale_custom_edit;
	wxStaticText* _description;
	wxStaticText* _colour_conversion_label;
	wxChoice* _colour_conversion;
	wxButton* _edit_colour_conversion_button;
	wxStaticText* _range_label;
	wxChoice* _range;
};
