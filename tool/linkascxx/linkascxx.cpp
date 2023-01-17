
#include <sys/types.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include <unordered_map>
#include <vector>
#include <string>
#include <map>

namespace DOSLIBLinker {

	enum {
		LNKLOG_DEBUGMORE = -2,
		LNKLOG_DEBUG = -1,
		LNKLOG_NORMAL = 0,
		LNKLOG_WARNING = 1,
		LNKLOG_ERROR = 2
	};

	/* some formats (OMF) use 1-based indices */
	template <typename T> static inline constexpr T from1based(const T v) {
		return v - T(1u);
	}

	template <typename T> static inline constexpr T to1based(const T v) {
		return v + T(1u);
	}

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

	template <typename T> class _base_handlearray {
		public:
			static constexpr size_t			undef = ~((size_t)(0ul));
			typedef T				ent_type;
			typedef size_t				ref_type;
			std::vector<T>				ref;
		public:
			inline size_t size(void) const { return ref.size(); }
		public:
			bool exists(const size_t r) const {
				return r < ref.size();
			}

			inline const T &get(const size_t r) const {
				return ref.at(r); /* std::vector will throw an exception if out of range */
			}

			inline T &get(const size_t r) {
				return ref.at(r); /* std::vector will throw an exception if out of range */
			}

			size_t allocate(void) {
				const size_t r = ref.size();
				ref.emplace(ref.end());
				assert(ref.size() == (r+size_t(1u)));
				return r;
			}

			size_t allocate(const T &val) {
				const size_t r = ref.size();
				ref.emplace(ref.end(),val);
				assert(ref.size() == (r+size_t(1u)));
				return r;
			}

			void clear(void) {
				ref.clear();
			}
	};

	/* NTS: All strings, segments, groups, etc. are allocated in a vector and always referred to
	 *      through a reference (literally just a vector index). By design, you can only allocate
	 *      from the storage. You cannot delete it, you cannot move it around (that would break
	 *      the reference). There is a clear() if you need to wipe the slate clean. That is it. */

	class string_table_t {
		public:
			static constexpr size_t				undef = ~((size_t)(0ul));
			typedef std::string				ent_type;
			typedef size_t					ref_type;
			std::vector<std::string>			ref;
			std::unordered_map<std::string,size_t>		ref2rev;
		public:
			inline size_t size(void) const { return ref.size(); }
		public:
			bool exists(const size_t r) const {
				return r < ref.size();
			}

			bool exists(const std::string &r) const {
				return ref2rev.find(r) != ref2rev.end();
			}

			size_t lookup(const std::string &r) const {
				const auto i = ref2rev.find(r);
				if (i != ref2rev.end()) return i->second;
				return undef;
			}

			inline const std::string &get(const size_t r) const {
				return ref.at(r); /* std::vector will throw an exception if out of range */
			}

			inline std::string &get(const size_t r) {
				return ref.at(r); /* std::vector will throw an exception if out of range */
			}

			size_t add(const std::string &r) {
				{
					const auto i = ref2rev.find(r);
					if (i != ref2rev.end()) return i->second;
				}

				const size_t ri = ref.size();
				ref.emplace(ref.end(),r);
				assert(ref.size() == (ri + size_t(1u)));
				ref2rev[r] = ri;
				return ri;
			}

			void clear(void) {
				ref.clear();
				ref2rev.clear();
			}
	};

	struct source_t {
		public:
			std::string				path;
			std::string				name; /* name string (OMF THEADR/LHEADR) */
			size_t					index = ~((size_t)(0ul));
			off_t					file_offset = off_t(0ul) - off_t(1ul);
	};
	typedef _base_handlearray<source_t> source_list_t;
	typedef source_list_t::ref_type source_ref_t;

	typedef string_table_t::ref_type string_ref_t;

	typedef uint64_t					alignment_t;
	static constexpr alignment_t				alignment_undef = 0;

	static inline constexpr alignment_t alignment2mask(const alignment_t b) {
		return ~(b - alignment_t(1u));
	}

	static inline constexpr alignment_t alignmask2value(const alignment_t b) {
		return (~b) + alignment_t(1u);
	}

	static constexpr alignment_t				byte_alignment = alignment2mask(1);
	static constexpr alignment_t				word_alignment = alignment2mask(2);
	static constexpr alignment_t				dword_alignment = alignment2mask(4);
	static constexpr alignment_t				qword_alignment = alignment2mask(8);
	static constexpr alignment_t				para_alignment = alignment2mask(16);
	static constexpr alignment_t				page256_alignment = alignment2mask(256);
	static constexpr alignment_t				page4k_alignment = alignment2mask(4096);

	typedef int64_t						segment_size_t;
	static constexpr segment_size_t				segment_size_undef = int64_t(-0x8000000000000000ll);

	typedef uint32_t					segment_flags_t;
	static constexpr segment_flags_t			SEGFLAG_DELETED =        segment_flags_t(1u) << segment_flags_t(0u); // removed, usually when merging with another
	static constexpr segment_flags_t			SEGFLAG_NOEMIT =         segment_flags_t(1u) << segment_flags_t(1u);
	static constexpr segment_flags_t			SEGFLAG_PRIVATE =        segment_flags_t(1u) << segment_flags_t(2u);
	static constexpr segment_flags_t			SEGFLAG_PUBLIC =         segment_flags_t(1u) << segment_flags_t(3u);
	static constexpr segment_flags_t			SEGFLAG_STACK =          segment_flags_t(1u) << segment_flags_t(4u);
	static constexpr segment_flags_t			SEGFLAG_COMMON =         segment_flags_t(1u) << segment_flags_t(5u);
	static constexpr segment_flags_t			SEGFLAG_ABSOLUTEFRAME =  segment_flags_t(1u) << segment_flags_t(6u); // segmentframe is absolute segment value
	static constexpr segment_flags_t			SEGFLAG_ABSOLUTEOFFS =   segment_flags_t(1u) << segment_flags_t(7u); // segmentoffset is absolute offset value
	static constexpr segment_flags_t			SEGFLAG_COMBINEABLE =    segment_flags_t(1u) << segment_flags_t(8u);
	static constexpr segment_flags_t			SEGFLAG_SEGMENTED =      segment_flags_t(1u) << segment_flags_t(9u);
	static constexpr segment_flags_t			SEGFLAG_FLAT =           segment_flags_t(1u) << segment_flags_t(10u);

