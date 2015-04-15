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

/** @file src/config.h
 *  @brief Class holding configuration.
 */

#ifndef DCPOMATIC_CONFIG_H
#define DCPOMATIC_CONFIG_H

#include "isdcf_metadata.h"
#include "colour_conversion.h"
#include "video_content.h"
#include <dcp/metadata.h>
#include <dcp/certificates.h>
#include <dcp/signer.h>
#include <boost/shared_ptr.hpp>
#include <boost/signals2.hpp>
#include <boost/filesystem.hpp>
#include <vector>

class ServerDescription;
class Scaler;
class Filter;
class CinemaSoundProcessor;
class DCPContentType;
class Ratio;
class Cinema;

/** @class Config
 *  @brief A singleton class holding configuration.
 */
class Config : public boost::noncopyable
{
public:
	/** @return number of threads to use for J2K encoding on the local machine */
	int num_local_encoding_threads () const {
		return _num_local_encoding_threads;
	}

	boost::filesystem::path default_directory () const {
		return _default_directory;
	}

	boost::filesystem::path default_directory_or (boost::filesystem::path a) const;

	/** @return base port number to use for J2K encoding servers */
	int server_port_base () const {
		return _server_port_base;
	}

	void set_use_any_servers (bool u) {
		_use_any_servers = u;
		changed ();
	}

	bool use_any_servers () const {
		return _use_any_servers;
	}

	/** @param s New list of servers */
	void set_servers (std::vector<std::string> s) {
		_servers = s;
		changed ();
	}

	/** @return Host names / IP addresses of J2K encoding servers that should definitely be used */
	std::vector<std::string> servers () const {
		return _servers;
	}

	/** @return The IP address of a TMS that we can copy DCPs to */
	std::string tms_ip () const {
		return _tms_ip;
	}
	
	/** @return The path on a TMS that we should changed DCPs to */
	std::string tms_path () const {
		return _tms_path;
	}

	/** @return User name to log into the TMS with */
	std::string tms_user () const {
		return _tms_user;
	}

	/** @return Password to log into the TMS with */
	std::string tms_password () const {
		return _tms_password;
	}

	/** @return The cinema sound processor that we are using */
	CinemaSoundProcessor const * cinema_sound_processor () const {
		return _cinema_sound_processor;
	}

	std::list<boost::shared_ptr<Cinema> > cinemas () const {
		return _cinemas;
	}
	
	std::list<int> allowed_dcp_frame_rates () const {
		return _allowed_dcp_frame_rates;
	}

	bool allow_any_dcp_frame_rate () const {
		return _allow_any_dcp_frame_rate;
	}
	
	ISDCFMetadata default_isdcf_metadata () const {
		return _default_isdcf_metadata;
	}

	boost::optional<std::string> language () const {
		return _language;
	}

	int default_still_length () const {
		return _default_still_length;
	}

	Ratio const * default_container () const {
		return _default_container;
	}

	DCPContentType const * default_dcp_content_type () const {
		return _default_dcp_content_type;
	}

	std::string dcp_issuer () const {
		return _dcp_issuer;
	}

	int default_j2k_bandwidth () const {
		return _default_j2k_bandwidth;
	}

	int default_audio_delay () const {
		return _default_audio_delay;
	}

	std::vector<PresetColourConversion> colour_conversions () const {
		return _colour_conversions;
	}

	std::string mail_server () const {
		return _mail_server;
	}

	std::string mail_user () const {
		return _mail_user;
	}

	std::string mail_password () const {
		return _mail_password;
	}

	std::string kdm_subject () const {
		return _kdm_subject;
	}

	std::string kdm_from () const {
		return _kdm_from;
	}

	std::string kdm_cc () const {
		return _kdm_cc;
	}

	std::string kdm_bcc () const {
		return _kdm_bcc;
	}
	
	std::string kdm_email () const {
		return _kdm_email;
	}

	boost::shared_ptr<const dcp::Signer> signer () const {
		return _signer;
	}

