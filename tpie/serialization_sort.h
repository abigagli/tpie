// -*- mode: c++; tab-width: 4; indent-tabs-mode: t; eval: (progn (c-set-style "stroustrup") (c-set-offset 'innamespace 0)); -*-
// vi:set ts=4 sts=4 sw=4 noet :
// Copyright 2013, The TPIE development team
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
#ifndef __TPIE_SERIALIZATION_SORT_H__
#define __TPIE_SERIALIZATION_SORT_H__

#include <queue>
#include <boost/filesystem.hpp>

#include <tpie/array.h>
#include <tpie/tempname.h>
#include <tpie/tpie_log.h>
#include <tpie/stats.h>

#include <tpie/serialization2.h>
#include <tpie/serialization_stream.h>

namespace tpie {

template <typename pred_t>
class serialization_predicate_wrapper {
public:
	typedef bool result_type;
	typedef typename pred_t::first_argument_type first_argument_type;
	typedef typename pred_t::second_argument_type second_argument_type;

private:
	pred_t m_pred;

	struct unserializer {
		const char * p;
		unserializer(const char * p) : p(p) {}
		void read(char * const s, const memory_size_type n) {
			std::copy(p, p + n, s);
			p += n;
		}
	};

public:

	serialization_predicate_wrapper(pred_t pred) : m_pred(pred) {}

	bool operator()(const char * a, const char * b) const {
		first_argument_type v1;
		second_argument_type v2;
		{
			unserializer u1(a);
			unserialize(u1, v1);
		}
		{
			unserializer u2(b);
			unserialize(u2, v2);
		}
		return m_pred(v1, v2);
	}
};

template <typename T, typename pred_t>
class serialization_internal_sort {
	array<char> m_buffer;
	memory_size_type m_index;

	array<char *> m_indexes;
	memory_size_type m_expectedItems;
	memory_size_type m_items;

	const char * idx;
	memory_size_type m_itemsRead;

	memory_size_type m_largestItem;

	pred_t m_pred;

	bool m_full;

public:
	serialization_internal_sort(pred_t pred = pred_t())
		: m_index(0)
		, m_expectedItems(0)
		, m_items(0)
		, m_largestItem(0)
		, m_pred(pred)
		, m_full(false)
	{
	}

	static memory_size_type memory_usage(memory_size_type buffer,
										 memory_size_type nItems) {
		return array<char>::memory_usage(buffer) + array<char *>::memory_usage(nItems);
	}

	void begin(memory_size_type buffer, memory_size_type nItems) {
		m_indexes.resize(nItems);
		m_expectedItems = nItems;
		m_buffer.resize(buffer);
		m_items = m_largestItem = m_index = 0;
		m_full = false;
	}

	///////////////////////////////////////////////////////////////////////////
	/// \brief True if all items up to and including this one fits in buffer.
	///
	/// Once push() returns false, it will keep returning false until
	/// the sequence is sorted, read out, and the buffer has been cleared.
	///////////////////////////////////////////////////////////////////////////
	bool push(const T & item) {
		if (m_indexes.size() == 0)
			m_indexes.resize(m_expectedItems > 0 ? m_expectedItems : 16);

		if (m_items == 0)
			m_index = 0;

		if (m_index >= m_buffer.size()) {
			m_full = true;
			return false;
		}

		if (m_items == m_indexes.size()) {
			log_warning() << "Expected " << m_indexes.size() << ' ' << m_expectedItems << " items, but got more" << std::endl;
			memory_size_type newSize = 2*m_indexes.size();
			array<char *> indexes(newSize);
			std::copy(m_indexes.begin(), m_indexes.end(), indexes.begin());
			m_indexes.swap(indexes);
		}
		memory_size_type oldIndex = m_index;
		serialize(*this, item);
		if (!m_full) {
			m_indexes[m_items++] = &m_buffer[oldIndex];
			m_largestItem = std::max(m_largestItem, m_index - oldIndex);
		}
		return !m_full;
	}

	memory_size_type get_largest_item_size() {
		return m_largestItem;
	}

	void write(const char * const s, const memory_size_type n) {
		if (m_full || m_index + n > m_buffer.size()) {
			m_full = true;
			return;
		}
		std::copy(s, s+n, &m_buffer[m_index]);
		m_index += n;
	}

	void sort() {
		serialization_predicate_wrapper<pred_t> pred(m_pred);
		std::sort(m_indexes.get(), m_indexes.get() + m_items, pred);
		m_itemsRead = 0;
	}

	void pull(T & item) {
		idx = m_indexes[m_itemsRead++];
		unserialize(*this, item);
	}

	void read(char * const s, const memory_size_type n) {
		std::copy(idx, idx + n, s);
		idx += n;
	}

