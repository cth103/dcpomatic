/*
    Copyright (C) 2015-2021 Carl Hetherington <cth@carlh.net>

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


#include "wx/about_dialog.h"
#include "wx/dcpomatic_button.h"
#include "wx/editable_list.h"
#include "wx/i18n_setup.h"
#include "wx/id.h"
#include "wx/invalid_certificate_period_dialog.h"
#include "wx/file_dialog.h"
#include "wx/file_picker_ctrl.h"
#include "wx/job_view_dialog.h"
#include "wx/kdm_config_dialog.h"
#include "wx/kdm_timing_panel.h"
#include "wx/nag_dialog.h"
#include "wx/new_dkdm_folder_dialog.h"
#include "wx/report_problem_dialog.h"
#include "wx/screens_panel.h"
#include "wx/short_kdm_output_panel.h"
#include "wx/static_text.h"
#include "wx/tall_kdm_output_panel.h"
#include "wx/wx_signal_manager.h"
#include "wx/wx_util.h"
#include "wx/wx_variant.h"
#include "lib/cinema.h"
#include "lib/collator.h"
#include "lib/compose.hpp"
#include "lib/constants.h"
#include "lib/config.h"
#include "lib/cross.h"
#include "lib/dcpomatic_log.h"
#include "lib/dkdm_wrapper.h"
#include "lib/exceptions.h"
#include "lib/file_log.h"
#include "lib/job_manager.h"
#include "lib/kdm_util.h"
#include "lib/kdm_with_metadata.h"
#include "lib/null_log.h"
#include "lib/screen.h"
#include "lib/send_kdm_email_job.h"
#include "lib/util.h"
#include "lib/variant.h"
#include <dcp/encrypted_kdm.h>
#include <dcp/decrypted_kdm.h>
#include <dcp/exceptions.h>
#include <dcp/filesystem.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/dnd.h>
#include <wx/filepicker.h>
#include <wx/preferences.h>
#include <wx/splash.h>
#include <wx/srchctrl.h>
#include <wx/treectrl.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#ifdef DCPOMATIC_OSX
#include <ApplicationServices/ApplicationServices.h>
#endif
#include <boost/bind/bind.hpp>
#include <unordered_set>

#ifdef check
#undef check
#endif


using std::dynamic_pointer_cast;
using std::exception;
using std::list;
using std::make_shared;
using std::map;
using std::pair;
using std::shared_ptr;
using std::string;
using std::unordered_set;
using std::vector;
using boost::bind;
using boost::optional;
using boost::ref;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


enum {
	ID_help_report_a_problem = DCPOMATIC_MAIN_MENU,
};


class DOMFrame : public wxFrame
{
public:
	explicit DOMFrame (wxString const & title)
		: wxFrame (nullptr, -1, title)
		, _config_dialog (nullptr)
		, _job_view (nullptr)
	{
#if defined(DCPOMATIC_WINDOWS)
		if (Config::instance()->win32_console ()) {
			AllocConsole();

			HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
			int hCrt = _open_osfhandle((intptr_t) handle_out, _O_TEXT);
			FILE* hf_out = _fdopen(hCrt, "w");
			setvbuf(hf_out, NULL, _IONBF, 1);
			*stdout = *hf_out;

			HANDLE handle_in = GetStdHandle(STD_INPUT_HANDLE);
			hCrt = _open_osfhandle((intptr_t) handle_in, _O_TEXT);
			FILE* hf_in = _fdopen(hCrt, "r");
			setvbuf(hf_in, NULL, _IONBF, 128);
			*stdin = *hf_in;

			std::cout << variant::insert_dcpomatic_kdm_creator("%1 is starting.\n");
		}
#endif

		auto bar = new wxMenuBar;
		setup_menu (bar);
		SetMenuBar (bar);

		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_exit, this),             wxID_EXIT);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::edit_preferences, this),      wxID_PREFERENCES);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::help_about, this),            wxID_ABOUT);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::help_report_a_problem, this), ID_help_report_a_problem);

		/* Use a panel as the only child of the Frame so that we avoid
		   the dark-grey background on Windows.
		*/
		auto overall_panel = new wxPanel (this, wxID_ANY);
		auto main_sizer = new wxBoxSizer (wxHORIZONTAL);

		auto horizontal = new wxBoxSizer (wxHORIZONTAL);
		auto left = new wxBoxSizer (wxVERTICAL);
		auto right = new wxBoxSizer (wxVERTICAL);

		horizontal->Add (left, 1, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_X_GAP * 2);
		horizontal->Add (right, 1, wxEXPAND);

		wxFont subheading_font (*wxNORMAL_FONT);
		subheading_font.SetWeight (wxFONTWEIGHT_BOLD);

		auto h = new StaticText (overall_panel, _("Screens"));
		h->SetFont (subheading_font);
		left->Add (h, 0, wxBOTTOM, DCPOMATIC_SIZER_Y_GAP);
		_screens = new ScreensPanel (overall_panel);
		left->Add (_screens, 1, wxEXPAND | wxBOTTOM, DCPOMATIC_SIZER_Y_GAP);

		/// TRANSLATORS: translate the word "Timing" here; do not include the "KDM|" prefix
		h = new StaticText (overall_panel, S_("KDM|Timing"));
		h->SetFont (subheading_font);
		right->Add (h);
		_timing = new KDMTimingPanel (overall_panel);
		right->Add (_timing, 0, wxTOP, DCPOMATIC_SIZER_Y_GAP);

		h = new StaticText (overall_panel, _("DKDM"));
		h->SetFont (subheading_font);
		right->Add(h, 0);

		_dkdm_search = new wxSearchCtrl(overall_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(200, search_ctrl_height()));
