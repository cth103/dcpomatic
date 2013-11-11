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

template <class T>
class MultipleWidget
{
public:
	MultipleWidget (wxWindow* parent, T* wrapped)
		: _wrapped (wrapped)
		, _sizer (0)
		, _button (new wxButton (parent, wxID_ANY, _("Multiple values")))
	{
		_button->SetToolTip (_("Click the button to set all selections to the same value"));
		_button->Hide ();
		_button->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&MultipleWidget::button_clicked, this));
	}

	T* wrapped () const
	{
		return _wrapped;
	}

	void add (wxGridBagSizer* sizer, wxGBPosition position)
	{
		_sizer = sizer;
		_position = position;
		_sizer->Add (_wrapped, _position);
	}

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

	boost::signals2::signal<void (void)> SetAllSame;

private:
	void button_clicked ()
	{
		SetAllSame ();
	}
	
	T* _wrapped;
	wxGridBagSizer* _sizer;
	wxGBPosition _position;
	wxButton* _button;
};


/** Set up some MultipleWidget<SpinCtrl> using a (possibly) multiple selection of objects of type T.
 *  The value is obtained from the T objects using getter.
 */
template <class T>
void
set_multiple (std::vector<boost::shared_ptr<T> > data, MultipleWidget<wxSpinCtrl>* widget, boost::function<int (T*)> getter)
{
	if (data.empty ()) {
		widget->set_single ();
		widget->wrapped()->SetValue (0);
		return;
	}
	
	typename std::vector<boost::shared_ptr<T> >::iterator i = data.begin();
	int first = boost::bind (getter, data.front().get()) ();
	while (i != data.end() && boost::bind (getter, i->get())() == first) {
		++i;
	}

	if (i == data.end ()) {
		/* All values are the same */
		widget->set_single ();
		checked_set (widget->wrapped(), first);
	} else {
		/* At least one different value */
		widget->set_multiple ();
	}
}

#endif
