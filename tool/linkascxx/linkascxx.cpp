
#include <sys/types.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>

#include <vector>
#include <string>

namespace DOSLIBLinker {

	enum {
		LNKLOG_DEBUGMORE = -2,
		LNKLOG_DEBUG = -1,
		LNKLOG_NORMAL = 0,
		LNKLOG_WARNING = 1,
		LNKLOG_ERROR = 2
	};

	typedef void (*log_callback_t)(const int level,void *user,const char *str);

	void log_callback_silent(const int level,void *user,const char *str);
	void log_callback_default(const int level,void *user,const char *str);

	struct log_t {
		log_callback_t		callback = log_callback_default;
		int			min_level = LNKLOG_NORMAL;
		void*			user = NULL;

		void			log(const int level,const char *fmt,...);
	};

	class file_io {
		public:
			file_io();
			virtual ~file_io();
		public:
			virtual bool			ok(void) const;
			virtual off_t			tell(void);
			virtual bool			seek(const off_t o);
			virtual bool			read(void *buf,size_t len);
			virtual bool			write(const void *buf,size_t len);
			virtual void			close(void);
	};

	class file_stdio_io : public file_io {
		public:
			file_stdio_io();
			file_stdio_io(const char *path,const char *how=NULL);
			virtual ~file_stdio_io();
		public:
			virtual bool			ok(void) const;
			virtual off_t			tell(void);
			virtual bool			seek(const off_t o);
			virtual bool			read(void *buf,size_t len);
			virtual bool			write(const void *buf,size_t len);
			virtual void			close(void);
		public:
			virtual bool			open(const char *path,const char *how=NULL);
		private:
			FILE*				fp = NULL;
	};

}

namespace DOSLIBLinker {

	void log_callback_silent(const int level,void *user,const char *str) {
		(void)level;
		(void)user;
		(void)str;
	}

	void log_callback_default(const int level,void *user,const char *str) {
		(void)user;
		fprintf(stderr,"DOSLIBLinker::log level %d: %s\n",level,str);
	}

	void log_t::log(const int level,const char *fmt,...) {
		if (level >= min_level) {
			va_list va;
			const size_t tmpsize = 512;
			char *tmp = new char[tmpsize];

			va_start(va,fmt);
			vsnprintf(tmp,tmpsize,fmt,va);
			callback(level,user,tmp);
			va_end(va);

			delete[] tmp;
		}
	}

	file_io::file_io() {
	}

	file_io::~file_io() {
	}

	bool file_io::ok(void) const {
		return false;
	}

	off_t file_io::tell(void) {
		return 0;
	}

	bool file_io::seek(const off_t o) {
		(void)o;
		return false;
	}

	bool file_io::read(void *buf,size_t len) {
		(void)buf;
		(void)len;
		return false;
	}

	bool file_io::write(const void *buf,size_t len) {
		(void)buf;
		(void)len;
		return false;
	}

	void file_io::close(void) {
	}

	file_stdio_io::file_stdio_io() : file_io() {
	}

	file_stdio_io::file_stdio_io(const char *path,const char *how) : file_io() {
		open(path,how);
	}

	file_stdio_io::~file_stdio_io() {
		close();
	}

	bool file_stdio_io::ok(void) const {
		return (fp != NULL);
	}

	off_t file_stdio_io::tell(void) {
		if (fp != NULL) return ftell(fp);
		return 0;
	}

	bool file_stdio_io::seek(const off_t o) {
		if (fp != NULL) {
			fseek(fp,o,SEEK_SET);
			return (ftell(fp) == o);
		}

		return false;
	}

	bool file_stdio_io::read(void *buf,size_t len) {
		if (fp != NULL && len != size_t(0))
			return (fread(buf,len,1,fp) == 1);

		return false;
	}

	bool file_stdio_io::write(const void *buf,size_t len) {
		if (fp != NULL && len != size_t(0))
			return (fwrite(buf,len,1,fp) == 1);

		return false;
	}

	void file_stdio_io::close(void) {
		if (fp != NULL) {
			fclose(fp);
			fp = NULL;
		}
	}

	bool file_stdio_io::open(const char *path,const char *how) {
		close();

		if (how == NULL)
			how = "rb";

		if ((fp=fopen(path,how)) == NULL)
			return false;

		return true;
	}

}

int main(int argc,char **argv) {
	(void)argc;
	(void)argv;
	return 0;
}

