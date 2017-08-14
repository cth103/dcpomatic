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

#ifndef DCPOMATIC_CONFIG_DIALOG_H
#define DCPOMATIC_CONFIG_DIALOG_H

#include "wx_util.h"
#include "editable_list.h"
#include "lib/config.h"
#include "lib/ratio.h"
#include "lib/filter.h"
#include "lib/dcp_content_type.h"
#include "lib/log.h"
#include "lib/util.h"
#include "lib/cross.h"
#include "lib/exceptions.h"
#include <dcp/locale_convert.h>
#include <dcp/exceptions.h>
#include <dcp/certificate_chain.h>
#include <wx/stdpaths.h>
#include <wx/preferences.h>
#include <wx/spinctrl.h>
#include <wx/filepicker.h>
#include <RtAudio.h>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <iostream>

class Page
{
public:
	Page (wxSize panel_size, int border)
		: _border (border)
		, _panel (0)
		, _panel_size (panel_size)
		, _window_exists (false)
	{
		_config_connection = Config::instance()->Changed.connect (boost::bind (&Page::config_changed_wrapper, this));
	}

	virtual ~Page () {}

protected:
	wxWindow* create_window (wxWindow* parent)
	{
		_panel = new wxPanel (parent, wxID_ANY, wxDefaultPosition, _panel_size);
		wxBoxSizer* s = new wxBoxSizer (wxVERTICAL);
		_panel->SetSizer (s);

		setup ();
		_window_exists = true;
		config_changed ();

		_panel->Bind (wxEVT_DESTROY, boost::bind (&Page::window_destroyed, this));

		return _panel;
	}

	int _border;
	wxPanel* _panel;

private:
	virtual void config_changed () = 0;
	virtual void setup () = 0;

	void config_changed_wrapper ()
	{
		if (_window_exists) {
			config_changed ();
		}
	}

	void window_destroyed ()
	{
		_window_exists = false;
	}

	wxSize _panel_size;
	boost::signals2::scoped_connection _config_connection;
	bool _window_exists;
};

class StockPage : public wxStockPreferencesPage, public Page
{
public:
	StockPage (Kind kind, wxSize panel_size, int border)
		: wxStockPreferencesPage (kind)
		, Page (panel_size, border)
	{}

	wxWindow* CreateWindow (wxWindow* parent)
	{
		return create_window (parent);
	}
};

class StandardPage : public wxPreferencesPage, public Page
{
public:
	StandardPage (wxSize panel_size, int border)
		: Page (panel_size, border)
	{}

	wxWindow* CreateWindow (wxWindow* parent)
	{
		return create_window (parent);
	}
};

class GeneralPage : public StockPage
{
public:
	GeneralPage (wxSize panel_size, int border)
		: StockPage (Kind_General, panel_size, border)
	{}

protected:
	void add_language_controls (wxGridBagSizer* table, int& r)
	{
		_set_language = new wxCheckBox (_panel, wxID_ANY, _("Set language"));
		table->Add (_set_language, wxGBPosition (r, 0));
		_language = new wxChoice (_panel, wxID_ANY);
		std::vector<std::pair<std::string, std::string> > languages;
		languages.push_back (std::make_pair ("Čeština", "cs_CZ"));
		languages.push_back (std::make_pair ("汉语/漢語", "zh_CN"));
		languages.push_back (std::make_pair ("Dansk", "da_DK"));
		languages.push_back (std::make_pair ("Deutsch", "de_DE"));
		languages.push_back (std::make_pair ("English", "en_GB"));
		languages.push_back (std::make_pair ("Español", "es_ES"));
		languages.push_back (std::make_pair ("Français", "fr_FR"));
		languages.push_back (std::make_pair ("Italiano", "it_IT"));
		languages.push_back (std::make_pair ("Nederlands", "nl_NL"));
		languages.push_back (std::make_pair ("Русский", "ru_RU"));
		languages.push_back (std::make_pair ("Polski", "pl_PL"));
		languages.push_back (std::make_pair ("Português europeu", "pt_PT"));
		languages.push_back (std::make_pair ("Português do Brasil", "pt_BR"));
		languages.push_back (std::make_pair ("Svenska", "sv_SE"));
		languages.push_back (std::make_pair ("Slovenský jazyk", "sk_SK"));
		languages.push_back (std::make_pair ("українська мова", "uk_UA"));
		checked_set (_language, languages);
		table->Add (_language, wxGBPosition (r, 1));
		++r;

		wxStaticText* restart = add_label_to_sizer (
			table, _panel, _("(restart DCP-o-matic to see language changes)"), false, wxGBPosition (r, 0), wxGBSpan (1, 2)
			);
		wxFont font = restart->GetFont();
		font.SetStyle (wxFONTSTYLE_ITALIC);
		font.SetPointSize (font.GetPointSize() - 1);
		restart->SetFont (font);
		++r;

		_set_language->Bind (wxEVT_CHECKBOX, boost::bind (&GeneralPage::set_language_changed, this));
		_language->Bind     (wxEVT_CHOICE,   boost::bind (&GeneralPage::language_changed,     this));
	}