	typedef uint16_t					cpu_model_t;
	static constexpr cpu_model_t				cpu_model_undef = 0;
	static constexpr cpu_model_t				CPUMODEL_X86_16BIT = 0x0102;
	static constexpr cpu_model_t				CPUMODEL_X86_32BIT = 0x0104;
	static constexpr cpu_model_t				CPUMODEL_X86_64BIT = 0x0108;

	typedef int32_t						segment_frame_t;
	static constexpr segment_frame_t			segment_frame_undef = int32_t(-0x80000000l);

	typedef int64_t						segment_offset_t;
	static constexpr int64_t				segment_offset_undef = int64_t(-0x8000000000000000ll);

	typedef uint32_t					fragment_flags_t;
	// none defined

	struct segment_t;
	struct fragment_t;
	struct group_t;

	typedef _base_handlearray<segment_t> segment_list_t;
	typedef segment_list_t::ref_type segment_ref_t;

	typedef _base_handlearray<fragment_t> fragment_list_t;
	typedef fragment_list_t::ref_type fragment_ref_t;

	typedef _base_handlearray<group_t> group_list_t;
	typedef group_list_t::ref_type group_ref_t;

	/* fragments are allocated from a global table, and everything else merely refers to
	 * the fragment through a reference. This way you can combine fragments easily when
	 * joining segments of the same name and such without breaking references */
	struct fragment_t {
		public:
			segment_ref_t				insegment = segment_list_t::undef; // what segment this fragment has been assigned to
			segment_offset_t			org_offset = segment_offset_undef; // any specific offset requested by the data (Microsoft MASM ORG directive support)
			segment_offset_t			segmentoffset = segment_offset_undef; // assigned offset of fragment within segment counting from segment_t.segmentoffset
			segment_size_t				fragmentsize = segment_size_undef; /* size of fragment, which may be larger than the data array */
			source_ref_t				source = source_list_t::undef; /* source of fragment */
			alignment_t				alignment = alignment_undef;
			fragment_flags_t			flags = 0;
			std::vector<uint8_t>			data;
	};

	struct segment_t {
		public:
			string_ref_t				segmentname = string_table_t::undef;
			group_ref_t				group = group_list_t::undef;
			string_ref_t				classname = string_table_t::undef;
			string_ref_t				overlayname = string_table_t::undef;
			alignment_t				alignment = alignment_undef;
			segment_size_t				segmentsize = segment_size_undef;
			segment_offset_t			segmentoffset = segment_offset_undef;
			segment_frame_t				segmentframe = segment_frame_undef;
			cpu_model_t				cpumodel = cpu_model_undef;
			source_ref_t				source = source_list_t::undef; /* source of segment, undef if combined from fragments of multiple sources */
			segment_flags_t				flags = 0;
			std::vector<fragment_ref_t>		fragments;
	};

	struct group_t {
		public:
			string_ref_t				groupname = string_table_t::undef;
			source_ref_t				source = source_list_t::undef; /* source of group, undef if combined from multiple sources */
			std::vector<segment_ref_t>		segments; /* segments with membership in the group */
	};

	class linkenv {
		public:
			source_list_t				sources;
			string_table_t				strings;
			segment_list_t				segments;
			fragment_list_t				fragments;
			group_list_t				groups;
			std::vector<segment_ref_t>		segment_order;
		public:
			log_t					log;
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

namespace DOSLIBLinker {

	class OMF_record_data : public std::vector<uint8_t> {
		public:
			OMF_record_data();
			~OMF_record_data();
		public:
			void rewind(void);
			bool seek(const size_t o);
			bool advance(const size_t o);
			size_t offset(void) const;
			size_t remains(void) const;
			bool eof(void) const;
		public:
			uint8_t gb(void);
			uint16_t gw(void);
			uint32_t gd(void);
			uint16_t gidx(void);
			std::string glenstr(void);
		private:
			size_t readp = 0;
	};

	struct OMF_record {
		OMF_record_data		data;
		uint8_t			type = 0;
		off_t			file_offset = 0;

		void			clear(void);
	};

	class OMF_record_reader {
		public:
			bool		is_library = false;
			uint32_t	dict_offset = 0;
			uint32_t	block_size = 0;
			uint32_t	dict_size = 0;
			uint8_t		lib_flags = 0;
		public:
			void		clear(void);
			bool		read(OMF_record &rec,file_io &fio,log_t &log);
	};

	class OMF_reader {
		public:
			struct LIDATA_heap_t {
				size_t				lidata_offset = ~((size_t)0); /* offset within LIDATA */
				uint32_t			repeat = 0; /* repeat count */
				uint8_t				length = 0; /* data length */
				std::vector<LIDATA_heap_t>	sub_blocks; /* sub blocks ref to index */
			};
		public:
			typedef _base_handlearray<string_ref_t> LNAMES_list_t;
			typedef LNAMES_list_t::ref_type LNAME_ref_t;

			typedef _base_handlearray<segment_ref_t> SEGDEF_list_t;
			typedef SEGDEF_list_t::ref_type SEGDEF_ref_t;

			typedef _base_handlearray<group_ref_t> GRPDEF_list_t;
			typedef GRPDEF_list_t::ref_type GRPDEF_ref_t;
		public:
			source_ref_t			current_source = source_list_t::undef;
			std::vector<LIDATA_heap_t>	LIDATA_records; /* LIDATA records converted to vector for FIXUPP later on */
			size_t				LIDATA_expanded_at; /* offset within fragment LIDATA was expanded to */
			LNAMES_list_t			LNAMES;
			SEGDEF_list_t			SEGDEF;
			GRPDEF_list_t			GRPDEF;
			OMF_record_reader		recrdr;
		public:
			GRPDEF_ref_t			special_FLAT_GRPDEF = group_list_t::undef; /* FLAT GRPDEF if any, which also means the memory model is FLAT not SEGMENTED */
		public:
			void				clear(void);
			bool				read(linkenv &lenv,const char *path);
			bool				read(linkenv &lenv,file_io &fio,const char *path);
			bool				read_module(linkenv &lenv,file_io &fio);
			bool				read_rec_LNAMES(linkenv &lenv,OMF_record &rec);
			bool				read_rec_SEGDEF(linkenv &lenv,OMF_record &rec);
			bool				read_rec_GRPDEF(linkenv &lenv,OMF_record &rec);
			bool				read_rec_LEDATA(linkenv &lenv,OMF_record &rec);
			bool				read_rec_LIDATA(linkenv &lenv,OMF_record &rec);
		private:
			bool				read_LIDATA_block(linkenv &lenv,const size_t base_lidata,LIDATA_heap_t &ent,OMF_record &rec,const bool fmt32,const size_t depth=0);
			void				LIDATA_records_dump_debug(linkenv &lenv,const size_t depth,const std::vector<LIDATA_heap_t> &r);
			bool				expand_LIDATA_blocks(fragment_t &cfr,size_t &copyto,std::vector<LIDATA_heap_t> &r,const unsigned char *lidata,const size_t lidatasize);
	};

}

namespace DOSLIBLinker {

