/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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

#include "lib/config.h"
#include "new_film_dialog.h"
#include "wx_util.h"
#ifdef DCPOMATIC_USE_OWN_PICKER
#include "dir_picker_ctrl.h"
#endif
#include <wx/stdpaths.h>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>

using namespace std;
using namespace boost;

boost::optional<boost::filesystem::path> NewFilmDialog::_directory;

NewFilmDialog::NewFilmDialog (wxWindow* parent)
	: TableDialog (parent, _("New Film"), 2, 1, true)
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

	_use_template = new wxCheckBox (this, wxID_ANY, _("From template"));
	add (_use_template);
	_template_name = new wxChoice (this, wxID_ANY);
	add (_template_name);

	_name->SetFocus ();
	_template_name->Enable (false);

	BOOST_FOREACH (string i, Config::instance()->templates ()) {
		_template_name->Append (std_to_wx (i));
	}

	_use_template->Bind (wxEVT_CHECKBOX, bind (&NewFilmDialog::use_template_clicked, this));

	layout ();
}

void
NewFilmDialog::use_template_clicked ()
{
	_template_name->Enable (_use_template->GetValue ());
}

NewFilmDialog::~NewFilmDialog ()
{
	_directory = wx_to_std (_folder->GetPath ());
}

boost::filesystem::path
NewFilmDialog::path () const
{
	filesystem::path p;
	p /= wx_to_std (_folder->GetPath ());
	p /= wx_to_std (_name->GetValue ());
	return p;
}

optional<string>
NewFilmDialog::template_name () const
{
	if (!_use_template->GetValue() || _template_name->GetSelection() == -1) {
		return optional<string> ();
	}

	return wx_to_std (_template_name->GetString(_template_name->GetSelection()));
}
