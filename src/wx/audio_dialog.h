#include <boost/shared_ptr.hpp>
#include <wx/wx.h>

class AudioPlot;
class Film;

class AudioDialog : public wxDialog
{
public:
	AudioDialog (wxWindow *, boost::shared_ptr<Film>);

private:
	AudioPlot* _plot;
};