	OMF_record_data::OMF_record_data() : std::vector<uint8_t>() {
	}

	OMF_record_data::~OMF_record_data() {
	}

	uint8_t OMF_record_data::gb(void) {
		if ((readp+size_t(1u)) <= size()) { const uint8_t r = *(data()+readp); readp++; return r; }
		return 0;
	}

	uint16_t OMF_record_data::gw(void) {
		if ((readp+size_t(2u)) <= size()) { const uint16_t r = *((uint16_t*)(data()+readp)); readp += 2; return le16toh(r); }
		return 0;
	}

	uint32_t OMF_record_data::gd(void) {
		if ((readp+size_t(4u)) <= size()) { const uint32_t r = *((uint32_t*)(data()+readp)); readp += 4; return le32toh(r); }
		return 0;
	}

	uint16_t OMF_record_data::gidx(void) {
		uint16_t r = gb();
		if (r & 0x80) r = ((r & 0x7Fu) << 8u) + gb();
		return r;
	}

	std::string OMF_record_data::glenstr(void) {
		/* <len> <string> */
		size_t len = gb();
		if (len > remains()) len = remains();

		const size_t bp = readp; readp += len;
		assert(readp <= size());

		return std::string((const char*)(data()+bp),len);
	}

	void OMF_record_data::rewind(void) {
		readp = 0;
	}

	bool OMF_record_data::eof(void) const {
		return readp >= size();
	}

	bool OMF_record_data::seek(const size_t o) {
		if (o <= size()) {
			readp = o;
			return true;
		}
		else {
			readp = size();
			return false;
		}
	}

	bool OMF_record_data::advance(const size_t o) {
		return seek(readp+o);
	}

	size_t OMF_record_data::remains(void) const {
		return size_t(size() - readp);
	}

	size_t OMF_record_data::offset(void) const {
		return readp;
	}

	void OMF_record::clear(void) {
		data.clear();
		data.rewind();
	}

	void OMF_record_reader::clear(void) {
		is_library = false;
		dict_offset = 0;
		block_size = 0;
		dict_size = 0;
		lib_flags = 0;
	}

	bool OMF_record_reader::read(OMF_record &rec,file_io &fio,log_t &log) {
		unsigned char tmp[3],chk;

		rec.clear();
		rec.file_offset = fio.tell();

		if (dict_offset != uint32_t(0)) {
			if (rec.file_offset >= (off_t)dict_offset)
				return false;
		}

		/* <byte: record type> <word: record length> <data> <byte: checksum>
		 *
		 * Data length includes checksum but not type and length fields. */
		if (!fio.read(tmp,3)) return false;
		rec.type = tmp[0];

		const size_t len = le16toh( *((uint16_t*)(tmp+1)) );
		if (len == 0) {
			log.log(LNKLOG_WARNING,"OMF record with zero length");
			return false;
		}

		rec.data.resize(len - size_t(1u)); /* do not include checksum in the data field */
		if (!fio.read(rec.data.data(),len - size_t(1u))) {
			log.log(LNKLOG_WARNING,"OMF record with incomplete data (eof)");
			return false;
		}

		/* now check the checksum */
		if (!fio.read(&chk,1)) {
			log.log(LNKLOG_WARNING,"OMF record with incomplete data (eof on checksum byte)");
			return false;
		}

		/* checksum byte is a checksum byte if the byte value here is nonzero */
		if (chk != 0u) {
			unsigned char sum = 0;

			for (size_t i=0;i < 3;i++) sum += tmp[i];
			for (size_t i=0;i < rec.data.size();i++) sum += rec.data[i];
			sum += chk;

			if (sum != 0u) {
				log.log(LNKLOG_WARNING,"OMF record with bad checksum");
				return false;
			}
		}

		/* In order to read .LIB files properly this code needs to know the block size */
		if (rec.file_offset == 0 && rec.type == 0xF0/*Library Header Record*/) {
			/* <dictionary offset> <dictionary size in blocks> <flags>
			 * The record length is used to indicate block size */
			is_library = true;
			block_size = rec.data.size() + 3/*header*/ + 1/*checksum*/;
			dict_offset = rec.data.gd();
			dict_size = rec.data.gw();
			lib_flags = rec.data.gb();

			/* must be a power of 2 or else it's not valid */
			if ((block_size & (block_size - uint32_t(1ul))) == uint32_t(0ul)) {
				dict_size *= block_size;
			}
			else {
				log.log(LNKLOG_WARNING,"OMF ignoring library header record because block size %lu is not a power of 2",(unsigned long)block_size);
				is_library = false;
			}

			log.log(LNKLOG_DEBUG,"OMF file is library with block_size=%lu, dict_offset=%lu, dict_size=%lu, flags=0x%02x",
				(unsigned long)block_size,(unsigned long)dict_offset,(unsigned long)dict_size,lib_flags);
		}

		if (block_size != uint32_t(0)) {
			if (rec.type == 0x8A/*module end*/ || rec.type == 0x8B/*module end*/) {
				off_t p = fio.tell();
				p += (off_t)(block_size - uint32_t(1u));
				p -= p % (off_t)block_size;
				if (!fio.seek(p)) {
					log.log(LNKLOG_WARNING,"OMF end of module, unable to seek forward to next within library");
					return false;
				}
			}
		}

		return true;
	}

	void OMF_reader::clear(void) {
		special_FLAT_GRPDEF = GRPDEF_list_t::undef;
		current_source = source_list_t::undef;
		LIDATA_records.clear();
		SEGDEF.clear();
		GRPDEF.clear();
		LNAMES.clear();
		recrdr.clear();
	}

