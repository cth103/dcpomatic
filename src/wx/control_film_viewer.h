#include "lib/dcpomatic_time.h"
#include "lib/types.h"
#include "lib/film.h"
#include <wx/wx.h>
#include <boost/shared_ptr.hpp>
#include <boost/signals2.hpp>

class FilmViewer;
class Film;
class ClosedCaptionsDialog;
class Content;
class PlayerVideo;
class wxToggleButton;

class Controls : public wxPanel
{
public:
	Controls (wxWindow* parent, boost::shared_ptr<FilmViewer>, bool outline_content = true, bool jump_to_selected = true);

	boost::shared_ptr<Film> film () const;
	void back_frame ();
	void forward_frame ();

private:
	void update_position_label ();
	void update_position_slider ();
	void rewind_clicked (wxMouseEvent &);
	void back_clicked (wxKeyboardState& s);
	void forward_clicked (wxKeyboardState &);
	void slider_moved (bool page);
	void slider_released ();
	void play_clicked ();
	void frame_number_clicked ();
	void jump_to_selected_clicked ();
	void setup_sensitivity ();
	void timecode_clicked ();
	void check_play_state ();
	void active_jobs_changed (boost::optional<std::string>);
	DCPTime nudge_amount (wxKeyboardState& ev);
	void image_changed (boost::weak_ptr<PlayerVideo>);
	void film_change (ChangeType type, Film::Property p);
	void outline_content_changed ();
	void eye_changed ();
	void position_changed ();
	void started ();
	void stopped ();
	void film_changed ();

	boost::shared_ptr<Film> _film;
	boost::shared_ptr<FilmViewer> _viewer;

	wxSizer* _v_sizer;
	bool _slider_being_moved;
	bool _was_running_before_slider;

	wxCheckBox* _outline_content;
	wxChoice* _eye;
	wxCheckBox* _jump_to_selected;
	wxSlider* _slider;
	wxButton* _rewind_button;
	wxButton* _back_button;
	wxButton* _forward_button;
	wxStaticText* _frame_number;
	wxStaticText* _timecode;
	wxToggleButton* _play_button;

	ClosedCaptionsDialog* _closed_captions_dialog;
};
