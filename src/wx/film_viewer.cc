#include <iostream>
#include "lib/film.h"
#include "film_viewer.h"

using namespace std;

class ThumbPanel : public wxPanel
{
public:
	ThumbPanel (wxFrame* parent)
		: wxPanel (parent)
		, _bitmap (0)
	{
	}

	void paint_event (wxPaintEvent& ev)
	{
		if (!_bitmap) {
			return;
		}

		cout << "RENDER\n";
		
		wxPaintDC dc (this);
		dc.DrawBitmap (*_bitmap, 0, 0, false);
	}

	void set_bitmap (wxBitmap* bitmap)
	{
		_bitmap = bitmap;
	}

	DECLARE_EVENT_TABLE ();

private:
	wxBitmap* _bitmap;
};

BEGIN_EVENT_TABLE (ThumbPanel, wxPanel)
EVT_PAINT (ThumbPanel::paint_event)
END_EVENT_TABLE ()

FilmViewer::FilmViewer (Film* f, wxFrame* p)
	: _film (f)
	, _image (0)
	, _scaled_image (0)
	, _bitmap (0)
{
	_thumb_panel = new ThumbPanel (p);
	_thumb_panel->Show (true);
	int x, y;
	_thumb_panel->GetSize (&x, &y);
	cout << x << " " << y << "\n";
}

void
FilmViewer::load_thumbnail (int n)
{
	if (_film == 0 && _film->num_thumbs() <= n) {
		return;
	}

	_image = new wxImage (wxString (_film->thumb_file(n).c_str (), wxConvUTF8));
	_scaled_image = new wxImage (_image->Scale (512, 512));
	_bitmap = new wxBitmap (*_scaled_image);
	_thumb_panel->set_bitmap (_bitmap);
}

wxPanel *
FilmViewer::get_widget ()
{
	return _thumb_panel;
}
