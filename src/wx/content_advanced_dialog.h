/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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


#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/optional.hpp>
#include <memory>
#include <vector>


class Content;
class Filter;
class LanguageTagWidget;


class ContentAdvancedDialog : public wxDialog
{
public:
	ContentAdvancedDialog (wxWindow* parent, std::shared_ptr<Content> content);

	bool ignore_video() const;

	std::vector<Filter const*> filters() {
		return _filters_list;
	}

	boost::optional<double> video_frame_rate() const;

private:
	void edit_filters ();
	void filters_changed (std::vector<Filter const *> filters);
	void setup_filters ();
	void set_video_frame_rate ();
	void video_frame_rate_changed ();
	void setup_sensitivity ();
	void burnt_subtitle_changed ();
	void burnt_subtitle_language_changed ();

	std::shared_ptr<Content> _content;
	bool _filters_allowed = false;
	std::vector<Filter const*> _filters_list;

	wxStaticText* _filters;
	wxButton* _filters_button;
	wxTextCtrl* _video_frame_rate;
	wxButton* _set_video_frame_rate;
	wxCheckBox* _burnt_subtitle;
	LanguageTagWidget* _burnt_subtitle_language;
	wxCheckBox* _ignore_video;
};

