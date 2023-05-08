/*
    Copyright (C) 2018-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_CHANGE_SIGNALLER_H
#define DCPOMATIC_CHANGE_SIGNALLER_H


#include <boost/thread.hpp>
#include <vector>


enum class ChangeType
{
	PENDING,
	DONE,
	CANCELLED
};


template <class T, class P>
class ChangeSignal
{
public:
	ChangeSignal(T* thing_, P property_, ChangeType type_)
		: thing(thing_)
		, property(property_)
		, type(type_)
	{}

	T* thing;
	P property;
	ChangeType type;
};


class ChangeSignalDespatcherBase
{
protected:
	static boost::mutex _instance_mutex;
};


template <class T, class P>
class ChangeSignalDespatcher : public ChangeSignalDespatcherBase
{
public:
	ChangeSignalDespatcher() = default;

	ChangeSignalDespatcher(ChangeSignalDespatcher const&) = delete;
	ChangeSignalDespatcher& operator=(ChangeSignalDespatcher const&) = delete;

	void signal_change(ChangeSignal<T, P> const& signal)
	{
		if (_suspended) {
			boost::mutex::scoped_lock lm(_mutex);
			_pending.push_back(signal);
		} else {
			signal.thing->signal_change(signal.type, signal.property);
		}
	}

	void suspend()
	{
		boost::mutex::scoped_lock lm(_mutex);
		_suspended = true;
	}

	void resume()
	{
		boost::mutex::scoped_lock lm(_mutex);
		auto pending = _pending;
		lm.unlock();

		for (auto signal: pending) {
			signal.thing->signal_change(signal.type, signal.property);
		}

		lm.lock();
		_pending.clear();
		_suspended = false;
	}

	static ChangeSignalDespatcher* instance()
	{
		static boost::mutex _instance_mutex;
		static boost::mutex::scoped_lock lm(_instance_mutex);
		static ChangeSignalDespatcher<T, P>* _instance;
		if (!_instance) {
			_instance = new ChangeSignalDespatcher<T, P>();
		}
		return _instance;
	}

private:
	std::vector<ChangeSignal<T, P>> _pending;
	bool _suspended = false;
	boost::mutex _mutex;
};


template <class T, class P>
class ChangeSignaller
{
public:
	ChangeSignaller (T* t, P p)
		: _thing(t)
		, _property(p)
		, _done(true)
	{
		ChangeSignalDespatcher<T, P>::instance()->signal_change({_thing, _property, ChangeType::PENDING});
	}

	~ChangeSignaller ()
	{
		ChangeSignalDespatcher<T, P>::instance()->signal_change({_thing, _property, _done ? ChangeType::DONE : ChangeType::CANCELLED});
	}

	void abort ()
	{
		_done = false;
	}

private:
	T* _thing;
	P _property;
	bool _done;
};


#endif
