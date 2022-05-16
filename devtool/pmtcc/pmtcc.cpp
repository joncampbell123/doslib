
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include <exception>
#include <string>
#include <vector>
#include <memory>

using namespace std;

#ifndef O_BINARY
#define O_BINARY (0)
#endif

//////////////////// text position, to allow "syntax error at line X column Y"
struct textposition {
	unsigned int		column;
	unsigned int		row;

	textposition();
	void next_line(void);
	void reset(void);
};

textposition::textposition() {
	reset();
}

void textposition::next_line(void) {
	row++;
	column = 1;
}

void textposition::reset(void) {
	column = row = 1;
}

////////////////////// source stream management

struct sourcestream {
	sourcestream();
	virtual ~sourcestream();
	virtual const char *getname(void);
	virtual void rewind(void);
	virtual void close(void);
	virtual int getc(void);

	bool eof = false;
};

const char *sourcestream::getname(void) {
	return "";
}

sourcestream::sourcestream() {
}

sourcestream::~sourcestream() {
}

void sourcestream::rewind(void) {
	eof = false;
}

void sourcestream::close(void) {
}

int sourcestream::getc(void) {
	eof = true;
	return -1;
}

typedef int sourcestream_ref;

///////////////////////// source parsing stack

struct sourcestack {
	struct entry {
		unique_ptr<sourcestream>	src;
		textposition			pos;

		void				rewind(void);
		int				getc(void);
		bool				eof(void);
	};

	std::vector<entry>			stk; /* front() = bottom of stack  back() aka stk[stk.size()-1] = top of stack */

	void push(unique_ptr<sourcestream> &src/*will take ownership*/);
	entry &top(void);
	void pop(void);

	void _errifempty(void);
};

void sourcestack::entry::rewind(void) {
	sourcestream *s;
	if ((s=src.get()) != NULL) s->rewind();
}

int sourcestack::entry::getc(void) {
	sourcestream *s;
	if ((s=src.get()) != NULL) return s->getc();
	return -1;
}

bool sourcestack::entry::eof(void) {
	sourcestream *s;
	if ((s=src.get()) != NULL) return s->eof;
	return true;
}

void sourcestack::push(unique_ptr<sourcestream> &src) {
	/* do not permit stack depth to exceed 128 */
	if (stk.size() >= 128u)
		throw std::runtime_error("source stack too deep");

	entry e;
	e.src = std::move(src); /* unique_ptr does not permit copy */
	e.pos.reset();
	stk.push_back(std::move(e));
}

void sourcestack::_errifempty(void) {
	if (stk.empty())
		throw std::runtime_error("source stack empty");
}

sourcestack::entry &sourcestack::top(void) {
	_errifempty();
	return stk.back();
}

void sourcestack::pop(void) {
	_errifempty();
	stk.pop_back();
}

/////////////////////// source stream provider

unique_ptr<sourcestream> source_stream_open_null(const char *path) {
	(void)path;
	return NULL;
}

// change this if compiling from something other than normal files such as memory or perhaps container formats
unique_ptr<sourcestream> (*source_stream_open)(const char *path) = source_stream_open_null;

//////////////////////// source file stream provider

struct sourcestream_file : public sourcestream {
	sourcestream_file(int fd,const char *path);
	virtual ~sourcestream_file();
	virtual const char *getname(void);
	virtual void rewind(void);
	virtual void close(void);
	virtual int getc(void);
private:
	std::string		file_path;
	int			file_fd = -1;
};

sourcestream_file::sourcestream_file(int fd,const char *path) : sourcestream() {
	file_path = path;
	file_fd = fd;
}

sourcestream_file::~sourcestream_file() {
	close();
}

const char *sourcestream_file::getname(void) {
	return file_path.c_str();
}

void sourcestream_file::rewind(void) {
	sourcestream::rewind();
	if (file_fd >= 0) lseek(file_fd,0,SEEK_SET);
}

void sourcestream_file::close(void) {
	if (file_fd >= 0) {
		::close(file_fd);
		file_fd = -1;
	}
	sourcestream::close();
}

int sourcestream_file::getc(void) {
	if (file_fd >= 0) {
		unsigned char c;
		if (read(file_fd,&c,1) == 1) return (int)c;
	}

	eof = true;
	return -1;
}

unique_ptr<sourcestream> source_stream_open_file(const char *path) {
	int fd = open(path,O_RDONLY|O_BINARY);
	if (fd < 0) return NULL;
	return unique_ptr<sourcestream>(reinterpret_cast<sourcestream*>(new sourcestream_file(fd,path)));
}

///////////////////////////////////////

struct options_state {
	std::vector<std::string>		source_files;
};

static options_state				options;

static void help(void) {
	fprintf(stderr,"pmtcc [options] [source file ...]\n");
	fprintf(stderr," -h --help                         help\n");
}

static int parse_argv(options_state &ost,int argc,char **argv) {
	char *a;
	int i=1;

	while (i < argc) {
		a = argv[i++];
		if (*a == '-') {
			do { a++; } while (*a == '-');

			if (!strcmp(a,"h") || !strcmp(a,"help")) {
				help();
				return 1;
			}
			else {
				fprintf(stderr,"Unknown switch %s\n",a);
				return 1;
			}
		}
		else {
			ost.source_files.push_back(a);
		}
	}

	return 0;
}

int main(int argc,char **argv) {
	if (parse_argv(/*&*/options,argc,argv))
		return 1;

	/* open from files */
	source_stream_open = source_stream_open_file;

	/* parse each file one by one */
	for (auto si=options.source_files.begin();si!=options.source_files.end();si++) {
		sourcestack srcstack;

		unique_ptr<sourcestream> sfile = source_stream_open((*si).c_str());
		if (sfile.get() == NULL) {
			fprintf(stderr,"Unable to open source file '%s'\n",(*si).c_str());
			return 1;
		}
		srcstack.push(sfile);

		while (!srcstack.top().eof()) {
			int c = srcstack.top().getc();
			if (c >= 0) printf("%c",(char)c);
		}
		printf("\n");
	}

	return 0;
}

