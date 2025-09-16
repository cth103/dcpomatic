/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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


#include "dcpomatic_button.h"
#include "file_dialog.h"
#include "verify_dcp_result_panel.h"
#include "wx_util.h"
#include "lib/verify_dcp_job.h"
#include <dcp/html_formatter.h>
#include <dcp/text_formatter.h>
#include <dcp/pdf_formatter.h>
#include <dcp/verify.h>
#include <dcp/verify_report.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/notebook.h>
#include <wx/treectrl.h>
LIBDCP_ENABLE_WARNINGS
#include <fmt/format.h>
#include <boost/algorithm/string.hpp>


using std::list;
using std::map;
using std::shared_ptr;
using std::string;
using std::vector;


VerifyDCPResultPanel::VerifyDCPResultPanel(wxWindow* parent)
	: wxPanel(parent, wxID_ANY)
	, _types{
		dcp::VerificationNote::Type::ERROR,
		dcp::VerificationNote::Type::BV21_ERROR,
		dcp::VerificationNote::Type::WARNING
	}
{
	auto sizer = new wxBoxSizer(wxVERTICAL);
	auto notebook = new wxNotebook(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 400));
	sizer->Add(notebook, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	auto names = map<dcp::VerificationNote::Type, wxString>{
		{ dcp::VerificationNote::Type::ERROR, _("Errors") },
		{ dcp::VerificationNote::Type::BV21_ERROR, _("SMPTE Bv2.1 errors") },
		{ dcp::VerificationNote::Type::WARNING, _("Warnings") }
	};

	for (auto const type: _types) {
		auto panel = new wxPanel(notebook, wxID_ANY);
		_pages[type] = new wxTreeCtrl(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTR_HIDE_ROOT | wxTR_HAS_BUTTONS | wxTR_NO_LINES);
		auto sizer = new wxBoxSizer(wxHORIZONTAL);
		sizer->Add(_pages[type], 1, wxEXPAND);
		panel->SetSizer(sizer);
		notebook->AddPage(panel, names[type]);
	}

	_summary = new wxStaticText(this, wxID_ANY, {});
	sizer->Add(_summary, 0, wxALL, DCPOMATIC_DIALOG_BORDER);

	auto save_sizer = new wxBoxSizer(wxHORIZONTAL);
	_save_text_report = new Button(this, _("Save report as text..."));
	save_sizer->Add(_save_text_report, 0, wxALL, DCPOMATIC_SIZER_GAP);
	_save_html_report = new Button(this, _("Save report as HTML..."));
	save_sizer->Add(_save_html_report, 0, wxALL, DCPOMATIC_SIZER_GAP);
	_save_pdf_report = new Button(this, _("Save report as PDF..."));
	save_sizer->Add(_save_pdf_report, 0, wxALL, DCPOMATIC_SIZER_GAP);
	sizer->Add(save_sizer);

	SetSizer(sizer);
	sizer->Layout();
	sizer->SetSizeHints(this);

	_save_text_report->bind(&VerifyDCPResultPanel::save_text_report, this);
	_save_html_report->bind(&VerifyDCPResultPanel::save_html_report, this);
	_save_pdf_report->bind(&VerifyDCPResultPanel::save_pdf_report, this);

	_save_text_report->Enable(false);
	_save_html_report->Enable(false);
	_save_pdf_report->Enable(false);
}


