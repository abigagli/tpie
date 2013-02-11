// -*- mode: c++; tab-width: 4; indent-tabs-mode: t; eval: (progn (c-set-style "stroustrup") (c-set-offset 'innamespace 0)); -*-
// vi:set ts=4 sts=4 sw=4 noet :
// Copyright 2008, 2011, The TPIE development team
// 
// This file is part of TPIE.
// 
// TPIE is free software: you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License as published by the
// Free Software Foundation, either version 3 of the License, or (at your
// option) any later version.
// 
// TPIE is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
// License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with TPIE.  If not, see <http://www.gnu.org/licenses/>

#ifndef _TPIE_LOG_H
#define _TPIE_LOG_H
///////////////////////////////////////////////////////////////////////////
/// \file tpie_log.h
/// Logging functionality and log_level codes for different priorities of log messages.
///////////////////////////////////////////////////////////////////////////

#include <tpie/config.h>
#include <tpie/logstream.h>
#include <fstream>

namespace tpie {

/** A simple logger that writes messages to a tpie temporary file */
class TPIE_PUBLIC file_log_target: public log_target {
public:
	std::ofstream m_out;
	std::string m_path;
	log_level m_threshold;
	
    /** Construct a new file logger
	 * \param threshold record messages at or above this severity threshold
	 * */
	file_log_target(log_level threshold);

	/** Implement \ref log_target virtual method to record message
	 * \param level severity of message
	 * \param message content of message
	 * */
	void log(log_level level, const char * message, size_t);
};

/** A simple logger that writes messages to stderr */
class TPIE_PUBLIC stderr_log_target: public log_target {
public:
	log_level m_threshold;

    /** Construct a new stderr logger
	 * \param threshold record messages at or above this severity threshold
	 * */
	stderr_log_target(log_level threshold);
	
