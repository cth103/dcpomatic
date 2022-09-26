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


#ifndef DCPOMATIC_ENUM_INDEXED_VECTOR_H
#define DCPOMATIC_ENUM_INDEXED_VECTOR_H


#include <vector>


template <typename Type, typename Enum>
class EnumIndexedVector
{
public:
	EnumIndexedVector()
		: _data(static_cast<int>(Enum::COUNT))
	{}

	EnumIndexedVector(EnumIndexedVector const& other)
		: _data(other._data)
	{}

	EnumIndexedVector& operator=(EnumIndexedVector const& other) {
		if (this == &other) {
			return *this;
		}

		_data = other._data;
		return *this;
	}

	typename std::vector<Type>::reference operator[](int index) {
		return _data[index];
	}

	typename std::vector<Type>::const_reference operator[](int index) const {
		return _data[index];
	}

	typename std::vector<Type>::reference operator[](Enum index) {
		return _data[static_cast<int>(index)];
	}

	typename std::vector<Type>::const_reference operator[](Enum index) const {
		return _data[static_cast<int>(index)];
	}

	void clear() {
		std::fill(_data.begin(), _data.end(), {});
	}

	typename std::vector<Type>::const_iterator begin() const {
		return _data.begin();
	}

	typename std::vector<Type>::const_iterator end() const {
		return _data.end();
	}

	typename std::vector<Type>::iterator begin() {
		return _data.begin();
	}

	typename std::vector<Type>::iterator end() {
		return _data.end();
	}

private:
	std::vector<Type> _data;
};


#endif