	dcp::Certificate decryption_certificate () const {
		return _decryption_certificate;
	}

	std::string decryption_private_key () const {
		return _decryption_private_key;
	}

	bool check_for_updates () const {
		return _check_for_updates;
	}

	bool check_for_test_updates () const {
		return _check_for_test_updates;
	}

	int maximum_j2k_bandwidth () const {
		return _maximum_j2k_bandwidth;
	}

	int log_types () const {
		return _log_types;
	}

#ifdef DCPOMATIC_WINDOWS	
	bool win32_console () const {
		return _win32_console;
	}
#endif	

	std::vector<boost::filesystem::path> history () const {
		return _history;
	}

	/** @param n New number of local encoding threads */
	void set_num_local_encoding_threads (int n) {
		maybe_set (_num_local_encoding_threads, n);
	}

	void set_default_directory (boost::filesystem::path d) {
		maybe_set (_default_directory, d);
	}

	/** @param p New server port */
	void set_server_port_base (int p) {
		maybe_set (_server_port_base, p);
	}

	/** @param i IP address of a TMS that we can copy DCPs to */
	void set_tms_ip (std::string i) {
		maybe_set (_tms_ip, i);
	}

	/** @param p Path on a TMS that we should changed DCPs to */
	void set_tms_path (std::string p) {
		maybe_set (_tms_path, p);
	}

	/** @param u User name to log into the TMS with */
	void set_tms_user (std::string u) {
		maybe_set (_tms_user, u);
	}

	/** @param p Password to log into the TMS with */
	void set_tms_password (std::string p) {
		maybe_set (_tms_password, p);
	}

	void add_cinema (boost::shared_ptr<Cinema> c) {
		_cinemas.push_back (c);
		changed ();
	}

	void remove_cinema (boost::shared_ptr<Cinema> c) {
		_cinemas.remove (c);
		changed ();
	}

	void set_allowed_dcp_frame_rates (std::list<int> const & r) {
		maybe_set (_allowed_dcp_frame_rates, r);
	}

	void set_allow_any_dcp_frame_rate (bool a) {
		maybe_set (_allow_any_dcp_frame_rate, a);
	}

	void set_default_isdcf_metadata (ISDCFMetadata d) {
		maybe_set (_default_isdcf_metadata, d);
	}

	void set_language (std::string l) {
		if (_language && _language.get() == l) {
			return;
		}
		_language = l;
		changed ();
	}

	void unset_language () {
		if (!_language) {
			return;
		}
			
		_language = boost::none;
		changed ();
	}

	void set_default_still_length (int s) {
		maybe_set (_default_still_length, s);
	}

	void set_default_container (Ratio const * c) {
		maybe_set (_default_container, c);
	}

	void set_default_dcp_content_type (DCPContentType const * t) {
		maybe_set (_default_dcp_content_type, t);
	}

	void set_dcp_issuer (std::string i) {
		maybe_set (_dcp_issuer, i);
	}

	void set_default_j2k_bandwidth (int b) {
		maybe_set (_default_j2k_bandwidth, b);
	}

	void set_default_audio_delay (int d) {
		maybe_set (_default_audio_delay, d);
	}

	void set_colour_conversions (std::vector<PresetColourConversion> const & c) {
		maybe_set (_colour_conversions, c);
	}

	void set_mail_server (std::string s) {
		maybe_set (_mail_server, s);
	}

	void set_mail_user (std::string u) {
		maybe_set (_mail_user, u);
	}

	void set_mail_password (std::string p) {
		maybe_set (_mail_password, p);
	}

	void set_kdm_subject (std::string s) {
		maybe_set (_kdm_subject, s);
	}

	void set_kdm_from (std::string f) {
		maybe_set (_kdm_from, f);
	}

	void set_kdm_cc (std::string f) {
		maybe_set (_kdm_cc, f);
	}

	void set_kdm_bcc (std::string f) {
		maybe_set (_kdm_bcc, f);
	}
	
	void set_kdm_email (std::string e) {
		maybe_set (_kdm_email, e);
	}

