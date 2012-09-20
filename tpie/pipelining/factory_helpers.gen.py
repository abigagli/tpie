# -*- mode: c++; tab-width: 4; indent-tabs-mode: t; eval: (progn (c-set-style "stroustrup") (c-set-offset 'innamespace 0)); -*-
# vi:set ts=4 sts=4 sw=4 noet :
# Copyright 2012, The TPIE development team
#
# This file is part of TPIE.
#
# TPIE is free software: you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version.
#
# TPIE is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
# License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with TPIE.  If not, see <http://www.gnu.org/licenses/>

import sys
p = sys.stdout.write

infinity = 6

def header():
	print """// This file was generated by factory_helpers.gen.py, DO NOT EDIT
#ifndef __TPIE_PIPELINING_FACTORY_HELPERS_H__
#define __TPIE_PIPELINING_FACTORY_HELPERS_H__

#include <tpie/pipelining/factory_base.h>

namespace tpie {
namespace pipelining {
"""

def footer():
	p("""} // namespace pipelining
} // namespace tpie

#endif // __TPIE_PIPELINING_FACTORY_HELPERS_H__""")

def gen(types, terminal, templated):
	indices = range(1, 1+types)

	cl = "factory_" + str(types)
	if terminal:
		cl = "term" + cl
	if templated:
		cl = "temp" + cl


	print """%s
/// \class %s
/// Push segment factory for %s-argument %s%s.
%s""" % ("/"*79,
		cl,
		str(types),
		"templated " if templated else "",
		"terminator" if terminal else "generator",
		"/"*79);

	if templated:
		if terminal:
			p("template <typename Holder")
			generator = "typename Holder::type"
		else:
			p("template <typename Holder")
			generator = "typename Holder::template type<dest_t>"
	else:
		if terminal:
			p("template <typename R")
			generator = "R"
		else:
			p("template <template <typename dest_t> class R")
			generator = "R<dest_t>"

	for x in indices:
		p(", typename T%s" % (str(x),))
	print ">"

	print "struct " + cl + " : public factory_base {"
	if terminal:
		print "\ttypedef %s generated_type;\n" % (generator,)
	else:
		print """\ttemplate<typename dest_t>
	struct generated {
		typedef %s type;
	};
""" % (generator,)

	if types > 0:
		print "\tinline " + cl + "(%s) : %s {}\n" % (
				", ".join(["T%d t%d" % (x, x) for x in indices]),
				", ".join(["t%d(t%d)" % (x, x) for x in indices]),)

	print "%s\tinline %s construct(%s) const {" % (
			"" if terminal else "\ttemplate <typename dest_t>\n",
			generator,
			"" if terminal else "const dest_t & dest")
	print "\t\t%s r%s%s%s%s%s;" % (
			generator,
			"(" if not terminal or types > 0 else "",
			"" if terminal else "dest",
			", " if not terminal and types > 0 else "",
			", ".join(["t%d" % x for x in indices]),
			")" if not terminal or types > 0 else "",
			)
	print """\t\tthis->init_segment(r);
		return r;
	}"""

	for x in indices:
		if x == 1:
			print "private:"
		print "\tT" + str(x) + " t" + str(x) + ";"

	print "};\n"

if __name__ == "__main__":
	header()
	xs = range(0, infinity+1)
	for x in xs:
		gen(types=x, terminal=False, templated=False)
		gen(types=x, terminal=False, templated=True)
	for x in xs:
		gen(types=x, terminal=True, templated=False)
		gen(types=x, terminal=True, templated=True)
	footer()
