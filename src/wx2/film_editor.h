#include <wx/wx.h>

class Film;

class FilmEditor : public wxPanel
{
public:
	FilmEditor (Film* f, wxFrame *);

private:
	Film* _film;
};
