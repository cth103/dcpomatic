#include <iostream>
#include <boost/bind.hpp>
#include <wx/graphics.h>
#include "audio_plot.h"
#include "lib/decoder_factory.h"
#include "lib/audio_decoder.h"
#include "lib/audio_analysis.h"
#include "wx/wx_util.h"

using std::cout;
using std::vector;
using std::max;
using std::min;
using boost::bind;
using boost::shared_ptr;

AudioPlot::AudioPlot (wxWindow* parent, shared_ptr<AudioAnalysis> a, int c)
	: wxPanel (parent)
	, _analysis (a)
	, _channel (c)
{
	Connect (wxID_ANY, wxEVT_PAINT, wxPaintEventHandler (AudioPlot::paint), 0, this);

	SetMinSize (wxSize (640, 512));
}

void
AudioPlot::paint (wxPaintEvent &)
{
	wxPaintDC dc (this);
	wxGraphicsContext* gc = wxGraphicsContext::Create (dc);
	if (!gc) {
		return;
	}

	int const width = GetSize().GetWidth();
	float const xs = width / float (_analysis->points (_channel));
	int const height = GetSize().GetHeight ();
	float const ys = height / 60;

	wxGraphicsPath grid = gc->CreatePath ();
	gc->SetFont (gc->CreateFont (*wxSMALL_FONT));
	for (int i = -60; i <= 0; i += 10) {
		int const y = height - (i + 60) * ys;
		grid.MoveToPoint (0, y);
		grid.AddLineToPoint (width, y);
		gc->DrawText (std_to_wx (String::compose ("%1dB", i)), width - 32, y - 12);
	}
	gc->SetPen (*wxLIGHT_GREY_PEN);
	gc->StrokePath (grid);

	wxGraphicsPath path[AudioPoint::COUNT];

	for (int i = 0; i < AudioPoint::COUNT; ++i) {
		path[i] = gc->CreatePath ();
		path[i].MoveToPoint (0, height - (max (_analysis->get_point(_channel, 0)[i], -60.0f) + 60) * ys);
	}

	for (int i = 0; i < _analysis->points(_channel); ++i) {
		for (int j = 0; j < AudioPoint::COUNT; ++j) {
			path[j].AddLineToPoint (i * xs, height - (max (_analysis->get_point(_channel, i)[j], -60.0f) + 60) * ys);
		}
	}

	gc->SetPen (*wxBLUE_PEN);
	gc->StrokePath (path[AudioPoint::RMS]);

	gc->SetPen (*wxRED_PEN);
	gc->StrokePath (path[AudioPoint::PEAK]);

	delete gc;
}
