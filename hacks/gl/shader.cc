#include <wx/wx.h>
#include <wx/glcanvas.h>



class GLCanvas
{
public:
	GLCanvas (wxFrame* parent)
	{
		_canvas = new wxGLCanvas(parent);
		Bind(wxEVT_PAINT, bind(&GLCanvas::paint, this);
	}

	wxWindow* get()
	{
		return _canvas;
	}

	void paint()
	{

	}


private:
	wxGLCanvas* _canvas;
};



class MyApp : public wxApp
{
public:
	bool OnInit()
	{
		auto sizer = new wxBoxSizer(wxHORIZONTAL);
		_frame = new wxFrame(nullptr, wxID_ANY, wxT("Hello world"));
		_canvas = new GLCanvas(_frame);
		sizer->Add(_canvas->get(), 1, wxEXPAND);
		_frame->SetSizerAndFit(sizer);
		_frame->Show();
		return true;
	}

private:
	wxFrame* _frame;
	GLCanvas* _canvas;
};