void
VerifyDCPResultPanel::add(vector<shared_ptr<const VerifyDCPJob>> jobs)
{
	_jobs = jobs;

	for (auto const type: _types) {
		_pages[type]->DeleteAllItems();
		_pages[type]->AddRoot(wxT(""));
	}

	map<dcp::VerificationNote::Type, int> counts;
	for (auto type: _types) {
		counts[type] = 0;
	}

	for (auto job: jobs) {
		auto job_counts = add(job, jobs.size() > 1);
		for (auto const type: _types) {
			counts[type] += job_counts[type];
		}
	}

	wxString summary_text;

	if (counts[dcp::VerificationNote::Type::ERROR] == 1) {
		/// TRANSLATORS: this will be used at the start of a string like "1 error, 2 Bv2.1 errors and 3 warnings."
		summary_text = _("1 error, ");
	} else {
		/// TRANSLATORS: this will be used at the start of a string like "1 error, 2 Bv2.1 errors and 3 warnings."
		summary_text = wxString::Format(_("%d errors, "), counts[dcp::VerificationNote::Type::ERROR]);
	}

	if (counts[dcp::VerificationNote::Type::BV21_ERROR] == 1) {
		/// TRANSLATORS: this will be used in the middle of a string like "1 error, 2 Bv2.1 errors and 3 warnings."
		summary_text += _("1 Bv2.1 error, ");
	} else {
		/// TRANSLATORS: this will be used in the middle of a string like "1 error, 2 Bv2.1 errors and 3 warnings."
		summary_text += wxString::Format(_("%d Bv2.1 errors, "), counts[dcp::VerificationNote::Type::BV21_ERROR]);
	}

	if (counts[dcp::VerificationNote::Type::WARNING] == 1) {
		/// TRANSLATORS: this will be used at the end of a string like "1 error, 2 Bv2.1 errors and 3 warnings."
		summary_text += _("and 1 warning.");
	} else {
		/// TRANSLATORS: this will be used at the end of a string like "1 error, 2 Bv2.1 errors and 3 warnings."
		summary_text += wxString::Format(_("and %d warnings."), counts[dcp::VerificationNote::Type::WARNING]);
	}

	_summary->SetLabel(summary_text);

	_save_text_report->Enable(true);
	_save_html_report->Enable(true);
	_save_pdf_report->Enable(true);

	for (auto type: _types) {
		_pages[type]->ExpandAll();
	}
}


