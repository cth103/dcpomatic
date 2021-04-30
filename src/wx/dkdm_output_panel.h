/*
    Copyright (C) 2015-2020 Carl Hetherington <cth@carlh.net>

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

#include "lib/kdm_with_metadata.h"
#include "wx_util.h"
#include "name_format_editor.h"
#include <dcp/types.h>
#include <wx/wx.h>
#include <boost/filesystem.hpp>

class wxRadioButton;
class wxDirPickerCtrl;
class DirPickerCtrl;
class KDMTimingPanel;
class Job;
class Log;

class DKDMOutputPanel : public wxPanel
{
public:
	DKDMOutputPanel (wxWindow* parent);

	void setup_sensitivity ();

	boost::filesystem::path directory () const;

	std::pair<std::shared_ptr<Job>, int> make (
		std::list<KDMWithMetadataPtr > kdms,
		std::string name,
		std::function<bool (boost::filesystem::path)> confirm_overwrite
		);

private:
	NameFormatEditor* _filename_format;
	wxCheckBox* _write_to;
#ifdef DCPOMATIC_USE_OWN_PICKER
	DirPickerCtrl* _folder;
#else
	wxDirPickerCtrl* _folder;
#endif
	wxCheckBox* _email;
};
