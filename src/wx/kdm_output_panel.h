/*
    Copyright (C) 2015-2017 Carl Hetherington <cth@carlh.net>

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

#include "lib/screen_kdm.h"
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

class KDMOutputPanel : public wxPanel
{
public:
	KDMOutputPanel (wxWindow* parent, bool interop);

	void setup_sensitivity ();

	boost::filesystem::path directory () const;
	dcp::Formulation formulation () const;
	bool forensic_mark_video () const {
		return _forensic_mark_video;
	}
	bool forensic_mark_audio () const {
		return _forensic_mark_audio;
	}

	std::pair<boost::shared_ptr<Job>, int> make (
		std::list<ScreenKDM> screen_kdms,
		std::string name,
		KDMTimingPanel* timing,
		boost::function<bool (boost::filesystem::path)> confirm_overwrite,
		boost::shared_ptr<Log> log
		);

private:
	void kdm_write_type_changed ();
	void advanced_clicked ();

	wxChoice* _type;
	NameFormatEditor* _container_name_format;
	NameFormatEditor* _filename_format;
	wxCheckBox* _write_to;
#ifdef DCPOMATIC_USE_OWN_PICKER
	DirPickerCtrl* _folder;
#else
	wxDirPickerCtrl* _folder;
#endif
	wxRadioButton* _write_flat;
	wxRadioButton* _write_folder;
	wxRadioButton* _write_zip;
	wxCheckBox* _email;
	bool _forensic_mark_video;
	bool _forensic_mark_audio;
};
