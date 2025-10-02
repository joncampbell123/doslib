
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

std::vector<scope_t>				scopes;
std::vector<scope_id_t>				scope_stack;

//////////////////////////////////////////////////////////////////////////////

scope_t &scope(scope_id_t id) {
#if 1//DEBUG
	if (id < scopes.size())
		return scopes[id];

	throw std::out_of_range("scope out of range");
#else
	return scopes[id];
#endif
}

scope_id_t new_scope(void) {
	const scope_id_t r = scopes.size();
	scopes.resize(r+1u);
	return r;
}

scope_id_t push_new_scope(void) {
	const scope_id_t r = new_scope();
	scope_stack.push_back(r);
	return r;
}

void pop_scope(void) {
#if 1//DEBUG
	if (scope_stack.empty()) throw std::runtime_error("Attempt to pop scope stack when empty");
	if (scope_stack.size() == 1) throw std::runtime_error("Attempt to pop global scope");
	assert(scope_stack[0] == scope_global);
#endif
	scope_stack.pop_back();
}

scope_id_t current_scope(void) {
#if 1//DEBUG
	assert(!scope_stack.empty());
#endif
	return scope_stack[scope_stack.size()-1u];
}