map<dcp::VerificationNote::Type, int>
VerifyDCPResultPanel::add(shared_ptr<const VerifyDCPJob> job, bool many)
{
	map<dcp::VerificationNote::Type, int> counts;
	for (auto type: _types) {
		counts[type] = 0;
	}

	map<dcp::VerificationNote::Type, wxTreeItemId> root;

	for (auto type: _types) {
		root[type] = _pages[type]->GetRootItem();
		if (many) {
			DCPOMATIC_ASSERT(!job->directories().empty());
			root[type] = _pages[type]->AppendItem(root[type], std_to_wx(job->directories()[0].filename().string()));
		}
	}

	int constexpr limit_per_type = 20;

	auto substitute = [](wxString message, dcp::VerificationNote const& note) {
		if (note.reference_hash()) {
			message.Replace(char_to_wx("%reference_hash"), std_to_wx(note.reference_hash().get()));
		}
		if (note.calculated_hash()) {
			message.Replace(char_to_wx("%calculated_hash"), std_to_wx(note.calculated_hash().get()));
		}
		if (note.frame()) {
			message.Replace(char_to_wx("%frame"), std_to_wx(fmt::to_string(note.frame().get())));
			message.Replace(
				char_to_wx("%timecode"),
				std_to_wx(
					dcp::Time(note.frame().get(), note.frame_rate().get(), note.frame_rate().get()).as_string(dcp::Standard::SMPTE)
					));
		}
		if (note.note()) {
			message.Replace(char_to_wx("%n"), std_to_wx(note.note().get()));
		}
		if (note.file()) {
			message.Replace(char_to_wx("%f"), std_to_wx(note.file()->filename().string()));
		}
		if (note.line()) {
			message.Replace(char_to_wx("%l"), std_to_wx(fmt::to_string(note.line().get())));
		}
		if (note.component()) {
			message.Replace(char_to_wx("%component"), std_to_wx(fmt::to_string(note.component().get())));
		}
		if (note.size()) {
			message.Replace(char_to_wx("%size"), std_to_wx(fmt::to_string(note.size().get())));
		}
		if (note.id()) {
			message.Replace(char_to_wx("%id"), std_to_wx(note.id().get()));
		}
		if (note.other_id()) {
			message.Replace(char_to_wx("%other_id"), std_to_wx(note.other_id().get()));
		}
		if (note.cpl_id()) {
			message.Replace(char_to_wx("%cpl"), std_to_wx(note.cpl_id().get()));
		}

		return message;
	};

	auto add_line = [this, &root](dcp::VerificationNote::Type type, wxString message) {
		_pages[type]->AppendItem(root[type], message);
	};

	auto add = [&add_line, &substitute](vector<dcp::VerificationNote> const& notes, wxString message, wxString more_message = {}) {
		for (auto const& note: notes) {
			add_line(note.type(), substitute(message, note));
		}
		if (notes.size() == limit_per_type && !more_message.IsEmpty()) {
			add_line(notes[0].type(), more_message);
		}
	};

	if (job->finished_in_error() && job->error_summary() != "") {
		/* We have an error that did not come from dcp::verify */
		add_line(dcp::VerificationNote::Type::ERROR, std_to_wx(job->error_summary()));
	}

	/* Gather notes by code, discarding more than limit_per_type so we don't get overwhelmed if
	 * every frame of a long DCP has a note.
	 */
	std::map<dcp::VerificationNote::Code, std::vector<dcp::VerificationNote>> notes_by_code;

	for (auto const& note: job->result().notes) {
		counts[note.type()]++;
		auto type_iter = notes_by_code.find(note.code());
		if (type_iter != notes_by_code.end()) {
			if (type_iter->second.size() < limit_per_type) {
				type_iter->second.push_back(note);
			}
		} else {
			notes_by_code[note.code()] = { note };
		}
	}

	for (auto const& i: notes_by_code) {
		switch (i.first) {
		case dcp::VerificationNote::Code::FAILED_READ:
			add(i.second, _("Could not read DCP (%n)"));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_CPL_HASHES:
			add(i.second, _("The hash (%reference_hash) of the CPL %cpl in the PKL does not agree with the CPL file (%calculated_hash).  This probably means that the CPL file is corrupt."));
			break;
		case dcp::VerificationNote::Code::INVALID_PICTURE_FRAME_RATE:
			add(i.second, _("The picture in a reel has a frame rate of %n, which is not valid."));
			break;
		case dcp::VerificationNote::Code::INCORRECT_PICTURE_HASH:
			add(i.second, _("The hash (%calculated_hash) of the picture asset %f does not agree with the PKL file (%reference_hash).  This probably means that the asset file is corrupt."));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_PICTURE_HASHES:
			add(i.second, _("The PKL and CPL hashes disagree for picture asset %f."));
			break;
		case dcp::VerificationNote::Code::INCORRECT_SOUND_HASH:
			add(i.second, _("The hash (%calculated_hash) of the sound asset %f does not agree with the PKL file (%reference_hash).  This probably means that the asset file is corrupt."));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_SOUND_HASHES:
			add(i.second, _("The PKL and CPL hashes disagree for sound asset %f."));
			break;
		case dcp::VerificationNote::Code::EMPTY_ASSET_PATH:
			add(i.second, _("An asset has an empty path in the ASSETMAP."));
			break;
		case dcp::VerificationNote::Code::MISSING_ASSET:
			add(i.second, _("The asset %f is missing."));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_STANDARD:
			add(i.second, _("Parts of the DCP are written according to the Interop standard and parts according to SMPTE."));
			break;
		case dcp::VerificationNote::Code::INVALID_XML:
			for (auto const& note: i.second) {
				if (note.line()) {
					add({ note }, _("The XML in %f is malformed on line %l (%n)."));
				} else {
					add({ note }, _("The XML in %f is malformed (%n)."));
				}
			}
			break;
		case dcp::VerificationNote::Code::MISSING_ASSETMAP:
			add(i.second, _("No ASSETMAP or ASSETMAP.xml file was found."));
			break;
		case dcp::VerificationNote::Code::INVALID_INTRINSIC_DURATION:
			add(i.second, _("The asset %n has an intrinsic duration of less than 1 second, which is invalid."));
			break;
		case dcp::VerificationNote::Code::INVALID_DURATION:
			add(i.second, _("The asset %n has a duration of less than 1 second, which is invalid."));
			break;
		case dcp::VerificationNote::Code::INVALID_PICTURE_FRAME_SIZE_IN_BYTES:
			add(
				i.second,
				_("Frame %frame (timecode %timecode) in asset %f has an instantaneous bit rate that is over the limit of 250Mbit/s."),
				_("More frames (not listed) have an instantaneous bit rate that is over the limit of 250Mbit/s.")
			);
			break;
		case dcp::VerificationNote::Code::NEARLY_INVALID_PICTURE_FRAME_SIZE_IN_BYTES:
			add(
				i.second,
				_("Frame %frame (timecode %timecode) in asset %f has an instantaneous bit rate that is close to the limit of 250Mbit/s."),
				_("More frames (not listed) have an instantaneous bit rate that is close to the limit of 250Mbit/s.")
			);
			break;
		case dcp::VerificationNote::Code::EXTERNAL_ASSET:
			add(i.second, _("This DCP refers to at the asset %n in another DCP (and perhaps others), so it is a \"version file\" (VF)"));
			break;
		case dcp::VerificationNote::Code::THREED_ASSET_MARKED_AS_TWOD:
			add(i.second, _("The asset %f is 3D but its MXF is marked as 2D."));
			break;
		case dcp::VerificationNote::Code::INVALID_STANDARD:
			add(i.second, _("This DCP uses the Interop standard, but it should be made with SMPTE."));
			break;
		case dcp::VerificationNote::Code::INVALID_LANGUAGE:
			add(i.second, _("The invalid language tag %n is used."));
			break;
		case dcp::VerificationNote::Code::INVALID_PICTURE_SIZE_IN_PIXELS:
			add(i.second, _("The video asset %f uses the invalid image size %n."));
			break;
		case dcp::VerificationNote::Code::INVALID_PICTURE_FRAME_RATE_FOR_2K:
			add(i.second, _("The video asset %f uses the invalid frame rate %n."));
			break;
		case dcp::VerificationNote::Code::INVALID_PICTURE_FRAME_RATE_FOR_4K:
			add(i.second, _("The video asset %f uses the frame rate %n which is invalid for 4K video."));
			break;
		case dcp::VerificationNote::Code::INVALID_PICTURE_ASSET_RESOLUTION_FOR_3D:
			add(i.second, _("The video asset %f uses the frame rate %n which is invalid for 3D video."));
			break;
		case dcp::VerificationNote::Code::INVALID_CLOSED_CAPTION_XML_SIZE_IN_BYTES:
			add(i.second, _("The XML in the closed caption asset %f takes up %n bytes which is over the 256KB limit."));
			break;
		case dcp::VerificationNote::Code::INVALID_TIMED_TEXT_SIZE_IN_BYTES:
			add(i.second, _("The timed text asset %f takes up %n bytes which is over the 115MB limit."));
			break;
		case dcp::VerificationNote::Code::INVALID_TIMED_TEXT_FONT_SIZE_IN_BYTES:
			add(i.second, _("The fonts in the timed text asset %f take up %n bytes which is over the 10MB limit."));
			break;
		case dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE:
			add(i.second, _("The subtitle asset %f contains no <Language> tag."));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_SUBTITLE_LANGUAGES:
			add(i.second, _("Not all subtitle assets specify the same <Language> tag."));
			break;
		case dcp::VerificationNote::Code::MISSING_SUBTITLE_START_TIME:
			add(i.second, _("The subtitle asset %f contains no <StartTime> tag."));
			break;
		case dcp::VerificationNote::Code::INVALID_SUBTITLE_START_TIME:
			add(i.second, _("The subtitle asset %f has a <StartTime> which is not zero."));
			break;
		case dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME:
			add(i.second, _("The first subtitle or closed caption happens before 4s into the first reel."));
			break;
		case dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION:
			add(i.second, _("At least one subtitle has zero or negative duration."));
			break;
		case dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION_BV21:
			add(i.second, _("At least one subtitle lasts less than 15 frames."));
			break;
		case dcp::VerificationNote::Code::INVALID_SUBTITLE_SPACING:
			add(i.second, _("At least one pair of subtitles is separated by less than 2 frames."));
			break;
		case dcp::VerificationNote::Code::INVALID_SUBTITLE_LINE_COUNT:
			add(i.second, _("There are more than 3 subtitle lines in at least one place."));
			break;
		case dcp::VerificationNote::Code::NEARLY_INVALID_SUBTITLE_LINE_LENGTH:
			add(i.second, _("There are more than 52 characters in at least one subtitle line."));
			break;
		case dcp::VerificationNote::Code::INVALID_SUBTITLE_LINE_LENGTH:
			add(i.second, _("There are more than 79 characters in at least one subtitle line."));
			break;
		case dcp::VerificationNote::Code::INVALID_CLOSED_CAPTION_LINE_COUNT:
			add(i.second, _("There are more than 3 closed caption lines in at least one place."));
			break;
		case dcp::VerificationNote::Code::INVALID_CLOSED_CAPTION_LINE_LENGTH:
			add(i.second, _("There are more than 32 characters in at least one closed caption line."));
			break;
		case dcp::VerificationNote::Code::INVALID_SOUND_FRAME_RATE:
			add(i.second, _("The sound asset %f has an invalid frame rate of %n."));
			break;
		case dcp::VerificationNote::Code::INVALID_SOUND_BIT_DEPTH:
			add(i.second, _("The sound asset %f has an invalid bit depth of %n."));
			break;
		case dcp::VerificationNote::Code::MISSING_CPL_ANNOTATION_TEXT:
			add(i.second, _("The CPL %cpl has no <AnnotationText> tag."));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_CPL_ANNOTATION_TEXT:
			add(i.second, _("The CPL %cpl has an <AnnotationText> which is not the same as its <ContentTitleText>."));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_ASSET_DURATION:
			add(i.second, _("At least one asset in a reel does not have the same duration as the others."));
			break;
		case dcp::VerificationNote::Code::MISSING_MAIN_SUBTITLE_FROM_SOME_REELS:
			add(i.second, _("The DCP has subtitles but at least one reel has no subtitle asset."));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_CLOSED_CAPTION_ASSET_COUNTS:
			add(i.second, _("The DCP has closed captions but not every reel has the same number of closed caption assets."));
			break;
		case dcp::VerificationNote::Code::MISSING_SUBTITLE_ENTRY_POINT:
			add(i.second, _("The subtitle asset %n has no <EntryPoint> tag."));
			break;
		case dcp::VerificationNote::Code::INCORRECT_SUBTITLE_ENTRY_POINT:
			add(i.second, _("Subtitle asset %n has a non-zero <EntryPoint>."));
			break;
		case dcp::VerificationNote::Code::MISSING_CLOSED_CAPTION_ENTRY_POINT:
			add(i.second, _("The closed caption asset %n has no <EntryPoint> tag."));
			break;
		case dcp::VerificationNote::Code::INCORRECT_CLOSED_CAPTION_ENTRY_POINT:
			add(i.second, _("Closed caption asset %n has a non-zero <EntryPoint>."));
			break;
		case dcp::VerificationNote::Code::MISSING_HASH:
			add(i.second, _("The asset %n has no <Hash> in the CPL."));
			break;
		case dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE:
			add(i.second, _("The DCP is a feature but has no FFEC (first frame of end credits) marker."));
			break;
		case dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE:
			add(i.second, _("The DCP is a feature but has no FFMC (first frame of moving credits) marker."));
			break;
		case dcp::VerificationNote::Code::MISSING_FFOC:
			add(i.second, _("The DCP has no FFOC (first frame of content) marker."));
			break;
		case dcp::VerificationNote::Code::MISSING_LFOC:
			add(i.second, _("The DCP has no LFOC (last frame of content) marker."));
			break;
		case dcp::VerificationNote::Code::INCORRECT_FFOC:
			add(i.second, _("The DCP has a FFOC of %n instead of 1."));
			break;
		case dcp::VerificationNote::Code::INCORRECT_LFOC:
			add(i.second, _("The DCP has a LFOC of %n instead of the reel duration minus one."));
			break;
		case dcp::VerificationNote::Code::MISSING_CPL_METADATA:
			add(i.second, _("The CPL %cpl has no CPL metadata tag."));
			break;
		case dcp::VerificationNote::Code::MISSING_CPL_METADATA_VERSION_NUMBER:
			add(i.second, _("The CPL %cpl has no CPL metadata version number tag."));
			break;
		case dcp::VerificationNote::Code::MISSING_EXTENSION_METADATA:
			add(i.second, _("The CPL %cpl has no CPL extension metadata tag."));
			break;
		case dcp::VerificationNote::Code::INVALID_EXTENSION_METADATA:
			add(i.second, _("The CPL %f has an invalid CPL extension metadata tag (%n)"));
			break;
		case dcp::VerificationNote::Code::UNSIGNED_CPL_WITH_ENCRYPTED_CONTENT:
			add(i.second, _("The CPL %cpl has encrypted content but is not signed."));
			break;
		case dcp::VerificationNote::Code::UNSIGNED_PKL_WITH_ENCRYPTED_CONTENT:
			add(i.second, _("The PKL %n has encrypted content but is not signed."));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_PKL_ANNOTATION_TEXT_WITH_CPL:
			add(i.second, _("The PKL %n has an <AnnotationText> which does not match its CPL's <ContentTitleText>."));
			break;
		case dcp::VerificationNote::Code::PARTIALLY_ENCRYPTED:
			add(i.second, _("The DCP has encrypted content, but not all its assets are encrypted."));
			break;
		case dcp::VerificationNote::Code::INVALID_JPEG2000_CODESTREAM:
			add(
				i.second,
				_("A picture frame has an invalid JPEG2000 codestream (%n)."),
				_("More picture frames (not listed) have invalid JPEG2000 codestreams.")
			);
			break;
		case dcp::VerificationNote::Code::INVALID_JPEG2000_GUARD_BITS_FOR_2K:
			add(
				i.second,
				_("A 2K JPEG2000 frame has %n guard bits instead of 1."),
				_("More 2K JPEG2000 frames (not listed) have an invalid number of guard bits.")
			);
			break;
		case dcp::VerificationNote::Code::INVALID_JPEG2000_GUARD_BITS_FOR_4K:
			add(
				i.second,
				_("A 4K JPEG2000 frame has %n guard bits instead of 2."),
				_("More 4K JPEG2000 frames (not listed) have an invalid number of guard bits.")
			);
			break;
		case dcp::VerificationNote::Code::INVALID_JPEG2000_TILE_SIZE:
			add(
				i.second,
				_("A JPEG2000 tile size does not match the image size."),
				_("More JPEG2000 tile sizes (not listed) do not match the image size.")
			);
			break;
		case dcp::VerificationNote::Code::INVALID_JPEG2000_CODE_BLOCK_WIDTH:
			add(
				i.second,
				_("A JPEG2000 frame has a code-block width of %n instead of 32."),
				_("More JPEG2000 frames (not listed) have an invalid code-block width.")
			);
			break;
		case dcp::VerificationNote::Code::INVALID_JPEG2000_CODE_BLOCK_HEIGHT:
			add(
				i.second,
				_("A JPEG2000 frame has a code-block height of %n instead of 32."),
				_("More JPEG2000 frames (not listed) have an invalid code-block height.")
			);
			break;
		case dcp::VerificationNote::Code::INCORRECT_JPEG2000_POC_MARKER_COUNT_FOR_2K:
			add(
				i.second,
				_("A 2K JPEG2000 frame has %n POC marker(s) instead of 0."),
				_("More 2K JPEG2000 frames (not listed) have too many POC markers.")
			);
			break;
		case dcp::VerificationNote::Code::INCORRECT_JPEG2000_POC_MARKER_COUNT_FOR_4K:
			add(
				i.second,
				_("A 4K JPEG2000 frame has %n POC marker(s) instead of 1."),
				_("More 4K JPEG2000 frames (not listed) have too many POC markers.")
			);
			break;
		case dcp::VerificationNote::Code::INCORRECT_JPEG2000_POC_MARKER:
			add(
				i.second,
				_("A JPEG2000 frame contains an invalid POC marker (%n)."),
				_("More JPEG2000 frames (not listed) contain invalid POC markers.")
			);
			break;
		case dcp::VerificationNote::Code::INVALID_JPEG2000_POC_MARKER_LOCATION:
			add(
				i.second,
				_("A JPEG2000 frame contains a POC marker in an invalid location."),
				_("More JPEG2000 frames (not listed) contain POC markers in invalid locations.")
			);
			break;
		case dcp::VerificationNote::Code::INVALID_JPEG2000_TILE_PARTS_FOR_2K:
			add(
				i.second,
				_("A 2K JPEG2000 frame contains %n tile parts instead of 3."),
				_("More 2K JPEG2000 frames (not listed) contain the wrong number of tile parts.")
			);
			break;
		case dcp::VerificationNote::Code::INVALID_JPEG2000_TILE_PARTS_FOR_4K:
			add(
				i.second,
				_("A 4K JPEG2000 frame contains %n tile parts instead of 6."),
				_("More JPEG2000 frames (not listed) contain the wrong number of tile parts.")
			);
			break;
		case dcp::VerificationNote::Code::INVALID_JPEG2000_RSIZ_FOR_2K:
			add(
				i.second,
				_("A 2K JPEG2000 frame contains an invalid Rsiz (capabilities) value of %n"),
				_("More JPEG2000 frames (not listed) contain invalid Rsiz values.")
			);
			break;
		case dcp::VerificationNote::Code::INVALID_JPEG2000_RSIZ_FOR_4K:
			add(
				i.second,
				_("A 4K JPEG2000 frame contains an invalid Rsiz (capabilities) value of %n"),
				_("More JPEG2000 frames (not listed) contain invalid Rsiz values.")
			);
			break;
		case dcp::VerificationNote::Code::MISSING_JPEG200_TLM_MARKER:
			add(
				i.second,
				_("A JPEG2000 frame has no TLM marker."),
				_("More JPEG2000 frames (not listed) have no TLM marker.")
			);
			break;
		case dcp::VerificationNote::Code::SUBTITLE_OVERLAPS_REEL_BOUNDARY:
			add(i.second, _("A subtitle lasts longer than the reel it is in."));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_TIMED_TEXT_RESOURCE_ID:
			add(i.second, _("The Resource ID in a timed text MXF did not match the ID of the contained XML."));
			break;
		case dcp::VerificationNote::Code::INCORRECT_TIMED_TEXT_ASSET_ID:
			add(i.second, _("The Asset ID in a timed text MXF is the same as the Resource ID or that of the contained XML."));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_TIMED_TEXT_DURATION:
		{
			for (auto const& note: i.second) {
				vector<string> parts;
				boost::split(parts, note.note().get(), boost::is_any_of(" "));
				add(
					{ note },
					wxString::Format(
						_("The reel duration (%s) of some timed text is not the same as the ContainerDuration (%s) of its MXF."),
						std_to_wx(parts[0]),
						std_to_wx(parts[1])
						)
				   );
			}
			break;
		}
		case dcp::VerificationNote::Code::MISSED_CHECK_OF_ENCRYPTED:
			add(i.second, _("Part of the DCP could not be checked because no KDM was available."));
			break;
		case dcp::VerificationNote::Code::EMPTY_TEXT:
			add(i.second, _("At least one <Text> node in a subtitle or closed caption is empty."));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_CLOSED_CAPTION_VALIGN:
			add(i.second, _("Some closed <Text> or <Image> nodes have different vertical alignments within a <Subtitle>."));
			break;
		case dcp::VerificationNote::Code::INCORRECT_CLOSED_CAPTION_ORDERING:
			add(i.second, _("Some closed captions are not listed in the order of their vertical position."));
			break;
		case dcp::VerificationNote::Code::UNEXPECTED_ENTRY_POINT:
			add(i.second, _("There is a <EntryPoint> tag inside a <MainMarkers>."));
			break;
		case dcp::VerificationNote::Code::UNEXPECTED_DURATION:
			add(i.second, _("There is a <Duration> tag inside a <MainMarkers>."));
			break;
		case dcp::VerificationNote::Code::INVALID_CONTENT_KIND:
			add(i.second, _("An invalid <ContentKind> %n has been used."));
			break;
		case dcp::VerificationNote::Code::INVALID_MAIN_PICTURE_ACTIVE_AREA:
			add(i.second, _("The <MainPictureActiveArea> is either not a multiple of 2, or is bigger than an asset."));
			break;
		case dcp::VerificationNote::Code::DUPLICATE_ASSET_ID_IN_PKL:
			add(i.second, _("The PKL %n has more than one asset with the same ID."));
			break;
		case dcp::VerificationNote::Code::DUPLICATE_ASSET_ID_IN_ASSETMAP:
			add(i.second, _("The ASSETMAP %n has more than one asset with the same ID."));
			break;
		case dcp::VerificationNote::Code::MISSING_SUBTITLE:
			add(i.second, _("The subtitle asset %n contains no subtitles."));
			break;
		case dcp::VerificationNote::Code::INVALID_SUBTITLE_ISSUE_DATE:
			add(i.second, _("<IssueDate> has an invalid value %n"));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_SOUND_CHANNEL_COUNTS:
			add(i.second, _("Sound assets do not all have the same channel count."));
			break;
		case dcp::VerificationNote::Code::INVALID_MAIN_SOUND_CONFIGURATION:
			add(i.second, _("<MainSoundConfiguration> is invalid (%n)"));
			break;
		case dcp::VerificationNote::Code::MISSING_FONT:
			add(i.second, _("The font file for font ID \"%n\" was not found, or was not referred to in the ASSETMAP."));
			break;
		case dcp::VerificationNote::Code::INVALID_JPEG2000_TILE_PART_SIZE:
			add(
				i.second,
				_("Frame %frame has an image component that is too large (component %component is %size bytes in size)."),
				_("More frames (not listed) have image components that are too large.")
			);
			break;
		case dcp::VerificationNote::Code::INCORRECT_SUBTITLE_NAMESPACE_COUNT:
			add(i.second, _("The XML in the subtitle asset %n has more than one namespace declaration."));
			break;
		case dcp::VerificationNote::Code::MISSING_LOAD_FONT_FOR_FONT:
			add(i.second, _("A subtitle or closed caption refers to a font with ID %id that does not have a corresponding <LoadFont> node."));
			break;
		case dcp::VerificationNote::Code::MISSING_LOAD_FONT:
			add(i.second, _("The SMPTE subtitle asset %id has <Text> nodes but no <LoadFont> node"));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_ASSET_MAP_ID:
			add(i.second, _("The asset with ID %id in the asset map actually has an id of %other_id"));
			break;
		case dcp::VerificationNote::Code::EMPTY_CONTENT_VERSION_LABEL_TEXT:
			add(i.second, _("The <LabelText> in a <ContentVersion> in CPL %cpl is empty"));
			break;
		case dcp::VerificationNote::Code::INVALID_CPL_NAMESPACE:
			add(i.second, _("The CPL %cpl has an invalid namespace %n"));
			break;
		case dcp::VerificationNote::Code::MISSING_CPL_CONTENT_VERSION:
			add(i.second, _("The CPL %cpl has no <ContentVersion> tag"));
			break;
		case dcp::VerificationNote::Code::MATCHING_CPL_HASHES:
		case dcp::VerificationNote::Code::CORRECT_PICTURE_HASH:
		case dcp::VerificationNote::Code::VALID_PICTURE_FRAME_SIZES_IN_BYTES:
		case dcp::VerificationNote::Code::VALID_RELEASE_TERRITORY:
		case dcp::VerificationNote::Code::VALID_CPL_ANNOTATION_TEXT:
		case dcp::VerificationNote::Code::MATCHING_PKL_ANNOTATION_TEXT_WITH_CPL:
		case dcp::VerificationNote::Code::ALL_ENCRYPTED:
		case dcp::VerificationNote::Code::NONE_ENCRYPTED:
		case dcp::VerificationNote::Code::VALID_CONTENT_KIND:
		case dcp::VerificationNote::Code::VALID_MAIN_PICTURE_ACTIVE_AREA:
		case dcp::VerificationNote::Code::VALID_CONTENT_VERSION_LABEL_TEXT:
			/* These are all "OK" messages which we don't report here */
			break;
		case dcp::VerificationNote::Code::INVALID_PKL_NAMESPACE:
			add(i.second, _("The PKL %f has an invalid namespace %n"));
			break;
		}
	}

	if (counts[dcp::VerificationNote::Type::ERROR] == 0) {
		add_line(dcp::VerificationNote::Type::ERROR, _("No errors found."));
	}

	if (counts[dcp::VerificationNote::Type::BV21_ERROR] == 0) {
		add_line(dcp::VerificationNote::Type::BV21_ERROR, _("No SMPTE Bv2.1 errors found."));
	}

	if (counts[dcp::VerificationNote::Type::WARNING] == 0) {
		add_line(dcp::VerificationNote::Type::WARNING, _("No warnings found."));
	}

	return counts;
}


template <class T>
void save(wxWindow* parent, wxString filter, vector<shared_ptr<const VerifyDCPJob>> jobs)
{
	FileDialog dialog(parent, _("Verification report"), filter, wxFD_SAVE | wxFD_OVERWRITE_PROMPT, "SaveVerificationReport");
	if (!dialog.show()) {
		return;
	}

	T formatter(dialog.path());
	auto results = std::vector<dcp::VerificationResult>();
	for (auto job: jobs) {
		results.push_back(job->result());
	}
	dcp::verify_report(results, formatter);
}


void
VerifyDCPResultPanel::save_text_report()
{
	save<dcp::TextFormatter>(this, char_to_wx("Text files (*.txt)|*.txt"), _jobs);
}


void
VerifyDCPResultPanel::save_html_report()
{
	save<dcp::HTMLFormatter>(this, char_to_wx("HTML files (*.htm;*html)|*.htm;*.html"), _jobs);
}


void
VerifyDCPResultPanel::save_pdf_report()
{
	save<dcp::PDFFormatter>(this, char_to_wx("PDF files (*.pdf)|*.pdf"), _jobs);
}
