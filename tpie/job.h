// -*- mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup"; -*-
// vi:set ts=4 sts=4 sw=4 noet cino+=(0 :
// Copyright 2011, The TPIE development team
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

#ifndef __TPIE_JOB_MANAGER_H__
#define __TPIE_JOB_MANAGER_H__

#include <stddef.h>
#include <boost/thread.hpp>

namespace tpie {

class job {

public:

	/////////////////////////////////////////////////////////
	/// \brief Default constructor
	/////////////////////////////////////////////////////////
	inline job() : m_dependencies(0) {}

	/////////////////////////////////////////////////////////
	/// \brief Default destructor
	/////////////////////////////////////////////////////////
	virtual ~job() {}

	/////////////////////////////////////////////////////////
	/// \brief Called by the worker thread
	/////////////////////////////////////////////////////////
	virtual void operator()() = 0;

	/////////////////////////////////////////////////////////
	/// \brief Wait for this job and its subjobs to complete
	/////////////////////////////////////////////////////////
	void join();

	/////////////////////////////////////////////////////////
	/// \brief Add this job to the job pool
	///
	/// \param parent (optional) The parent job, or 0 if this is a root job
	/////////////////////////////////////////////////////////
	void enqueue(job * parent = 0);

protected:

	/////////////////////////////////////////////////////////
	/// \brief Called when this job and all subjobs are done
	/////////////////////////////////////////////////////////
	virtual void on_done() {}

	/////////////////////////////////////////////////////////
	/// \brief Called when a subjob is done
	/////////////////////////////////////////////////////////
	virtual void child_done(job *) {}

private:

	size_t m_dependencies;
	job * m_parent;

	/////////////////////////////////////////////////////////
	/// \brief Notified when this job and subjobs are done.
	/////////////////////////////////////////////////////////
	boost::condition_variable m_done;

	/////////////////////////////////////////////////////////
	/// \brief Run this job.
	///
	/// Invoke operator() and call done().
	/////////////////////////////////////////////////////////
	void run();

	/////////////////////////////////////////////////////////
	/// \brief Called when this job or a subjob is done.
	///
	/// Decrement m_dependencies and call on_done() and notify
	/// waiters, if applicable.
	/////////////////////////////////////////////////////////
	void done();

	/////////////////////////////////////////////////////////
	/// The job manager needs to invoke run() on us.
	/////////////////////////////////////////////////////////
	friend class job_manager;
};

/////////////////////////////////////////////////////////
/// \brief Initialize job subsystem.
/////////////////////////////////////////////////////////
void init_job();

/////////////////////////////////////////////////////////
/// \brief Deinitialize job subsystem.
/////////////////////////////////////////////////////////
void finish_job();

} // namespace tpie

#endif