	bool OMF_reader::read(linkenv &lenv,const char *path) {
		file_stdio_io fio(path);
		if (fio.ok()) {
			return read(lenv,fio,path);
		}
		else {
			lenv.log.log(LNKLOG_ERROR,"OMF reader: Unable to open and read '%s'",path);
			return false;
		}
	}

	bool OMF_reader::read(linkenv &lenv,file_io &fio,const char *path) {
		OMF_record rec;

		recrdr.clear();

		if (!recrdr.read(rec,fio,lenv.log)) {
			lenv.log.log(LNKLOG_ERROR,"Unable to read first OMF record in '%s'",path);
			return false;
		}

		if (recrdr.is_library) {
			size_t module_index = 0;

			lenv.log.log(LNKLOG_DEBUG,"OMF file '%s' is a library",path);

			while (recrdr.read(rec,fio,lenv.log)) {
				if (rec.type == 0x80/*THEADR*/ || rec.type == 0x82/*LHEADR*/) {
					{
						source_t &src = lenv.sources.get(current_source=lenv.sources.allocate());
						src.file_offset = rec.file_offset;
						src.index = module_index++;
						src.path = path;

						/* THEADR/LHEADR contains a single string */
						src.name = rec.data.glenstr();
					}

					/* read all records until the next MODEND */
					if (!read_module(lenv,fio)) return false;
				}
			}
		}
		else if (rec.type == 0x80/*THEADR*/ || rec.type == 0x82/*LHEADR*/) {
			lenv.log.log(LNKLOG_DEBUG,"OMF file '%s' is an object",path);

			{
				source_t &src = lenv.sources.get(current_source=lenv.sources.allocate());
				src.path = path;

				/* THEADR/LHEADR contains a single string */
				src.name = rec.data.glenstr();
			}

			/* read all records until the next MODEND */
			if (!read_module(lenv,fio)) return false;
		}

		return true;
	}

	bool OMF_reader::read_rec_LNAMES(linkenv &lenv,OMF_record &rec) {
		/* std::string s = rec.glenstr();
		 * string_ref_t t = lenv.strings.add(s);
		 * LNAMES_ref_t l = LNAMES.allocate(t); */
		while (!rec.data.eof()) LNAMES.allocate(lenv.strings.add(rec.data.glenstr()));
		return true;
	}

	bool OMF_reader::expand_LIDATA_blocks(fragment_t &cfr,size_t &copyto,std::vector<LIDATA_heap_t> &r,const unsigned char *lidata,const size_t lidatasize) {
		for (auto ri=r.begin();ri!=r.end();ri++) {
			auto &ent = *ri;

			if (ent.repeat >= (4*1024*1024)) return false;
			if ((ent.repeat*size_t(ent.length)) >= (4*1024*1024)) return false;
			if ((ent.repeat*ent.sub_blocks.size()) >= (4*1024*1024)) return false;

			for (size_t rep=0;rep < ent.repeat;rep++) {
				/* there can be sub blocks, or data, not both */
				if (ent.sub_blocks.empty()) {
					if (ent.length != 0) {
						assert(copyto == cfr.data.size());
						assert((size_t(ent.lidata_offset)+size_t(ent.length)) <= lidatasize);

						cfr.data.resize(cfr.data.size()+size_t(ent.length));
						assert((copyto+size_t(ent.length)) == cfr.data.size());
						memcpy((void*)(((unsigned char*)cfr.data.data())+copyto),lidata+size_t(ent.lidata_offset),ent.length);
						copyto += size_t(ent.length);
						assert(copyto == cfr.data.size());
					}
				}
				else {
					if (!expand_LIDATA_blocks(cfr,copyto,ent.sub_blocks,lidata,lidatasize)) return false;
				}
			}
		}

		return true;
	}

	bool OMF_reader::read_LIDATA_block(linkenv &lenv,const size_t base_lidata,LIDATA_heap_t &ent,OMF_record &rec,const bool fmt32,const size_t depth) {
		ent.repeat = fmt32 ? rec.data.gd() : rec.data.gw();
		const uint16_t block_count = rec.data.gw();

		/* data block = <repeat count> 0 <data length> <data>             -> <data> repeated <repeat count> times
		 *            = <repeat count> <block count> <data block>         -> [ <block count> entries of type <data block> ] repeated <repeat count> times
		 *
		 * Yes, this is recursive. */

		if (ent.repeat == 0) {
			lenv.log.log(LNKLOG_ERROR,"LIDATA data sub block repeat count == 0");
			return false;
		}
		if (rec.data.eof()) {
			lenv.log.log(LNKLOG_ERROR,"LIDATA data sub block header unexpected eof");
			return false; /* either a data byte length or data block follows, an eof here is unexpected */
		}
		if (ent.repeat > (4*1024*1024)) {
			lenv.log.log(LNKLOG_ERROR,"LIDATA data sub block repeat count too large");
			return false;
		}

		if (block_count == 0) {
			ent.length = rec.data.gb();
			ent.lidata_offset = rec.data.offset() - base_lidata;
			if (size_t(ent.length) > rec.data.remains()) {
				lenv.log.log(LNKLOG_ERROR,"LIDATA data block contents truncated");
				return false;
			}
			assert(rec.data.advance(ent.length));
			assert(rec.data.offset() <= rec.data.size());
		}
		else {
			if (depth >= 8) {
				lenv.log.log(LNKLOG_ERROR,"LIDATA data sub block recursion too deep");
				return false;
			}
			if (block_count > 1024) {
				lenv.log.log(LNKLOG_ERROR,"LIDATA data sub block count too large");
				return false;
			}

			for (size_t bc=0;bc < block_count;bc++) {
				ent.sub_blocks.emplace(ent.sub_blocks.end());
				if (!read_LIDATA_block(lenv,base_lidata,ent.sub_blocks.back(),rec,fmt32,depth+1)) {
					lenv.log.log(LNKLOG_ERROR,"LIDATA unable to read data sub block");
					return false;
				}
			}
		}

		return true;
	}

