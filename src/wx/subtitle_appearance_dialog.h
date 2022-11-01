/*
    Copyright (C) 2015-2021 Carl Hetherington <cth@carlh.net>

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


#include "timecode.h"
#include "lib/change_signaller.h"
#include "lib/rgba.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/signals2.hpp>


class CheckBox;
class Content;
class FFmpegSubtitleStream;
class Film;
class Job;
class RGBAColourPicker;
class wxColourPickerCtrl;
class wxGridBagSizer;
class wxRadioButton;
class wxWidget;


class SubtitleAppearanceDialog : public wxDialog
{
public:
	SubtitleAppearanceDialog (wxWindow* parent, std::shared_ptr<const Film> film, std::shared_ptr<Content> content, std::shared_ptr<TextContent> caption);

	void apply ();

private:
	void setup_sensitivity ();
	void restore ();
	CheckBox* set_to (wxWindow* w, int& r);
	void content_change (ChangeType type);
	void active_jobs_changed (boost::optional<std::string> last);
	void add_colours ();

	std::weak_ptr<const Film> _film;
	CheckBox* _force_colour;
	wxColourPickerCtrl* _colour;
	CheckBox* _force_effect;
	wxChoice* _effect;
	CheckBox* _force_effect_colour;
	wxColourPickerCtrl* _effect_colour;
	CheckBox* _force_fade_in;
	Timecode<dcpomatic::ContentTime>* _fade_in;
	CheckBox* _force_fade_out;
	Timecode<dcpomatic::ContentTime>* _fade_out;
	wxSpinCtrl* _outline_width;
	wxGridBagSizer* _table;
	std::map<RGBA, RGBAColourPicker*> _pickers;

	wxBoxSizer* _overall_sizer;
	wxScrolled<wxPanel>* _colours_panel;
	wxStaticText* _finding = nullptr;
	wxFlexGridSizer* _colour_table;

	std::shared_ptr<Content> _content;
	std::shared_ptr<TextContent> _text;
	std::shared_ptr<FFmpegSubtitleStream> _stream;

	boost::signals2::scoped_connection _content_connection;
	boost::signals2::scoped_connection _job_manager_connection;

	std::weak_ptr<Job> _job;

	static int const NONE;
	static int const OUTLINE;
	static int const SHADOW;
};
