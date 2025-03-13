/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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


#include "preferences_page.h"


class wxChoice;
class wxGridBagSizer;

class CheckBox;
class FilePickerCtrl;


namespace dcpomatic {
namespace preferences {


class GeneralPage : public Page
{
public:
	GeneralPage(wxSize panel_size, int border);

	wxString GetName() const override;

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon() const override;
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

	CheckBox* _set_language = nullptr;
	wxChoice* _language = nullptr;
	FilePickerCtrl* _config_file = nullptr;
	FilePickerCtrl* _cinemas_file = nullptr;
	CheckBox* _check_for_updates = nullptr;
	CheckBox* _check_for_test_updates = nullptr;
};


}
}

