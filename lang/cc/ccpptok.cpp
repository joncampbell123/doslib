
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

/////////////////////////////////////////////////////////////////////

static int pptok_macro_expansion(const pptok_state_t::pptok_macro_ent_t* macro,pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t);

/////////////////////////////////////////////////////////////////////

static int pptok_undef(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
	/* #undef has already been parsed.
	 * the last token we didn't use is left in &t for the caller to parse as most recently obtained,
	 * unless set to token_type_t::none in which case it will fetch another one */
	identifier_id_t s_id = identifier_none;
	pptok_macro_t macro;
	int r;

	(void)pst;

	if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
		return r;

	if (t.type != token_type_t::identifier || t.v.identifier == identifier_none)
		CCERR_RET(EINVAL,t.pos,"Identifier expected");

	identifier.assign(/*to*/s_id,/*from*/t.v.identifier);

	do {
		if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
			return r;

		if (t.type == token_type_t::newline) {
			t = token_t();
			break;
		}

		/* ignore trailing tokens */
	} while (1);

#if 0//DEBUG
	for (size_t i=0;i < pst.cond_block.size();i++) fprintf(stderr,"  ");
	fprintf(stderr,"UNDEF '%s'\n",s_id.to_str().c_str());
#endif

	if (!pst.delete_macro(s_id)) {
#if 0//DEBUG
		fprintf(stderr,"  Macro wasn't defined anyway\n");
#endif
	}

	identifier.release(s_id);
	return 1;
}

static int pptok_eval_expr(integer_value_t &r,std::deque<token_t>::iterator &ib,std::deque<token_t>::iterator ie,const bool subexpr=false) {
	std::stack< std::pair<unsigned char,token_t> > os;
	std::stack<integer_value_t> vs;
	bool expect_op2 = false;
	int er;

	enum {
		NONE=0,
		LOG_OR, /* || */
		LOG_AND, /* && */
		BIT_OR, /* | */
		BIT_XOR, /* ^ */
		BIT_AND, /* & */
		EQU, /* == != */
		CMP, /* < <= > >= */
		SHF, /* << >> */
		AS, /* + - */
		MDR, /* * / % */
		NOT, /* ! ~ */
		NEG /* leading + - */
	};

	if (ib == ie) return errno_return(EINVAL);

	while (ib != ie || !os.empty()) {
		unsigned char lev = NONE;

		if (ib != ie) {
			if (subexpr && (*ib).type == token_type_t::closeparenthesis) {
				/* step past close parenthesis and force loop to end and pop stacks */
				ib++;
				ie = ib;
			}
			else {
				switch ((*ib).type) {
					case token_type_t::plus:
						if (expect_op2) {
							expect_op2 = false; lev = AS; break;
						}
						else {
							lev = NEG; break;
						}
					case token_type_t::minus:
						if (expect_op2) {
							expect_op2 = false; lev = AS; break;
						}
						else {
							lev = NEG; break;
						}
					case token_type_t::tilde:
						lev = NOT; break;
						continue; /* loop while loop again */
					case token_type_t::exclamation:
						lev = NOT; break;
						continue; /* loop while loop again */
					case token_type_t::integer:
						if (expect_op2 == true) return errno_return(EINVAL);
						vs.push((*ib).v.integer); ib++;
						expect_op2 = true;
						continue;
					case token_type_t::openparenthesis:
						if (expect_op2 == true) return errno_return(EINVAL);
						ib++;
						if ((er=pptok_eval_expr(r,ib,ie,/*subexpr*/true)) < 1)
							return er;
						vs.push(r);
						expect_op2 = true;
						continue;
					case token_type_t::pipepipe:
						expect_op2 = false; lev = LOG_OR; break;
					case token_type_t::ampersandampersand:
						expect_op2 = false; lev = LOG_AND; break;
					case token_type_t::pipe:
						expect_op2 = false; lev = BIT_OR; break;
					case token_type_t::caret:
						expect_op2 = false; lev = BIT_XOR; break;
					case token_type_t::ampersand:
						expect_op2 = false; lev = BIT_AND; break;
					case token_type_t::equalequal:
					case token_type_t::exclamationequals:
						expect_op2 = false; lev = EQU; break;
					case token_type_t::lessthan:
					case token_type_t::greaterthan:
					case token_type_t::lessthanequals:
					case token_type_t::greaterthanequals:
						expect_op2 = false; lev = CMP; break;
					case token_type_t::lessthanlessthan:
					case token_type_t::greaterthangreaterthan:
						expect_op2 = false; lev = SHF; break;
					case token_type_t::star:
					case token_type_t::forwardslash:
					case token_type_t::percent:
						expect_op2 = false; lev = MDR; break;
					default:
						return errno_return(EINVAL);
				}
			}
		}

		while (!os.empty()) {
			if (lev <= os.top().first) {
				if (os.top().first == NOT || os.top().first == NEG) {
					if (vs.size() > 0) {
						integer_value_t &a = vs.top();

						/* does not pop, just modifies in place */
						switch (os.top().second.type) {
							case token_type_t::plus:
								/* nothing */
								break;
							case token_type_t::minus:
								a.v.s = -a.v.s;
								break;
							case token_type_t::tilde:
								a.v.u = ~a.v.u;
								break;
							case token_type_t::exclamation:
								a.v.u = (a.v.u == 0) ? 1 : 0;
								break;
							default:
								return errno_return(EINVAL);
						}

#if 0//DEBUG
						fprintf(stderr,"Reduce op %u <= %u: %s\n",lev,os.top().first,os.top().second.to_str().c_str());
#endif

						os.pop();
					}
					else {
						break;
					}
				}
				else {
					if (vs.size() < 2) return errno_return(EINVAL);

					/* a, b pushed to stack in a, b order so pops off b, a */
					integer_value_t b = vs.top(); vs.pop();
					integer_value_t a = vs.top(); vs.pop();

					/* a OP b,
					 * put result in a */
					switch (os.top().second.type) {
						case token_type_t::pipepipe:
							a.v.u = ((a.v.u != 0) || (b.v.u != 0)) ? 1 : 0;
							break;
						case token_type_t::ampersandampersand:
							a.v.u = ((a.v.u != 0) && (b.v.u != 0)) ? 1 : 0;
							break;
						case token_type_t::pipe:
							a.v.u = a.v.u | b.v.u;
							break;
						case token_type_t::caret:
							a.v.u = a.v.u ^ b.v.u;
							break;
						case token_type_t::ampersand:
							a.v.u = a.v.u & b.v.u;
							break;
						case token_type_t::equalequal:
							a.v.u = (a.v.u == b.v.u) ? 1 : 0;
							break;
						case token_type_t::exclamationequals:
							a.v.u = (a.v.u != b.v.u) ? 1 : 0;
							break;
						case token_type_t::lessthan:
							a.v.u = (a.v.s < b.v.s) ? 1 : 0;
							break;
						case token_type_t::greaterthan:
							a.v.u = (a.v.s > b.v.s) ? 1 : 0;
							break;
						case token_type_t::lessthanequals:
							a.v.u = (a.v.s <= b.v.s) ? 1 : 0;
							break;
						case token_type_t::greaterthanequals:
							a.v.u = (a.v.s >= b.v.s) ? 1 : 0;
							break;
						case token_type_t::lessthanlessthan:
							a.v.u = a.v.s << b.v.s;
							break;
						case token_type_t::greaterthangreaterthan:
							a.v.u = a.v.s >> b.v.s;
							break;
						case token_type_t::plus:
							a.v.u = a.v.s + b.v.s;
							break;
						case token_type_t::minus:
							a.v.u = a.v.s - b.v.s;
							break;
						case token_type_t::star:
							a.v.u = a.v.s * b.v.s;
							break;
						case token_type_t::forwardslash:
							if (b.v.s == 0) return errno_return(EINVAL);
							a.v.u = a.v.s / b.v.s;
							break;
						case token_type_t::percent:
							if (b.v.s == 0) return errno_return(EINVAL);
							a.v.u = a.v.s % b.v.s;
							break;
						default:
							return errno_return(EINVAL);
					}

#if 0//DEBUG
					fprintf(stderr,"Reduce op %u <= %u: %s\n",lev,os.top().first,os.top().second.to_str().c_str());
#endif

					vs.push(a);
					os.pop();
				}
			}
			else {
				break;
			}
		}

		if (lev != NONE) {
			assert(ib != ie);
			os.push( std::pair<unsigned char,token_t>(lev,*ib) );
			ib++;
		}
	}

	if (!os.empty())
		return errno_return(EINVAL);
	if (vs.size() != 1)
		return errno_return(EINVAL);

	r = vs.top();
	return 1;
}