#ifndef __WXGTK3__
		/* The cancel button seems to be strangely broken in GTK3; clicking on it twice sometimes works */
		_dkdm_search->ShowCancelButton (true);
#endif

		right->Add(_dkdm_search, 0, wxTOP | wxBOTTOM, DCPOMATIC_SIZER_Y_GAP);

		class DKDMDropTarget : public wxFileDropTarget
		{
		public:
			DKDMDropTarget(DOMFrame* frame)
				: _frame(frame)
			{}

			bool OnDropFiles(wxCoord, wxCoord, wxArrayString const& filenames) override
			{
				for (size_t i = 0; i < filenames.GetCount(); ++i) {
					_frame->add_dkdm(boost::filesystem::path(wx_to_std(filenames[0])));
				}

				return true;
			}

		private:
			DOMFrame* _frame;
		};

		auto dkdm_sizer = new wxBoxSizer (wxHORIZONTAL);
		_dkdm = new wxTreeCtrl (
			overall_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTR_HIDE_ROOT | wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT
		);
		_dkdm->SetDropTarget(new DKDMDropTarget(this));
		dkdm_sizer->Add(_dkdm, 1, wxEXPAND | wxBOTTOM, DCPOMATIC_SIZER_Y_GAP);
		auto dkdm_buttons = new wxBoxSizer(wxVERTICAL);
		_add_dkdm = new Button (overall_panel, _("Add..."));
		dkdm_buttons->Add (_add_dkdm, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, DCPOMATIC_BUTTON_STACK_GAP);
		_add_dkdm_folder = new Button (overall_panel, _("Add folder..."));
		dkdm_buttons->Add (_add_dkdm_folder, 0, wxALL | wxEXPAND, DCPOMATIC_BUTTON_STACK_GAP);
		_remove_dkdm = new Button (overall_panel, _("Remove"));
		dkdm_buttons->Add (_remove_dkdm, 0, wxALL | wxEXPAND, DCPOMATIC_BUTTON_STACK_GAP);
		_export_dkdm = new Button (overall_panel, _("Export..."));
		dkdm_buttons->Add (_export_dkdm, 0, wxALL | wxEXPAND, DCPOMATIC_BUTTON_STACK_GAP);
		dkdm_sizer->Add (dkdm_buttons, 0, wxEXPAND | wxLEFT | wxRIGHT, DCPOMATIC_SIZER_GAP);
		right->Add (dkdm_sizer, 1, wxEXPAND | wxBOTTOM, DCPOMATIC_SIZER_Y_GAP);

		update_dkdm_view();

		h = new StaticText (overall_panel, _("Output"));
		h->SetFont (subheading_font);
		right->Add(h, 0, wxTOP, DCPOMATIC_SUBHEADING_TOP_PAD);

		if (layout_for_short_screen(this)) {
			_output = new ShortKDMOutputPanel(overall_panel);
		} else {
			_output = new TallKDMOutputPanel(overall_panel);
		}

		right->Add (_output, 0, wxTOP, DCPOMATIC_SIZER_Y_GAP);

		_create = new Button (overall_panel, _("Create KDMs"));
		right->Add(_create, 0, wxTOP, DCPOMATIC_SIZER_GAP);

		main_sizer->Add (horizontal, 1, wxALL | wxEXPAND, DCPOMATIC_DIALOG_BORDER);
		overall_panel->SetSizer (main_sizer);

		/* Instantly save any config changes when using a DCP-o-matic GUI */
		Config::instance()->Changed.connect(boost::bind(&DOMFrame::config_changed, this, _1));
		setup_log();

		_screens->ScreensChanged.connect(boost::bind(&DOMFrame::screens_changed, this));
		_create->Bind (wxEVT_BUTTON, bind (&DOMFrame::create_kdms, this));
		_dkdm->Bind(wxEVT_TREE_SEL_CHANGED, boost::bind(&DOMFrame::dkdm_selection_changed, this));
		_dkdm->Bind (wxEVT_TREE_BEGIN_DRAG, boost::bind (&DOMFrame::dkdm_begin_drag, this, _1));
		_dkdm->Bind (wxEVT_TREE_END_DRAG, boost::bind (&DOMFrame::dkdm_end_drag, this, _1));
		_dkdm->Bind(wxEVT_TREE_ITEM_EXPANDED, boost::bind(&DOMFrame::dkdm_expanded, this, _1));
		_dkdm->Bind(wxEVT_TREE_ITEM_COLLAPSED, boost::bind(&DOMFrame::dkdm_collapsed, this, _1));
		_add_dkdm->Bind (wxEVT_BUTTON, bind (&DOMFrame::add_dkdm_clicked, this));
		_add_dkdm_folder->Bind (wxEVT_BUTTON, bind (&DOMFrame::add_dkdm_folder_clicked, this));
		_remove_dkdm->Bind (wxEVT_BUTTON, bind (&DOMFrame::remove_dkdm_clicked, this));
		_export_dkdm->Bind (wxEVT_BUTTON, bind (&DOMFrame::export_dkdm_clicked, this));
		_dkdm_search->Bind(wxEVT_TEXT, boost::bind(&DOMFrame::dkdm_search_changed, this));
		_timing->TimingChanged.connect(boost::bind(&DOMFrame::setup_sensitivity, this));
		_output->MethodChanged.connect(boost::bind(&DOMFrame::setup_sensitivity, this));

		setup_sensitivity ();
	}