	bool can_read() {
		return m_itemsRead < m_items;
	}

	///////////////////////////////////////////////////////////////////////////
	/// \brief Deallocate buffer and call reset().
	///////////////////////////////////////////////////////////////////////////
	void end() {
		m_buffer.resize(0);
		reset();
	}

	///////////////////////////////////////////////////////////////////////////
	/// \brief Reset sorter, but keep the remembered largest item size and
	/// buffer size.
	///////////////////////////////////////////////////////////////////////////
	void reset() {
		m_index = m_items = m_itemsRead = 0;
		m_full = false;
	}
};

template <typename T, typename pred_t>
class serialization_sort {
	serialization_internal_sort<T, pred_t> m_sorter;
	memory_size_type m_sortedRunsCount;
	memory_size_type m_sortedRunsOffset;
	memory_size_type m_memAvail;
	memory_size_type m_runSize;
	memory_size_type m_minimumItemSize;
	serialization_reader m_reader;
	bool m_open;
	pred_t m_pred;

	memory_size_type m_firstFileUsed;
	memory_size_type m_lastFileUsed;

	std::string m_tempDir;

public:
	serialization_sort(memory_size_type memAvail, memory_size_type minimumItemSize = sizeof(T), pred_t pred = pred_t())
		: m_sorter(pred)
		, m_sortedRunsCount(0)
		, m_sortedRunsOffset(0)
		, m_memAvail(memAvail)
		, m_minimumItemSize(minimumItemSize)
		, m_open(false)
		, m_pred(pred)
		, m_firstFileUsed(0)
		, m_lastFileUsed(0)
	{
	}

	~serialization_sort() {
		m_sortedRunsOffset = 0;
		log_info() << "Remove " << m_firstFileUsed << " through " << m_lastFileUsed << std::endl;
		for (memory_size_type i = m_firstFileUsed; i <= m_lastFileUsed; ++i) {
			std::string path = sorted_run_path(i);
			if (!boost::filesystem::exists(path)) continue;

			serialization_reader rd;
			rd.open(path);
			log_info() << "- " << i << ' ' << rd.file_size() << std::endl;
			increment_temp_file_usage(-static_cast<stream_offset_type>(rd.file_size()));
			rd.close();
			boost::filesystem::remove(path);
		}
	}

	void calc_sorter_params(memory_size_type memAvail) {
		if (memAvail <= serialization_writer::memory_usage()) {
			log_error() << "Not enough memory for run formation; have " << memAvail
				<< " bytes but " << serialization_writer::memory_usage()
				<< " is required for writing a run." << std::endl;
			throw exception("Not enough memory for run formation");
		}
		memAvail -= serialization_writer::memory_usage();

		memory_size_type itsz = m_minimumItemSize;
		memory_size_type lo = 0;
		memory_size_type hi = 1;
		while (m_sorter.memory_usage(hi, (hi + (itsz - 1)) / itsz) <= memAvail) {
			lo = hi;
			hi = lo * 2;
		}
		while (lo < hi - 1) {
			memory_size_type mid = lo + (hi - lo) / 2;
			if (m_sorter.memory_usage(mid, (mid + (itsz - 1)) / itsz) <= memAvail)
				lo = mid;
			else
				hi = mid;
		}
		m_runSize = (lo + (itsz - 1)) / itsz;

		log_info() << "Item buffer = " << lo << ", item size = " << itsz
			<< ", expected no. items = " << m_runSize
			<< ", memory usage = " << m_sorter.memory_usage(lo, m_runSize)
			<< " <= " << memAvail << std::endl;

		m_sorter.begin(lo, m_runSize);
	}

	void begin() {
		log_info() << "Before begin; mem usage = "
			<< get_memory_manager().used() << std::endl;
		calc_sorter_params(m_memAvail);
		log_info() << "After internal sorter begin; mem usage = "
			<< get_memory_manager().used() << std::endl;
		m_tempDir = tempname::tpie_dir_name();
		boost::filesystem::create_directory(m_tempDir);
	}

	void push(const T & item) {
		if (m_sorter.push(item)) return;
		end_run();
		if (!m_sorter.push(item)) {
			throw exception("Couldn't fit a single item in buffer");
		}
	}

