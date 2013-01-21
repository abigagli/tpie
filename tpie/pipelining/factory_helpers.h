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

#ifndef __TPIE_PIPELINING_FACTORY_HELPERS_H__
#define __TPIE_PIPELINING_FACTORY_HELPERS_H__

#include <tpie/pipelining/factory_base.h>
#include <tuple>

namespace tpie {
namespace pipelining {

///////////////////////////////////////////////////////////////////////////////
/// \class factory
/// Node factory for variadic argument generators.
///////////////////////////////////////////////////////////////////////////////
template <template <typename dest_t> class R, typename... T>
class factory : public factory_base {
public:
	factory(T... v) : v(v...) {}

	template<typename dest_t>
	struct generated {
		typedef R<dest_t> type;
	};

	template <typename dest_t>
	inline R<dest_t> construct(const dest_t & dest) const {
		return invoker<sizeof...(T)>::go(dest, *this);
	}

private:
	std::tuple<T...> v;

	template <size_t N, size_t... S>
	class invoker {
	public:
		template <typename dest_t>
		static R<dest_t> go(const dest_t & dest, const factory & parent) {
			return invoker<N-1, N-1, S...>::go(dest, parent);
		}
	};

	template <size_t... S>
	class invoker<0, S...> {
	public:
		template <typename dest_t>
		static R<dest_t> go(const dest_t & dest, const factory & parent) {
			R<dest_t> r(dest, std::get<S>(parent.v)...);
			parent.init_node(r);
			return r;
		}
	};

	template <size_t N, size_t... S>
	friend class invoker;
};

///////////////////////////////////////////////////////////////////////////////
/// \class tempfactory
/// Node factory for variadic argument templated generators.
///////////////////////////////////////////////////////////////////////////////
template <typename Holder, typename... T>
class tempfactory : public factory_base {
public:
	tempfactory(T... v) : v(v...) {}

	template<typename dest_t>
	struct generated {
		typedef typename Holder::template type<dest_t> type;
	};

	template <typename dest_t>
	inline typename Holder::template type<dest_t> construct(const dest_t & dest) const {
		return invoker<sizeof...(T)>::go(dest, *this);
	}

private:
	std::tuple<T...> v;

	template <size_t N, size_t... S>
	class invoker {
	public:
		template <typename dest_t>
		static typename Holder::template type<dest_t> go(const dest_t & dest, const tempfactory & parent) {
			return invoker<N-1, N-1, S...>::go(dest, parent);
		}
	};

	template <size_t... S>
	class invoker<0, S...> {
	public:
		template <typename dest_t>
		static typename Holder::template type<dest_t> go(const dest_t & dest, const tempfactory & parent) {
			typename Holder::template type<dest_t> r(dest, std::get<S>(parent.v)...);
			parent.init_node(r);
			return r;
		}
	};

	template <size_t N, size_t... S>
	friend class invoker;
};

///////////////////////////////////////////////////////////////////////////////
/// \class termfactory
/// Node factory for variadic argument terminators.
///////////////////////////////////////////////////////////////////////////////
template <typename R, typename... T>
class termfactory : public factory_base {
public:
	typedef R generated_type;

	termfactory(T... v) : v(v...) {}

	inline R construct() const {
		return invoker<sizeof...(T)>::go(*this);
	}

private:
	std::tuple<T...> v;

	template <size_t N, size_t... S>
	class invoker {
	public:
		static R go(const termfactory & parent) {
			return invoker<N-1, N-1, S...>::go(parent);
		}
	};

	template <size_t... S>
	class invoker<0, S...> {
	public:
		static R go(const termfactory & parent) {
			R r(std::get<S>(parent.v)...);
			parent.init_node(r);
			return r;
		}
	};

	template <size_t N, size_t... S>
	friend class invoker;
};

///////////////////////////////////////////////////////////////////////////////
// Legacy typedefs
///////////////////////////////////////////////////////////////////////////////

template <template <typename dest_t> class R>
using factory_0 = factory<R>;
template <template <typename dest_t> class R, typename T1>
using factory_1 = factory<R, T1>;
template <template <typename dest_t> class R, typename T1, typename T2>
using factory_2 = factory<R, T1, T2>;
template <template <typename dest_t> class R, typename T1, typename T2, typename T3>
using factory_3 = factory<R, T1, T2, T3>;
template <template <typename dest_t> class R, typename T1, typename T2, typename T3, typename T4>
using factory_4 = factory<R, T1, T2, T3, T4>;
template <template <typename dest_t> class R, typename T1, typename T2, typename T3, typename T4, typename T5>
using factory_5 = factory<R, T1, T2, T3, T4, T5>;
template <template <typename dest_t> class R, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
using factory_6 = factory<R, T1, T2, T3, T4, T5, T6>;

template <typename Holder>
using tempfactory_0 = tempfactory<Holder>;
template <typename Holder, typename T1>
using tempfactory_1 = tempfactory<Holder, T1>;
template <typename Holder, typename T1, typename T2>
using tempfactory_2 = tempfactory<Holder, T1, T2>;
template <typename Holder, typename T1, typename T2, typename T3>
using tempfactory_3 = tempfactory<Holder, T1, T2, T3>;
template <typename Holder, typename T1, typename T2, typename T3, typename T4>
using tempfactory_4 = tempfactory<Holder, T1, T2, T3, T4>;
template <typename Holder, typename T1, typename T2, typename T3, typename T4, typename T5>
using tempfactory_5 = tempfactory<Holder, T1, T2, T3, T4, T5>;
template <typename Holder, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
using tempfactory_6 = tempfactory<Holder, T1, T2, T3, T4, T5, T6>;

template <typename R>
using termfactory_0 = termfactory<R>;
template <typename R, typename T1>
using termfactory_1 = termfactory<R, T1>;
template <typename R, typename T1, typename T2>
using termfactory_2 = termfactory<R, T1, T2>;
template <typename R, typename T1, typename T2, typename T3>
using termfactory_3 = termfactory<R, T1, T2, T3>;
template <typename R, typename T1, typename T2, typename T3, typename T4>
using termfactory_4 = termfactory<R, T1, T2, T3, T4>;
template <typename R, typename T1, typename T2, typename T3, typename T4, typename T5>
using termfactory_5 = termfactory<R, T1, T2, T3, T4, T5>;
template <typename R, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
using termfactory_6 = termfactory<R, T1, T2, T3, T4, T5, T6>;

} // namespace pipelining
} // namespace tpie

#endif // __TPIE_PIPELINING_FACTORY_HELPERS_H__