static int pptok_line(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
	std::string msg;
	int r;

	(void)pst;

	/* line number */
	if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
		return r;

	/* canonical GCC behavior: The number parameter can be a macro */
	if (t.type == token_type_t::identifier) {
		const pptok_state_t::pptok_macro_ent_t* macro = pst.lookup_macro(t.v.identifier);
		if (macro) {
			if ((r=pptok_macro_expansion(macro,pst,lst,buf,sfo,t)) < 1) /* which affects pptok_lgtok() */
				return r;

			pst.macro_expansion_counter++;
			if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
				return r;
		}
	}

	if (t.type != token_type_t::integer)
		return errno_return(EINVAL);
	if (t.v.integer.v.s < 0ll || t.v.integer.v.s > 0xFFFFll)
		return errno_return(EINVAL);

	const unsigned int row = (unsigned int)t.v.integer.v.u;
	std::string name;

	if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
		return r;

	/* optional quoted filename */
	if (t.type == token_type_t::strliteral) {
		name = csliteral(t.v.csliteral).makestring();
		if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
			return r;

		/* TODO: This needs to become the token source name */
	}

	do {
		if (t.type == token_type_t::newline) {
			t = token_t();
			break;
		}
		else if (t.type == token_type_t::backslashnewline) {
			continue;
		}

		if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
			return r;
	} while(1);

	buf.pos.row = row;
	return 1;
}

static int pptok_errwarn(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t,const bool is_err) {
	const int line = t.pos.row;
	std::string msg;
	int r;

	(void)pst;

	do {
		if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
			return r;

		if (t.type == token_type_t::newline) {
			t = token_t();
			break;
		}
		else if (t.type == token_type_t::backslashnewline) {
			continue;
		}
		else if (t.type == token_type_t::strliteral) {
			if (!msg.empty()) msg += " ";
			msg += csliteral(t.v.csliteral).makestring();
		}
		else if (t.type == token_type_t::identifier) {
			if (!msg.empty()) msg += " ";
			msg += identifier(t.v.identifier).to_str();
		}
		else {
			if (!msg.empty()) msg += " ";
			msg += token_type_t_str(t.type);
		}
	} while (1);

	printf("%s(%d): %s\n",is_err?"Error":"Warning",line,msg.c_str());
	return 1;
}

static int pptok_if(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t,const bool is_if) {
	/* #if has already been parsed.
	 * the last token we didn't use is left in &t for the caller to parse as most recently obtained,
	 * unless set to token_type_t::none in which case it will fetch another one */
	std::deque<token_t> expr;
	int r;

	(void)pst;

	do {
		if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
			return r;

		if (t.type == token_type_t::newline) {
			t = token_t();
			break;
		}
		else if (t.type == token_type_t::backslashnewline) {
			continue;
		}
		else if (t.type == token_type_t::floating) {
			return errno_return(EINVAL);
		}
		else if (t.type == token_type_t::strliteral) {
			return errno_return(EINVAL);
		}
		else if (t.type == token_type_t::charliteral) {
			return errno_return(EINVAL);
		}
		else if (t.type == token_type_t::r_true || t.type == token_type_t::r_false) {
			const bool res = (t.type == token_type_t::r_true);
			t = token_t(token_type_t::integer);
			t.v.integer.init();
			t.v.integer.v.u = (res > 0) ? 1 : 0;
		}
		else if (t.type == token_type_t::identifier) {
			if (identifier(t.v.identifier) == "defined") { /* defined(MACRO) */
				int paren = 0;
				int res = -1;

				do {
					if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
						return r;

					if (t.type == token_type_t::openparenthesis) {
						paren++;
					}
					else if (t.type == token_type_t::closeparenthesis) {
						if (paren == 0) return errno_return(EINVAL);
						paren--;
						if (paren == 0) break;
					}
					else if (t.type == token_type_t::identifier) {
						if (res >= 0) return errno_return(EINVAL);
						res = (pst.lookup_macro(t.v.identifier) != NULL) ? 1 : 0;
					}
					else if (t.type == token_type_t::newline) {
						pptok_lgtok_ungetch(pst,t);
						break;
					}
					else {
						break;
					}
				} while (1);

				if (res < 0)
					return errno_return(EINVAL);

				t = token_t(token_type_t::integer);
				t.v.integer.init();
				t.v.integer.v.u = (res > 0) ? 1 : 0;
			}
			else {
				const pptok_state_t::pptok_macro_ent_t* macro = pst.lookup_macro(t.v.identifier);
				if (macro) {
					if ((r=pptok_macro_expansion(macro,pst,lst,buf,sfo,t)) < 1) /* which affects pptok_lgtok() */
						return r;

					pst.macro_expansion_counter++;
					continue;
				}
				else {
					t = token_t(token_type_t::integer);
					t.v.integer.init();
					t.v.integer.v.u = 0;
				}
			}
		}

		expr.push_back(std::move(t));
	} while (1);

#if 0//DEBUG
	for (size_t i=0;i < pst.cond_block.size();i++) fprintf(stderr,"  ");
	fprintf(stderr,"%s (SUBST1)\n",is_if?"IF":"ELIF");
	for (auto i=expr.begin();i!=expr.end();i++) fprintf(stderr,"  > %s\n",(*i).to_str().c_str());
#endif

	integer_value_t rv;
	rv.init();

	{
		std::deque<token_t>::iterator ib = expr.begin();

		if ((r=pptok_eval_expr(rv,ib,expr.end())) < 1)
			return r;

		if (ib != expr.end())
			return errno_return(EINVAL);
	}

#if 0//DEBUG
	fprintf(stderr,"%s Result %s\n",is_if?"IF":"ELIF",rv.to_str().c_str());
#endif

	if (expr.empty())
		return errno_return(EINVAL);

	const bool cond = (rv.v.u != 0);

	if (is_if) {
		pptok_state_t::cond_block_t cb;

		/* if the condition matches and the parent condition if any is true */
		if (pst.condb_true())
			cb.state = cond ? pptok_state_t::cond_block_t::IS_TRUE : pptok_state_t::cond_block_t::WAITING;
		else
			cb.state = pptok_state_t::cond_block_t::DONE; /* never will be true */

		pst.cond_block.push(std::move(cb));
	}
	else {
		if (pst.cond_block.empty())
			return errno_return(EINVAL); /* #elifdef to what?? */

		auto &ent = pst.cond_block.top();
		if (ent.flags & pptok_state_t::cond_block_t::FL_ELSE) return errno_return(EINVAL); /* #elif cannot follow #else */

		if (ent.state == pptok_state_t::cond_block_t::WAITING) {
			if (cond) ent.state = pptok_state_t::cond_block_t::IS_TRUE;
		}
		else if (ent.state == pptok_state_t::cond_block_t::IS_TRUE) {
			ent.state = pptok_state_t::cond_block_t::DONE;
		}
	}

	return 1;
}

