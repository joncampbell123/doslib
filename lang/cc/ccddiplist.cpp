
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>

#include "cc.hpp"

ddip_list_t::ddip_list_t() : BT() { }
ddip_list_t::~ddip_list_t() { }

ddip_t *ddip_list_t::funcparampair(void) {
	if (!empty()) {
		size_t si = size() - 1u;
		do {
			ddip_t &p = (*this)[si];
			if (p.dd_flags & declarator_t::FL_FUNCTION)
				return &p;
		} while ((si--) != 0u);
	}

	return NULL;
}

void ddip_list_t::addcombine(ddip_t &&x) {
	if (x.empty())
		return;

	if (!empty()) {
		ddip_t &top = back();
		if (ptrmergeable(top,x)) {
			for (auto &p : x.ptr)
				top.ptr.push_back(std::move(p));
			for (auto &a : x.arraydef) {
				top.arraydef.push_back(a);
				a = ast_node_none;
			}
			return;
		}
		if (arraymergeable(top,x)) {
			for (auto &a : x.arraydef) {
				top.arraydef.push_back(a);
				a = ast_node_none;
			}
			return;
		}
	}

	push_back(std::move(x));
}

void ddip_list_t::addcombine(const ddip_t &x) {
	if (x.empty())
		return;

	if (!empty()) {
		ddip_t &top = back();
		if (ptrmergeable(top,x)) {
			for (const auto &p : x.ptr)
				top.ptr.push_back(p);
			for (const auto &a : x.arraydef) {
				top.arraydef.push_back(a);
				ast_node(a).addref();
			}
			return;
		}
		if (arraymergeable(top,x)) {
			for (const auto &a : x.arraydef) {
				top.arraydef.push_back(a);
				if (a != ast_node_none) ast_node(a).addref();
			}
			return;
		}
	}

	push_back(x);
}

//////////////////////////////////////////////////////////////////////////////

std::string ddip_list_to_str(const ddip_list_t &dl) {
	std::string r;
	size_t i=0;

	if (!dl.empty()) {
		while (i < dl.size()) {
			if (i != 0)
				r += "(";

			const auto &e = dl[i++];
			for (const auto &p : e.ptr) {
				r += "*";
				for (unsigned int x=0;x < TQI__MAX;x++) { if (p.tq&(1u<<x)) r += std::string(" ") + type_qualifier_idx_t_str[x]; }
				if (p.tq) r += " ";
			}
		}

		assert(i != 0u);

		do {
			const auto &e = dl[--i];

			if (e.dd_flags & declarator_t::FL_FUNCTION) {
				if (!r.empty()) r += " ";
				r += "(";
				for (size_t pi=0;pi < e.parameters.size();pi++) {
					auto &p = e.parameters[pi];
					std::string sr;

					for (unsigned int i=0;i < TSI__MAX;i++) {
						if (p.spec.type_specifier&(1u<<i)) {
							if (!sr.empty()) sr += " ";
							sr += type_specifier_idx_t_str[i];
						}
					}
					for (unsigned int i=0;i < TQI__MAX;i++) {
						if (p.spec.type_qualifier&(1u<<i)) {
							if (!sr.empty()) sr += " ";
							sr += type_qualifier_idx_t_str[i];
						}
					}
					if (p.spec.type_identifier_symbol != symbol_none) {
						symbol_t &sym = symbol(p.spec.type_identifier_symbol);
						if (!sr.empty()) sr += " ";
						if (sym.name != identifier_none)
							sr += identifier(sym.name).to_str();
						else
							sr += "<anon>";
					}

					sr += ddip_list_to_str(p.decl.ddip);

					if (pi != 0) r += ",";
					r += sr;
				}
				r += ")";
			}

			for (const auto &a : e.arraydef) {
				r += "[";
				if (a != ast_node_none) {
					ast_node_t &an = ast_node(a);
					if (an.t.type == token_type_t::integer)
						r += std::to_string(an.t.v.integer.v.s);
					else
						r += "<expr>";
				}
				r += "]";
			}

			if (i != 0)
				r += ")";
		} while (i != 0u);
	}

	return r;
}

