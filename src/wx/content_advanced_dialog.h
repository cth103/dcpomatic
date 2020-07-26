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


#include "lib/warnings.h"
DCPOMATIC_DISABLE_WARNINGS
#include <wx/wx.h>
DCPOMATIC_ENABLE_WARNINGS
#include <boost/shared_ptr.hpp>
#include <vector>


class Content;
class Filter;


class ContentAdvancedDialog : public wxDialog
{
public:
	ContentAdvancedDialog (wxWindow* parent, boost::shared_ptr<Content> content);

private:
	void ignore_video_changed (wxCommandEvent& ev);
	void edit_filters ();
	void filters_changed (std::vector<Filter const *> filters);
	void setup_filters ();

	boost::shared_ptr<Content> _content;

	wxStaticText* _filters;
	wxButton* _filters_button;
};