static int pptok_ifdef(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t,const bool is_ifdef,const bool match_cond) {
	/* #ifdef has already been parsed.
	 * the last token we didn't use is left in &t for the caller to parse as most recently obtained,
	 * unless set to token_type_t::none in which case it will fetch another one */
	identifier_id_t s_id = identifier_none;
	pptok_macro_t macro;
	int r;

	(void)pst;

	if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
		return r;

	if (t.type != token_type_t::identifier || t.v.identifier == identifier_none)
		CCERR_RET(EINVAL,t.pos,"Identifier expected");

	identifier.assign(/*to*/s_id,/*from*/t.v.identifier);

	do {
		if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
			return r;

		if (t.type == token_type_t::newline) {
			t = token_t();
			break;
		}

		/* ignore trailing tokens */
	} while (1);

#if 0//DEBUG
	for (size_t i=0;i < pst.cond_block.size();i++) fprintf(stderr,"  ");
	fprintf(stderr,"%s '%s'\n",is_ifdef?(match_cond?"IFDEF":"IFNDEF"):(match_cond?"ELIFDEF":"ELIFNDEF"),s_id.to_str().c_str());
#endif

	const bool cond = (pst.lookup_macro(s_id) != NULL) == match_cond;

	if (is_ifdef) {
		pptok_state_t::cond_block_t cb;

		/* if the condition matches and the parent condition if any is true */
		if (pst.condb_true())
			cb.state = cond ? pptok_state_t::cond_block_t::IS_TRUE : pptok_state_t::cond_block_t::WAITING;
		else
			cb.state = pptok_state_t::cond_block_t::DONE; /* never will be true */

		pst.cond_block.push(std::move(cb));
	}
	else {
		if (pst.cond_block.empty())
			return errno_return(EINVAL); /* #elifdef to what?? */

		auto &ent = pst.cond_block.top();
		if (ent.flags & pptok_state_t::cond_block_t::FL_ELSE) return errno_return(EINVAL); /* #elifdef,etc cannot follow #else */

		if (ent.state == pptok_state_t::cond_block_t::WAITING) {
			if (cond) ent.state = pptok_state_t::cond_block_t::IS_TRUE;
		}
		else if (ent.state == pptok_state_t::cond_block_t::IS_TRUE) {
			ent.state = pptok_state_t::cond_block_t::DONE;
		}
	}

	identifier.release(s_id);
	return 1;
}

static int pptok_else(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
	/* #else has already been parsed.
	 * the last token we didn't use is left in &t for the caller to parse as most recently obtained,
	 * unless set to token_type_t::none in which case it will fetch another one */
	pptok_macro_t macro;
	int r;

	(void)pst;

	do {
		if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
			return r;

		if (t.type == token_type_t::newline) {
			t = token_t();
			break;
		}

		/* ignore trailing tokens */
	} while (1);

#if 0//DEBUG
	for (size_t i=0;i < pst.cond_block.size();i++) fprintf(stderr,"  ");
	fprintf(stderr,"ELSE condempty=%u\n",pst.cond_block.empty());
#endif

	if (pst.cond_block.empty())
		return errno_return(EINVAL); /* #else to what?? */

	auto &ent = pst.cond_block.top();
	if (ent.flags & pptok_state_t::cond_block_t::FL_ELSE) return errno_return(EINVAL); /* #else cannot follow #else */
	ent.flags |= pptok_state_t::cond_block_t::FL_ELSE;

	/* if nothing else was true, this block is true
	 * if the previous block was true, this is false */
	if (ent.state == pptok_state_t::cond_block_t::WAITING)
		ent.state = pptok_state_t::cond_block_t::IS_TRUE;
	else if (ent.state == pptok_state_t::cond_block_t::IS_TRUE)
		ent.state = pptok_state_t::cond_block_t::DONE;
	return 1;
}

static int pptok_endif(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
	/* #endif has already been parsed.
	 * the last token we didn't use is left in &t for the caller to parse as most recently obtained,
	 * unless set to token_type_t::none in which case it will fetch another one */
	pptok_macro_t macro;
	int r;

	(void)pst;

	do {
		if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
			return r;

		if (t.type == token_type_t::newline) {
			t = token_t();
			break;
		}

		/* ignore trailing tokens */
	} while (1);

#if 0//DEBUG
	for (size_t i=1;i < pst.cond_block.size();i++) fprintf(stderr,"  ");
	fprintf(stderr,"ENDIF\n");
#endif

	if (pst.cond_block.empty())
		return errno_return(EPIPE);

	pst.cond_block.pop();
	return 1;
}

