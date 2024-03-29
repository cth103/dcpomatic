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


#include <dcp/language_tag.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/signals2.hpp>


class wxButton;
class wxSizer;
class wxStaticText;
class wxWindow;


/** A widget which displays and allows the user to edit a RegionSubtag i.e. a representation of a region of the world,
 *  perhaps a "territory" where a DCP will be released.
 */
class RegionSubtagWidget
{
public:
	RegionSubtagWidget(wxWindow* parent, wxString tooltip, boost::optional<dcp::LanguageTag::RegionSubtag> tag, boost::optional<wxString> size_to_fit = boost::none);

	RegionSubtagWidget(RegionSubtagWidget const&) = delete;
	RegionSubtagWidget& operator=(RegionSubtagWidget const&) = delete;

	wxSizer* sizer() const {
		return _sizer;
	}

	boost::optional<dcp::LanguageTag::RegionSubtag> get() const {
		return _tag;
	}
	void set(boost::optional<dcp::LanguageTag::RegionSubtag> tag);
	void enable(bool e);

	boost::signals2::signal<void (boost::optional<dcp::LanguageTag::RegionSubtag>)> Changed;

private:
	void edit ();

	wxStaticText* _region;
	wxButton* _edit;
	wxWindow* _parent;
	boost::optional<dcp::LanguageTag::RegionSubtag> _tag;
	wxSizer* _sizer;
};

