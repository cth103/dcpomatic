/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

/** @file  src/exceptions.h
 *  @brief Our exceptions.
 */

#include <stdexcept>
#include <sstream>
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
	FileError (std::string m, std::string f)
		: StringError (m)
		, _file (f)
	{}

	virtual ~FileError () throw () {}

	std::string file () const {
		return _file;
	}

private:
	std::string _file;
};
	

/** @class OpenFileError.
 *  @brief Indicates that some error occurred when trying to open a file.
 */
class OpenFileError : public FileError
{
public:
	/** @param f File that we were trying to open */
	OpenFileError (std::string f)
		: FileError ("could not open file " + f, f)
	{}
};

/** @class CreateFileError.
 *  @brief Indicates that some error occurred when trying to create a file.
 */
class CreateFileError : public FileError
{
public:
	/** @param f File that we were trying to create */
	CreateFileError (std::string f)
		: FileError ("could not create file " + f, f)
	{}
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
	WriteFileError (std::string f, int e)
		: FileError ("", f)
	{
		std::stringstream s;
		s << "could not write to file " << f;
		if (e) {
			s << " (" << strerror (e) << ")";
		}
		_what = s.str ();
	}
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
	MissingSettingError (std::string s)
		: SettingError (s, "missing required setting " + s)
	{}
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

/** @class NetworkError.
 *  @brief Indicates some problem with communication on the network.
 */
class NetworkError : public StringError
{
public:
	NetworkError (std::string s)
		: StringError (s)
	{}
};

class PlayError : public StringError
{
public:
	PlayError (std::string s)
		: StringError (s)
	{}
};

class DVDError : public StringError
{
public:
	DVDError (std::string s)
		: StringError (s)
	{}
};
