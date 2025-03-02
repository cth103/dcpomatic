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


namespace dcpomatic {
namespace preferences {


class KeysPage : public Page
{
public:
	KeysPage(wxSize panel_size, int border);

	wxString GetName() const override;

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon() const override;
#endif

private:

	void setup() override;

	void export_decryption_certificate();
	void config_changed() override {}
	bool nag_alter_decryption_chain();
	void decryption_advanced();
	void signing_advanced();
	void export_decryption_chain_and_key();
	void import_decryption_chain_and_key();
	void remake_signing();
};


}
}

