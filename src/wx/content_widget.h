/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/

/** @file  src/wx/content_widget.h
 *  @brief ContentWidget class.
 */


#ifndef DCPOMATIC_CONTENT_WIDGET_H
#define DCPOMATIC_CONTENT_WIDGET_H


#include "wx_util.h"
#include "lib/content.h"
#include "lib/warnings.h"
DCPOMATIC_DISABLE_WARNINGS
#include <wx/gbsizer.h>
#include <wx/spinctrl.h>
#include <wx/wx.h>
DCPOMATIC_ENABLE_WARNINGS
#include <vector>


/** @class ContentWidget
 *  @brief A widget which represents some Content state and which can be used
 *  when multiple pieces of content are selected.
 *
 *  @param S Type of ContentPart being manipulated (e.g. VideoContent)
 *  @param T Type of the widget (e.g. wxSpinCtrl)
 *  @param U Data type of state as used by the model.
 *  @param V Data type of state as used by the view.
 */
template <class S, class T, typename U, typename V>
class ContentWidget
{
public:
	/** @param parent Parent window.
	 *  @param wrapped Control widget that we are wrapping.
	 *  @param property ContentProperty that the widget is handling.
	 *  @param part Part of Content that the property is in (e.g. &Content::video)
	 *  @param model_getter Function on the Content to get the value.
	 *  @param model_setter Function on the Content to set the value.
	 *  @param view_changed Function called when the view has changed; useful for linking controls.
	 *  @param view_to_model Function to convert a view value to a model value.
	 *  @param model_to_view Function to convert a model value to a view value.
	 */
	ContentWidget (
		wxWindow* parent,
		T* wrapped,
		int property,
		std::function<std::shared_ptr<S> (Content*)> part,
		std::function<U (S*)> model_getter,
		std::function<void (S*, U)> model_setter,
		std::function<void ()> view_changed,
		std::function<U (V)> view_to_model,
		std::function<V (U)> model_to_view
		)
		: _wrapped (wrapped)
		, _sizer (0)
		, _button (new wxButton (parent, wxID_ANY, _("Multiple values")))
		, _property (property)
		, _part (part)
		, _model_getter (model_getter)
		, _model_setter (model_setter)
		, _view_changed (view_changed)
		, _view_to_model (view_to_model)
		, _model_to_view (model_to_view)
		, _ignore_model_changes (false)
	{
		_button->SetToolTip (_("Click the button to set all selected content to the same value."));
		_button->Hide ();
		_button->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&ContentWidget::button_clicked, this));
	}

	ContentWidget (ContentWidget const&) = delete;
	ContentWidget& operator= (ContentWidget const&) = delete;

	/** @return the widget that we are wrapping */
	T* wrapped () const
	{
		return _wrapped;
	}

	typedef std::vector<std::shared_ptr<Content> > List;

	/** Set the content that this control is working on (i.e. the selected content) */
	void set_content (List content)
	{
		for (typename std::list<boost::signals2::connection>::iterator i = _connections.begin(); i != _connections.end(); ++i) {
			i->disconnect ();
		}

		_connections.clear ();

		_content = content;

		_wrapped->Enable (!_content.empty ());

		update_from_model ();

		for (typename List::iterator i = _content.begin(); i != _content.end(); ++i) {
#if BOOST_VERSION >= 106100
			_connections.push_back ((*i)->Change.connect (boost::bind (&ContentWidget::model_changed, this, boost::placeholders::_1, boost::placeholders::_3)));
#else
			_connections.push_back ((*i)->Change.connect (boost::bind (&ContentWidget::model_changed, this, _1, _3)));
#endif
		}
	}

	/** Add this widget to a wxGridBagSizer */
	void add (wxGridBagSizer* sizer, wxGBPosition position, wxGBSpan span = wxDefaultSpan, int flag = 0)
	{
		_sizer = sizer;
		_position = position;
		_span = span;
		_sizer->Add (_wrapped, _position, _span, flag);
	}

	/** Update the view from the model */
	void update_from_model ()
	{
		if (_content.empty ()) {
			set_single ();
			return;
		}

		typename List::iterator i = _content.begin ();
		U const v = boost::bind (_model_getter, _part(_content.front().get()).get())();
		while (i != _content.end() && boost::bind (_model_getter, _part(i->get()).get())() == v) {
			++i;
		}

		if (i == _content.end ()) {
			set_single ();
			checked_set (_wrapped, _model_to_view (v));
		} else {
			set_multiple ();
		}
	}

	void view_changed ()
	{
		_ignore_model_changes = true;
		for (size_t i = 0; i < _content.size(); ++i) {
			boost::bind (_model_setter, _part (_content[i].get()).get(), _view_to_model (wx_get (_wrapped))) ();
		}
		if (_view_changed) {
			_view_changed ();
		}
		_ignore_model_changes = false;
	}

	void show (bool s)
	{
		_wrapped->Show (s);
	}

