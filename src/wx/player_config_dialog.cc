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

/** @file src/player_config_dialog.cc
 *  @brief A dialogue to edit DCP-o-matic Player configuration.
 */

#include "config_dialog.h"
#include "wx_util.h"
#include "editable_list.h"
#include "filter_dialog.h"
#include "dir_picker_ctrl.h"
#include "file_picker_ctrl.h"
#include "isdcf_metadata_dialog.h"
#include "server_dialog.h"
#include "make_chain_dialog.h"
#include "email_dialog.h"
#include "name_format_editor.h"
#include "nag_dialog.h"
#include "config_dialog.h"
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

using std::vector;
using std::string;
using std::list;
using std::cout;
using std::pair;
using std::make_pair;
using std::map;
using boost::bind;
using boost::shared_ptr;
using boost::function;
using boost::optional;
using dcp::locale_convert;

class PlayerGeneralPage : public GeneralPage
{
public:
	PlayerGeneralPage (wxSize panel_size, int border)
		: GeneralPage (panel_size, border)
	{}

private:
	void setup ()
	{
		wxGridBagSizer* table = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		_panel->GetSizer()->Add (table, 1, wxALL | wxEXPAND, _border);

		int r = 0;
		add_language_controls (table, r);
		add_play_sound_controls (table, r);
		add_update_controls (table, r);

		add_label_to_sizer (table, _panel, _("Start player as"), true, wxGBPosition(r, 0));
		_player_mode = new wxChoice (_panel, wxID_ANY);
		_player_mode->Append (_("window"));
		_player_mode->Append (_("full screen"));
		_player_mode->Append (_("full screen with controls on second monitor"));
		table->Add (_player_mode, wxGBPosition(r, 1));
		++r;

		_respect_kdm = new wxCheckBox (_panel, wxID_ANY, _("Respect KDM validity periods"));
		table->Add (_respect_kdm, wxGBPosition(r, 0), wxGBSpan(1, 2));
		++r;

		_player_mode->Bind (wxEVT_CHOICE, bind(&PlayerGeneralPage::player_mode_changed, this));
		_respect_kdm->Bind (wxEVT_CHECKBOX, bind(&PlayerGeneralPage::respect_kdm_changed, this));
	}

	void config_changed ()
	{
		GeneralPage::config_changed ();

		switch (Config::instance()->player_mode()) {
		case Config::PLAYER_MODE_WINDOW:
			checked_set (_player_mode, 0);
			break;
		case Config::PLAYER_MODE_FULL:
			checked_set (_player_mode, 1);
			break;
		case Config::PLAYER_MODE_DUAL:
			checked_set (_player_mode, 2);
			break;
		}

		checked_set (_respect_kdm, Config::instance()->respect_kdm_validity_periods());
	}

private:
	void player_mode_changed ()
	{
		switch (_player_mode->GetSelection()) {
		case 0:
			Config::instance()->set_player_mode(Config::PLAYER_MODE_WINDOW);
			break;
		case 1:
			Config::instance()->set_player_mode(Config::PLAYER_MODE_FULL);
			break;
		case 2:
			Config::instance()->set_player_mode(Config::PLAYER_MODE_DUAL);
			break;
		}
	}

	void respect_kdm_changed ()
	{
		Config::instance()->set_respect_kdm_validity_periods(_respect_kdm->GetValue());
	}

	wxChoice* _player_mode;
	wxCheckBox* _respect_kdm;
};

wxPreferencesEditor*
create_player_config_dialog ()
{
	wxPreferencesEditor* e = new wxPreferencesEditor ();

#ifdef DCPOMATIC_OSX
	/* Width that we force some of the config panels to be on OSX so that
	   the containing window doesn't shrink too much when we select those panels.
	   This is obviously an unpleasant hack.
	*/
	wxSize ps = wxSize (520, -1);
	int const border = 16;
#else
	wxSize ps = wxSize (-1, -1);
	int const border = 8;
#endif

	e->AddPage (new PlayerGeneralPage (ps, border));
	e->AddPage (new KeysPage (ps, border));
	return e;
}