	void OMF_reader::LIDATA_records_dump_debug(linkenv &lenv,const size_t depth,const std::vector<LIDATA_heap_t> &r) {
		std::string pref;

		for (size_t s=0;s < depth;s++) pref += "  ";

		for (auto ri=r.begin();ri!=r.end();ri++) {
			const auto &ent = *ri;

			lenv.log.log(LNKLOG_DEBUG,"%sEntry: lidataofs=%lu repeat=%lu length=%lu subblocks=%lu",
				pref.c_str(),(unsigned long)ent.lidata_offset,
				(unsigned long)ent.repeat,(unsigned long)ent.length,(unsigned long)ent.sub_blocks.size());

			LIDATA_records_dump_debug(lenv,depth+size_t(1u),ent.sub_blocks);
		}
	}

	bool OMF_reader::read_rec_LIDATA(linkenv &lenv,OMF_record &rec) {
		const bool fmt32 = (rec.type == 0xA3/*LEDATA32*/);

		LIDATA_records.clear();

		/* <segment index> <data offset> <data> */
		const size_t segidx = from1based(rec.data.gidx());
		if (!SEGDEF.exists(segidx)) {
			lenv.log.log(LNKLOG_ERROR,"LIDATA refers to invalid SEGDEF index");
			return false;
		}
		const segment_ref_t segref = SEGDEF.get(segidx);
		segment_t &sego = lenv.segments.get(segref);
		const segment_offset_t dataoffset = segment_offset_t(fmt32 ? rec.data.gd() : rec.data.gw());

		if (!rec.data.eof()) {
			bool newfrag = true;

			if (!sego.fragments.empty()) {
				const auto &fr = lenv.fragments.get(sego.fragments.back());
				if ((fr.org_offset+segment_offset_t(fr.data.size())) == dataoffset) newfrag = false;
			}

			if (newfrag) {
				fragment_ref_t frref = lenv.fragments.allocate();
				fragment_t &fr = lenv.fragments.get(frref);
				sego.fragments.push_back(frref);
				fr.source = current_source;
				fr.org_offset = dataoffset;
				fr.insegment = segref;
			}
			else {
				assert(!sego.fragments.empty());
			}

			fragment_t &cfr = lenv.fragments.get(sego.fragments.back());
			assert((cfr.org_offset+segment_offset_t(cfr.data.size())) == dataoffset);

			const size_t base_lidata = rec.data.offset();

			while (!rec.data.eof()) {
				LIDATA_records.emplace(LIDATA_records.end());
				if (!read_LIDATA_block(lenv,base_lidata,LIDATA_records.back(),rec,fmt32)) {
					lenv.log.log(LNKLOG_ERROR,"LIDATA unable to read data block");
					return false;
				}
			}

			size_t copyto = LIDATA_expanded_at = cfr.data.size();
			if (!expand_LIDATA_blocks(cfr,copyto,LIDATA_records,(const unsigned char*)rec.data.data()+base_lidata,rec.data.size()-base_lidata)) {
				lenv.log.log(LNKLOG_ERROR,"LIDATA expansion failed");
				return false;
			}
		}

#if 0
		lenv.log.log(LNKLOG_DEBUG,"LIDATA:");
		LIDATA_records_dump_debug(lenv,1,LIDATA_records);
#endif

		return true;
	}

	bool OMF_reader::read_rec_LEDATA(linkenv &lenv,OMF_record &rec) {
		const bool fmt32 = (rec.type == 0xA1/*LEDATA32*/);

		LIDATA_records.clear();

		/* <segment index> <data offset> <data> */
		const size_t segidx = from1based(rec.data.gidx());
		if (!SEGDEF.exists(segidx)) {
			lenv.log.log(LNKLOG_ERROR,"LEDATA refers to invalid SEGDEF index");
			return false;
		}
		const segment_ref_t segref = SEGDEF.get(segidx);
		segment_t &sego = lenv.segments.get(segref);
		const segment_offset_t dataoffset = segment_offset_t(fmt32 ? rec.data.gd() : rec.data.gw());

		if (!rec.data.eof()) {
			bool newfrag = true;

			if (!sego.fragments.empty()) {
				const auto &fr = lenv.fragments.get(sego.fragments.back());
				if ((fr.org_offset+segment_offset_t(fr.data.size())) == dataoffset) newfrag = false;
			}

			if (newfrag) {
				fragment_ref_t frref = lenv.fragments.allocate();
				fragment_t &fr = lenv.fragments.get(frref);
				sego.fragments.push_back(frref);
				fr.source = current_source;
				fr.org_offset = dataoffset;
				fr.insegment = segref;
			}
			else {
				assert(!sego.fragments.empty());
			}

			fragment_t &cfr = lenv.fragments.get(sego.fragments.back());
			assert((cfr.org_offset+segment_offset_t(cfr.data.size())) == dataoffset);

			const uint8_t *data = (const uint8_t*)rec.data.data()+rec.data.offset();
			const size_t datalen = rec.data.remains();
			assert((data+datalen) == (const uint8_t*)rec.data.data()+rec.data.size());
			assert(datalen != size_t(0));

			const size_t copyto = cfr.data.size();
			cfr.data.resize(cfr.data.size()+datalen);
			assert((cfr.org_offset+segment_offset_t(cfr.data.size())) == (dataoffset+segment_offset_t(datalen)));

			assert((copyto+datalen) == cfr.data.size());
			memcpy((uint8_t*)cfr.data.data()+copyto,data,datalen);
		}

		return true;
	}