	void add_play_sound_controls (wxGridBagSizer* table, int& r)
	{
		_sound = new wxCheckBox (_panel, wxID_ANY, _("Play sound via"));
		table->Add (_sound, wxGBPosition (r, 0));
                _sound_output = new wxChoice (_panel, wxID_ANY);
                table->Add (_sound_output, wxGBPosition (r, 1));
                ++r;

                RtAudio audio (DCPOMATIC_RTAUDIO_API);
                for (unsigned int i = 0; i < audio.getDeviceCount(); ++i) {
                        RtAudio::DeviceInfo dev = audio.getDeviceInfo (i);
                        if (dev.probed && dev.outputChannels > 0) {
                                _sound_output->Append (std_to_wx (dev.name));
                        }
                }

		_sound->Bind        (wxEVT_CHECKBOX, boost::bind (&GeneralPage::sound_changed, this));
		_sound_output->Bind (wxEVT_CHOICE,   boost::bind (&GeneralPage::sound_output_changed, this));
	}

	void add_update_controls (wxGridBagSizer* table, int& r)
	{
		_check_for_updates = new wxCheckBox (_panel, wxID_ANY, _("Check for updates on startup"));
		table->Add (_check_for_updates, wxGBPosition (r, 0), wxGBSpan (1, 2));
		++r;

		_check_for_test_updates = new wxCheckBox (_panel, wxID_ANY, _("Check for testing updates on startup"));
		table->Add (_check_for_test_updates, wxGBPosition (r, 0), wxGBSpan (1, 2));
		++r;

		_check_for_updates->Bind (wxEVT_CHECKBOX, boost::bind (&GeneralPage::check_for_updates_changed, this));
		_check_for_test_updates->Bind (wxEVT_CHECKBOX, boost::bind (&GeneralPage::check_for_test_updates_changed, this));
	}

	virtual void config_changed ()
	{
		Config* config = Config::instance ();

		checked_set (_set_language, static_cast<bool>(config->language()));

		/* Backwards compatibility of config file */

		std::map<std::string, std::string> compat_map;
		compat_map["fr"] = "fr_FR";
		compat_map["it"] = "it_IT";
		compat_map["es"] = "es_ES";
		compat_map["sv"] = "sv_SE";
		compat_map["de"] = "de_DE";
		compat_map["nl"] = "nl_NL";
		compat_map["ru"] = "ru_RU";
		compat_map["pl"] = "pl_PL";
		compat_map["da"] = "da_DK";
		compat_map["pt"] = "pt_PT";
		compat_map["sk"] = "sk_SK";
		compat_map["cs"] = "cs_CZ";
		compat_map["uk"] = "uk_UA";

		std::string lang = config->language().get_value_or ("en_GB");
		if (compat_map.find (lang) != compat_map.end ()) {
			lang = compat_map[lang];
		}

		checked_set (_language, lang);

		checked_set (_check_for_updates, config->check_for_updates ());
		checked_set (_check_for_test_updates, config->check_for_test_updates ());

		checked_set (_sound, config->sound ());

		boost::optional<std::string> const current_so = get_sound_output ();
                boost::optional<std::string> configured_so;

                if (config->sound_output()) {
                        configured_so = config->sound_output().get();
                } else {
                        /* No configured output means we should use the default */
                        RtAudio audio (DCPOMATIC_RTAUDIO_API);
			try {
				configured_so = audio.getDeviceInfo(audio.getDefaultOutputDevice()).name;
			} catch (RtAudioError& e) {
				/* Probably no audio devices at all */
			}
                }

                if (configured_so && current_so != configured_so) {
                        /* Update _sound_output with the configured value */
                        unsigned int i = 0;
                        while (i < _sound_output->GetCount()) {
                                if (_sound_output->GetString(i) == std_to_wx(*configured_so)) {
                                        _sound_output->SetSelection (i);
                                        break;
                                }
                                ++i;
                        }
                }

		setup_sensitivity ();
	}

private:
	void setup_sensitivity ()
	{
		_language->Enable (_set_language->GetValue ());
		_check_for_test_updates->Enable (_check_for_updates->GetValue ());
		_sound_output->Enable (_sound->GetValue ());
	}

        /** @return Currently-selected preview sound output in the dialogue */
        boost::optional<std::string> get_sound_output ()
        {
                int const sel = _sound_output->GetSelection ();
                if (sel == wxNOT_FOUND) {
                        return boost::optional<std::string> ();
                }

                return wx_to_std (_sound_output->GetString (sel));
        }

	void set_language_changed ()
	{
		setup_sensitivity ();
		if (_set_language->GetValue ()) {
			language_changed ();
		} else {
			Config::instance()->unset_language ();
		}
	}

	void language_changed ()
	{
		int const sel = _language->GetSelection ();
		if (sel != -1) {
			Config::instance()->set_language (string_client_data (_language->GetClientObject (sel)));
		} else {
			Config::instance()->unset_language ();
		}
	}

	void check_for_updates_changed ()
	{
		Config::instance()->set_check_for_updates (_check_for_updates->GetValue ());
	}

	void check_for_test_updates_changed ()
	{
		Config::instance()->set_check_for_test_updates (_check_for_test_updates->GetValue ());
	}

	void sound_changed ()
	{
		Config::instance()->set_sound (_sound->GetValue ());
	}

        void sound_output_changed ()
        {
                RtAudio audio (DCPOMATIC_RTAUDIO_API);
		boost::optional<std::string> const so = get_sound_output();
                if (!so || *so == audio.getDeviceInfo(audio.getDefaultOutputDevice()).name) {
                        Config::instance()->unset_sound_output ();
                } else {
                        Config::instance()->set_sound_output (*so);
                }
        }

	wxCheckBox* _set_language;
	wxChoice* _language;
	wxCheckBox* _sound;
	wxChoice* _sound_output;
	wxCheckBox* _check_for_updates;
	wxCheckBox* _check_for_test_updates;
};

#endif
