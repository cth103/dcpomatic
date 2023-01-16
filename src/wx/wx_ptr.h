/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_WX_PTR_H
#define DCPOMATIC_WX_PTR_H


#include "lib/dcpomatic_assert.h"
#include <utility>


template <class T>
class wx_ptr
{
public:
	wx_ptr() {}

	explicit wx_ptr(T* wx)
		: _wx(wx)
	{}

	wx_ptr(wx_ptr&) = delete;
	wx_ptr& operator=(wx_ptr&) = delete;

	wx_ptr(wx_ptr&& other)
	{
		_wx = other._wx;
		other._wx = nullptr;
	}

	wx_ptr& operator=(wx_ptr&& other)
	{
		if (this != &other) {
			_wx = other._wx;
			other._wx = nullptr;
		}
		return *this;
	}

	~wx_ptr()
	{
		if (_wx) {
			_wx->Destroy();
		}
	}

	wx_ptr& operator=(T* ptr)
	{
		if (_wx) {
			_wx->Destroy();
		}
		_wx = ptr;
		return *this;
	}

	T* operator->()
	{
		DCPOMATIC_ASSERT(_wx);
		return _wx;
	}

	operator bool() const
	{
		return _wx != nullptr;
	}

	void reset()
	{
		if (_wx) {
			_wx->Destroy();
			_wx = nullptr;
		}
	}

	template <typename... Args>
	void reset(Args&&... args)
	{
		if (_wx) {
			_wx->Destroy();
			_wx = nullptr;
		}
		_wx = new T(std::forward<Args>(args)...);
	}

private:
	T* _wx = nullptr;
};



template <class T, typename... Args>
wx_ptr<T>
make_wx(Args... args)
{
	return wx_ptr<T>(new T(std::forward<Args>(args)...));
}


#endif
