#include <wx/wx.h>

class Film;
class ThumbPanel;

class FilmViewer
{
public:
	FilmViewer (Film *, wxFrame *);

	void load_thumbnail (int);
	wxPanel* get_widget ();

private:
	Film* _film;

	wxImage* _image;
	wxImage* _scaled_image;
	wxBitmap* _bitmap;
	ThumbPanel* _thumb_panel;
};
