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

#include <tpie/serialization2.h>
#include <tpie/array.h>

namespace tpie {

template <typename T, typename pred_t>
class serialization_sort {
	tpie::array<char> m_buffer;
	memory_size_type m_index;

	tpie::array<char *> m_indexes;
	memory_size_type m_items;

	const char * idx;
	memory_size_type m_itemsRead;

	pred_t m_pred;

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
	serialization_sort(memory_size_type buffer, memory_size_type items, pred_t pred = pred_t())
		: m_buffer(buffer)
		, m_index(0)
		, m_indexes(items)
		, m_items(0)
		, m_pred(pred)
	{
	}

	void serialize(const T & item) {
		m_indexes[m_items++] = &m_buffer[m_index];
		tpie::serialize(*this, item);
	}

	void write(const char * const s, const memory_size_type n) {
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

};

}

#endif // __TPIE_SERIALIZATION_SORT_H__
