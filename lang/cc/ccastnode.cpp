
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

//////////////////////////////////////////////////////////////////////////////

ast_node_pool ast_node;

//////////////////////////////////////////////////////////////////////////////

void ast_node_t::common_move(ast_node_t &o) {
	t = std::move(o.t);
	ref = o.ref; o.ref = 0;
	next = o.next; o.next = ast_node_none;
	child = o.child; o.child = ast_node_none;
}

std::string ast_node_t::to_str(void) const {
	return std::string(); // TODO
}

void ast_node_t::arraycopy(std::vector<ast_node_id_t> &d,const std::vector<ast_node_id_t> &s) {
	for (auto &id : d)
		if (id != ast_node_none)
			ast_node(id).release();

	d = s;

	for (auto &id : d)
		if (id != ast_node_none)
			ast_node(id).addref();
}

void ast_node_t::arrayrelease(std::vector<ast_node_id_t> &d) {
	for (auto &id : d)
		if (id != ast_node_none)
			ast_node(id).release();
}

ast_node_t &ast_node_t::set_child(const ast_node_id_t n) {
	if (child != n) ast_node.assign(/*to*/child,/*from*/n);
	return *this;
}

ast_node_t &ast_node_t::set_next(const ast_node_id_t n) {
	if (next != n) ast_node.assign(/*to*/next,/*from*/n);
	return *this;
}

ast_node_t &ast_node_t::clear_and_move_assign(token_t &tt) {
	ref = 0;
	t = std::move(tt);
	set_next(ast_node_none);
	set_child(ast_node_none);
	ast_node.update_next(this);
	return *this;
}

ast_node_t &ast_node_t::clear(const token_type_t tt) {
	ref = 0;
	t = token_t(tt);
	set_next(ast_node_none);
	set_child(ast_node_none);
	ast_node.update_next(this);
	return *this;
}

ast_node_t &ast_node_t::addref(void) {
	ref++;
	return *this;
}

ast_node_t &ast_node_t::release(void) {
	if (ref == 0) throw std::runtime_error("ast_node attempt to release when ref == 0");
	if (--ref == 0) clear(token_type_t::none);
	return *this;
}

