
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

bool ast_constexpr_leftshift_integer(token_t &r,token_t &op1,token_t &op2) {
	r = op1;
	r.v.integer.flags |= op2.v.integer.flags & integer_value_t::FL_SIGNED;

	if (op2.v.integer.v.v >= 0ll && op2.v.integer.v.v <= 63ll) {
		if (op2.v.integer.v.u != 0ull) {
			const uint64_t chkmsk = (uint64_t)(UINT64_MAX) << (uint64_t)(64ull - op2.v.integer.v.u);
			if (op1.v.integer.v.u & chkmsk) r.v.integer.flags |= integer_value_t::FL_OVERFLOW;
			r.v.integer.v.u = op1.v.integer.v.u << op2.v.integer.v.u;
		}
	}
	else {
		r.v.integer.flags |= integer_value_t::FL_OVERFLOW;
	}

	return true;
}

bool ast_constexpr_leftshift(token_t &r,token_t &op1,token_t &op2) {
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				return ast_constexpr_leftshift_integer(r,op1,op2);
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_rightshift_integer(token_t &r,token_t &op1,token_t &op2) {
	r = op1;
	r.v.integer.flags |= op2.v.integer.flags & integer_value_t::FL_SIGNED;

	if (op2.v.integer.v.v >= 0ll && op2.v.integer.v.v <= 63ll) {
		if (r.v.integer.flags & integer_value_t::FL_SIGNED)
			r.v.integer.v.s = op1.v.integer.v.s >> op2.v.integer.v.s;
		else
			r.v.integer.v.u = op1.v.integer.v.u >> op2.v.integer.v.u;
	}
	else {
		r.v.integer.flags |= integer_value_t::FL_OVERFLOW;
	}

	return true;
}

bool ast_constexpr_rightshift(token_t &r,token_t &op1,token_t &op2) {
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				return ast_constexpr_rightshift_integer(r,op1,op2);
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_lessthan_equals(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				if (op1.v.integer.flags & integer_value_t::FL_SIGNED)
					r.v.integer.v.u = (op1.v.integer.v.v <= op2.v.integer.v.v) ? 1 : 0;
				else
					r.v.integer.v.u = (op1.v.integer.v.u <= op2.v.integer.v.u) ? 1 : 0;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_greaterthan_equals(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				if (op1.v.integer.flags & integer_value_t::FL_SIGNED)
					r.v.integer.v.u = (op1.v.integer.v.v >= op2.v.integer.v.v) ? 1 : 0;
				else
					r.v.integer.v.u = (op1.v.integer.v.u >= op2.v.integer.v.u) ? 1 : 0;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_lessthan(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				if (op1.v.integer.flags & integer_value_t::FL_SIGNED)
					r.v.integer.v.u = (op1.v.integer.v.v < op2.v.integer.v.v) ? 1 : 0;
				else
					r.v.integer.v.u = (op1.v.integer.v.u < op2.v.integer.v.u) ? 1 : 0;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_greaterthan(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				if (op1.v.integer.flags & integer_value_t::FL_SIGNED)
					r.v.integer.v.u = (op1.v.integer.v.v > op2.v.integer.v.v) ? 1 : 0;
				else
					r.v.integer.v.u = (op1.v.integer.v.u > op2.v.integer.v.u) ? 1 : 0;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_equals(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				r.v.integer.v.u = (op1.v.integer.v.u == op2.v.integer.v.u) ? 1 : 0;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_not_equals(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				r.v.integer.v.u = (op1.v.integer.v.u != op2.v.integer.v.u) ? 1 : 0;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_binary_not(token_t &r,token_t &op) {
	/* TODO: type promotion/conversion */
	switch (op.type) {
		case token_type_t::integer:
			r = op;
			r.v.integer.v.u = ~r.v.integer.v.u;
			return true;
		default:
			break;
	};

	return false;
}

bool ast_constexpr_logical_not(token_t &r,token_t &op) {
	/* TODO: type promotion/conversion */
	switch (op.type) {
		case token_type_t::integer:
			r = op;
			r.v.integer.v.u = ast_constexpr_to_bool(op.v.integer) ? 0 : 1;
			return true;
		default:
			break;
	};

	return false;
}

bool ast_constexpr_negate(token_t &r,token_t &op) {
	/* TODO: type promotion/conversion */
	switch (op.type) {
		case token_type_t::integer:
			r = op;
			r.v.integer.v.v = -r.v.integer.v.v;
			r.v.integer.flags |= integer_value_t::FL_SIGNED;
			return true;
		default:
			break;
	};

	return false;
}

bool ast_constexpr_add(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				r.v.integer.v.u += op2.v.integer.v.u;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_subtract(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				r.v.integer.v.u -= op2.v.integer.v.u;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_logical_or(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				r.v.integer.v.u = (ast_constexpr_to_bool(op1.v.integer) || ast_constexpr_to_bool(op2.v.integer)) ? 1 : 0;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_binary_or(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				r.v.integer.v.v |= op2.v.integer.v.v;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_binary_xor(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				r.v.integer.v.v ^= op2.v.integer.v.v;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_logical_and(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				r.v.integer.v.u = (ast_constexpr_to_bool(op1.v.integer) && ast_constexpr_to_bool(op2.v.integer)) ? 1 : 0;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_binary_and(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				r.v.integer.v.v &= op2.v.integer.v.v;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_multiply(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				r = op1;
				if (op1.v.integer.flags & integer_value_t::FL_SIGNED)
					r.v.integer.v.v *= op2.v.integer.v.v;
				else
					r.v.integer.v.u *= op2.v.integer.v.u;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_divide(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				/* divide by zero? NOPE! */
				if (op2.v.integer.v.u == 0ull)
					return false;

				r = op1;
				if (op1.v.integer.flags & integer_value_t::FL_SIGNED)
					r.v.integer.v.v /= op2.v.integer.v.v;
				else
					r.v.integer.v.u /= op2.v.integer.v.u;
				return true;
			default:
				break;
		};
	}

	return false;
}

bool ast_constexpr_modulus(token_t &r,token_t &op1,token_t &op2) {
	/* TODO: type promotion/conversion */
	if (op1.type == op2.type) {
		switch (op1.type) {
			case token_type_t::integer:
				/* divide by zero? NOPE! */
				if (op2.v.integer.v.u == 0ull)
					return false;

				r = op1;
				if (op1.v.integer.flags & integer_value_t::FL_SIGNED)
					r.v.integer.v.v %= op2.v.integer.v.v;
				else
					r.v.integer.v.u %= op2.v.integer.v.u;
				return true;
			default:
				break;
		};
	}

	return false;
}

void ast_node_reduce(ast_node_id_t &eroot,const std::string &prefix) {
#define OP_ONE_PARAM_TEVAL ast_node_id_t op1 = erootnode.child

#define OP_TWO_PARAM_TEVAL ast_node_id_t op1 = erootnode.child; \
	ast_node_id_t op2 = ast_node(op1).next

	if (eroot == ast_node_none)
		return;

#if 1//DEBUG
	if (prefix.empty()) {
		fprintf(stderr,"%senum expr (reducing node#%lu):\n",prefix.c_str(),(unsigned long)eroot);
		debug_dump_ast(prefix+"  ",eroot);
	}
#endif

again:
	for (ast_node_id_t n=eroot;n!=ast_node_none;n=ast_node(n).next)
		ast_node_reduce(ast_node(n).child,prefix+"  ");

	if (eroot != ast_node_none) {
		/* WARNING: stale references will occur if any code during this switch statement creates new AST nodes */
		ast_node_t &erootnode = ast_node(eroot);
		switch (erootnode.t.type) {
			case token_type_t::op_sizeof:
				{
					size_t ptrdref = 0;
					OP_ONE_PARAM_TEVAL;
					do {
						assert(op1 != ast_node_none);
						ast_node_t &an = ast_node(op1);

						if (an.t.type == token_type_t::op_binary_not) {
							op1 = an.child;
						}
						else if (an.t.type == token_type_t::op_pointer_deref) {
							op1 = an.child;
							ptrdref++;
						}
						else if (an.t.type == token_type_t::op_address_of) {
							op1 = an.child;
							ptrdref = ptr_deref_sizeof_addressof;

							while (op1 != ast_node_none && ast_node(op1).t.type == token_type_t::op_address_of)
								op1 = ast_node(op1).child;

							break;
						}
						else {
							break;
						}
					} while(1);
					if (ast_constexpr_sizeof(erootnode.t,ast_node(op1).t,ptrdref)) {
						erootnode.set_child(ast_node_none);
						goto again;
					}
					break;
				}

			case token_type_t::op_alignof:
				{
					size_t ptrdref = 0;
					OP_ONE_PARAM_TEVAL;
					do {
						assert(op1 != ast_node_none);
						ast_node_t &an = ast_node(op1);

						if (an.t.type == token_type_t::op_binary_not) {
							op1 = an.child;
						}
						else if (an.t.type == token_type_t::op_pointer_deref) {
							op1 = an.child;
							ptrdref++;
						}
						else if (an.t.type == token_type_t::op_address_of) {
							op1 = an.child;
							ptrdref = ptr_deref_sizeof_addressof;

							while (op1 != ast_node_none && ast_node(op1).t.type == token_type_t::op_address_of)
								op1 = ast_node(op1).child;

							break;
						}
						else {
							break;
						}
					} while(1);
					if (ast_constexpr_alignof(erootnode.t,ast_node(op1).t,ptrdref)) {
						erootnode.set_child(ast_node_none);
						goto again;
					}
					break;
				}

			case token_type_t::op_negate:
				{
					OP_ONE_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t)) {
						if (ast_constexpr_negate(erootnode.t,ast_node(op1).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_binary_not:
				{
					OP_ONE_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t)) {
						if (ast_constexpr_binary_not(erootnode.t,ast_node(op1).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_logical_not:
				{
					OP_ONE_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t)) {
						if (ast_constexpr_logical_not(erootnode.t,ast_node(op1).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_leftshift:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_leftshift(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_rightshift:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_rightshift(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_lessthan_equals:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_lessthan_equals(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_greaterthan_equals:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_greaterthan_equals(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_lessthan:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_lessthan(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_greaterthan:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_greaterthan(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_equals:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_equals(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_not_equals:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_not_equals(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_add:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_add(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_subtract:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_subtract(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_logical_or:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_logical_or(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_binary_or:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_binary_or(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_binary_xor:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_binary_xor(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_logical_and:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_logical_and(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_binary_and:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_binary_and(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_multiply:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_multiply(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_divide:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_divide(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_modulus:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) && is_ast_constexpr(ast_node(op2).t)) {
						if (ast_constexpr_modulus(erootnode.t,ast_node(op1).t,ast_node(op2).t)) {
							erootnode.set_child(ast_node_none);
							goto again;
						}
					}
					break;
				}

			case token_type_t::op_comma:
				{
					OP_TWO_PARAM_TEVAL;
					if (is_ast_constexpr(ast_node(op1).t) || is_ast_strconstexpr(ast_node(op1).t)) {
						ast_node_id_t nn = ast_node.returnmove(erootnode.next);
						op1 = ast_node.returnmove(erootnode.child);
						op2 = ast_node.returnmove(ast_node(op1).next);

						ast_node.assignmove(eroot,op2);

						ast_node(eroot).set_next(nn);
						ast_node.release(op2);
						ast_node.release(op1);
						ast_node.release(nn);
						goto again;
					}
					break;
				}

			case token_type_t::op_ternary:
				{
					if (is_ast_constexpr(ast_node(erootnode.child).t)) {
						ast_node_id_t nn = ast_node.returnmove(erootnode.next);
						ast_node_id_t cn = ast_node.returnmove(erootnode.child);
						ast_node_id_t tc = ast_node.returnmove(ast_node(cn).next);
						ast_node_id_t fc = ast_node.returnmove(ast_node(tc).next);

						if (ast_constexpr_to_bool(ast_node(cn).t))
							ast_node.assignmove(eroot,tc);
						else
							ast_node.assignmove(eroot,fc);

						ast_node(eroot).set_next(nn);
						ast_node.release(fc);
						ast_node.release(tc);
						ast_node.release(cn);
						ast_node.release(nn);
						goto again;
					}
					break;
				}

			case token_type_t::op_symbol:
				{
					symbol_t &sym = symbol(ast_node(eroot).t.v.symbol);
					if (sym.sym_type == symbol_t::CONST) {
						if (sym.expr != ast_node_none) {
							/* non-destructive copy the token from the symbol.
							 * this will not work if the node has children or sibling (next) */
							if (ast_node(sym.expr).child == ast_node_none && ast_node(sym.expr).next == ast_node_none) {
								ast_node(eroot).t = ast_node(sym.expr).t;
								goto again;
							}
						}
					}
					break;
				}

			default:
				{
					for (ast_node_id_t n=eroot;n!=ast_node_none;n=ast_node(n).next)
						ast_node_reduce(ast_node(n).next,"  ");
					break;
				}
		}
	}

#if 1//DEBUG
	if (prefix.empty()) {
		fprintf(stderr,"%senum expr (reduce-complete):\n",prefix.c_str());
		debug_dump_ast(prefix+"  ",eroot);
	}
#endif
#undef OP_TWO_PARAM_TEVAL
#undef OP_ONE_PARAM_TEVAL
}

