// -*- mode: c++; tab-width: 4; indent-tabs-mode: t; eval: (progn (c-set-style "stroustrup") (c-set-offset 'innamespace 0)); -*-
// vi:set ts=4 sts=4 sw=4 noet :
// Copyright 2008, The TPIE development team
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

///////////////////////////////////////////////////////////////////////////////
/// \file progress_indicator_base.h  Progress indicator base
///////////////////////////////////////////////////////////////////////////////

#ifndef _TPIE_PROGRESS_INDICATOR_BASE_H
#define _TPIE_PROGRESS_INDICATOR_BASE_H

#include <tpie/portability.h>
#include <algorithm>
#include <boost/thread.hpp>
#include <tpie/imported/cycle.h>
#include <tpie/execution_time_predictor.h>
#include <tpie/tpie_log.h>

namespace tpie {

enum description_importance {
	IMPORTANCE_NONE,
	IMPORTANCE_LOG,
	IMPORTANCE_MINOR,
	IMPORTANCE_MAJOR
};

///////////////////////////////////////////////////////////////////////////////
/// \brief The base class for indicating the progress of some task.
///
/// At times, especially when processing large data sets, the user might want
/// the program to provide information about how much progress has been made. 
/// TPIE provides a class hierarchy with an abstract base class 
/// \ref progress_indicator_base for realizing such indicators. TPIE offers 
/// terminal-based indicators, such as progress_indicator_arrow that shows an 
/// extending arrow or progress_indicator_spin that shows a spinning "wheel". 
/// To allow for other types of indicators such as graphics-based indicators
/// for (interfaces to) indicators provided by other APIs, the terminal-based
/// indicators inherit from progress_indicator_terminal which in turn inherits
/// from progress_indicator_base. To include other types of non-terminal-based
/// indicators, the user thus should subclass progress_indicator_base. All
/// indicators are based upon the following concept: The indicator is given
/// a range [minRange , maxRange ] and a parameter stepValue/ 
/// For each update to the indicator and starting at minRange , the progress 
/// status will be advanced by stepValue units. 
///////////////////////////////////////////////////////////////////////////////

class TPIE_PUBLIC progress_indicator_base {
public:
	///////////////////////////////////////////////////////////////////////////
	/// Initializes the indicator.
	/// \param  range     The upper bound of the counting range.
	///////////////////////////////////////////////////////////////////////////
	
	progress_indicator_base(stream_size_type range) : 
	    m_range(range),
	    m_current(0),
		//m_lastUpdate(getticks()),
		m_next(0),
		m_predictor(0) {
		compute_threshold();
	}

	///////////////////////////////////////////////////////////////////////////
	/// The destructor. Nothing is done.
	///////////////////////////////////////////////////////////////////////////
	virtual ~progress_indicator_base() {
	    // Do nothing.
	};

	///////////////////////////////////////////////////////////////////////////
	///  Record an increment to the indicator and advance the indicator.
	///////////////////////////////////////////////////////////////////////////
	void step(stream_size_type step=1) {
	    m_current += step;
 		
#ifndef TPIE_NDEBUG
		{
			ticks currentTicks = getticks();
			if (elapsed(currentTicks,m_lastCalled) > m_frequency * m_threshold * 5)
				tpie::log_debug() << "Step was not called for an estimated " 
								  << (elapsed(currentTicks,m_lastCalled) / (m_frequency * m_threshold))
								  << " seconds" << std::endl;;
			m_lastCalled = currentTicks;
		}
#endif	
		if (m_current > m_next) {
			ticks currentTicks = getticks();
			m_next = static_cast<stream_size_type>(
				static_cast<double>(m_current) * (elapsed(currentTicks, m_start) + m_threshold)/
				elapsed(currentTicks, m_start));
			if (m_next > m_current *2) m_next=m_current*2; //For bad guestimation in the beginning
			refresh();
		}
	}

	void raw_step(stream_size_type step) {
		m_current += step;
#ifndef TPIE_NDEBUG
		m_lastCalled = getticks();
#endif	
		refresh();
	}

	///////////////////////////////////////////////////////////////////////////
	/// \brief Initialize progress indicator.
	/// \param range The number of times step() is going to be called.
	///////////////////////////////////////////////////////////////////////////
	virtual void init(stream_size_type range=0) {
		if (range != 0) set_range(range);
	    m_current = 0;
		refresh();
		
		m_start = getticks();
		m_next = 1;

#ifndef TPIE_NDEBUG
		m_lastCalled = getticks();
#endif
	}

	///////////////////////////////////////////////////////////////////////////
	/// Advance the indicator to the end
	///////////////////////////////////////////////////////////////////////////
	virtual void done() {}
	
	///////////////////////////////////////////////////////////////////////////
	/// Set the upper bound of the counting range. This method
	/// also implies a reset of the counter. In order to be able
	/// to set the uper bound independent of setting the lower bound,
	/// no range checking is done.
	///
	/// \param  range  The new upper bound.
	///////////////////////////////////////////////////////////////////////////
	virtual void set_range(stream_size_type range) {
	    m_range = range;
	}
  
	///////////////////////////////////////////////////////////////////////////
	/// Display the indicator.
	///////////////////////////////////////////////////////////////////////////
	virtual void refresh() = 0;

	///////////////////////////////////////////////////////////////////////////
	/// Get the current value of the step counter.
	///////////////////////////////////////////////////////////////////////////
	stream_size_type get_current() { return m_current; }
	
	///////////////////////////////////////////////////////////////////////////
	/// Get the maximum value of the current range.
	///////////////////////////////////////////////////////////////////////////
	stream_size_type get_range() { return m_range; }

	execution_time_predictor * get_time_predictor() {return m_predictor;}
	void set_time_predictor(execution_time_predictor * p) {m_predictor = p;}

	std::string estimated_remaining_time() {
		if (m_range == 0 || m_predictor == 0) return "";
		return m_predictor->estimate_remaining_time( double(m_current) / double(m_range) );
	}

	virtual void push_breadcrumb(const char *, description_importance) {}
	virtual void pop_breadcrumb() {}
protected:
	/**  The upper bound of the counting range.  */
	stream_size_type m_range;

	/**  The current progress count [m_minRange...m_maxRange].  */
	stream_size_type m_current;
	
private:
	/**  The number of ticks elapsed when refresh was called last */
#ifndef TPIE_NDEBUG
	ticks m_lastCalled;
#endif

	stream_size_type m_next;
	ticks m_start;

	/**  The approximate frequency of calls to refresh in hz */
	static const unsigned int m_frequency;

	/**  The threshold for elapsed ticks before refresh is called again */
	static double m_threshold;

	/**  Indicates whether or not m_threshold has been computed */
	static bool m_thresholdComputed;

	execution_time_predictor * m_predictor;
	///////////////////////////////////////////////////////////////////////////
	/// Makes sure m_threshold has been set.
	///////////////////////////////////////////////////////////////////////////
	TPIE_PUBLIC  static void compute_threshold();

	TPIE_PUBLIC progress_indicator_base();
	TPIE_PUBLIC progress_indicator_base(const progress_indicator_base& other);
};

}  //  tpie namespace

#endif // _TPIE_PROGRESS_INDICATOR_BASE