	void reset_kdm_email ();

	void set_signer (boost::shared_ptr<const dcp::Signer> s) {
		maybe_set (_signer, s);
	}

	void set_decryption_certificate (dcp::Certificate c) {
		maybe_set (_decryption_certificate, c);
	}

	void set_decryption_private_key (std::string k) {
		maybe_set (_decryption_private_key, k);
	}

	void set_check_for_updates (bool c) {
		maybe_set (_check_for_updates, c);
	}

	void set_check_for_test_updates (bool c) {
		maybe_set (_check_for_test_updates, c);
	}

	void set_maximum_j2k_bandwidth (int b) {
		maybe_set (_maximum_j2k_bandwidth, b);
	}

	void set_log_types (int t) {
		maybe_set (_log_types, t);
	}

#ifdef DCPOMATIC_WINDOWS	
	void set_win32_console (bool c) {
		maybe_set (_win32_console, c);
	}
#endif	

	void clear_history () {
		_history.clear ();
		changed ();
	}

	void add_to_history (boost::filesystem::path p);
	
	void changed ();
	boost::signals2::signal<void ()> Changed;

	static Config* instance ();
	static void drop ();
	static void restore_defaults ();

private:
	Config ();
	boost::filesystem::path file () const;
	void read ();
	void write () const;
	void make_decryption_keys ();
	void set_defaults ();
	void set_kdm_email_to_default ();

	template <class T>
	void maybe_set (T& member, T new_value) {
		if (member == new_value) {
			return;
		}
		member = new_value;
		changed ();
	}

	/** number of threads to use for J2K encoding on the local machine */
	int _num_local_encoding_threads;
	/** default directory to put new films in */
	boost::filesystem::path _default_directory;
	/** base port number to use for J2K encoding servers;
	 *  this port and the one above it will be used.
	 */
	int _server_port_base;
	/** true to broadcast on the `any' address to look for servers */
	bool _use_any_servers;
	/** J2K encoding servers that should definitely be used */
	std::vector<std::string> _servers;
	/** The IP address of a TMS that we can copy DCPs to */
	std::string _tms_ip;
	/** The path on a TMS that we should write DCPs to */
	std::string _tms_path;
	/** User name to log into the TMS with */
	std::string _tms_user;
	/** Password to log into the TMS with */
	std::string _tms_password;
	/** Our cinema sound processor */
	CinemaSoundProcessor const * _cinema_sound_processor;
	std::list<int> _allowed_dcp_frame_rates;
	/** Allow any video frame rate for the DCP; if true, overrides _allowed_dcp_frame_rates */
	bool _allow_any_dcp_frame_rate;
	/** Default ISDCF metadata for newly-created Films */
	ISDCFMetadata _default_isdcf_metadata;
	boost::optional<std::string> _language;
	int _default_still_length;
	Ratio const * _default_container;
	DCPContentType const * _default_dcp_content_type;
	std::string _dcp_issuer;
	int _default_j2k_bandwidth;
	int _default_audio_delay;
	std::vector<PresetColourConversion> _colour_conversions;
	std::list<boost::shared_ptr<Cinema> > _cinemas;
	std::string _mail_server;
	std::string _mail_user;
	std::string _mail_password;
	std::string _kdm_subject;
	std::string _kdm_from;
	std::string _kdm_cc;
	std::string _kdm_bcc;
	std::string _kdm_email;
	boost::shared_ptr<const dcp::Signer> _signer;
	dcp::Certificate _decryption_certificate;
	std::string _decryption_private_key;
	/** true to check for updates on startup */
	bool _check_for_updates;
	bool _check_for_test_updates;
	/** maximum allowed J2K bandwidth in bits per second */
	int _maximum_j2k_bandwidth;
	int _log_types;
#ifdef DCPOMATIC_WINDOWS	
	bool _win32_console;
#endif	
	std::vector<boost::filesystem::path> _history;
	
	/** Singleton instance, or 0 */
	static Config* _instance;
};

#endif
