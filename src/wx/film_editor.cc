#include "film_editor.h"

FilmEditor::FilmEditor (Film* f, wxFrame* p)
	: wxPanel (p)
	, _film (f)
{
	new wxButton (this, 0, wxT("FUCK"));
}
