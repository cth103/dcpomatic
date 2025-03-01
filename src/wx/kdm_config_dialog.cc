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


/** @file src/kdm_config_dialog.cc
 *  @brief A dialogue to edit DCP-o-matic KDM Creator configuration.
 */


#include "check_box.h"
#include "config_dialog.h"
#include "email_preferences_page.h"
#include "kdm_email_preferences_page.h"
#include "file_picker_ctrl.h"
#include "static_text.h"
#include "wx_variant.h"


using boost::bind;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


class KDMGeneralPage : public preferences::GeneralPage
{
public:
	KDMGeneralPage(wxSize panel_size, int border)
		: GeneralPage(panel_size, border)
	{}

private:
	void setup() override
	{
		auto table = new wxGridBagSizer(DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		_panel->GetSizer()->Add(table, 1, wxALL | wxEXPAND, _border);

		int r = 0;
		add_language_controls(table, r);
		add_config_file_controls(table, r);
		add_update_controls(table, r);

		add_label_to_sizer(table, _panel, _("Debug log file"), true, wxGBPosition(r, 0));
		_debug_log_file = new FilePickerCtrl(_panel, _("Select debug log file"), char_to_wx("*"), false, true, "DebugLogPath");
		table->Add(_debug_log_file, wxGBPosition(r, 1));
		++r;

		_debug_log_file->Bind(wxEVT_FILEPICKER_CHANGED, bind(&KDMGeneralPage::debug_log_file_changed, this));
	}

	void config_changed() override
	{
		GeneralPage::config_changed();

		auto config = Config::instance();
		if (config->kdm_debug_log_file()) {
			checked_set(_debug_log_file, *config->kdm_debug_log_file());
		}
	}

private:
	void debug_log_file_changed()
	{
		if (auto path = _debug_log_file->path()) {
			Config::instance()->set_kdm_debug_log_file(*path);
		}
	}

	FilePickerCtrl* _debug_log_file = nullptr;
};


/** @class KDMAdvancedPage
 *  @brief Advanced page of the preferences dialog for the KDM creator.
 */
class KDMAdvancedPage : public preferences::Page
{
public:
	KDMAdvancedPage(wxSize panel_size, int border)
		: Page(panel_size, border)
	{}

	wxString GetName() const override
	{
		return _("Advanced");
	}

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon() const override
	{
		return wxBitmap(icon_path("advanced"), wxBITMAP_TYPE_PNG);
	}
#endif

private:
	void add_top_aligned_label_to_sizer(wxSizer* table, wxWindow* parent, wxString text)
	{
		int flags = wxALIGN_TOP | wxTOP | wxLEFT;
#ifdef __WXOSX__
		flags |= wxALIGN_RIGHT;
		text += char_to_wx(":");
#endif
		auto m = new StaticText(parent, text);
		table->Add(m, 0, flags, DCPOMATIC_SIZER_Y_GAP);
	}

	void setup() override
	{
		auto table = new wxFlexGridSizer(2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		table->AddGrowableCol(1, 1);
		_panel->GetSizer()->Add(table, 1, wxALL | wxEXPAND, _border);

		{
			add_top_aligned_label_to_sizer(table, _panel, _("Log"));
			auto t = new wxBoxSizer(wxVERTICAL);
			_log_general = new CheckBox(_panel, _("General"));
			t->Add(_log_general, 1, wxEXPAND | wxALL);
			_log_warning = new CheckBox(_panel, _("Warnings"));
			t->Add(_log_warning, 1, wxEXPAND | wxALL);
			_log_error = new CheckBox(_panel, _("Errors"));
			t->Add(_log_error, 1, wxEXPAND | wxALL);
			_log_debug_email = new CheckBox(_panel, _("Debug: email sending"));
			t->Add(_log_debug_email, 1, wxEXPAND | wxALL);
			table->Add(t, 0, wxALL, 6);
		}

		_log_general->bind(&KDMAdvancedPage::log_changed, this);
		_log_warning->bind(&KDMAdvancedPage::log_changed, this);
		_log_error->bind(&KDMAdvancedPage::log_changed, this);
		_log_debug_email->bind(&KDMAdvancedPage::log_changed, this);
	}

	void config_changed() override
	{
		auto config = Config::instance();

		checked_set(_log_general, config->log_types() & LogEntry::TYPE_GENERAL);
		checked_set(_log_warning, config->log_types() & LogEntry::TYPE_WARNING);
		checked_set(_log_error, config->log_types() & LogEntry::TYPE_ERROR);
		checked_set(_log_debug_email, config->log_types() & LogEntry::TYPE_DEBUG_EMAIL);
	}

	void log_changed()
	{
		int types = 0;
		if (_log_general->GetValue()) {
			types |= LogEntry::TYPE_GENERAL;
		}
		if (_log_warning->GetValue()) {
			types |= LogEntry::TYPE_WARNING;
		}
		if (_log_error->GetValue()) {
			types |= LogEntry::TYPE_ERROR;
		}
		if (_log_debug_email->GetValue()) {
			types |= LogEntry::TYPE_DEBUG_EMAIL;
		}
		Config::instance()->set_log_types(types);
	}

	CheckBox* _log_general = nullptr;
	CheckBox* _log_warning = nullptr;
	CheckBox* _log_error = nullptr;
	CheckBox* _log_debug_email = nullptr;
};


wxPreferencesEditor*
create_kdm_config_dialog()
{
	auto e = new wxPreferencesEditor(variant::wx::insert_dcpomatic_kdm_creator(_("%s Preferences")));

#ifdef DCPOMATIC_OSX
	/* Width that we force some of the config panels to be on OSX so that
	   the containing window doesn't shrink too much when we select those panels.
	   This is obviously an unpleasant hack.
	*/
	auto ps = wxSize(520, -1);
	int const border = 16;
#else
	auto ps = wxSize(-1, -1);
	int const border = 8;
#endif

	e->AddPage(new KDMGeneralPage(wxSize(-1, 500), border));
	e->AddPage(new preferences::KeysPage(ps, border));
	e->AddPage(new preferences::EmailPage(ps, border));
	e->AddPage(new preferences::KDMEmailPage(ps, border));
	e->AddPage(new KDMAdvancedPage(ps, border));
	return e;
}
