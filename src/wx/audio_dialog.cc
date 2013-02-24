#include "audio_dialog.h"
#include "audio_plot.h"
#include "audio_analysis.h"
#include "film.h"

using boost::shared_ptr;

AudioDialog::AudioDialog (wxWindow* parent, boost::shared_ptr<Film> film)
	: wxDialog (parent, wxID_ANY, _("Audio"), wxDefaultPosition, wxSize (640, 512), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	wxBoxSizer* sizer = new wxBoxSizer (wxVERTICAL);

	shared_ptr<AudioAnalysis> a;
	
	try {
		a.reset (new AudioAnalysis (film->audio_analysis_path ()));
		_plot = new AudioPlot (this, a, 0);
		sizer->Add (_plot, 1);
	} catch (...) {
		
	}

	SetSizer (sizer);
	sizer->Layout ();
	sizer->SetSizeHints (this);
}