static int pptok_define(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
	/* #define has already been parsed.
	 * the last token we didn't use is left in &t for the caller to parse as most recently obtained,
	 * unless set to token_type_t::none in which case it will fetch another one */
	identifier_id_t s_id_va_args_subst = identifier_none;
	identifier_id_t s_id = identifier_none;
	pptok_macro_t macro;
	int r;

	(void)pst;

	if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
		return r;

	if (t.type != token_type_t::identifier || t.v.identifier == identifier_none)
		CCERR_RET(EINVAL,t.pos,"Identifier expected");

	identifier.assign(/*to*/s_id,/*from*/t.v.identifier);

	/* if the next character is '(' (without a space), it's a parameter list.
	 * a space and then '(' doesn't count. that's how GCC behaves, anyway. */
	if (buf.peekb() == '(') {
		buf.discardb(); /* don't bother with parsing as token */

		macro.flags |= pptok_macro_t::FL_PARENTHESIS;

		do {
			identifier_id_t s_p = identifier_none;

			if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
				return r;
			if (t.type == token_type_t::newline)
				CCERR_RET(EINVAL,t.pos,"Unexpected newline");
			if (t.type == token_type_t::closeparenthesis)
				break;
			if (macro.flags & pptok_macro_t::FL_VARIADIC) /* you can't put additional params past the ... or even a comma */
				CCERR_RET(EINVAL,t.pos,"Cannot define macro parameters after variadic ellipsis");
			if (t.type == token_type_t::ellipsis) {
				macro.flags |= pptok_macro_t::FL_VARIADIC;
				continue;
			}
			if (t.type != token_type_t::identifier)
				CCERR_RET(EINVAL,t.pos,"Identifier expected");

			eat_whitespace(buf,sfo);

			/* GNU GCC arg... variadic macros that predate __VA_ARGS__ */
			if (buf.peekb(0) == '.' && buf.peekb(1) == '.' && buf.peekb(2) == '.') {
				identifier.assign(/*to*/s_id_va_args_subst,/*from*/t.v.identifier);
				macro.flags |= pptok_macro_t::FL_VARIADIC | pptok_macro_t::FL_NO_VA_ARGS;
				buf.discardb(3);
			}
			else {
				identifier.assign(/*to*/s_p,/*from*/t.v.identifier);
				macro.parameters.push_back(s_p);
			}

			if (buf.peekb() == ',') { buf.discardb(); }
			else if (buf.peekb() == ')') { buf.discardb(); break; }
			else CCERR_RET(EINVAL,buf.pos,"Unexpected characters in macro parameter list");
		} while (1);
	}

	do {
		if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
			return r;

		if (t.type == token_type_t::identifier) {
			if (identifier(t.v.identifier) == "__VA_ARGS__") {
				if (macro.flags & pptok_macro_t::FL_VARIADIC) {
					t = token_t(token_type_t::r___VA_ARGS__,t.pos,t.source_file);
				}
			}
			else if (identifier(t.v.identifier) == "__VA_OPT__") {
				if (macro.flags & pptok_macro_t::FL_VARIADIC) {
					t = token_t(token_type_t::r___VA_OPT__,t.pos,t.source_file);
				}
			}
		}

		if (t.type == token_type_t::newline) {
			t = token_t();
			break;
		}
		else if (t.type == token_type_t::backslashnewline) { /* \ + newline continues the macro past newline */
			macro.tokens.push_back(std::move(token_t(token_type_t::newline,t.pos,t.source_file)));
		}
		else if (t.type == token_type_t::identifier) {
			/* if the identifier matches a paraemeter then put in a parameter reference,
			 * else pass the identifier along. */

			if (!macro.tokens.empty() && macro.tokens[macro.tokens.size()-1u].type == token_type_t::pound)
				macro.flags |= pptok_macro_t::FL_STRINGIFY;

			if (s_id_va_args_subst != identifier_none && identifier(s_id_va_args_subst) == identifier(t.v.identifier)) {
				/* GNU arg... variadic convert to __VA_ARGS__ */
				macro.tokens.push_back(std::move(token_t(token_type_t::r___VA_ARGS__,t.pos,t.source_file)));
			}
			else {
				for (auto pi=macro.parameters.begin();pi!=macro.parameters.end();pi++) {
					if (identifier(*pi) == identifier(t.v.identifier)) {
						t = token_t(token_type_t::r_macro_paramref);
						t.v.paramref = size_t(pi - macro.parameters.begin());
						break;
					}
				}
			}

			macro.tokens.push_back(std::move(t));
		}
		else if (pptok_define_allowed_token(t)) {
			if (t.type == token_type_t::r___VA_ARGS__) {
				if (macro.flags & pptok_macro_t::FL_NO_VA_ARGS) {
					/* do not expand __VA_ARGS__ if GNU args... syntax used */
					continue;
				}
				else if (macro.tokens.size() >= 2) {
					const size_t i = macro.tokens.size() - 2;
					/* GNU extension: expression, ##__VA_ARGS__ which means the same thing as expression __VA_OPT__(,) __VA_ARGS__ */
					if (macro.tokens[i].type == token_type_t::comma && macro.tokens[i+1].type == token_type_t::poundpound) {
						/* convert ,##__VA_ARGS__ to __VA__OPT__(,) __VA_ARGS__ and then continue to bottom to add __VA_ARGS__ */
						macro.tokens.resize(i+4);
						macro.tokens[i+0] = std::move(token_t(token_type_t::r___VA_OPT__));
						macro.tokens[i+1] = std::move(token_t(token_type_t::openparenthesis));
						macro.tokens[i+2] = std::move(token_t(token_type_t::comma,t.pos,t.source_file));
						macro.tokens[i+3] = std::move(token_t(token_type_t::closeparenthesis));
					}
				}
			}

			macro.tokens.push_back(std::move(t));
		}
		else {
			return errno_return(EINVAL);
		}
	} while (1);

#if 0//DEBUG
	fprintf(stderr,"MACRO '%s'",identifier(s_id).to_str().c_str());
	if (macro.flags & pptok_macro_t::FL_PARENTHESIS) fprintf(stderr," PARENTHESIS");
	if (macro.flags & pptok_macro_t::FL_VARIADIC) fprintf(stderr," VARIADIC");
	if (macro.flags & pptok_macro_t::FL_NO_VA_ARGS) fprintf(stderr," NO_VA_ARGS");
	if (macro.flags & pptok_macro_t::FL_STRINGIFY) fprintf(stderr," STRINGIFY");
	fprintf(stderr,"\n");
	fprintf(stderr,"  parameters:\n");
	for (auto i=macro.parameters.begin();i!=macro.parameters.end();i++)
		fprintf(stderr,"    > %s\n",identifier(*i).to_str().c_str());
	fprintf(stderr,"  tokens:\n");
	for (auto i=macro.tokens.begin();i!=macro.tokens.end();i++)
		fprintf(stderr,"    > %s\n",(*i).to_str().c_str());
#endif

	// NTS:
	//  Open Watcom header rel/h/win/win16.h
	//    WM_SPOOLERSTATUS is defined twice, but with the exact same value.
	//    many compilers overlook #defining a macro multiple times IF each
	//    time the value is exactly the same. We need to do that too:
	//    If the macro exists but the tokens are the same, silently ignore
	//    it instead of throwing an error.
	if (!pst.create_macro(s_id,macro))
		return errno_return(EEXIST);

	identifier.release(s_id_va_args_subst);
	identifier.release(s_id);
	return 1;
}