private:
	void config_changed(Config::Property what)
	{
		/* Instantly save any config changes when using a DCP-o-matic GUI */
		Config::instance()->write();

		switch (what) {
		case Config::CINEMAS_FILE:
			_screens->update();
			break;
		case Config::KDM_DEBUG_LOG:
			setup_log();
			break;
		default:
			break;
		}
	}

	void setup_log()
	{
		if (auto p = Config::instance()->kdm_debug_log_file()) {
			dcpomatic_log = make_shared<FileLog>(*p);
		} else {
			dcpomatic_log = make_shared<NullLog>();
		}
	}

	void file_exit ()
	{
		/* false here allows the close handler to veto the close request */
		Close (false);
	}

	void edit_preferences ()
	{
		if (!_config_dialog) {
			_config_dialog = create_kdm_config_dialog();
		}
		_config_dialog->Show (this);
	}

	void help_about ()
	{
		AboutDialog dialog(this);
		dialog.ShowModal();
	}

	void help_report_a_problem ()
	{
		ReportProblemDialog dialog(this, shared_ptr<Film>());
		if (dialog.ShowModal() == wxID_OK) {
			dialog.report();
		}
	}

	void setup_menu (wxMenuBar* m)
	{
#ifndef DCPOMATIC_OSX
		auto file = new wxMenu;
		file->Append (wxID_EXIT, _("&Quit"));
		auto edit = new wxMenu;
		edit->Append (wxID_PREFERENCES, _("&Preferences...\tCtrl-P"));
#endif

		wxMenu* help = new wxMenu;
#ifdef DCPOMATIC_OSX
		/* This will get moved to the program menu, so we just need to add it to some menu that
		 * does get used on macOS.
		 */
		help->Append(wxID_PREFERENCES, _("&Preferences...\tCtrl-,"));
		help->Append(wxID_ABOUT, variant::wx::insert_dcpomatic_kdm_creator(_("About %s")));
#else
		help->Append (wxID_ABOUT, _("About"));
#endif
		if (variant::show_report_a_problem()) {
			help->Append(ID_help_report_a_problem, _("Report a problem..."));
		}

#ifndef DCPOMATIC_OSX
		m->Append (file, _("&File"));
		m->Append (edit, _("&Edit"));
#endif
		m->Append (help, _("&Help"));
	}

	bool confirm_overwrite (boost::filesystem::path path)
	{
		if (dcp::filesystem::is_directory(path)) {
			return confirm_dialog (
				this,
				wxString::Format(_("Folder %s already exists.  Do you want to overwrite it?"), std_to_wx(path.string()).data())
				);
		} else {
			return confirm_dialog (
				this,
				wxString::Format(_("File %s already exists.  Do you want to overwrite it?"), std_to_wx(path.string()).data())
				);
		}
	}

	/** @id if not nullptr this is filled in with the wxTreeItemId of the selection */
	shared_ptr<DKDMBase> selected_dkdm (wxTreeItemId* id = nullptr) const
	{
		wxArrayTreeItemIds selections;
		_dkdm->GetSelections (selections);
		if (selections.GetCount() != 1) {
			if (id) {
				*id = 0;
			}
			return {};
		}

		if (id) {
			*id = selections[0];
		}

		auto i = _dkdm_id.find (selections[0]);
		if (i == _dkdm_id.end()) {
			return {};
		}

		return i->second;
	}

	void create_kdms ()
	{
		try {
			auto dkdm_base = selected_dkdm ();
			if (!dkdm_base) {
				return;
			}

			list<KDMWithMetadataPtr> kdms;
			string title;

			auto dkdm = std::dynamic_pointer_cast<DKDM>(dkdm_base);
			if (!dkdm) {
				return;
			}

			/* Decrypt the DKDM */
			dcp::DecryptedKDM decrypted (dkdm->dkdm(), Config::instance()->decryption_chain()->key().get());
			title = decrypted.content_title_text ();

			/* This is the signer for our new KDMs */
			auto signer = Config::instance()->signer_chain ();
			if (!signer->valid ()) {
				throw InvalidSignerError ();
			}

			vector<KDMCertificatePeriod> period_checks;

			std::function<dcp::DecryptedKDM (dcp::LocalTime, dcp::LocalTime)> make_kdm = [this, decrypted, title](dcp::LocalTime begin, dcp::LocalTime end) {
				/* Make an empty KDM */
				dcp::DecryptedKDM kdm (
					begin,
					end,
					_output->annotation_text(),
					title,
					dcp::LocalTime().as_string()
					);

				/* Add keys from the DKDM */
				for (auto const& j: decrypted.keys()) {
					kdm.add_key(j);
				}

				return kdm;
			};

			CinemaList cinemas;

			for (auto i: _screens->screens()) {

				auto kdm = kdm_for_screen(
					make_kdm,
					i.first,
					*cinemas.cinema(i.first),
					*cinemas.screen(i.second),
					_timing->from(),
					_timing->until(),
					_output->formulation(),
					!_output->forensic_mark_video(),
					_output->forensic_mark_audio() ? boost::optional<int>() : 0,
					period_checks
					);

				if (kdm) {
					kdms.push_back(kdm);
				}
			}

			if (kdms.empty()) {
				return;
			}

			if (
				find_if(
					period_checks.begin(),
					period_checks.end(),
					[](KDMCertificatePeriod const& p) { return p.overlap != KDMCertificateOverlap::KDM_WITHIN_CERTIFICATE; }
				       ) != period_checks.end()) {
				InvalidCertificatePeriodDialog dialog(this, period_checks);
				if (dialog.ShowModal() == wxID_CANCEL) {
					return;
				}
			}

			auto result = _output->make (
				kdms, title, bind (&DOMFrame::confirm_overwrite, this, _1)
				);

			if (result.first) {
				JobManager::instance()->add (result.first);
				if (_job_view) {
					_job_view->Destroy ();
					_job_view = 0;
				}
				_job_view = new JobViewDialog (this, _("Send KDM emails"), result.first);
				_job_view->ShowModal ();
			}

			if (result.second > 0) {
				/* XXX: proper plural form support in wxWidgets? */
				wxString s = result.second == 1 ? _("%d KDM written to %s") : _("%d KDMs written to %s");
				message_dialog (
					this,
					wxString::Format (s, result.second, std_to_wx(_output->directory().string()).data())
					);
			}
		} catch (dcp::BadKDMDateError& e) {
			if (e.starts_too_early()) {
				error_dialog(this, _("The KDM start period is before (or close to) the start of the signing certificate's validity period.  Use a later start time for this KDM."));
			} else {
				error_dialog(
					this,
					variant::wx::insert_dcpomatic_kdm_creator(
						_("The KDM end period is after (or close to) the end of the signing certificates' validity period.  "
						  "Either use an earlier end time for this KDM or re-create your signing certificates in the %s preferences window.")
						)
					);
			}
			return;
		} catch (dcp::NotEncryptedError& e) {
			error_dialog (this, _("CPL's content is not encrypted."));
		} catch (exception& e) {
			error_dialog (this, std_to_wx(e.what()));
		} catch (...) {
			error_dialog (this, _("An unknown exception occurred."));
		}
	}

	void setup_sensitivity ()
	{
		_screens->setup_sensitivity ();
		_output->setup_sensitivity ();
		wxArrayTreeItemIds sel;
		_dkdm->GetSelections (sel);
		auto group = dynamic_pointer_cast<DKDMGroup>(selected_dkdm());
		auto dkdm = dynamic_pointer_cast<DKDM>(selected_dkdm());
		_create->Enable(!_screens->screens().empty() && _timing->valid() && sel.GetCount() > 0 && dkdm && _output->method_selected());
		_remove_dkdm->Enable (sel.GetCount() > 0 && (!group || group->name() != "root"));
		_export_dkdm->Enable (sel.GetCount() > 0 && dkdm);
	}

	void dkdm_selection_changed()
	{
		_selected_dkdm = selected_dkdm();
		if (_selected_dkdm) {
			auto dkdm = std::dynamic_pointer_cast<DKDM>(_selected_dkdm);
			if (dkdm) {
				try {
					dcp::DecryptedKDM decrypted(dkdm->dkdm(), Config::instance()->decryption_chain()->key().get());
					if (decrypted.annotation_text()) {
						_output->set_annotation_text(*decrypted.annotation_text());
					}
				} catch (...) {}
			}
		}
		setup_sensitivity();
	}

	void dkdm_expanded(wxTreeEvent& ev)
	{
		if (_ignore_expand) {
			return;
		}

		auto iter = _dkdm_id.find(ev.GetItem());
		if (iter != _dkdm_id.end()) {
			_expanded_dkdm_groups.insert(iter->second);
		}
	}

	void dkdm_collapsed(wxTreeEvent& ev)
	{
		auto iter = _dkdm_id.find(ev.GetItem());
		if (iter != _dkdm_id.end()) {
			_expanded_dkdm_groups.erase(iter->second);
		}
	}

	void dkdm_begin_drag (wxTreeEvent& ev)
	{
		ev.Allow ();
	}

	void dkdm_end_drag (wxTreeEvent& ev)
	{
		auto from = _dkdm_id.find (_dkdm->GetSelection ());
		auto to = _dkdm_id.find (ev.GetItem ());
		if (from == _dkdm_id.end() || to == _dkdm_id.end() || from->first == to->first) {
			return;
		}

		auto group = dynamic_pointer_cast<DKDMGroup> (to->second);
		if (!group) {
			group = to->second->parent();
		}

		/* Check we're not adding a group to one of its children */
		auto to_parent = group;
		while (to_parent) {
			if (from->second == to_parent) {
				return;
			}
			to_parent = to_parent->parent();
		}

		DCPOMATIC_ASSERT (group);
		DCPOMATIC_ASSERT (from->second->parent ());

		from->second->parent()->remove (from->second);
		add_dkdm(from->second, group, dynamic_pointer_cast<DKDM>(to->second));

		update_dkdm_view();
	}

	void add_dkdm_clicked ()
	{
		FileDialog dialog(this, _("Select DKDM file"), char_to_wx("XML files|*.xml|All files|*.*"), wxFD_MULTIPLE, "AddDKDMPath");
		if (!dialog.show()) {
			return;
		}

		for (auto path: dialog.paths()) {
			add_dkdm(path);
		}
	}

	void add_dkdm(boost::filesystem::path path)
	{
		auto chain = Config::instance()->decryption_chain();
		DCPOMATIC_ASSERT (chain->key());

		try {
			dcp::EncryptedKDM ekdm(dcp::file_to_string(path, MAX_KDM_SIZE));
			/* Decrypt the DKDM to make sure that we can */
			dcp::DecryptedKDM dkdm(ekdm, chain->key().get());

			auto new_dkdm = make_shared<DKDM>(ekdm);

			if (Config::instance()->dkdms()->contains(new_dkdm->dkdm().id())) {
				error_dialog(
					this,
					wxString::Format(_("DKDM %s is already in the DKDM list and will not be added again."), std_to_wx(new_dkdm->dkdm().id()))
					);
				return;
			}

			auto group = dynamic_pointer_cast<DKDMGroup> (selected_dkdm());
			if (!group) {
				group = Config::instance()->dkdms ();
			}
			add_dkdm(new_dkdm, group);
		} catch (dcp::KDMFormatError& e) {
			error_dialog (
				this,
				_("Could not read file as a KDM.  Perhaps it is badly formatted, or not a KDM at all."),
				std_to_wx(e.what())
				);
			return;
		} catch (dcp::KDMDecryptionError &) {
			error_dialog (
				this,
				_("Could not decrypt the DKDM.  Perhaps it was not created with the correct certificate.")
				);
		} catch (dcp::MiscError& e) {
			error_dialog (
				this,
				_("Could not read file as a KDM.  It is much too large.  Make sure you are loading a DKDM (XML) file."),
				std_to_wx(e.what())
				);
		}

		update_dkdm_view();
	}

	void add_dkdm_folder_clicked ()
	{
		NewDKDMFolderDialog dialog(this);
		if (dialog.ShowModal() != wxID_OK) {
			return;
		}

		auto new_dkdm = make_shared<DKDMGroup>(wx_to_std(dialog.get()));
		auto parent = dynamic_pointer_cast<DKDMGroup>(selected_dkdm());
		if (!parent) {
			parent = Config::instance()->dkdms ();
		}
		add_dkdm(new_dkdm, parent);
		update_dkdm_view();
	}

	void update_dkdm_view()
	{
		_dkdm->DeleteAllItems();
		_dkdm_id.clear();
		add_dkdm_to_view(Config::instance()->dkdms());
		if (_selected_dkdm) {
			auto selection_in_id_map = std::find_if(_dkdm_id.begin(), _dkdm_id.end(), [this](pair<wxTreeItemId, shared_ptr<DKDMBase>> const& entry) {
				return entry.second == _selected_dkdm;
			});
			if (selection_in_id_map != _dkdm_id.end()) {
				_dkdm->SelectItem(selection_in_id_map->first);
			}
		}
	}

	/** @return true if this thing or any of its children match a search string */
	bool matches(shared_ptr<DKDMBase> base, string const& search)
	{
		if (search.empty()) {
			return true;
		}

		auto name = base->name();
		transform(name.begin(), name.end(), name.begin(), ::tolower);
		if (name.find(search) != string::npos) {
			return true;
		}

		auto group = dynamic_pointer_cast<DKDMGroup>(base);
		if (!group) {
			return false;
		}

		auto const children = group->children();
		return std::any_of(children.begin(), children.end(), [this, search](shared_ptr<DKDMBase> child) {
			return matches(child, search);
		});
	}

	/** Add DKDMs to the view that match the current search */
	void add_dkdm_to_view(shared_ptr<DKDMBase> base)
	{
		auto search = wx_to_std(_dkdm_search->GetValue());
		transform(search.begin(), search.end(), search.begin(), ::tolower);

		optional<wxTreeItemId> group_to_expand;

		if (!base->parent()) {
			/* This is the root group */
			_dkdm_id[_dkdm->AddRoot(char_to_wx("root"))] = base;
		} else {
			/* Add base to the view */
			wxTreeItemId added;
			auto parent_id = dkdm_to_id(base->parent());
			added = _dkdm->AppendItem(parent_id, std_to_wx(base->name()));
			/* Expand the group (later) if it matches the search or it was manually expanded */
			if (!search.empty() || _expanded_dkdm_groups.find(base) != _expanded_dkdm_groups.end()) {
				group_to_expand = added;
			}
			_dkdm_id[added] = base;
		}

		/* Add children */
		auto group = dynamic_pointer_cast<DKDMGroup>(base);
		if (group) {
			auto children = group->children();
			children.sort(
				[this](shared_ptr<DKDMBase> a, shared_ptr<DKDMBase> b) { return _collator.compare(a->name(), b->name()) < 0; }
			);

			for (auto i: children) {
				if (matches(i, search)) {
					add_dkdm_to_view(i);
				}
			}
		}

		if (group_to_expand) {
			_ignore_expand = true;
			_dkdm->Expand(*group_to_expand);
			_ignore_expand = false;
		}
	}

	/** @param group Group to add dkdm to */
	void add_dkdm(shared_ptr<DKDMBase> dkdm, shared_ptr<DKDMGroup> group, shared_ptr<DKDM> previous = shared_ptr<DKDM>())
	{
		group->add (dkdm, previous);
		/* We're messing with a Config-owned object here, so tell it that something has changed.
		   This isn't nice.
		*/
		Config::instance()->changed ();
	}

	wxTreeItemId dkdm_to_id (shared_ptr<DKDMBase> dkdm)
	{
		for (auto const& i: _dkdm_id) {
			if (i.second == dkdm) {
				return i.first;
			}
		}
		DCPOMATIC_ASSERT (false);
	}

	void remove_dkdm_clicked ()
	{
		auto removed = selected_dkdm ();
		if (!removed) {
			return;
		}

		if (removed->contains_dkdm()) {
			if (NagDialog::maybe_nag(
				    this, Config::NAG_DELETE_DKDM,
				    _("You are about to remove a DKDM.  This will make it impossible to decrypt the DCP that the DKDM was made for, and it cannot be undone.  "
				      "Are you sure?"),
				    true)) {
				return;
			}
		}

		_dkdm->Delete (dkdm_to_id (removed));
		auto dkdms = Config::instance()->dkdms ();
		dkdms->remove (removed);
		Config::instance()->changed ();
	}

	void export_dkdm_clicked ()
	{
		auto removed = selected_dkdm ();
		if (!removed) {
			return;
		}

		auto dkdm = dynamic_pointer_cast<DKDM>(removed);
		if (!dkdm) {
			return;
		}

		wxFileDialog dialog(
			this, _("Select DKDM File"), wxEmptyString, wxEmptyString, char_to_wx("XML files (*.xml)|*.xml"),
			wxFD_SAVE | wxFD_OVERWRITE_PROMPT
			);

		if (dialog.ShowModal() == wxID_OK) {
			dkdm->dkdm().as_xml(wx_to_std(dialog.GetPath()));
		}
	}

	void dkdm_search_changed()
	{
		update_dkdm_view();
	}

	void screens_changed()
	{
		setup_sensitivity();
	}

	wxPreferencesEditor* _config_dialog;
	ScreensPanel* _screens;
	KDMTimingPanel* _timing;
	wxTreeCtrl* _dkdm;
	wxSearchCtrl* _dkdm_search;
	typedef std::map<wxTreeItemId, std::shared_ptr<DKDMBase>> DKDMMap;
	DKDMMap _dkdm_id;
	/* Keep a separate track of the selected DKDM so that when a search happens, and some things
	 * get removed from the view, we can restore the selection when they are re-added.
	 */
	shared_ptr<DKDMBase> _selected_dkdm;
	/* Keep expanded groups for the same reason */
	unordered_set<shared_ptr<DKDMBase>> _expanded_dkdm_groups;
	/* true if we are "artificially" expanding a group because it contains something found
	 * in a search.
	 */
	bool _ignore_expand = false;
	wxButton* _add_dkdm;
	wxButton* _add_dkdm_folder;
	wxButton* _remove_dkdm;
	wxButton* _export_dkdm;
	wxButton* _create;
	KDMOutputPanel* _output = nullptr;
	JobViewDialog* _job_view;
	Collator _collator;
};