private:

	void set_single ()
	{
		if (_wrapped->IsShown() || !_sizer) {
			return;
		}

		_sizer->Detach (_button);
		_button->Hide ();
		_sizer->Add (_wrapped, _position, _span);
		_wrapped->Show ();
		_sizer->Layout ();
	}

	void set_multiple ()
	{
		if (_button->IsShown() || !_sizer) {
			return;
		}

		_wrapped->Hide ();
		_sizer->Detach (_wrapped);
		_button->Show ();
		_sizer->Add (_button, _position, _span);
		_sizer->Layout ();
	}

	void button_clicked ()
	{
		U const v = boost::bind (_model_getter, _part(_content.front().get()).get())();
		for (auto const& i: _content) {
			boost::bind (_model_setter, _part(i.get()).get(), v)();
		}
	}

	void model_changed (ChangeType type, int property)
	{
		if (type == ChangeType::DONE && property == _property && !_ignore_model_changes) {
			update_from_model ();
		}
	}

	T* _wrapped;
	wxGridBagSizer* _sizer;
	wxGBPosition _position;
	wxGBSpan _span;
	wxButton* _button;
	List _content;
	int _property;
	std::function<std::shared_ptr<S> (Content *)> _part;
	std::function<U (S*)> _model_getter;
	std::function<void (S*, U)> _model_setter;
	std::function<void ()> _view_changed;
	std::function<U (V)> _view_to_model;
	std::function<V (U)> _model_to_view;
	std::list<boost::signals2::connection> _connections;
	bool _ignore_model_changes;
};

template <typename U, typename V>
V caster (U x)
{
	return static_cast<V> (x);
}

template <class S>
class ContentSpinCtrl : public ContentWidget<S, wxSpinCtrl, int, int>
{
public:
	ContentSpinCtrl (
		wxWindow* parent,
		wxSpinCtrl* wrapped,
		int property,
		std::function<std::shared_ptr<S> (Content *)> part,
		std::function<int (S*)> getter,
		std::function<void (S*, int)> setter,
		std::function<void ()> view_changed = std::function<void ()>()
		)
		: ContentWidget<S, wxSpinCtrl, int, int> (
			parent,
			wrapped,
			property,
			part,
			getter, setter,
			view_changed,
			&caster<int, int>,
			&caster<int, int>
			)
	{
		wrapped->Bind (wxEVT_COMMAND_SPINCTRL_UPDATED, boost::bind (&ContentWidget<S, wxSpinCtrl, int, int>::view_changed, this));
	}
};

template <class S>
class ContentSpinCtrlDouble : public ContentWidget<S, wxSpinCtrlDouble, double, double>
{
public:
	ContentSpinCtrlDouble (
		wxWindow* parent,
		wxSpinCtrlDouble* wrapped,
		int property,
		std::function<std::shared_ptr<S> (Content *)> part,
		std::function<double (S*)> getter,
		std::function<void (S*, double)> setter,
		std::function<void ()> view_changed = std::function<void ()>()
		)
		: ContentWidget<S, wxSpinCtrlDouble, double, double> (
			parent,
			wrapped,
			property,
			part,
			getter, setter,
			view_changed,
			&caster<double, double>,
			&caster<double, double>
			)
	{
		wrapped->Bind (wxEVT_COMMAND_SPINCTRLDOUBLE_UPDATED, boost::bind (&ContentWidget<S, wxSpinCtrlDouble, double, double>::view_changed, this));
	}
};

template <class S, class U>
class ContentChoice : public ContentWidget<S, wxChoice, U, int>
{
public:
	ContentChoice (
		wxWindow* parent,
		wxChoice* wrapped,
		int property,
		std::function<std::shared_ptr<S> (Content *)> part,
		std::function<U (S*)> getter,
		std::function<void (S*, U)> setter,
		std::function<U (int)> view_to_model,
		std::function<int (U)> model_to_view,
		std::function<void ()> view_changed = std::function<void()>()
		)
		: ContentWidget<S, wxChoice, U, int> (
			parent,
			wrapped,
			property,
			part,
			getter,
			setter,
			view_changed,
			view_to_model,
			model_to_view
			)
	{
		wrapped->Bind (wxEVT_COMMAND_CHOICE_SELECTED, boost::bind (&ContentWidget<S, wxChoice, U, int>::view_changed, this));
	}

};

#endif
