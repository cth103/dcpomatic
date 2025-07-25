/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#include "check_box.h"
#include "film_name_location_dialog.h"
#include "wx_util.h"
/* This must come after wx_util.h as it defines DCPOMATIC_USE_OWN_PICKER */
#ifdef DCPOMATIC_USE_OWN_PICKER
#include "dir_picker_ctrl.h"
#endif
#include "lib/config.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/stdpaths.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/filesystem.hpp>


using std::string;
using boost::bind;
using boost::optional;


boost::optional<boost::filesystem::path> FilmNameLocationDialog::_directory;


FilmNameLocationDialog::FilmNameLocationDialog (wxWindow* parent, wxString title, bool offer_templates)
	: TableDialog (parent, title, 2, 1, true)
{
	add (_("Film name"), true);
	_name = add (new wxTextCtrl (this, wxID_ANY));

	add (_("Create in folder"), true);

#ifdef DCPOMATIC_USE_OWN_PICKER
	_folder = new DirPickerCtrl (this);
	_folder->Changed.connect (bind(&FilmNameLocationDialog::folder_changed, this));
#else
	_folder = new wxDirPickerCtrl(this, wxID_ANY, wxEmptyString, char_to_wx(wxDirSelectorPromptStr), wxDefaultPosition, wxSize (300, -1));
	_folder->Bind (wxEVT_DIRPICKER_CHANGED, bind(&FilmNameLocationDialog::folder_changed, this));
#endif

	auto dir = _directory.get_value_or(Config::instance()->default_directory_or(wx_to_std(wxStandardPaths::Get().GetDocumentsDir())));
	_folder->SetPath (std_to_wx(dir.string()));
	add (_folder);

	if (offer_templates) {
		_use_template = new CheckBox (this, _("From template"));
		add (_use_template);
		_template_name = new wxChoice (this, wxID_ANY);
		add (_template_name);
	}

	_name->SetFocus ();

	if (offer_templates) {
		_template_name->Enable (false);

		for (auto i: Config::instance()->templates ()) {
			_template_name->Append (std_to_wx(i));
		}

		_use_template->bind(&FilmNameLocationDialog::use_template_clicked, this);
	}

	layout ();

	_name->Bind (wxEVT_TEXT, bind(&FilmNameLocationDialog::setup_sensitivity, this));
	setup_sensitivity ();
}


void
FilmNameLocationDialog::setup_sensitivity ()
{
	auto ok = dynamic_cast<wxButton *>(FindWindowById(wxID_OK, this));
	if (ok) {
		ok->Enable (!_name->GetValue().IsEmpty());
	}
}


void
FilmNameLocationDialog::use_template_clicked ()
{
	_template_name->Enable (_use_template->GetValue());
}


void
FilmNameLocationDialog::folder_changed ()
{
	_directory = wx_to_std (_folder->GetPath());
}


boost::filesystem::path
FilmNameLocationDialog::path () const
{
	boost::filesystem::path p;
	p /= wx_to_std (_folder->GetPath());
	p /= wx_to_std (_name->GetValue());
	return p;
}


boost::optional<string>
FilmNameLocationDialog::template_name () const
{
	if (!_use_template->GetValue() || _template_name->GetSelection() == -1) {
		return {};
	}

	return wx_to_std (_template_name->GetString(_template_name->GetSelection()));
}


/** Check the path that is in our controls and offer confirmations or errors as required.
 *  @return true if the path should be used.
 */
bool
FilmNameLocationDialog::check_path ()
{
	if (boost::filesystem::is_directory(path()) && !boost::filesystem::is_empty(path())) {
		if (!confirm_dialog (
			    this,
			    wxString::Format(
				    _("The directory %s already exists and is not empty.  Are you sure you want to use it?"),
				    std_to_wx(path().string())
				    )
			    )) {
			return false;
		}
	} else if (boost::filesystem::is_regular_file(path())) {
		error_dialog (
			this,
			wxString::Format(_("%s already exists as a file, so you cannot use it for a film."), std_to_wx(path().string()))
			);
		return false;
	}

	return true;
}

