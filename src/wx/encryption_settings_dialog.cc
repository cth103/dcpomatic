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


#include "check_box.h"
#include "encryption_settings_dialog.h"
#include "lib/film.h"


using std::shared_ptr;


EncryptionSettingsDialog::EncryptionSettingsDialog(wxWindow* parent, shared_ptr<const Film> film)
	: TableDialog(parent, _("Encryption settings"), 1, 0, true)
{
	_picture = add(new CheckBox(this, _("Encrypt picture")));
	_sound = add(new CheckBox(this, _("Encrypt sound")));
	_text = add(new CheckBox(this, _("Encrypt text")));

	layout();

	_picture->set(film->encrypt_picture());
	_sound->set(film->encrypt_sound());
	_text->set(film->encrypt_text());
}


bool
EncryptionSettingsDialog::picture() const
{
	return _picture->get();
}


bool
EncryptionSettingsDialog::sound() const
{
	return _sound->get();
}


bool
EncryptionSettingsDialog::text() const
{
	return _text->get();
}

