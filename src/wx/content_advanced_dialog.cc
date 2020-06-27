/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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

#include "content_advanced_dialog.h"
#include "wx_util.h"
#include "lib/content.h"
#include "lib/video_content.h"
#include <wx/gbsizer.h>
#include <boost/bind.hpp>

using boost::bind;
using boost::shared_ptr;

ContentAdvancedDialog::ContentAdvancedDialog (wxWindow* parent, shared_ptr<Content> content)
	: wxDialog (parent, wxID_ANY, _("Advanced content settings"))
	, _content (content)
{
	wxGridBagSizer* sizer = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);

	int r = 0;
	wxCheckBox* ignore_video = new wxCheckBox (this, wxID_ANY, _("Ignore this content's video and use only audio, subtitles and closed captions"));
	sizer->Add (ignore_video, wxGBPosition(r, 0), wxGBSpan(1, 2));
	++r;

	wxSizer* overall = new wxBoxSizer (wxVERTICAL);
	overall->Add (sizer, 1, wxALL, DCPOMATIC_DIALOG_BORDER);
	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		overall->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizerAndFit (overall);

	ignore_video->Enable (static_cast<bool>(_content->video));
	ignore_video->SetValue (_content->video ? !content->video->use() : false);

	ignore_video->Bind (wxEVT_CHECKBOX, bind(&ContentAdvancedDialog::ignore_video_changed, this, _1));
}


void
ContentAdvancedDialog::ignore_video_changed (wxCommandEvent& ev)
{
	 if (_content->video) {
		 _content->video->set_use (!ev.IsChecked());
	 }
}


