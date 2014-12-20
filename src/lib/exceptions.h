/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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

/** @file  src/lib/exceptions.h
 *  @brief Our exceptions.
 */

#ifndef DCPOMATIC_EXCEPTIONS_H
#define DCPOMATIC_EXCEPTIONS_H

#include <boost/thread.hpp>
extern "C" {
#include <libavutil/pixfmt.h>
}
#include <boost/exception/all.hpp>
#include <boost/filesystem.hpp>
#include <stdexcept>
#include <cstring>

/** @class StringError
 *  @brief A parent class for exceptions using messages held in a std::string
 */
class StringError : public std::exception
{
public:
	/** @param w Error message */
	StringError (std::string w) {
		_what = w;
	}

	virtual ~StringError () throw () {}

	/** @return error message */
	char const * what () const throw () {
		return _what.c_str ();
	}

protected:
	/** error message */
	std::string _what;
};

/** @class DecodeError
 *  @brief A low-level problem with the decoder (possibly due to the nature
 *  of a source file).
 */
class DecodeError : public StringError
{
public:
	DecodeError (std::string s)
		: StringError (s)
	{}
};

/** @class EncodeError
 *  @brief A low-level problem with an encoder.
 */
class EncodeError : public StringError
{
public:
	EncodeError (std::string s)
		: StringError (s)
	{}
};

/** @class FileError.
 *  @brief Parent class for file-related errors.
 */
class FileError : public StringError
{
public:
	/** @param m Error message.
	 *  @param f Name of the file that this exception concerns.
	 */
	FileError (std::string m, boost::filesystem::path f)
		: StringError (m)
		, _file (f)
	{}

	virtual ~FileError () throw () {}

	/** @return name of the file that this exception concerns */
	boost::filesystem::path file () const {
		return _file;
	}

private:
	/** name of the file that this exception concerns */
	boost::filesystem::path _file;
};

class JoinError : public StringError
{
public:
	JoinError (std::string s)
		: StringError (s)
	{}
};

/** @class OpenFileError.
 *  @brief Indicates that some error occurred when trying to open a file.
 */
class OpenFileError : public FileError
{
public:
	/** @param f File that we were trying to open */
	OpenFileError (boost::filesystem::path f);
};

/** @class CreateFileError.
 *  @brief Indicates that some error occurred when trying to create a file.
 */
class CreateFileError : public FileError
{
public:
	/** @param f File that we were trying to create */
	CreateFileError (boost::filesystem::path f);
};


/** @class ReadFileError.
 *  @brief Indicates that some error occurred when trying to read from a file
 */
class ReadFileError : public FileError
{
public:
	/** @param f File that we were trying to read from.
	 *  @param e errno value, or 0.
	 */
	ReadFileError (boost::filesystem::path f, int e = 0);
};

/** @class WriteFileError.
 *  @brief Indicates that some error occurred when trying to write to a file
 */
class WriteFileError : public FileError
{
public:
	/** @param f File that we were trying to write to.
	 *  @param e errno value, or 0.
	 */
	WriteFileError (boost::filesystem::path f, int e);
};

/** @class SettingError.
 *  @brief Indicates that something is wrong with a setting.
 */
class SettingError : public StringError
{
public:
	/** @param s Name of setting that was required.
	 *  @param m Message.
	 */
	SettingError (std::string s, std::string m)
		: StringError (m)
		, _setting (s)
	{}

	virtual ~SettingError () throw () {}

	/** @return name of setting in question */
	std::string setting () const {
		return _setting;
	}

private:
	std::string _setting;
};

/** @class MissingSettingError.
 *  @brief Indicates that a Film is missing a setting that is required for some operation.
 */
class MissingSettingError : public SettingError
{
public:
	/** @param s Name of setting that was required */
	MissingSettingError (std::string s);
};

/** @class BadSettingError
 *  @brief Indicates that a setting is bad in some way.
 */
class BadSettingError : public SettingError
{
public:
	/** @param s Name of setting that is bad */
	BadSettingError (std::string s, std::string m)
		: SettingError (s, m)
	{}
};

/** @class NetworkError
 *  @brief Indicates some problem with communication on the network.
 */
class NetworkError : public StringError
{
public:
	NetworkError (std::string s)
		: StringError (s)
	{}
};

/** @class KDMError
 *  @brief A problem with a KDM.
 */
class KDMError : public StringError
{
public:
	KDMError (std::string s)
		: StringError (s)
	{}
};

/** @class PixelFormatError
 *  @brief A problem with an unsupported pixel format.
 */
class PixelFormatError : public StringError
{
public:
	PixelFormatError (std::string o, AVPixelFormat f);
};

/** @class SubRipError
 *  @brief An error that occurs while parsing a SubRip file.
 */
class SubRipError : public FileError
{
public:
	SubRipError (std::string, std::string, boost::filesystem::path);
};

class DCPError : public StringError
{
public:
	DCPError (std::string s)
		: StringError (s)
	{}
};

class InvalidSignerError : public StringError
{
public:
	InvalidSignerError ();
};

class ProgrammingError : public StringError
{
public:
	ProgrammingError (std::string file, int line);
};

/** @class ExceptionStore
 *  @brief A parent class for classes which have a need to catch and
 *  re-throw exceptions.
 *
 *  This is intended for classes which run their own thread; they should do
 *  something like
 *
 *  void my_thread ()
 *  try {
 *    // do things which might throw exceptions
 *  } catch (...) {
 *    store_current ();
 *  }
 *
 *  and then in another thread call rethrow().  If any
 *  exception was thrown by my_thread it will be stored by
 *  store_current() and then rethrow() will re-throw it where
 *  it can be handled.
 */
class ExceptionStore
{
public:
	void rethrow () {
		boost::mutex::scoped_lock lm (_mutex);
		if (_exception) {
			boost::exception_ptr tmp = _exception;
			_exception = boost::exception_ptr ();
			boost::rethrow_exception (tmp);
		}
	}

protected:	
	
	void store_current () {
		boost::mutex::scoped_lock lm (_mutex);
		_exception = boost::current_exception ();
	}

private:
	boost::exception_ptr _exception;
	mutable boost::mutex _mutex;
};

#endif