/** @class App
 *  @brief The magic App class for wxWidgets.
 */
class App : public wxApp
{
public:
	App ()
		: wxApp ()
		, _frame (nullptr)
	{}

private:

	bool OnInit () override
	{
		wxSplashScreen* splash;

		try {
			wxInitAllImageHandlers ();

			Config::FailedToLoad.connect(boost::bind(&App::config_failed_to_load, this, _1));
			Config::Warning.connect (boost::bind (&App::config_warning, this, _1));

			splash = maybe_show_splash ();

			SetAppName(variant::wx::dcpomatic_kdm_creator());

			if (!wxApp::OnInit()) {
				return false;
			}

#ifdef DCPOMATIC_LINUX
			unsetenv ("UBUNTU_MENUPROXY");
#endif

#ifdef DCPOMATIC_OSX
			make_foreground_application ();
#endif

			dcpomatic_setup_path_encoding ();

			/* Enable i18n; this will create a Config object
			   to look for a force-configured language.  This Config
			   object will be wrong, however, because dcpomatic_setup
			   hasn't yet been called and there aren't any filters etc.
			   set up yet.
			*/
			dcpomatic::wx::setup_i18n();

			/* Set things up, including filters etc.
			   which will now be internationalised correctly.
			*/
			dcpomatic_setup ();

			/* Force the configuration to be re-loaded correctly next
			   time it is needed.
			*/
			Config::drop ();

			_frame = new DOMFrame(variant::wx::dcpomatic_kdm_creator());
			SetTopWindow (_frame);
			_frame->Maximize ();
			if (splash) {
				splash->Destroy ();
				splash = nullptr;
			}
			_frame->Show ();

			signal_manager = new wxSignalManager (this);
			Bind (wxEVT_IDLE, boost::bind (&App::idle, this));
		}
		catch (exception& e)
		{
			if (splash) {
				splash->Destroy ();
			}
			error_dialog(nullptr, variant::wx::insert_dcpomatic_kdm_creator(_("%s could not start")), std_to_wx(e.what()));
		}

		return true;
	}

