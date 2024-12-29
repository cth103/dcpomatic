/*
    Copyright (C) 2015-2022 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_KDM_OUTPUT_PANEL_H
#define DCPOMATIC_KDM_OUTPUT_PANEL_H


#include "wx_util.h"
#include "lib/kdm_with_metadata.h"
#include <dcp/types.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/filesystem.hpp>


class CheckBox;
class Choice;
class DirPickerCtrl;
class Job;
class KDMChoice;
class NameFormatEditor;
class wxDirPickerCtrl;
class wxRadioButton;


class KDMOutputPanel : public wxPanel
{
public:
	KDMOutputPanel (wxWindow* parent);

	void setup_sensitivity ();

	boost::filesystem::path directory () const;
	dcp::Formulation formulation () const;
	bool forensic_mark_video () const {
		return _forensic_mark_video;
	}
	bool forensic_mark_audio () const {
		return _forensic_mark_audio;
	}
	boost::optional<int> forensic_mark_audio_up_to () const {
		return _forensic_mark_audio_up_to;
	}

	std::pair<std::shared_ptr<Job>, int> make (
		std::list<KDMWithMetadataPtr> screen_kdms,
		std::string name,
		std::function<bool (boost::filesystem::path)> confirm_overwrite
		);

	bool method_selected() const;

	void set_annotation_text(std::string text);
	std::string annotation_text() const;

	boost::signals2::signal<void ()> MethodChanged;

protected:
	void create_destination_widgets(wxWindow* parent);
	void create_details_widgets(wxWindow* parent);
	void create_name_format_widgets(wxWindow* parent);

	KDMChoice* _type;
	wxTextCtrl* _annotation_text;
	NameFormatEditor* _container_name_format;
	NameFormatEditor* _filename_format;
	CheckBox* _write_to;
#ifdef DCPOMATIC_USE_OWN_PICKER
	DirPickerCtrl* _folder;
#else
	wxDirPickerCtrl* _folder;
#endif
	Choice* _write_collect;
	wxButton* _advanced;
	CheckBox* _email;
	wxButton* _add_email_addresses;
	bool _forensic_mark_video = true;
	bool _forensic_mark_audio = true;
	boost::optional<int> _forensic_mark_audio_up_to = 12;
	std::vector<std::string> _extra_addresses;

private:
	void kdm_write_type_changed ();
	void advanced_clicked ();
	void write_to_changed ();
	void email_changed ();
	void add_email_addresses_clicked ();
};


#endif