	bool OMF_reader::read_rec_SEGDEF(linkenv &lenv,OMF_record &rec) {
		const bool fmt32 = (rec.type == 0x99/*SEGDEF32*/);

		/* allocate a new segment */
		const segment_ref_t newsegref = lenv.segments.allocate();
		segment_t &newseg = lenv.segments.get(newsegref);
		lenv.segment_order.push_back(newsegref);

		/* keep track of this module's SEGDEFs vs the global linker state */
		SEGDEF.allocate(newsegref);

		/* <segment attributes> <segment length> <segment name index> <class name index> <overlay name index> */

		/* <alignment 7:5> <combine 4:2> <big 1:1> <P 0:0> */
		const uint8_t attr = rec.data.gb();

		/* alignment */
		switch ((attr >> 5u) & 7u) { /* 7:5 */
			case 0: /* absolute segment <frame number> <offset> */
				newseg.flags |= SEGFLAG_ABSOLUTEFRAME | SEGFLAG_ABSOLUTEOFFS;
				newseg.segmentframe = rec.data.gw();
				newseg.segmentoffset = rec.data.gb();
				newseg.alignment = byte_alignment;
				break;
			case 1:
				newseg.alignment = byte_alignment;
				break;
			case 2:
				newseg.alignment = word_alignment;
				break;
			case 3:
				newseg.alignment = para_alignment;
				break;
			case 4:
				newseg.alignment = page4k_alignment;
				break;
			case 5:
				newseg.alignment = dword_alignment;
				break;
			default:
				lenv.log.log(LNKLOG_ERROR,"Unknown alignment code in SEGDEF");
				return false;
		};

		/* combination */
		switch ((attr >> 2u) & 7u) { /* 4:2 */
			case 0:
				newseg.flags |= SEGFLAG_PRIVATE;
				break;
			case 2:
			case 4:
			case 7:
				newseg.flags |= SEGFLAG_PUBLIC | SEGFLAG_COMBINEABLE;
				break;
			case 5:
				newseg.flags |= SEGFLAG_PUBLIC | SEGFLAG_STACK | SEGFLAG_COMBINEABLE;
				break;
			case 6:
				newseg.flags |= SEGFLAG_PUBLIC | SEGFLAG_COMMON | SEGFLAG_COMBINEABLE;
				break;
			default:
				lenv.log.log(LNKLOG_ERROR,"Unknown combination code in SEGDEF");
				return false;
		}

		/* "Big". This is a way to extend the segment size field to allow 0x10000 or 0x100000000 which is otherwise impossible. */
		if (attr & 2u)
			newseg.segmentsize = fmt32 ? segment_size_t(0x100000000) : segment_size_t(0x10000);
		else
			newseg.segmentsize = 0;

		/* "P" bit. This is set when you use USE32 in your assembler */
		if (attr & 1u)
			newseg.cpumodel = CPUMODEL_X86_32BIT;
		else
			newseg.cpumodel = CPUMODEL_X86_16BIT;

		/* segment size */
		newseg.segmentsize += fmt32 ? rec.data.gd() : rec.data.gw();

		/* segment name index, which is an index into this module's LNAMES */
		{
			const size_t nameidx = from1based(rec.data.gidx());
			if (LNAMES.exists(nameidx)) { /* must exist */
				/* the LNAMES index must refer to a valid string index or else that means a bug occurred in this code */
				newseg.segmentname = LNAMES.get(nameidx);
			}
			else {
				lenv.log.log(LNKLOG_ERROR,"SEGDEF with invalid name index");
				return false;
			}
		}

		/* class name index, which is an index into this module's LNAMES */
		{
			const size_t nameidx = from1based(rec.data.gidx());
			if (LNAMES.exists(nameidx)) { /* must exist */
				/* the LNAMES index must refer to a valid string index or else that means a bug occurred in this code */
				newseg.classname = LNAMES.get(nameidx);
			}
			else {
				lenv.log.log(LNKLOG_ERROR,"SEGDEF with invalid class index");
				return false;
			}
		}

		/* overlay name index, which is an index into this module's LNAMES */
		{
			const size_t nameidx = from1based(rec.data.gidx());
			if (nameidx == 0) { /* may or may not exist */
				/* leave it undef */
			}
			else if (LNAMES.exists(nameidx)) {
				/* the LNAMES index must refer to a valid string index or else that means a bug occurred in this code */
				newseg.overlayname = LNAMES.get(nameidx);
			}
			else {
				lenv.log.log(LNKLOG_ERROR,"SEGDEF with invalid overlay index");
				return false;
			}
		}

		newseg.source = current_source;
		return true;
	}

	bool OMF_reader::read_rec_GRPDEF(linkenv &lenv,OMF_record &rec) {
		/* allocate a new group */
		const group_ref_t newgrpref = lenv.groups.allocate();
		group_t &newgrp = lenv.groups.get(newgrpref);

		/* keep track of this module's GRPDEFs vs the global linker state */
		GRPDEF.allocate(newgrpref);

		/* <groupnameidx> [ <0xFF> <SEGDEF idx> [ ... ] ] */

		/* group name index, which is an index into this module's LNAMES */
		{
			const size_t nameidx = from1based(rec.data.gidx());
			if (LNAMES.exists(nameidx)) { /* must exist */
				/* the LNAMES index must refer to a valid string index or else that means a bug occurred in this code */
				newgrp.groupname = LNAMES.get(nameidx);
			}
			else {
				return false;
			}
		}

		/* "FLAT" does not usually list any SEGDEFs but means the address space is flat with no segmentation. */
		if (lenv.strings.get(newgrp.groupname) == "FLAT") {
			if (special_FLAT_GRPDEF == group_list_t::undef)
				special_FLAT_GRPDEF = newgrpref;
		}

		while (!rec.data.eof()) {
			const uint8_t typ = rec.data.gb();

			if (typ == 0xFF) {
				const size_t segidx = from1based(rec.data.gidx());
				if (SEGDEF.exists(segidx)) {
					/* the SEGDEF index must exist or else that means a bug occurred in this code */
					const segment_ref_t segref = SEGDEF.get(segidx);
					auto &segdef = lenv.segments.get(segref);
					/* track membership */
					newgrp.segments.push_back(segref);
					/* mark the segment as having membership of this group, unless the segment was already marked */
					if (segdef.group == group_list_t::undef) {
						segdef.group = newgrpref;
					}
					else if (segdef.group != newgrpref) {
						lenv.log.log(LNKLOG_ERROR,"GRPDEF assigns membership of a SEGDEF that has already been given membership of another GRPDEF");
						return false;
					}
				}
				else {
					lenv.log.log(LNKLOG_ERROR,"GRPDEF refers to invalid SEGDEF index");
					return false;
				}
			}
			else {
				lenv.log.log(LNKLOG_ERROR,"GRPDEF with unsupported index value %02x",typ);
				return false;
			}
		}

		newgrp.source = current_source;
		return true;
	}