	/* An unhandled exception has occurred inside the main event loop */
	bool OnExceptionInMainLoop () override
	{
		try {
			throw;
		} catch (FileError& e) {
			error_dialog (
				nullptr,
				wxString::Format (
					_("An exception occurred: %s (%s)\n\n%s"),
					std_to_wx(e.what()),
					std_to_wx(e.file().string().c_str()),
					wx::report_problem()
					)
				);
		} catch (exception& e) {
			error_dialog (
				nullptr,
				wxString::Format (
					_("An exception occurred: %s.\n\n%s"),
					std_to_wx(e.what()),
					wx::report_problem()
					)
				);
		} catch (...) {
			error_dialog(nullptr, wxString::Format(_("An unknown exception occurred. %s"), wx::report_problem()));
		}

		/* This will terminate the program */
		return false;
	}

	void OnUnhandledException () override
	{
		error_dialog(nullptr, wxString::Format(_("An unknown exception occurred. %s"), wx::report_problem()));
	}

	void idle ()
	{
		signal_manager->ui_idle ();
	}

	void config_failed_to_load(Config::LoadFailure what)
	{
		report_config_load_failure(_frame, what);
	}

	void config_warning (string m)
	{
		message_dialog (_frame, std_to_wx(m));
	}

	DOMFrame* _frame;
};

IMPLEMENT_APP (App)
