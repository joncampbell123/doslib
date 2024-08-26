
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

#include <unordered_map>
#include <type_traits>
#include <stdexcept>
#include <vector>
#include <string>
#include <limits>
#include <new>

#ifndef O_BINARY
# define O_BINARY 0
#endif

namespace CIMCC/*TODO: Pick a different name by final release*/ {

	static constexpr int errno_return(int e) {
		return (e >= 0) ? (-e) : (e); /* whether errno values are positive or negative, return as negative */
	}

	//////////////////////////////////////////////////////////////////

#define SOURCE_OBJ_NO_COPY_ONLY_MOVE(sfclass) \
	/* no copy */ \
	sfclass(const sfclass &) = delete; \
	virtual sfclass &operator=(const sfclass &) = delete; \
	/* move allowed */ \
	sfclass(sfclass &&x) { common_move(x); } \
	virtual sfclass &operator=(sfclass &&x) { common_move(x); return *this; }

	struct source_file_object {
		enum {
			IF_BASE=0,
			IF_FD=1
		};

		unsigned int			iface;

		virtual const char*		getname(void);
		virtual ssize_t			read(void *,size_t);
		virtual void			close(void);

						source_file_object(const unsigned int iface=IF_BASE);
		virtual				~source_file_object();

						SOURCE_OBJ_NO_COPY_ONLY_MOVE(source_file_object);
		void				common_move(source_file_object &);
	};

	////////////////////////////////////////////////////////////

	source_file_object::source_file_object(const unsigned int new_iface) : iface(new_iface) {
	}

	source_file_object::~source_file_object() {
	}

	const char* source_file_object::getname(void) {
		return "(null)";
	}

	ssize_t source_file_object::read(void *,size_t) {
		return ssize_t(errno_return(ENOSYS));
	}

	void source_file_object::close(void) {
	}

	void source_file_object::common_move(source_file_object &) {
	}

	//////////////////////////////////////////////////////////////

	struct source_fd : public source_file_object {
		virtual const char*		getname(void);
		virtual ssize_t			read(void *buffer,size_t count);
		virtual void			close(void);

						source_fd();
						source_fd(const int new_fd/*takes ownership*/,const std::string &new_name);
		virtual				~source_fd();

						SOURCE_OBJ_NO_COPY_ONLY_MOVE(source_fd);
		void				common_move(source_fd &x);

		std::string			name;
		int				fd = -1;
	};

	////////////////////////////////////////////////////////////////

	source_fd::source_fd() {
	}

	source_fd::source_fd(const int new_fd/*takes ownership*/,const std::string &new_name) : source_file_object(IF_FD), name(new_name), fd(new_fd) {
	}

	source_fd::~source_fd() {
		close();
	}

	void source_fd::common_move(source_fd &x) {
		name = std::move(x.name);
		fd = x.fd; x.fd = -1;
	}

	const char* source_fd::getname(void) {
		return name.c_str();
	}

	ssize_t source_fd::read(void *buffer,size_t count) {
		if (fd >= 0) {
			const int r = ::read(fd,buffer,count);
			if (r >= 0) {
				assert(size_t(r) <= count);
				return r;
			}
			else {
				return ssize_t(errno_return(r));
			}
		}

		return ssize_t(errno_return(EBADF));
	}

	void source_fd::close(void) {
		if (fd >= 0) {
			::close(fd);
			fd = -1;
		}
	}

	////////////////////////////////////////////////////////////////////

}

static std::vector<std::string>		main_input_files;

static void help(void) {
	fprintf(stderr,"cimcc [options] [input file [...]]\n");
}

static int parse_argv(int argc,char **argv) {
	int i = 1; /* argv[0] is command */
	char *a;

	while (i < argc) {
		a = argv[i++];

		if (*a == '-') {
			do { a++; } while (*a == '-');

			if (!strcmp(a,"h") || !strcmp(a,"help")) {
				help();
				return -1;
			}
			else {
				fprintf(stderr,"Unknown option '%s'\n",a);
				return -1;
			}
		}
		else {
			if (main_input_files.size() > 1024) return -1;
			main_input_files.push_back(a);
		}
	}

	return 0;
}

int main(int argc,char **argv) {
	if (parse_argv(argc,argv) < 0)
		return 1;

	if (main_input_files.empty()) {
		fprintf(stderr,"No input files\n");
		return 1;
	}

	for (auto mifi=main_input_files.begin();mifi!=main_input_files.end();mifi++) {
		CIMCC::source_file_object *sfo = NULL;

		if (*mifi == "-") {
			struct stat st;

			if (fstat(0/*STDIN*/,&st)) {
				fprintf(stderr,"Cannot stat STDIN\n");
				return 1;
			}
			if (!(S_ISREG(st.st_mode) || S_ISSOCK(st.st_mode) || S_ISFIFO(st.st_mode))) {
				fprintf(stderr,"Cannot use STDIN. Must be a file, socket, or pipe\n");
				return 1;
			}
			sfo = new CIMCC::source_fd(0/*STDIN, takes ownership*/,"<stdin>");
			assert(sfo->iface == CIMCC::source_file_object::IF_FD);
		}
		else {
			int fd = open((*mifi).c_str(),O_RDONLY|O_BINARY);
			if (fd < 0) {
				fprintf(stderr,"Cannot open '%s', '%s'\n",(*mifi).c_str(),strerror(errno));
				return 1;
			}
			sfo = new CIMCC::source_fd(fd/*takes ownership*/,*mifi);
			assert(sfo->iface == CIMCC::source_file_object::IF_FD);
		}

		assert(sfo != NULL);
		delete sfo;
	}

	return 0;
}

