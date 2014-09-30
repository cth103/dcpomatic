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

/** @class VideoPanel
 *  @brief The video tab of the film editor.
 */
class VideoPanel : public ContentSubPanel
{
public:
	VideoPanel (ContentPanel *);

	void film_changed (Film::Property);
	void film_content_changed (int);
	void content_selection_changed ();

private:
	void edit_filters_clicked ();
	void edit_colour_conversion_clicked ();
	void fade_in_changed ();
	void fade_out_changed ();

	void setup_description ();

	ContentChoice<VideoContent, VideoFrameType>*    _frame_type;
	ContentSpinCtrl<VideoContent>*                  _left_crop;
	ContentSpinCtrl<VideoContent>*                  _right_crop;
	ContentSpinCtrl<VideoContent>*                  _top_crop;
	ContentSpinCtrl<VideoContent>*                  _bottom_crop;
	Timecode<ContentTime>*                          _fade_in;
	Timecode<ContentTime>*                          _fade_out;
	ContentChoice<VideoContent, VideoContentScale>* _scale;
	wxStaticText* _description;
	wxStaticText* _filters;
	wxButton* _filters_button;
	wxStaticText* _colour_conversion;
	wxButton* _colour_conversion_button;
};
