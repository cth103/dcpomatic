/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>


class FilePickerCtrl : public wxPanel
{
public:
	FilePickerCtrl(
		wxWindow* parent,
		wxString prompt,
		wxString wildcard,
		bool open,
		bool warn_overwrite,
		std::string initial_path_key,
		boost::optional<std::string> initial_filename = boost::optional<std::string>(),
		boost::optional<boost::filesystem::path> override_path = boost::optional<boost::filesystem::path>()
		);

	boost::optional<boost::filesystem::path> path() const;
	void set_path(boost::optional<boost::filesystem::path> path);
	void set_wildcard(wxString);

private:
	void browse_clicked ();
	void set_filename(boost::optional<std::string> filename);

	wxButton* _file;
	boost::optional<boost::filesystem::path> _path;
	wxSizer* _sizer;
	wxString _prompt;
	wxString _wildcard;
	bool _open;
	bool _warn_overwrite;
	std::string _initial_path_key;
	boost::optional<std::string> _initial_filename;
	boost::optional<boost::filesystem::path> _override_path;
};
