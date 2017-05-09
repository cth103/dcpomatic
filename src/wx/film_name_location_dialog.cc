/*
    Copyright (C) 2012-2017 Carl Hetherington <cth@carlh.net>

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

#include "wx_util.h"
#include "film_name_location_dialog.h"
#ifdef DCPOMATIC_USE_OWN_PICKER
#include "dir_picker_ctrl.h"
#endif
#include "lib/config.h"
#include "lib/compose.hpp"
#include <wx/stdpaths.h>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>

using namespace std;
using namespace boost;

boost::optional<boost::filesystem::path> FilmNameLocationDialog::_directory;

FilmNameLocationDialog::FilmNameLocationDialog (wxWindow* parent, wxString title, bool offer_templates)
	: TableDialog (parent, title, 2, 1, true)
{
	add (_("Film name"), true);
	_name = add (new wxTextCtrl (this, wxID_ANY));

	add (_("Create in folder"), true);

#ifdef DCPOMATIC_USE_OWN_PICKER
	_folder = new DirPickerCtrl (this);
#else
	_folder = new wxDirPickerCtrl (this, wxID_ANY, wxEmptyString, wxDirSelectorPromptStr, wxDefaultPosition, wxSize (300, -1));
#endif

	if (!_directory) {
		_directory = Config::instance()->default_directory_or (wx_to_std (wxStandardPaths::Get().GetDocumentsDir()));
	}

	_folder->SetPath (std_to_wx (_directory.get().string()));
	add (_folder);

	if (offer_templates) {
		_use_template = new wxCheckBox (this, wxID_ANY, _("From template"));
		add (_use_template);
		_template_name = new wxChoice (this, wxID_ANY);
		add (_template_name);
	}

	_name->SetFocus ();

	if (offer_templates) {
		_template_name->Enable (false);

		BOOST_FOREACH (string i, Config::instance()->templates ()) {
			_template_name->Append (std_to_wx (i));
		}

		_use_template->Bind (wxEVT_CHECKBOX, bind (&FilmNameLocationDialog::use_template_clicked, this));
	}

	layout ();
}

void
FilmNameLocationDialog::use_template_clicked ()
{
	_template_name->Enable (_use_template->GetValue ());
}

FilmNameLocationDialog::~FilmNameLocationDialog ()
{
	_directory = wx_to_std (_folder->GetPath ());
}

boost::filesystem::path
FilmNameLocationDialog::path () const
{
	filesystem::path p;
	p /= wx_to_std (_folder->GetPath ());
	p /= wx_to_std (_name->GetValue ());
	return p;
}

optional<string>
FilmNameLocationDialog::template_name () const
{
	if (!_use_template->GetValue() || _template_name->GetSelection() == -1) {
		return optional<string> ();
	}

	return wx_to_std (_template_name->GetString(_template_name->GetSelection()));
}

/** Check the path that is in our controls and offer confirmations or errors as required.
 *  @return true if the path should be used.
 */
bool
FilmNameLocationDialog::check_path ()
{
	if (boost::filesystem::is_directory (path()) && !boost::filesystem::is_empty(path())) {
		if (!confirm_dialog (
			    this,
			    std_to_wx (
				    String::compose (wx_to_std (_("The directory %1 already exists and is not empty.  "
								  "Are you sure you want to use it?")),
						     path().string().c_str())
				    )
			    )) {
			return false;
		}
	} else if (boost::filesystem::is_regular_file (path())) {
		error_dialog (
			this,
			String::compose (wx_to_std (_("%1 already exists as a file, so you cannot use it for a film.")), path().c_str())
			);
		return false;
	}

	return true;
}
