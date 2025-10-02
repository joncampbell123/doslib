
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

ast_node_pool::ast_node_pool() : pool_t() { }
ast_node_pool::~ast_node_pool() { }

ast_node_id_t ast_node_pool::alloc(token_type_t t) {
	ast_node_t &a = __internal_alloc();
	a.clear(t).addref();
	return next++;
}

ast_node_id_t ast_node_pool::alloc(token_t &t) {
	ast_node_t &a = __internal_alloc();
	a.clear_and_move_assign(t).addref();
	return next++;
}