	void end() {
		end_run();
		m_sorter.end();
		log_info() << "After internal sorter end; mem usage = "
			<< get_memory_manager().used() << std::endl;
		memory_size_type largestItem = m_sorter.get_largest_item_size();
		if (largestItem == 0) {
			log_warning() << "Largest item is 0 bytes; doing nothing." << std::endl;
			return;
		}
		if (m_memAvail <= serialization_writer::memory_usage()) {
			log_error() << "Not enough memory for merging. "
				<< "mem avail = " << m_memAvail
				<< ", writer usage = " << serialization_writer::memory_usage()
				<< std::endl;
			throw exception("Not enough memory for merging.");
		}
		memory_size_type perFanout = largestItem + serialization_reader::memory_usage();
		memory_size_type fanoutMemory = m_memAvail - serialization_writer::memory_usage();
		memory_size_type fanout = fanoutMemory / perFanout;
		if (fanout < 2) {
			log_error() << "Not enough memory for merging. "
				<< "mem avail = " << m_memAvail
				<< ", fanout memory = " << fanoutMemory
				<< ", per fanout = " << perFanout
				<< std::endl;
			throw exception("Not enough memory for merging.");
		}
		while (m_sortedRunsCount > 1) {
			memory_size_type newCount = 0;
			for (size_t i = 0; i < m_sortedRunsCount; i += fanout, ++newCount) {
				size_t till = std::min(m_sortedRunsCount, i + fanout);
				merge_runs(i, till, m_sortedRunsCount + newCount);
			}
			log_info() << "Advance offset by " << m_sortedRunsCount << std::endl;
			m_sortedRunsOffset += m_sortedRunsCount;
			m_sortedRunsCount = newCount;
		}
	}

private:
	std::string sorted_run_path(memory_size_type idx) {
		std::stringstream ss;
		ss << m_tempDir << '/' << (m_sortedRunsOffset + idx) << ".tpie";
		return ss.str();
	}

	void end_run() {
		m_sorter.sort();
		if (!m_sorter.can_read()) return;
		log_info() << "Write run " << m_sortedRunsCount << std::endl;
		serialization_writer ser;
		m_lastFileUsed = std::max(m_lastFileUsed, m_sortedRunsCount + m_sortedRunsOffset);
		ser.open(sorted_run_path(m_sortedRunsCount++));
		T item;
		while (m_sorter.can_read()) {
			m_sorter.pull(item);
			ser.serialize(item);
		}
		ser.close();
		log_info() << "+ " << (m_sortedRunsCount-1 + m_sortedRunsOffset) << ' ' << ser.file_size() << std::endl;
		increment_temp_file_usage(static_cast<stream_offset_type>(ser.file_size()));
		m_sorter.reset();
	}

	class mergepred {
		pred_t m_pred;

	public:
		typedef std::pair<T, size_t> item_type;

		mergepred(const pred_t & pred) : m_pred(pred) {}

		// Used with std::priority_queue, so invert the original relation.
		bool operator()(const item_type & a, const item_type & b) const {
			return m_pred(b.first, a.first);
		}
	};

	void merge_runs(memory_size_type a, memory_size_type b, memory_size_type dst) {
		log_info() << "Merge runs [" << a << ", " << b << ") into " << dst << std::endl;
		serialization_writer wr;
		wr.open(sorted_run_path(dst));
		m_lastFileUsed = std::max(m_lastFileUsed, m_sortedRunsOffset + dst);
		std::vector<serialization_reader> rd(b-a);
		mergepred p(m_pred);
		std::priority_queue<typename mergepred::item_type, std::vector<typename mergepred::item_type>, mergepred> pq(p);
		size_t i = 0;
		for (memory_size_type p = a; p != b; ++p, ++i) {
			rd[i].open(sorted_run_path(p));
			if (rd[i].can_read()) {
				T item;
				rd[i].unserialize(item);
				pq.push(std::make_pair(item, i));
			}
		}
		while (!pq.empty()) {
			wr.serialize(pq.top().first);
			size_t i = pq.top().second;
			pq.pop();
			if (rd[i].can_read()) {
				T item;
				rd[i].unserialize(item);
				pq.push(std::make_pair(item, i));
			}
		}
		i = 0;
		for (memory_size_type p = a; p != b; ++p, ++i) {
			increment_temp_file_usage(-static_cast<stream_offset_type>(rd[i].file_size()));
			log_info() << "- " << (p+m_sortedRunsOffset) << ' ' << rd[i].file_size() << std::endl;
			rd[i].close();
			boost::filesystem::remove(sorted_run_path(p));
		}
		if (m_firstFileUsed == m_sortedRunsOffset + a) m_firstFileUsed = m_sortedRunsOffset + b;
		wr.close();
		log_info() << "+ " << (dst+m_sortedRunsOffset) << ' ' << wr.file_size() << std::endl;
		increment_temp_file_usage(static_cast<stream_offset_type>(wr.file_size()));
	}

public:
	T pull() {
		T item;
		if (!m_open) {
			m_reader.open(sorted_run_path(0));
			m_open = true;
		}
		m_reader.unserialize(item);
		return item;
	}

	bool can_pull() {
		if (!m_open) return true;
		return m_reader.can_read();
	}
};

}

#endif // __TPIE_SERIALIZATION_SORT_H__
