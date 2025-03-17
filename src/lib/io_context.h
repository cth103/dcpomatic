/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_IO_CONTEXT_H
#define DCPOMATIC_IO_CONTEXT_H


#include <boost/asio.hpp>


namespace dcpomatic {

#ifdef DCPOMATIC_HAVE_BOOST_ASIO_IO_CONTEXT

using io_context = boost::asio::io_context;
using work_guard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

template <typename T>
void post(io_context& context, T handler)
{
	boost::asio::post(context, handler);
}

work_guard
make_work_guard(io_context& context);

#else

using io_context = boost::asio::io_service;
using work_guard = boost::asio::io_service::work;

template <typename T>
void post(io_context& context, T handler)
{
	context.post(handler);
}

work_guard
make_work_guard(io_context& context);

#endif

}


#endif

