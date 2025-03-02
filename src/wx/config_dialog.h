/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_CONFIG_DIALOG_H
#define DCPOMATIC_CONFIG_DIALOG_H


#include "editable_list.h"
#include "make_chain_dialog.h"
#include "wx_util.h"
#include "lib/config.h"
#include "lib/cross.h"
#include "lib/dcp_content_type.h"
#include "lib/exceptions.h"
#include "lib/filter.h"
#include "lib/log.h"
#include "lib/ratio.h"
#include <dcp/certificate_chain.h>
#include <dcp/exceptions.h>
#include <dcp/locale_convert.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/filepicker.h>
#include <wx/preferences.h>
#include <wx/spinctrl.h>
#include <wx/stdpaths.h>
#include <RtAudio.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/filesystem.hpp>


class AudioMappingView;
class CheckBox;


namespace dcpomatic {
namespace preferences {


class Page : public wxPreferencesPage
{
public:
	Page(wxSize panel_size, int border);
	virtual ~Page() {}

	wxWindow* CreateWindow(wxWindow* parent) override;

protected:
	wxWindow* create_window(wxWindow* parent);

	int _border;
	wxPanel* _panel;

private:
	virtual void config_changed() = 0;
	virtual void setup() = 0;

	void config_changed_wrapper();
	void window_destroyed();

	wxSize _panel_size;
	boost::signals2::scoped_connection _config_connection;
	bool _window_exists;
};


class GeneralPage : public Page
{
public:
	GeneralPage(wxSize panel_size, int border);

	wxString GetName() const override;

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon() const override
	{
		return wxBitmap(icon_path("general"), wxBITMAP_TYPE_PNG);
	}
#endif

protected:
	void add_language_controls(wxGridBagSizer* table, int& r);
	void add_config_file_controls(wxGridBagSizer* table, int& r);
	void add_update_controls(wxGridBagSizer* table, int& r);
	void config_changed() override;

private:
	void setup_sensitivity();
	void set_language_changed();
	void language_changed();
	void config_file_changed();
	void cinemas_file_changed();
	void export_cinemas_file();
	void check_for_updates_changed();
	void check_for_test_updates_changed();

	CheckBox* _set_language;
	wxChoice* _language;
	FilePickerCtrl* _config_file;
	FilePickerCtrl* _cinemas_file;
	CheckBox* _check_for_updates;
	CheckBox* _check_for_test_updates;
};


}
}

#endif
