
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

