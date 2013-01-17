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

#include <tpie/array.h>
#include <tpie/tempname.h>
#include <tpie/tpie_log.h>

#include <tpie/serialization2.h>
#include <tpie/serialization_stream.h>

namespace tpie {

template <typename T, typename pred_t>
class serialization_internal_sort {
	tpie::array<char> m_buffer;
	memory_size_type m_index;

	tpie::array<char *> m_indexes;
	memory_size_type m_items;

	const char * idx;
	memory_size_type m_itemsRead;

	memory_size_type m_largestItem;

	pred_t m_pred;

	bool full;

	struct predwrap {
		pred_t m_pred;
		predwrap(pred_t pred) : m_pred(pred) {}

		struct unserializer {
			const char * p;
			unserializer(const char * p) : p(p) {}
			void read(char * const s, const memory_size_type n) {
				std::copy(p, p + n, s);
				p += n;
			}
		};

		bool operator()(const char * a, const char * b) const {
			T v1;
			T v2;
			{
				unserializer u1(a);
				tpie::unserialize(u1, v1);
			}
			{
				unserializer u2(b);
				tpie::unserialize(u2, v2);
			}
			return m_pred(v1, v2);
		}
	};

public:
	serialization_internal_sort(memory_size_type buffer, pred_t pred = pred_t())
		: m_buffer(buffer)
		, m_index(0)
		, m_items(0)
		, m_largestItem(0)
		, m_pred(pred)
		, full(false)
	{
	}

	///////////////////////////////////////////////////////////////////////////
	/// \brief True if all items up to and including this one fits in buffer.
	///
	/// Once serialize() returns false, it will keep returning false until
	/// the sequence is sorted, read out, and the buffer has been cleared.
	///////////////////////////////////////////////////////////////////////////
	bool serialize(const T & item) {
		if (m_items == 0) {
			m_indexes.resize(16);
			m_index = 0;
		} else if (m_items == m_indexes.size()) {
			memory_size_type newSize = 2*m_indexes.size();
			tpie::array<char *> indexes(newSize);
			std::copy(m_indexes.begin(), m_indexes.end(), indexes.begin());
			m_indexes.swap(indexes);
		}
		memory_size_type oldIndex = m_index;
		tpie::serialize(*this, item);
		if (!full) {
			m_indexes[m_items++] = &m_buffer[oldIndex];
			m_largestItem = std::max(m_largestItem, m_index - oldIndex);
		}
		return !full;
	}

	memory_size_type get_largest_item_size() {
		return m_largestItem;
	}

	void write(const char * const s, const memory_size_type n) {
		if (full || m_index + n > m_buffer.size()) {
			full = true;
			return;
		}
		std::copy(s, s+n, &m_buffer[m_index]);
		m_index += n;
	}

	void sort() {
		predwrap pred(m_pred);
		std::sort(&m_indexes[0], &m_indexes[m_items], pred);
		m_itemsRead = 0;
	}

	void unserialize(T & item) {
		idx = m_indexes[m_itemsRead++];
		tpie::unserialize(*this, item);
	}

	void read(char * const s, const memory_size_type n) {
		std::copy(idx, idx + n, s);
		idx += n;
	}

	bool can_read() {
		return m_itemsRead < m_items;
	}

	///////////////////////////////////////////////////////////////////////////
	/// \brief Reset sorter and set buffer size to sz.
	///////////////////////////////////////////////////////////////////////////
	void resize(memory_size_type sz) {
		m_buffer.resize(sz);
		m_indexes.resize(0);
		m_index = m_items = m_itemsRead = m_largestItem = 0;
		full = false;
	}

	void reset() {
		m_indexes.resize(0);
		m_index = m_items = m_itemsRead = 0;
		full = false;
	}
};

template <typename T, typename pred_t>
class serialization_sort {
	serialization_internal_sort<T, pred_t> m_sorter;
	array<temp_file> m_sortedRuns;
	memory_size_type m_bufferSize;
	serialization_reader m_reader;
	bool m_open;
	pred_t m_pred;

public:
	serialization_sort(memory_size_type bufferSize, pred_t pred = pred_t())
		: m_sorter(bufferSize, pred)
		, m_bufferSize(bufferSize)
		, m_open(false)
		, m_pred(pred)
	{
	}

	void begin() {
	}

	void push(const T & item) {
		if (m_sorter.serialize(item)) return;
		end_run();
		if (!m_sorter.serialize(item)) {
			throw tpie::exception("Couldn't fit a single item in buffer");
		}
	}

	void end() {
		end_run();
		memory_size_type largestItem = m_sorter.get_largest_item_size();
		m_sorter.resize(0);
		if (largestItem == 0) {
			log_warning() << "Largest item is 0 bytes; doing nothing." << std::endl;
			return;
		}
		memory_size_type fanout = m_bufferSize / largestItem;
		while (m_sortedRuns.size() > 1) {
			array<temp_file> sortedRuns((m_sortedRuns.size() + (fanout-1)) / fanout);
			size_t j = 0;
			for (size_t i = 0; i < m_sortedRuns.size(); i += fanout, ++j) {
				size_t till = std::min(m_sortedRuns.size(), i + fanout);
				merge_runs(m_sortedRuns.get()+i, m_sortedRuns.get()+till, sortedRuns.get()+j);
			}
			m_sortedRuns.swap(sortedRuns);
		}
	}

private:
	void expand_temp_files() {
		array<temp_file> sortedRuns(m_sortedRuns.size()+1);
		for (size_t i = 0; i < m_sortedRuns.size(); ++i) {
			sortedRuns[i].set_path(m_sortedRuns[i].path());
			m_sortedRuns[i].set_persistent(true);
		}
		m_sortedRuns.swap(sortedRuns);
	}

	void end_run() {
		m_sorter.sort();
		if (!m_sorter.can_read()) return;
		expand_temp_files();
		temp_file & f = m_sortedRuns[m_sortedRuns.size()-1];
		serialization_writer ser;
		ser.open(f.path());
		T item;
		while (m_sorter.can_read()) {
			m_sorter.unserialize(item);
			ser.serialize(item);
		}
		ser.close();
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

	void merge_runs(temp_file * const a, temp_file * const b, temp_file * const dst) {
		serialization_writer wr;
		wr.open(dst->path());
		std::vector<serialization_reader> rd(b-a);
		mergepred p(m_pred);
		std::priority_queue<typename mergepred::item_type, std::vector<typename mergepred::item_type>, mergepred> pq(p);
		for (size_t i = 0; i < b-a; ++i) {
			rd[i].open((a+i)->path());
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
		for (size_t i = 0; i < b-a; ++i) {
			rd[i].close();
		}
		wr.close();
	}

public:
	T pull() {
		T item;
		if (!m_open) {
			m_reader.open(m_sortedRuns[0].path());
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
