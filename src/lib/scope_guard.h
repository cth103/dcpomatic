/*
    Copyright (C) 2022 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_SCOPE_GUARD_H
#define DCPOMATIC_SCOPE_GUARD_H


#include <functional>


class ScopeGuard
{
public:
	template <typename F>
	ScopeGuard (F const& function)
		: _function(function)
	{}

	ScopeGuard (ScopeGuard&& other)
		: _function(std::move(other._function))
	{
		other._function = []{};
	}

	ScopeGuard (ScopeGuard const&) = delete;
	ScopeGuard& operator=(ScopeGuard const&) = delete;

	~ScopeGuard ()
	{
		if (!_cancelled) {
			_function();
		}
	}

	void cancel()
	{
		_cancelled = true;
	}

private:
	std::function<void()> _function;
	bool _cancelled = false;
};


#endif

