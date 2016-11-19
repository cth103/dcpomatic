#include <wx/wx.h>
#include <wx/dataview.h>
#include <boost/bind.hpp>
#include <vector>

using std::cout;

class Model : public wxDataViewModel
{
public:
	Model ()
	{
		_content.push_back ("cock");
		_content.push_back ("piss");
		_content.push_back ("partridge");

		update ("");
	}

	unsigned int GetColumnCount () const {
		return 1;
	}

	wxString GetColumnType (unsigned int) const {
		return "string";
	}

	void GetValue (wxVariant& val, const wxDataViewItem& item, unsigned int column) const {
		val = wxVariant (_content[((size_t) item.GetID()) - 1]);
	}

	bool SetValue (const wxVariant &, const wxDataViewItem &, unsigned int) {
		return true;
	}

        wxDataViewItem GetParent(const wxDataViewItem& item) const {
		return wxDataViewItem (0);
        }

        bool IsContainer(const wxDataViewItem &) const {
		return false;
        }

        unsigned GetChildren(const wxDataViewItem& item, wxDataViewItemArray& children) const {
		if (item.IsOk ()) {
			return 0;
		}

		for (int i = 0; i < _data.GetCount(); ++i) {
			children.Add (_data[i]);
		}

		return _data.GetCount ();
        }

	void set_search (wxTextCtrl* search)
	{
		Cleared ();
		update (search->GetValue ());
	}

	void update (wxString search)
	{
		_data.Clear ();
		Cleared ();

		for (size_t i = 0; i < _content.size(); ++i) {
			if (search.IsEmpty() || _content[i].Find(search) != wxNOT_FOUND) {
				_data.Add (wxDataViewItem ((void *) (i + 1)));
			}
		}

		ItemsAdded (wxDataViewItem (0), _data);
	}

private:
	std::vector<wxString> _content;
	wxDataViewItemArray _data;
};

class App : public wxApp
{
public:
        bool OnInit () {
		if (!wxApp::OnInit()) {
			return false;
		}

		wxFrame* frame = new wxFrame (0, wxID_ANY, "Test");

		wxDataViewCtrl* ctrl = new wxDataViewCtrl (frame, wxID_ANY, wxDefaultPosition, wxSize (300, 600), wxDV_NO_HEADER);
		wxDataViewTextRenderer* renderer = new wxDataViewTextRenderer ("string", wxDATAVIEW_CELL_INERT);
		wxDataViewColumn* column = new wxDataViewColumn ("string", renderer, 0, 100, wxAlignment(wxALIGN_LEFT));
		Model* model = new Model;
		ctrl->AssociateModel (model);
		ctrl->AppendColumn (column);
		ctrl->SetExpanderColumn (column);

		wxTextCtrl* search = new wxTextCtrl (frame, wxID_ANY);
		search->Bind (wxEVT_TEXT, boost::bind (&Model::set_search, model, search));

		wxSizer* sizer = new wxBoxSizer (wxVERTICAL);
		sizer->Add (search);
		sizer->Add (ctrl);
		frame->SetSizerAndFit (sizer);
		frame->Show (true);
		return true;
        }
};

IMPLEMENT_APP(App)
