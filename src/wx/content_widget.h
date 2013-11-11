/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef DCPOMATIC_MULTIPLE_WIDGET_H
#define DCPOMATIC_MULTIPLE_WIDGET_H

#include <vector>
#include <wx/wx.h>
#include <wx/gbsizer.h>
#include <boost/function.hpp>
#include "wx_util.h"

/** A widget which represents some Content state and which can be used
 *  when multiple pieces of content are selected.
 *
 *  @param S Type containing the content being represented (e.g. VideoContent)
 *  @param T Type of the widget (e.g. wxSpinCtrl)
 */
template <class S, class T>
class ContentWidget
{
public:
	/** @param parent Parent window.
	 *  @param wrapped Control widget that we are wrapping.
	 *  @param property ContentProperty that the widget is handling.
	 *  @param getter Function on the Content to get the value.
	 *  @param setter Function on the Content to set the value.
	 */
	ContentWidget (wxWindow* parent, T* wrapped, int property, boost::function<int (S*)> getter, boost::function<void (S*, int)> setter)
		: _wrapped (wrapped)
		, _sizer (0)
		, _button (new wxButton (parent, wxID_ANY, _("Multiple values")))
		, _property (property)
		, _getter (getter)
		, _setter (setter)
		, _ignore_model_changes (false)
	{
		_button->SetToolTip (_("Click the button to set all selected content to the same value."));
		_button->Hide ();
		_button->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&ContentWidget::button_clicked, this));
		_wrapped->Bind (wxEVT_COMMAND_SPINCTRL_UPDATED, boost::bind (&ContentWidget::view_changed, this));
	}

	T* wrapped () const {
		return _wrapped;
	}

	typedef std::vector<boost::shared_ptr<S> > List;

	void set_content (List content)
	{
		for (typename std::list<boost::signals2::connection>::iterator i = _connections.begin(); i != _connections.end(); ++i) {
			i->disconnect ();
		}
		
		_content = content;

		_wrapped->Enable (!_content.empty ());

		update_from_model ();

		for (typename List::iterator i = _content.begin(); i != _content.end(); ++i) {
			_connections.push_back ((*i)->Changed.connect (boost::bind (&ContentWidget::model_changed, this, _2)));
		}
	}

	void add (wxGridBagSizer* sizer, wxGBPosition position)
	{
		_sizer = sizer;
		_position = position;
		_sizer->Add (_wrapped, _position);
	}


	void update_from_model ()
	{
		if (_content.empty ()) {
			set_single ();
			return;
		}

		typename List::iterator i = _content.begin ();
		int const v = boost::bind (_getter, _content.front().get())();
		while (i != _content.end() && boost::bind (_getter, i->get())() == v) {
			++i;
		}

		if (i == _content.end ()) {
			set_single ();
			checked_set (_wrapped, v);
		} else {
			set_multiple ();
		}
	}

private:
	
	void set_single ()
	{
		if (_wrapped->IsShown ()) {
			return;
		}

		_sizer->Detach (_button);
		_button->Hide ();
		_sizer->Add (_wrapped, _position);
		_wrapped->Show ();
		_sizer->Layout ();
	}

	void set_multiple ()
	{
		if (_button->IsShown ()) {
			return;
		}
		
		_wrapped->Hide ();
		_sizer->Detach (_wrapped);
		_button->Show ();
		_sizer->Add (_button, _position);
		_sizer->Layout ();
	}

	void button_clicked ()
	{
		int const v = boost::bind (_getter, _content.front().get())();
		for (typename List::iterator i = _content.begin (); i != _content.end(); ++i) {
			boost::bind (_setter, i->get(), v) ();
		}
	}

	void view_changed ()
	{
		for (size_t i = 0; i < _content.size(); ++i) {
			/* Only update our view on the last time round this loop */
			_ignore_model_changes = i < (_content.size() - 1);
			boost::bind (_setter, _content[i].get(), _wrapped->GetValue ()) ();
		}
	}

	void model_changed (int property)
	{
		if (property == _property && !_ignore_model_changes) {
			update_from_model ();
		}
	}
	
	T* _wrapped;
	wxGridBagSizer* _sizer;
	wxGBPosition _position;
	wxButton* _button;
	List _content;
	int _property;
	boost::function<int (S*)> _getter;
	boost::function<void (S*, int)> _setter;
	std::list<boost::signals2::connection> _connections;
	bool _ignore_model_changes;
};

#endif
