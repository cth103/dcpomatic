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


#include "i18n_setup.h"
#include "wx_util.h"
#include "lib/config.h"
#include "lib/i18n_setup.h"
#include <wx/wx.h>


void
dcpomatic::wx::setup_i18n()
{
	int language = wxLANGUAGE_DEFAULT;

	auto config_lang = Config::instance()->language();
	if (config_lang && !config_lang->empty ()) {
		auto const li = wxLocale::FindLanguageInfo(std_to_wx (config_lang.get ()));
		if (li) {
			language = li->Language;
		}
	}

	wxLocale* locale = nullptr;
	if (wxLocale::IsAvailable (language)) {
		locale = new wxLocale(language, wxLOCALE_LOAD_DEFAULT);

		locale->AddCatalogLookupPathPrefix(std_to_wx(LINUX_LOCALE_PREFIX));

		/* We have to include the wxWidgets .mo in our distribution,
		   so we rename it to avoid clashes with any other installation
		   of wxWidgets.
		*/
		locale->AddCatalog(char_to_wx("dcpomatic2-wxstd"));

		/* Fedora 29 (at least) installs wxstd3.mo instead of wxstd.mo */
		locale->AddCatalog(char_to_wx("wxstd3"));

		locale->AddCatalog(char_to_wx("wxstd"));
		locale->AddCatalog(char_to_wx("libdcpomatic2-wx"));
		locale->AddCatalog(char_to_wx("dcpomatic2"));

		if (!locale->IsOk()) {
			delete locale;
			locale = new wxLocale (wxLANGUAGE_ENGLISH);
		}
	}

	if (locale) {
		dcpomatic::setup_i18n(wx_to_std(locale->GetCanonicalName()));
	}
}

