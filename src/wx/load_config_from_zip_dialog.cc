/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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


#include "load_config_from_zip_dialog.h"
#include "wx_util.h"
#include "lib/config.h"
#include "lib/unzipper.h"


LoadConfigFromZIPDialog::LoadConfigFromZIPDialog(wxWindow* parent, boost::filesystem::path zip_file)
	: TableDialog(parent, _("Load configuration from ZIP file"), 1, 0, true)
{
	_use_current = add(new wxRadioButton(this, wxID_ANY, _("Copy the cinemas in the ZIP file over the current list at")));
	auto current_path = add(new wxStaticText(this, wxID_ANY, std_to_wx(Config::instance()->cinemas_file().string())));
	auto current_path_font = current_path->GetFont();
	current_path_font.SetFamily(wxFONTFAMILY_TELETYPE);
	current_path->SetFont(current_path_font);

	_use_zip = add(new wxRadioButton(this, wxID_ANY, _("Copy the cinemas in the ZIP file to the original location at")));
	auto zip_path = add(new wxStaticText(this, wxID_ANY, std_to_wx(Config::cinemas_file_from_zip(zip_file).string())));
	auto zip_path_font = zip_path->GetFont();
	zip_path_font.SetFamily(wxFONTFAMILY_TELETYPE);
	zip_path->SetFont(zip_path_font);

	_ignore = add(new wxRadioButton(this, wxID_ANY, _("Do not use the cinemas in the ZIP file")));

	layout();
}


Config::CinemasAction
LoadConfigFromZIPDialog::action() const
{
	if (_use_current->GetValue()) {
		return Config::CinemasAction::WRITE_TO_CURRENT_PATH;
	} else if (_use_zip->GetValue()) {
		return Config::CinemasAction::WRITE_TO_PATH_IN_ZIPPED_CONFIG;
	}

	return Config::CinemasAction::IGNORE;
}