static int pptok_macro_expansion(const pptok_state_t::pptok_macro_ent_t* macro,pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
	/* caller just parsed the identifier token */
	int r;

#if 0//DEBUG
	fprintf(stderr,"Hello macro '%s'\n",t.v.strliteral.to_str().c_str());
#endif

	if (pst.macro_expansion_counter > 1024)
		CCERR_RET(ELOOP,buf.pos,"Too many macro expansions");

	std::vector< std::vector<token_t> > params;
	std::vector< rbuf > params_str;

	if (macro->ment.flags & pptok_macro_t::FL_PARENTHESIS) {
		if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
			return r;
		if (t.type != token_type_t::openparenthesis)
			return errno_return(EINVAL);

		if (macro->ment.flags & pptok_macro_t::FL_STRINGIFY) {
			/* parse every arg as a string, then convert to tokens, but preserve the string
			 * so the code below can properly expand #parameter to string */

			/* NTS: #__VA_ARGS__ is valid, and it stringifies into every parameter past the
			 *      last named parameter INCLUDING the commas!
			 *
			 *      i.e. #define XYZ(a,...) #a #__VA_ARGS__
			 *           XYZ(a,b,c,d) would become "ab,c,d" */

			do {
				eat_whitespace(buf,sfo);
				if (buf.peekb() == ')') {
					buf.discardb();
					break;
				}

				bool allow_commas = false;
				unsigned int paren = 0;
				rbuf arg_str;

				/* Apparently in variadic macros, #__VA_ARGS__ expands to the extra parameters INCLUDING commas */
				if ((macro->ment.flags & pptok_macro_t::FL_VARIADIC) && params_str.size() >= macro->ment.parameters.size())
					allow_commas = true;

				/* if you're calling a macro with 4096 chars in it you are using too much, split it up! */
				arg_str.pos = buf.pos;
				arg_str.set_source_file(buf.source_file);
				if (!arg_str.allocate(4096))
					return errno_return(ENOMEM);

				do {
					if (buf.data_avail() < 1) rbuf_sfd_refill(buf,sfo);
					if (buf.data_avail() == 0) return errno_return(EINVAL);

					unsigned char c = buf.peekb();
					if (c == ',' && paren == 0 && !allow_commas) {
						buf.discardb();
						break;
					}

					/* string constants are copied as-is and then stringified like that,
					 * except that within strings, comma and parens are part of the string,
					 * however escapes like \t and \n are also stringified as-is HOWEVER
					 * the backslash even if copied still works as an escape i.e you can
					 * use \" to put \" in the stringified string without closing the string. */
					if (c == '\"' || c == '\'') {
						const unsigned char ec = c;
						if (arg_str.end >= arg_str.fence) return errno_return(ENOSPC);
						*(arg_str.end++) = c;
						buf.discardb();

						do {
							if (buf.data_avail() < 1) rbuf_sfd_refill(buf,sfo);
							if (buf.data_avail() == 0) return errno_return(EINVAL);
							c = buf.getb();
							if (arg_str.end >= arg_str.fence) return errno_return(ENOSPC);
							*(arg_str.end++) = c;

							if (c == ec) {
								break;
							}
							else if (c == '\\') {
								if (buf.data_avail() < 1) rbuf_sfd_refill(buf,sfo);
								if (buf.data_avail() == 0) return errno_return(EINVAL);
								c = buf.getb();
								if (arg_str.end >= arg_str.fence) return errno_return(ENOSPC);
								*(arg_str.end++) = c;
							}
						} while (1);

						continue;
					}

					if (c == '(') {
						paren++;
					}
					else if (c == ')') {
						if (paren == 0) break; /* leave it for the outer loop */
						paren--;
					}

					if (arg_str.end >= arg_str.fence) return errno_return(ENOSPC);
					*(arg_str.end++) = c;
					buf.discardb();

					if (c == ' ') eat_whitespace(buf,sfo);
				} while (1);

				while (arg_str.end > arg_str.data && is_whitespace(*(arg_str.end-1)))
					arg_str.end--;

				params_str.push_back(std::move(arg_str));
			} while (1);

#if 0//DEBUG
			fprintf(stderr,"  Parameters filled in at call (stringify):\n");
			for (auto i=params_str.begin();i!=params_str.end();i++) {
				assert((*i).data != NULL);
				fprintf(stderr,"    [%u]\n",(unsigned int)(i-params_str.begin()));
				fprintf(stderr,"      \"");
				fwrite((*i).data,(*i).data_avail(),1,stderr);
				fprintf(stderr,"\"\n");
			}
#endif

			for (auto i=params_str.begin();i!=params_str.end();i++) {
				source_file_object subsfo;
				std::vector<token_t> arg;
				auto &subbuf = *i;
				position_t saved_pos = subbuf.pos;

				eat_whitespace(subbuf,subsfo);
				while (subbuf.data_avail() > 0) {
					if ((r=pptok_lgtok(pst,lst,subbuf,subsfo,t)) < 1)
						return r;
					if (!pptok_define_allowed_token(t))
						return errno_return(EINVAL);

					arg.push_back(std::move(t));
					eat_whitespace(subbuf,subsfo);
				}

				params.push_back(std::move(arg));
				subbuf.data = subbuf.base;
				subbuf.pos = saved_pos;
			}
		}
		else {
			std::vector<token_t> arg;
			unsigned int paren = 0;

			do {
				if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
					return r;
				if (!pptok_define_allowed_token(t))
					return errno_return(EINVAL);

				if (t.type == token_type_t::closeparenthesis) {
					if (paren == 0) {
						if (!params.empty() || !arg.empty()) {
							params.push_back(std::move(arg)); arg.clear();
						}
						break;
					}
					else {
						paren--;
						arg.push_back(std::move(t));
					}
				}
				else if (t.type == token_type_t::openparenthesis) {
					paren++;
					arg.push_back(std::move(t));
				}
				else if (t.type == token_type_t::comma && paren == 0) {
					params.push_back(std::move(arg)); arg.clear();
				}
				else {
					arg.push_back(std::move(t));
				}
			} while (1);
		}

#if 0//DEBUG
		fprintf(stderr,"  Parameters filled in at call:\n");
		for (auto i=params.begin();i!=params.end();i++) {
			auto &ent = *i;

			fprintf(stderr,"    [%u]\n",(unsigned int)(i-params.begin()));
			for (auto j=ent.begin();j!=ent.end();j++)
				fprintf(stderr,"      %s\n",(*j).to_str().c_str());
		}

		if (params.size() < macro->ment.parameters.size())
			return errno_return(EPIPE);
		if (params.size() > macro->ment.parameters.size() && !(macro->ment.flags & pptok_macro_t::FL_VARIADIC))
			return errno_return(E2BIG);
#endif
	}

	/* inject tokens from macro */
	std::vector<token_t> out;
	for (auto i=macro->ment.tokens.begin();i!=macro->ment.tokens.end();i++) {
go_again:
		/* check for macro param stringification */
		if ((*i).type == token_type_t::pound) {
			auto i2 = i; i2++; /* assume i != macro->ment.tokens.end() */
			if (i2 != macro->ment.tokens.end()) {
				if ((*i2).type == token_type_t::r_macro_paramref) {
					/* #param stringification */
					i++; /* step forward to ref */
					assert((*i).v.paramref < params_str.size());
					const auto &rb = params_str[(*i).v.paramref];

					if (rb.data_avail() > 0) {
						token_t st;
						st.type = token_type_t::strliteral;
						st.set_source_file(rb.source_file);
						st.v.csliteral = csliteral.alloc();
						st.pos = rb.pos;

						if (!csliteral(st.v.csliteral).alloc(rb.data_avail()))
							return errno_return(ENOMEM);

						assert(csliteral(st.v.csliteral).length == rb.data_avail());
						memcpy(csliteral(st.v.csliteral).data,rb.data,rb.data_avail());
						out.push_back(std::move(st));
					}

					continue;
				}
				else if ((*i2).type == token_type_t::r___VA_ARGS__) {
					/* #__VA_ARGS__ stringification */
					i++; /* step forward to ref */

					if (macro->ment.flags & pptok_macro_t::FL_VARIADIC) {
						for (size_t pi=macro->ment.parameters.size();pi < params_str.size();pi++) {
							const auto &rb = params_str[pi];
							token_t st;

							st.type = token_type_t::strliteral;
							st.set_source_file(rb.source_file);
							st.v.csliteral = csliteral.alloc();
							st.pos = rb.pos;

							if (!csliteral(st.v.csliteral).alloc(rb.data_avail()))
								return errno_return(ENOMEM);

							assert(csliteral(st.v.csliteral).length == rb.data_avail());
							memcpy(csliteral(st.v.csliteral).data,rb.data,rb.data_avail());
							out.push_back(std::move(st));
						}

						continue;
					}
				}
			}
		}

		if ((*i).type == token_type_t::r_macro_paramref) {
			assert((*i).v.paramref < params.size());
			const auto &param = params[(*i).v.paramref];
			for (auto j=param.begin();j!=param.end();j++)
				out.push_back(*j);
		}
		else if ((*i).type == token_type_t::r___VA_OPT__) {
			i++; if (i == macro->ment.tokens.end()) return errno_return(EINVAL); /* skip __VA_OPT__ */
			if ((*i).type != token_type_t::openparenthesis) return errno_return(EINVAL);
			i++; if (i == macro->ment.tokens.end()) return errno_return(EINVAL); /* skip opening paren */

			unsigned int sparen = 0;
			bool do_copy = false;

			if ((macro->ment.flags & pptok_macro_t::FL_VARIADIC) && params.size() > macro->ment.parameters.size())
				do_copy = true;

			do {
				if (i == macro->ment.tokens.end()) return errno_return(EINVAL);
				const auto &current = (*i); i++;

				if (current.type == token_type_t::closeparenthesis) {
					if (sparen == 0) {
						break;
					}
					else {
						sparen--;
						if (do_copy) out.push_back(current);
					}
				}
				else if (current.type == token_type_t::openparenthesis) {
					sparen++;
					if (do_copy) out.push_back(current);
				}
				else if (current.type == token_type_t::pound && i != macro->ment.tokens.end() &&
						(*i).type == token_type_t::r_macro_paramref) {
					const auto &pncurrent = (*i); i++;
					assert(pncurrent.v.paramref < params_str.size());
					const auto &rb = params_str[pncurrent.v.paramref];

					if (rb.data_avail() > 0) {
						token_t st;
						st.type = token_type_t::strliteral;
						st.set_source_file(rb.source_file);
						st.v.csliteral = csliteral.alloc();
						st.pos = rb.pos;

						if (!csliteral(st.v.csliteral).alloc(rb.data_avail()))
							return errno_return(ENOMEM);

						assert(csliteral(st.v.csliteral).length == rb.data_avail());
						memcpy(csliteral(st.v.csliteral).data,rb.data,rb.data_avail());
						out.push_back(std::move(st));
					}
				}
				else if (current.type == token_type_t::r_macro_paramref) {
					assert(current.v.paramref < params.size());
					const auto &param = params[current.v.paramref];
					for (auto j=param.begin();j!=param.end();j++)
						out.push_back(*j);
				}
				else if (current.type == token_type_t::r___VA_OPT__) {
					/* GCC doesn't allow it, neither do we */
					fprintf(stderr,"You can't use __VA_OPT__ inside __VA_OPT__\n");
					return errno_return(EINVAL);
				}
				else {
					if (do_copy) out.push_back(current);
				}
			} while (1);

			/* this is a for loop that will do i++, jump back to beginning */
			if (i == macro->ment.tokens.end())
				break;
			else
				goto go_again;
		}
		else if ((*i).type == token_type_t::r___VA_ARGS__) {
			if (macro->ment.flags & pptok_macro_t::FL_VARIADIC) {
				for (size_t pi=macro->ment.parameters.size();pi < params.size();pi++) {
					if (pi != macro->ment.parameters.size())
						out.push_back(std::move(token_t(token_type_t::comma,(*i).pos,(*i).source_file)));

					const auto &param = params[pi];
					for (auto j=param.begin();j!=param.end();j++)
						out.push_back(*j);
				}
			}
		}
		else {
			out.push_back(*i);
		}
	}

	for (auto i=out.rbegin();i!=out.rend();i++)
		pst.macro_expansion.push_front(std::move(*i));

	return 1;
}