	/** Implement \ref log_target virtual method to record message
	 * \param level severity of message
	 * \param message content of message
	 * \param size lenght of message array
	 * */
	void log(log_level level, const char * message, size_t size);
};




///////////////////////////////////////////////////////////////////////////
/// \brief Returns the file name of the log stream.
/// This assumes that init_default_log has been called.
///////////////////////////////////////////////////////////////////////////
TPIE_PUBLIC const std::string& log_name();

///////////////////////////////////////////////////////////////////////////////
/// \internal \brief Used by tpie_init to initialize the log subsystem.
///////////////////////////////////////////////////////////////////////////////
TPIE_PUBLIC void init_default_log();

///////////////////////////////////////////////////////////////////////////////
/// \internal \brief Used by tpie_finish to deinitialize the log subsystem.
///////////////////////////////////////////////////////////////////////////////
TPIE_PUBLIC void finish_default_log();

TPIE_PUBLIC extern logstream log_singleton;

///////////////////////////////////////////////////////////////////////////
/// \brief Returns the only logstream object. 
///////////////////////////////////////////////////////////////////////////
inline logstream & get_log() {return log_singleton;}

///////////////////////////////////////////////////////////////////////////////
/// \brief Return logstream for writing fatal log messages.
///////////////////////////////////////////////////////////////////////////////
inline logstream & log_fatal() {return get_log() << setlevel(LOG_FATAL);}

///////////////////////////////////////////////////////////////////////////////
/// \brief Return logstream for writing error log messages.
///////////////////////////////////////////////////////////////////////////////
inline logstream & log_error() {return get_log() << setlevel(LOG_ERROR);}

///////////////////////////////////////////////////////////////////////////////
/// \brief Return logstream for writing info log messages.
///////////////////////////////////////////////////////////////////////////////
inline logstream & log_info() {return get_log() << setlevel(LOG_INFORMATIONAL);}

///////////////////////////////////////////////////////////////////////////////
/// \brief Return logstream for writing warning log messages.
///////////////////////////////////////////////////////////////////////////////
inline logstream & log_warning() {return get_log() << setlevel(LOG_WARNING);}

///////////////////////////////////////////////////////////////////////////////
/// \brief Return logstream for writing app_debug log messages.
///////////////////////////////////////////////////////////////////////////////
inline logstream & log_app_debug() {return get_log() << setlevel(LOG_APP_DEBUG);}

///////////////////////////////////////////////////////////////////////////////
/// \brief Return logstream for writing debug log messages.
///////////////////////////////////////////////////////////////////////////////
inline logstream & log_debug() {return get_log() << setlevel(LOG_DEBUG);}

///////////////////////////////////////////////////////////////////////////////
/// \brief Return logstream for writing mem_debug log messages.
///////////////////////////////////////////////////////////////////////////////
inline logstream & log_mem_debug() {return get_log() << setlevel(LOG_MEM_DEBUG);}

class TPIE_PUBLIC scoped_log_enabler {
private:
	bool m_orig;
public:
	inline bool get_orig() {return m_orig;}
	inline scoped_log_enabler(bool e) {
		m_orig = get_log().enabled(); 
		get_log().enable(e);
	}
	inline ~scoped_log_enabler() {
		get_log().enable(m_orig);
	}
};

#if TPL_LOGGING		
/// \def TP_LOG_FLUSH_LOG  \deprecated Use \ref get_log().flush() instead.
#define TP_LOG_FLUSH_LOG tpie::get_log().flush()
    
/// \def TP_LOG_FATAL \deprecated Use \ref log_fatal() instead.
#define TP_LOG_FATAL(msg) tpie::log_fatal() << msg
/// \def TP_LOG_WARNING \deprecated Use \ref log_warning() instead.
#define TP_LOG_WARNING(msg)	tpie::log_warning() << msg
/// \def TP_LOG_APP_DEBUG \deprecated Use \ref log_app_debug() instead.
#define TP_LOG_APP_DEBUG(msg) tpie::log_app_debug() << msg
/// \def TP_LOG_DEBUG \deprecated Use \ref log_debug() instead.
#define TP_LOG_DEBUG(msg) tpie::log_debug() << msg
/// \def TP_LOG_MEM_DEBUG \deprecated Use \ref log_mem_debug() instead.
#define TP_LOG_MEM_DEBUG(msg) tpie::log_mem_debug() << msg

#define TP_LOG_ID_MSG __FILE__ << " line " << __LINE__ << ": "

/** \def TP_LOG_FATAL_ID Macro to simplify \ref logging. \sa log_level. */
#define TP_LOG_FATAL_ID(msg) TP_LOG_FATAL(TP_LOG_ID_MSG << msg << std::endl)

/** \def TP_LOG_WARNING_ID Macro to simplify \ref logging. \sa log_level. */
#define TP_LOG_WARNING_ID(msg) TP_LOG_WARNING(TP_LOG_ID_MSG << msg << std::endl)

/** \def TP_LOG_APP_DEBUG_ID Macro to simplify \ref logging. \sa log_level. */
#define TP_LOG_APP_DEBUG_ID(msg) TP_LOG_APP_DEBUG(TP_LOG_ID_MSG << msg << std::endl)

/** \def TP_LOG_DEBUG_ID Macro to simplify \ref logging. \sa log_level. */
#define TP_LOG_DEBUG_ID(msg) TP_LOG_DEBUG(TP_LOG_ID_MSG << msg << std::endl)

/** \def TP_LOG_MEM_DEBUG_ID Macro to simplify \ref logging. \sa log_level. */
#define TP_LOG_MEM_DEBUG_ID(msg) TP_LOG_MEM_DEBUG(TP_LOG_ID_MSG << msg << std::endl)
    
#else // !TPL_LOGGING
    
// We are not compiling logging.
#define TP_LOG_FATAL(msg) 
#define TP_LOG_WARNING(msg) 
#define TP_LOG_APP_DEBUG(msg)
#define TP_LOG_DEBUG(msg) 
#define TP_LOG_MEM_DEBUG(msg)
    
#define TP_LOG_FATAL_ID(msg)
#define TP_LOG_WARNING_ID(msg)
#define TP_LOG_APP_DEBUG_ID(msg)
#define TP_LOG_DEBUG_ID(msg)
#define TP_LOG_MEM_DEBUG_ID(msg)
    
#define TP_LOG_FLUSH_LOG {}
    
#endif // TPL_LOGGING

}  //  tpie namespace

#endif // _TPIE_LOG_H 
