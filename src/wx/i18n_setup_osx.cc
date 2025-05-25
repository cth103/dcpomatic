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
#if wxCHECK_VERSION(3, 1, 6)
#include <wx/uilocale.h>
#endif
#include <CoreFoundation/CoreFoundation.h>


using std::string;


void
dcpomatic::wx::setup_i18n()
{
	wxLog::EnableLogging();

#if wxCHECK_VERSION(3, 1, 6)
	wxUILocale::UseDefault();
#endif

	auto get_locale_value = [](CFLocaleKey key) {
		CFLocaleRef cflocale = CFLocaleCopyCurrent();
		auto value = (CFStringRef) CFLocaleGetValue(cflocale, key);
		char buffer[64];
		CFStringGetCString(value, buffer, sizeof(buffer), kCFStringEncodingUTF8);
		CFRelease(cflocale);
		return string(buffer);
	};

	auto translations = new wxTranslations();

	auto config_lang = Config::instance()->language();
	if (config_lang && !config_lang->empty()) {
		translations->SetLanguage(std_to_wx(*config_lang));
	} else {
		/* We want to use the user's preferred language.  It seems that if we use the wxWidgets default we will get the
		 * language for the locale, which may not be what we want (e.g. for a machine in Germany, configured for DE locale,
		 * but with the preferred language set to English).
		 *
		 * Instead, the the language code from macOS then get the corresponding canonical language string with region,
		 * which wxTranslations::SetLanguage will accept.
		 */
		auto const language_code = get_locale_value(kCFLocaleLanguageCode);
		/* Ideally this would be wxUILocale (as wxLocale is deprecated) but we want to keep this building
		 * with the old wxWidgets we use for the older macOS builds.
		 */
		auto const info = wxLocale::FindLanguageInfo(std_to_wx(language_code));
		if (info) {
#if wxCHECK_VERSION(3, 1, 6)
			translations->SetLanguage(info->GetCanonicalWithRegion());
#else
			translations->SetLanguage(info->CanonicalName);
#endif
		}
	}

#ifdef DCPOMATIC_DEBUG
	wxFileTranslationsLoader::AddCatalogLookupPathPrefix(char_to_wx("build/src/wx/mo"));
	wxFileTranslationsLoader::AddCatalogLookupPathPrefix(char_to_wx("build/src/tools/mo"));
#endif

	translations->AddStdCatalog();
	translations->AddCatalog(char_to_wx("libdcpomatic2-wx"));
	translations->AddCatalog(char_to_wx("dcpomatic2"));

	wxTranslations::Set(translations);

	dcpomatic::setup_i18n(config_lang.get_value_or(""));
}


