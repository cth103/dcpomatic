/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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


#include "subtag_list_ctrl.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/srchctrl.h>
#include <wx/wx.h>
#include <boost/signals2.hpp>
LIBDCP_ENABLE_WARNINGS


/** A panel which offers a list of subtags in two columns: subtag and name, and has a search box to
 *  limit the view to a subset.  The list contained within is a SubtagListCtrl.
 */
class LanguageSubtagPanel : public wxPanel
{
public:
	LanguageSubtagPanel(wxWindow* parent);

	void set(dcp::LanguageTag::SubtagType type, std::string search, boost::optional<dcp::LanguageTag::SubtagData> subtag = boost::optional<dcp::LanguageTag::SubtagData>());
	boost::optional<dcp::LanguageTag::RegionSubtag> get() const;

	boost::signals2::signal<void (boost::optional<dcp::LanguageTag::SubtagData>)> SelectionChanged;
	boost::signals2::signal<void (std::string)> SearchChanged;

private:
	void search_changed();
	void selection_changed();

	wxSearchCtrl* _search;
	SubtagListCtrl* _list;
};
