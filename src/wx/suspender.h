/*
    Copyright (C) 2020-2021 Carl Hetherington <cth@carlh.net>

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


#include <functional>
#include <set>


class Suspender
{
public:
	Suspender (std::function<void (int)> handler);

	bool check (int property);

	class Block
	{
	public:
		Block (Suspender* s);
		~Block ();
	private:
		Suspender* _suspender;
	};

	Block block ();

private:
	friend class Block;

	void increment ();
	void decrement ();

	std::function<void (int)> _handler;
	int _count = 0;
	std::set<int> _pending;
};
