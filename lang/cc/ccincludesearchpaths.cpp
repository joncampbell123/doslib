
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

std::vector<std::string> cb_include_search_paths;
cb_include_search_t cb_include_search = cb_include_search_default;
cb_include_accept_path_t cb_include_accept_path = cb_include_accept_path_default;

//////////////////////////////////////////////////////////////////////////////

bool cb_include_accept_path_default(const std::string &/*p*/) {
	return true;
}

//////////////////////////////////////////////////////////////////////////////

source_file_object* cb_include_search_default(pptok_state_t &/*pst*/,lgtok_state_t &/*lst*/,const token_t &t,unsigned int fl) {
	source_file_object *sfo = NULL;

	if (fl & CBIS_USER_HEADER) {
		std::string path = csliteral(t.v.csliteral).makestring(); path_slash_translate(path);
		if (!path.empty() && cb_include_accept_path(path) && access(path.c_str(),R_OK) >= 0) {
			const int fd = open(path.c_str(),O_RDONLY|O_BINARY);
			if (fd >= 0) {
				sfo = new source_fd(fd/*takes ownership*/,path);
				assert(sfo->iface == source_file_object::IF_FD);
				return sfo;
			}
		}
	}

	for (auto ipi=cb_include_search_paths.begin();ipi!=cb_include_search_paths.end();ipi++) {
		std::string path = path_combine(*ipi,csliteral(t.v.csliteral).makestring()); path_slash_translate(path);
		if (!path.empty() && cb_include_accept_path(path) && access(path.c_str(),R_OK) >= 0) {
			const int fd = open(path.c_str(),O_RDONLY|O_BINARY);
			if (fd >= 0) {
				sfo = new source_fd(fd/*takes ownership*/,path);
				assert(sfo->iface == source_file_object::IF_FD);
				return sfo;
			}
		}
	}

	return NULL;
}