static int pptok_nexttok(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
	int r;

try_again:
	if (!pst.include_stk.empty()) {
		pptok_state_t::include_t &i = pst.include_stk.top();
		if ((r=pptok_lgtok(pst,lst,i.rb,*i.sfo,t)) < 0)
			return r;

		if (r == 0) {
			pst.include_pop();
			goto try_again;
		}
	}
	else {
		if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
			return r;
	}

	return 1;
}

/* returns <= 1 as normal, > 1 if it handled the pragma by itself */
static int pptok_pragma(pptok_state_t &pst,lgtok_state_t &lst,const std::vector<token_t> &pragma) {
	(void)pragma;
	(void)pst;
	(void)lst;

	if (pragma.empty())
		return 1;

	if (pragma[0].type == token_type_t::identifier) {
		if (identifier(pragma[0].v.identifier) == "once") {
			// TODO
			return 2;
		}
	}

	return 1;
}

int pptok(pptok_state_t &pst,lgtok_state_t &lst,rbuf &buf,source_file_object &sfo,token_t &t) {
	int r;

#define TRY_AGAIN \
	if (t.type != token_type_t::none) \
	goto try_again_w_token; \
	else \
	goto try_again;

try_again:
	if ((r=pptok_nexttok(pst,lst,buf,sfo,t)) < 1)
		return r;

try_again_w_token:
	switch (t.type) {
		case token_type_t::r___LINE__:
			if (!pst.condb_true())
				goto try_again;
			t.type = token_type_t::integer;
			t.v.integer.init();
			t.v.integer.v.u = t.pos.row;
			break;
		case token_type_t::r___FILE__:
			if (!pst.condb_true())
				goto try_again;
			t.type = token_type_t::strliteral;
			t.v.csliteral = csliteral.alloc();
			{
				const char *name = sfo.getname();
				if (name) {
					const size_t l = strlen(name);

					if (!csliteral(t.v.csliteral).alloc(l))
						return errno_return(ENOMEM);

					assert(csliteral(t.v.csliteral).length == l);
					memcpy(csliteral(t.v.csliteral).data,name,l);
				}
			}
			break;
		case token_type_t::r_ppdefine:
			if (!pst.condb_true())
				goto try_again;
			if ((r=pptok_define(pst,lst,buf,sfo,t)) < 1)
				return r;

			TRY_AGAIN; /* does not fall through */
		case token_type_t::r_ppundef:
			if (!pst.condb_true())
				goto try_again;
			if ((r=pptok_undef(pst,lst,buf,sfo,t)) < 1)
				return r;

			TRY_AGAIN; /* does not fall through */
		case token_type_t::r_ppifdef:
			if ((r=pptok_ifdef(pst,lst,buf,sfo,t,true,true)) < 1)
				return r;

			TRY_AGAIN; /* does not fall through */
		case token_type_t::r_ppifndef:
			if ((r=pptok_ifdef(pst,lst,buf,sfo,t,true,false)) < 1)
				return r;

			TRY_AGAIN; /* does not fall through */
		case token_type_t::r_ppelifdef:
			if ((r=pptok_ifdef(pst,lst,buf,sfo,t,false,true)) < 1)
				return r;

			TRY_AGAIN; /* does not fall through */
		case token_type_t::r_ppelifndef:
			if ((r=pptok_ifdef(pst,lst,buf,sfo,t,false,false)) < 1)
				return r;

			TRY_AGAIN; /* does not fall through */
		case token_type_t::r_ppelse:
			if ((r=pptok_else(pst,lst,buf,sfo,t)) < 1)
				return r;

			TRY_AGAIN; /* does not fall through */
		case token_type_t::r_ppendif:
			if ((r=pptok_endif(pst,lst,buf,sfo,t)) < 1)
				return r;

			TRY_AGAIN; /* does not fall through */
		case token_type_t::r_ppif:
			if ((r=pptok_if(pst,lst,buf,sfo,t,true)) < 1)
				return r;

			TRY_AGAIN; /* does not fall through */
		case token_type_t::r_ppelif:
			if ((r=pptok_if(pst,lst,buf,sfo,t,false)) < 1)
				return r;

			TRY_AGAIN; /* does not fall through */
		case token_type_t::r_pperror:
		case token_type_t::r_ppwarning:
			if (!pst.condb_true())
				goto try_again;
			if ((r=pptok_errwarn(pst,lst,buf,sfo,t,t.type == token_type_t::r_pperror)) < 1)
				return r;

			TRY_AGAIN; /* does not fall through */
		case token_type_t::r_ppline:
			if (!pst.condb_true())
				goto try_again;
			if ((r=pptok_line(pst,lst,buf,sfo,t)) < 1)
				return r;

			TRY_AGAIN; /* does not fall through */
		case token_type_t::r_ppinclude: {
			if (!pst.condb_true())
				goto try_again;
			if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
				return r;

			/* Apparently you can use macros with #include. FreeType does: #include FREETYPE_H */
			while (t.type == token_type_t::identifier) {
				const pptok_state_t::pptok_macro_ent_t* macro = pst.lookup_macro(t.v.identifier);
				if (macro) {
					if ((r=pptok_macro_expansion(macro,pst,lst,buf,sfo,t)) < 1)
						return r;

					pst.macro_expansion_counter++;
					if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
						return r;
				}
				else {
					break;
				}
			}

			if (t.type == token_type_t::strliteral) {
				source_file_object *sfo = cb_include_search(pst,lst,t,CBIS_USER_HEADER);
				if (sfo == NULL) {
					fprintf(stderr,"Unable to #include \"%s\"\n",csliteral(t.v.csliteral).makestring().c_str());
					return errno_return(ENOENT);
				}
				pst.include_push(sfo);
				goto try_again;
			}
			else if (t.type == token_type_t::anglestrliteral) {
				source_file_object *sfo = cb_include_search(pst,lst,t,CBIS_SYS_HEADER);
				if (sfo == NULL) sfo = cb_include_search(pst,lst,t,CBIS_USER_HEADER);
				if (sfo == NULL) {
					fprintf(stderr,"Unable to #include <%s>\n",csliteral(t.v.csliteral).makestring().c_str());
					return errno_return(ENOENT);
				}
				pst.include_push(sfo);
				goto try_again;
			}
			goto try_again_w_token; }
		case token_type_t::r_ppinclude_next: {
			if (!pst.condb_true())
				goto try_again;
			if ((r=pptok_lgtok(pst,lst,buf,sfo,t)) < 1)
				return r;
			if (t.type == token_type_t::strliteral) {
				source_file_object *sfo = cb_include_search(pst,lst,t,CBIS_USER_HEADER|CBIS_NEXT);
				if (sfo == NULL) {
					fprintf(stderr,"Unable to #include_next \"%s\"\n",csliteral(t.v.csliteral).makestring().c_str());
					return errno_return(ENOENT);
				}
				pst.include_push(sfo);
				goto try_again;
			}
			else if (t.type == token_type_t::anglestrliteral) {
				source_file_object *sfo = cb_include_search(pst,lst,t,CBIS_SYS_HEADER|CBIS_NEXT);
				if (sfo == NULL) sfo = cb_include_search(pst,lst,t,CBIS_USER_HEADER|CBIS_NEXT);
				if (sfo == NULL) {
					fprintf(stderr,"Unable to #include_next <%s>\n",csliteral(t.v.csliteral).makestring().c_str());
					return errno_return(ENOENT);
				}
				pst.include_push(sfo);
				goto try_again;
			}
			goto try_again_w_token; }
		case token_type_t::r__Pragma: { /* Newer C _Pragma("") */
			std::vector<token_t> pragma;
			token_t pp = std::move(t);
			int parens = 0;

			if ((r=pptok_nexttok(pst,lst,buf,sfo,t)) < 1)
				return r;
			if (t.type != token_type_t::openparenthesis) {
				fprintf(stderr,"_Pragma missing open parens\n");
				return errno_return(ENOENT);
			}

			/* Allow macro string substitution */
			/* TODO: Can you use _Pragma with "strings" " pasted" " together"? */
			if ((r=pptok(pst,lst,buf,sfo,t)) < 1)
				return r;
			if (t.type != token_type_t::strliteral) {
				fprintf(stderr,"_Pragma missing string literal\n");
				return errno_return(ENOENT);
			}
			if (csliteral(t.v.csliteral).length != csliteral(t.v.csliteral).units()) {
				fprintf(stderr,"_Pragma string literal cannot be wide char format\n");
				return errno_return(ENOENT);
			}

			source_null_file sfonull;
			rbuf parseme;

			if ((r=rbuf_copy_csliteral(parseme,t.v.csliteral)) < 1)
				return errno_return(ENOMEM);

			parseme.source_file = t.source_file;
			parseme.pos = t.pos;

			do {
				/* no substitution here */
				r = lgtok(lst,parseme,sfonull,t);
				if (r == 0)/*eof*/
					break;
				else if (r < 0)
					return r;

				/* offsets from our own parseme buf don't make sense in the context of the source file */
				t.pos = pp.pos;

				if (t.type == token_type_t::newline) {
					pragma.push_back(std::move(t));
				}
				else if (t.type == token_type_t::backslashnewline) { /* \ + newline continues the macro past newline */
					pragma.push_back(std::move(token_t(token_type_t::newline,t.pos,t.source_file)));
				}
				else {
					if (t.type == token_type_t::openparenthesis)
						parens++;
					else if (t.type == token_type_t::closeparenthesis)
						parens--;

					pragma.push_back(std::move(t));
				}
			} while (1);

			if (parens != 0) {
				fprintf(stderr,"Parenthesis mismatch in pragma\n");
				return errno_return(EINVAL);
			}

			if ((r=pptok(pst,lst,buf,sfo,t)) < 1)
				return r;
			if (t.type != token_type_t::closeparenthesis) {
				fprintf(stderr,"_Pragma missing close parens\n");
				return errno_return(ENOENT);
			}

			if (!pst.condb_true())
				goto try_again;

			if ((r=pptok_pragma(pst,lst,pragma)) < 1)
				return r;

			/* pptok_pragma handled it by itself if r > 1, else just return it to the C compiler layer above */
			if (r == 1) {
				pst.macro_expansion.push_front(std::move(token_t(token_type_t::closeparenthesis)));

				for (auto i=pragma.rbegin();i!=pragma.rend();i++)
					pst.macro_expansion.push_front(std::move(*i));

				pst.macro_expansion.push_front(std::move(token_t(token_type_t::openparenthesis)));
				pp.type = token_type_t::op_pragma;
				t = std::move(pp);
				return 1;
			}

			goto try_again; }
		case token_type_t::r___pragma: { /* Microsoft C/C++ __pragma() */
			std::vector<token_t> pragma;
			token_t pp = std::move(t);
			int parens = 1;

			if ((r=pptok_nexttok(pst,lst,buf,sfo,t)) < 1)
				return r;

			/* if it is not __pragma(...) then pass the tokens directly down */
			if (t.type != token_type_t::openparenthesis) {
				pptok_lgtok_ungetch(pst,t);
				t = std::move(pp);
				return 1;
			}

			do {
				/* allow substitution as we work */
				if ((r=pptok(pst,lst,buf,sfo,t)) < 1)
					return r;

				if (t.type == token_type_t::newline) {
					pragma.push_back(std::move(t));
				}
				else if (t.type == token_type_t::backslashnewline) { /* \ + newline continues the macro past newline */
					pragma.push_back(std::move(token_t(token_type_t::newline,t.pos,t.source_file)));
				}
				else {
					if (t.type == token_type_t::openparenthesis) {
						parens++;
					}
					else if (t.type == token_type_t::closeparenthesis) {
						parens--;
						if (parens == 0) break;
					}

					pragma.push_back(std::move(t));
				}
			} while (1);

			if (parens != 0) {
				fprintf(stderr,"Parenthesis mismatch in pragma\n");
				return errno_return(EINVAL);
			}

			if (!pst.condb_true())
				goto try_again;

			if ((r=pptok_pragma(pst,lst,pragma)) < 1)
				return r;

			/* pptok_pragma handled it by itself if r > 1, else just return it to the C compiler layer above */
			if (r == 1) {
				pst.macro_expansion.push_front(std::move(token_t(token_type_t::closeparenthesis)));

				for (auto i=pragma.rbegin();i!=pragma.rend();i++)
					pst.macro_expansion.push_front(std::move(*i));

				pst.macro_expansion.push_front(std::move(token_t(token_type_t::openparenthesis)));
				pp.type = token_type_t::op_pragma;
				t = std::move(pp);
				return 1;
			}

			goto try_again; }
		case token_type_t::r_pppragma: { /* Standard #pragma */
			std::vector<token_t> pragma;
			token_t pp = std::move(t);
			int parens = 0;

			/* eh, no, we're not going to directly support substitution here
			 *
			 * #pragma ...
			 * #pragma ... \
			 *         ... \
			 *         ... */
			do {
				if ((r=pptok_nexttok(pst,lst,buf,sfo,t)) < 1)
					return r;

				if (t.type == token_type_t::newline) {
					t = token_t();
					break;
				}
				else if (t.type == token_type_t::backslashnewline) { /* \ + newline continues the macro past newline */
					pragma.push_back(std::move(token_t(token_type_t::newline,t.pos,t.source_file)));
				}
				else {
					if (t.type == token_type_t::openparenthesis)
						parens++;
					else if (t.type == token_type_t::closeparenthesis)
						parens--;

					pragma.push_back(std::move(t));
				}
			} while (1);

			if (parens != 0) {
				fprintf(stderr,"Parenthesis mismatch in pragma\n");
				return errno_return(EINVAL);
			}

			if (!pst.condb_true())
				goto try_again;

			if ((r=pptok_pragma(pst,lst,pragma)) < 1)
				return r;

			/* pptok_pragma handled it by itself if r > 1, else just return it to the C compiler layer above */
			if (r == 1) {
				pst.macro_expansion.push_front(std::move(token_t(token_type_t::closeparenthesis)));

				for (auto i=pragma.rbegin();i!=pragma.rend();i++)
					pst.macro_expansion.push_front(std::move(*i));

				pst.macro_expansion.push_front(std::move(token_t(token_type_t::openparenthesis)));
				pp.type = token_type_t::op_pragma;
				t = std::move(pp);
				return 1;
			}

			goto try_again; }
		case token_type_t::identifier: { /* macro substitution */
			if (!pst.condb_true())
				goto try_again;

			const pptok_state_t::pptok_macro_ent_t* macro = pst.lookup_macro(t.v.identifier);
			if (macro) {
				if ((r=pptok_macro_expansion(macro,pst,lst,buf,sfo,t)) < 1)
					return r;

				pst.macro_expansion_counter++;
				goto try_again;
			}
			break; }
		default:
			if (!pst.condb_true())
				goto try_again;

			break;
	}

#undef TRY_AGAIN

	return 1;
}