	bool OMF_reader::read_module(linkenv &lenv,file_io &fio) {
		OMF_record rec;

		/* each module has it's own LNAMES, SEGDEF, GRPDEF */
		special_FLAT_GRPDEF = group_list_t::undef;
		LIDATA_records.clear();
		SEGDEF.clear();
		GRPDEF.clear();
		LNAMES.clear();

		/* caller has already read THEADR/LHEADR, read until MODEND */
		while (1) {
			if (!recrdr.read(rec,fio,lenv.log)) {
				lenv.log.log(LNKLOG_ERROR,"OMF module ended early without MODEND");
				return false;
			}

			if (rec.type == 0x8A/*module end*/ || rec.type == 0x8B/*module end*/) {
				/* TODO: Read entry point and other stuff */
				break;
			}
			else if (rec.type == 0x96/*LNAMES*/ || rec.type == 0xCA/*LLNAMES*/) {
				if (!read_rec_LNAMES(lenv,rec)) {
					lenv.log.log(LNKLOG_ERROR,"Error reading LNAMES");
					return false;
				}
			}
			else if (rec.type == 0x98/*SEGDEF*/ || rec.type == 0x99/*SEGDEF32*/) {
				if (!read_rec_SEGDEF(lenv,rec)) {
					lenv.log.log(LNKLOG_ERROR,"Error reading SEGDEF");
					return false;
				}
			}
			else if (rec.type == 0x9A/*GRPDEF*/) {
				if (!read_rec_GRPDEF(lenv,rec)) {
					lenv.log.log(LNKLOG_ERROR,"Error reading GRPDEF");
					return false;
				}
			}
			else if (rec.type == 0xA0/*LEDATA*/ || rec.type == 0xA1/*LEDATA32*/) {
				if (!read_rec_LEDATA(lenv,rec)) {
					lenv.log.log(LNKLOG_ERROR,"Error reading LEDATA");
					return false;
				}
			}
			else if (rec.type == 0xA2/*LIDATA*/ || rec.type == 0xA3/*LIDATA32*/) {
				if (!read_rec_LIDATA(lenv,rec)) {
					lenv.log.log(LNKLOG_ERROR,"Error reading LIDATA");
					return false;
				}
			}
		}

		/* All segments are segmented model, unless the FLAT GRPDEF was seen */
		for (auto si=SEGDEF.ref.begin();si!=SEGDEF.ref.end();si++) {
			auto &segref = lenv.segments.get(*si);

			/* if not already marked as either, decide now */
			if ((segref.flags & (SEGFLAG_FLAT|SEGFLAG_SEGMENTED)) == 0) {
				if (special_FLAT_GRPDEF != group_list_t::undef)
					segref.flags |= SEGFLAG_FLAT;
				else
					segref.flags |= SEGFLAG_SEGMENTED;
			}
		}

		return true;
	}

}

int main(int argc,char **argv) {
	(void)argc;
	(void)argv;
	DOSLIBLinker::linkenv lenv;
	DOSLIBLinker::OMF_reader omfr;
	lenv.log.min_level = DOSLIBLinker::LNKLOG_DEBUGMORE;
	if (!omfr.read(lenv,"/usr/src/doslib/fmt/omf/testfile/0014.obj")) fprintf(stderr,"Failed to read 0014.obj\n");
	if (!omfr.read(lenv,"/usr/src/doslib/hw/dos/dos386f/dos.lib")) fprintf(stderr,"Failed to read dos.lib\n");
	if (!omfr.read(lenv,"/usr/src/doslib/hw/cpu/dos86l/cpu.lib")) fprintf(stderr,"Failed to read cpu.lib\n");
	if (!omfr.read(lenv,"/usr/src/doslib/hw/cpu/dos86s/cpu.lib")) fprintf(stderr,"Failed to read cpu.lib\n");

	fprintf(stderr,"Sources:\n");
	for (auto si=lenv.sources.ref.begin();si!=lenv.sources.ref.end();si++) {
		fprintf(stderr,"  [%lu]: path='%s' name='%s' index=%ld fileofs=%ld\n",
			(unsigned long)(si - lenv.sources.ref.begin()),
			(*si).path.c_str(),
			(*si).name.c_str(),
			(signed long)((*si).index),
			(signed long)((*si).file_offset));
	}

	fprintf(stderr,"Segments:\n");
	for (auto si=lenv.segments.ref.begin();si!=lenv.segments.ref.end();si++) {
		const auto &seg = *si;

		fprintf(stderr,"  [%lu]: name='%s' group='%s' class='%s' overlay='%s'\n",
			(unsigned long)(si - lenv.segments.ref.begin()),
			(seg.segmentname != DOSLIBLinker::string_table_t::undef) ? lenv.strings.get(seg.segmentname).c_str() : "(undef)",
			(seg.group       != DOSLIBLinker::group_list_t::undef  ) ? lenv.strings.get(lenv.groups.get(seg.group).groupname).c_str() : "(undef)",
			(seg.classname   != DOSLIBLinker::string_table_t::undef) ? lenv.strings.get(seg.classname  ).c_str() : "(undef)",
			(seg.overlayname != DOSLIBLinker::string_table_t::undef) ? lenv.strings.get(seg.overlayname).c_str() : "(undef)");
		fprintf(stderr,"    alignment: mask=0x%lx value=%lu\n",
			(unsigned long)seg.alignment,(unsigned long)DOSLIBLinker::alignmask2value(seg.alignment));
		if (seg.segmentsize != DOSLIBLinker::segment_size_undef)
			fprintf(stderr,"    size: 0x%lx bytes\n",(unsigned long)seg.segmentsize);
		if (seg.segmentoffset != DOSLIBLinker::segment_offset_undef)
			fprintf(stderr,"    offset: 0x%lx bytes\n",(unsigned long)seg.segmentoffset);
		if (seg.segmentframe != DOSLIBLinker::segment_frame_undef)
			fprintf(stderr,"    frame: 0x%lx bytes\n",(unsigned long)seg.segmentframe);
		if (seg.source != DOSLIBLinker::source_list_t::undef) {
			const auto &src = lenv.sources.get(seg.source);
			fprintf(stderr,"    source: path='%s' name='%s' index=%ld offset=%ld\n",
				src.path.c_str(),src.name.c_str(),(signed long)src.index,(signed long)src.file_offset);
		}
		if (seg.cpumodel != DOSLIBLinker::cpu_model_undef) {
			fprintf(stderr,"    CPU model: ");
			switch (seg.cpumodel) {
				case DOSLIBLinker::CPUMODEL_X86_16BIT: fprintf(stderr,"80x86 16-bit"); break;
				case DOSLIBLinker::CPUMODEL_X86_32BIT: fprintf(stderr,"80x86 32-bit"); break;
				case DOSLIBLinker::CPUMODEL_X86_64BIT: fprintf(stderr,"80x86 64-bit"); break;
				default: fprintf(stderr,"Unknown 0x%x",(unsigned)seg.cpumodel); break;
			}
			fprintf(stderr,"\n");
		}
		fprintf(stderr,"    flags:");
		if (seg.flags & DOSLIBLinker::SEGFLAG_DELETED)       fprintf(stderr," DELETED");
		if (seg.flags & DOSLIBLinker::SEGFLAG_NOEMIT)        fprintf(stderr," NOEMIT");
		if (seg.flags & DOSLIBLinker::SEGFLAG_PRIVATE)       fprintf(stderr," PRIVATE");
		if (seg.flags & DOSLIBLinker::SEGFLAG_PUBLIC)        fprintf(stderr," PUBLIC");
		if (seg.flags & DOSLIBLinker::SEGFLAG_STACK)         fprintf(stderr," STACK");
		if (seg.flags & DOSLIBLinker::SEGFLAG_COMMON)        fprintf(stderr," COMMON");
		if (seg.flags & DOSLIBLinker::SEGFLAG_ABSOLUTEFRAME) fprintf(stderr," ABSOLUTEFRAME");
		if (seg.flags & DOSLIBLinker::SEGFLAG_ABSOLUTEOFFS)  fprintf(stderr," ABSOLUTEOFFS");
		if (seg.flags & DOSLIBLinker::SEGFLAG_COMBINEABLE)   fprintf(stderr," COMBINEABLE");
		if (seg.flags & DOSLIBLinker::SEGFLAG_SEGMENTED)     fprintf(stderr," SEGMENTED");
		if (seg.flags & DOSLIBLinker::SEGFLAG_FLAT)          fprintf(stderr," FLAT");
		fprintf(stderr,"\n");
	}

	fprintf(stderr,"Segment order:");
	for (auto soi=lenv.segment_order.begin();soi!=lenv.segment_order.end();soi++) {
		const auto &segdef = lenv.segments.get(*soi);
		fprintf(stderr," [%lu]'%s'",(unsigned long)(*soi),lenv.strings.get(segdef.segmentname).c_str());
	}
	fprintf(stderr,"\n");

	fprintf(stderr,"Groups:\n");
	for (auto si=lenv.groups.ref.begin();si!=lenv.groups.ref.end();si++) {
		const auto &grp = *si;
		fprintf(stderr,"  [%lu]: '%s'\n",(unsigned long)(si - lenv.groups.ref.begin()),lenv.strings.get(grp.groupname).c_str());
		if (!grp.segments.empty()) {
			fprintf(stderr,"    segments:");
			for (auto gsi=grp.segments.begin();gsi!=grp.segments.end();gsi++)
				fprintf(stderr," [%lu]'%s'",(unsigned long)(*gsi),lenv.strings.get(lenv.segments.get(*gsi).segmentname).c_str());
			fprintf(stderr,"\n");
		}
		if (grp.source != DOSLIBLinker::source_list_t::undef) {
			const auto &src = lenv.sources.get(grp.source);
			fprintf(stderr,"    source: path='%s' name='%s' index=%ld offset=%ld\n",
				src.path.c_str(),src.name.c_str(),(signed long)src.index,(signed long)src.file_offset);
		}
	}

	fprintf(stderr,"Strings:\n");
	for (auto si=lenv.strings.ref.begin();si!=lenv.strings.ref.end();si++)
		fprintf(stderr,"  [%lu]: '%s'\n",(unsigned long)(si - lenv.strings.ref.begin()),(*si).c_str());

	fprintf(stderr,"Fragments:\n");
	for (auto fi=lenv.fragments.ref.begin();fi!=lenv.fragments.ref.end();fi++) {
		const auto &fr = *fi;

		fprintf(stderr,"  [%lu] segment[%lu]'%s'",
			(unsigned long)(fi-lenv.fragments.ref.begin()),
			(unsigned long)(fr.insegment),
			(fr.insegment != DOSLIBLinker::segment_list_t::undef) ? lenv.strings.get(lenv.segments.get(fr.insegment).segmentname).c_str() : "");
		if (fr.org_offset != DOSLIBLinker::segment_offset_undef)
			fprintf(stderr," org_offset=0x%lx",(unsigned long)fr.org_offset);
		if (fr.segmentoffset != DOSLIBLinker::segment_offset_undef)
			fprintf(stderr," segmentoffset=0x%lx",(unsigned long)fr.org_offset);
		if (fr.fragmentsize != DOSLIBLinker::segment_size_undef)
			fprintf(stderr," fragsize=0x%lx",(unsigned long)fr.fragmentsize);
		if (fr.alignment != DOSLIBLinker::alignment_undef)
			fprintf(stderr," alignmask=0x%lx(%lu bytes)",(unsigned long)fr.alignment,(unsigned long)DOSLIBLinker::alignmask2value(fr.alignment));
		fprintf(stderr,"\n");

		if (fr.source != DOSLIBLinker::source_list_t::undef) {
			const auto &src = lenv.sources.get(fr.source);
			fprintf(stderr,"  source: path='%s' name='%s' index=%ld offset=%ld\n",
				src.path.c_str(),src.name.c_str(),(signed long)src.index,(signed long)src.file_offset);
		}

		if (!fr.data.empty()) {
			size_t si = 0;
			unsigned char col = 0;
			DOSLIBLinker::segment_offset_t addr = fr.org_offset;
			if (addr == DOSLIBLinker::segment_offset_undef) addr = DOSLIBLinker::segment_offset_t(0);

			fprintf(stderr,"  Data [%08lx-%08lx]:\n",(unsigned long)addr,(unsigned long)addr+(unsigned long)fr.data.size()-1ul);
			while (si < fr.data.size()) {
				if (col == 0) fprintf(stderr,"    %08lx:",(unsigned long)(addr & DOSLIBLinker::para_alignment));
				if (col == ((unsigned char)addr & 0xFu)) {
					fprintf(stderr," %02x",fr.data[si]);
					addr++;
					si++;
				}
				else {
					fprintf(stderr,"   ");
				}
				if ((++col) == 16) {
					fprintf(stderr,"\n");
					col = 0;
				}
			}
			if (col != 0) fprintf(stderr,"\n");
		}
	}

	return 0;
}

