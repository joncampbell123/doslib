
#include <sys/types.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include "byteorder.h"

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
			static constexpr size_t						undef = ~((size_t)(0ul));
			typedef std::string						ent_type;
			typedef size_t							ref_type;
			std::vector<std::string>					ref;
			std::unordered_map<std::string,size_t>				ref2rev;
			std::unordered_map<std::string, std::vector<size_t> >		ref2revic;
		private:
			const std::vector<size_t>					empty_ric;
		public:
			inline size_t size(void) const { return ref.size(); }
		public:
			static std::string ic(const std::string &s) {
				std::string r;

				for (auto si=s.begin();si!=s.end();si++) {
					if (*si >= 32 && *si < 127)
						r += toupper(*si);
					else
						r += *si;
				}

				return r;
			}
		public:
			bool exists(const size_t r) const {
				return r < ref.size();
			}

			bool exists(const std::string &r) const {
				return ref2rev.find(r) != ref2rev.end();
			}

			/* String "r" must be the return value of the ic() function. This function does not do it for you
			 * in order to avoid redundant calls to ic() when you are searching for symbols. */
			bool existsic(const std::string &r) const {
				return ref2revic.find(r) != ref2revic.end(); /* This will not match anything that wasn't added as case insensitive */
			}

			size_t lookup(const std::string &r) const {
				const auto i = ref2rev.find(r);
				if (i != ref2rev.end()) return i->second;
				return undef;
			}

			/* String "r" must be the return value of the ic() function. This function does not do it for you
			 * in order to avoid redundant calls to ic() when you are searching for symbols. */
			const std::vector<size_t> &lookupic(const std::string &r) const {
				const auto i = ref2revic.find(r); /* This will not match anything that wasn't added as case insensitive */
				if (i != ref2revic.end()) return i->second;
				return empty_ric;
			}

			inline const std::string &get(const size_t r) const {
				return ref.at(r); /* std::vector will throw an exception if out of range */
			}

			inline std::string &get(const size_t r) {
				return ref.at(r); /* std::vector will throw an exception if out of range */
			}

			size_t add(const std::string &r,const bool add_ic=false) {
				{
					const auto i = ref2rev.find(r);
					if (i != ref2rev.end()) return i->second;
				}

				const size_t ri = ref.size();
				ref.emplace(ref.end(),r);
				assert(ref.size() == (ri + size_t(1u)));
				if (add_ic) ref2revic[ic(r)].push_back(ri);
				ref2rev[r] = ri;
				return ri;
			}

			size_t addic(const std::string &r) {
				return add(r,/*add_ic=*/true);
			}

			void clear(void) {
				ref.clear();
				ref2rev.clear();
				ref2revic.clear();
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
	static constexpr alignment_t				para_alignment = alignment2mask(16);
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
	static constexpr fragment_flags_t			FRAGFLAG_EMPTY =         fragment_flags_t(1u) << fragment_flags_t(0u); // it's supposed to be empty (data.size() == 0)

	typedef uint32_t					group_flags_t;
	static constexpr group_flags_t				GRPFLAG_DELETED =        group_flags_t(1u) << group_flags_t(0u); // removed, usually when merging with another

	typedef uint32_t					symbol_flags_t;
	static constexpr symbol_flags_t				SYMFLAG_DELETED =        symbol_flags_t(1u) << symbol_flags_t(0u); // deleted
	static constexpr symbol_flags_t                         SYMFLAG_LOCAL =          symbol_flags_t(1u) << symbol_flags_t(1u); // local symbol
	static constexpr symbol_flags_t				SYMFLAG_NEAR =           symbol_flags_t(1u) << symbol_flags_t(2u); // near symbol
	static constexpr symbol_flags_t				SYMFLAG_FAR =            symbol_flags_t(1u) << symbol_flags_t(3u); // far symbol

	typedef uint8_t						symbol_type_t;
	static constexpr symbol_type_t				symbol_type_undef =      symbol_type_t(0xFFu);
	static constexpr symbol_type_t				symbol_type_extern =     symbol_type_t(0x00u); // extern symbol (EXTDEF)
	static constexpr symbol_type_t				symbol_type_common =     symbol_type_t(0x01u); // common symbol (COMDEF)
	static constexpr symbol_type_t				symbol_type_public =     symbol_type_t(0x02u); // public symbol (PUBDEF)

	typedef uint32_t					fixup_flags_t;
	static constexpr fixup_flags_t				fixup_flag_self_relative = fixup_flags_t(1u) << fixup_flags_t(0u); // self relative

	typedef uint8_t						fixup_how_t;
	static constexpr fixup_how_t				fixup_how_undef =        fixup_how_t(0xFFu);
	static constexpr fixup_how_t				fixup_how_offset8 =      fixup_how_t(0x00u); // low 8 bits of offset or 8-bit displacement
	static constexpr fixup_how_t				fixup_how_offset16 =     fixup_how_t(0x01u); // 16-bit offset
	static constexpr fixup_how_t				fixup_how_segbase16 =    fixup_how_t(0x02u); // 16-bit segment base value
	static constexpr fixup_how_t				fixup_how_far1616 =      fixup_how_t(0x03u); // 16:16 segment:offset
	static constexpr fixup_how_t				fixup_how_offset8hi =    fixup_how_t(0x04u); // high 8 bits of offset (not supported by MASM?)
	static constexpr fixup_how_t				fixup_how_offset32 =     fixup_how_t(0x05u); // 32-bit offset
	static constexpr fixup_how_t				fixup_how_far1632 =      fixup_how_t(0x06u); // 16:32 segment:offset (48 bits total)

	struct symbol_t;
	struct segment_t;
	struct fragment_t;
	struct fixup_t;
	struct group_t;

	typedef _base_handlearray<symbol_t> symbol_list_t;
	typedef symbol_list_t::ref_type symbol_ref_t;

	typedef _base_handlearray<segment_t> segment_list_t;
	typedef segment_list_t::ref_type segment_ref_t;

	typedef _base_handlearray<fragment_t> fragment_list_t;
	typedef fragment_list_t::ref_type fragment_ref_t;

	typedef _base_handlearray<fixup_t> fixup_list_t;
	typedef fixup_list_t::ref_type fixup_ref_t;

	typedef _base_handlearray<group_t> group_list_t;
	typedef group_list_t::ref_type group_ref_t;

	struct fixup_t {
		public:
			struct param_t {
				enum {
					TYPE_NONE = 0,
					TYPE_SEGDEF,
					TYPE_GRPDEF,
					TYPE_SYMBOL,
					TYPE_SEGMENTFRAME
				};
				unsigned int type = TYPE_NONE;
				union {
					segment_ref_t			segdef;
					group_ref_t			grpdef;
					symbol_ref_t			symbol;
					fragment_ref_t			fragment;
					segment_frame_t			segmentframe;
				} p;
			};
			struct param_t				frame,target;
			segment_offset_t			target_displacement = segment_offset_undef;
			fragment_ref_t				apply_to = fragment_list_t::undef;
			segment_offset_t			apply_at = segment_offset_undef;
			fixup_how_t				how = fixup_how_undef;
			fixup_flags_t				flags = 0;
	};

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
			segment_ref_t				segment_moved_to = segment_list_t::undef; /* if deleted, refer to this segment instead */
			segment_flags_t				flags = 0;
			std::vector<fragment_ref_t>		fragments;
	};

	struct group_t {
		public:
			group_flags_t				flags = 0;
			group_ref_t				group_moved_to = group_list_t::undef; /* if deleted, refer to this group instead */
			string_ref_t				groupname = string_table_t::undef;
			source_ref_t				source = source_list_t::undef; /* source of group, undef if combined from multiple sources */
			std::vector<segment_ref_t>		segments; /* segments with membership in the group */
	};

	struct symbol_t {
		public:
			symbol_flags_t				flags = 0;
			symbol_ref_t				symbol_moved_to = symbol_list_t::undef; /* symbol moved here if merged */
			string_ref_t				symbolname = symbol_list_t::undef; /* symbol name */
			source_ref_t				source = source_list_t::undef; /* source it came from */
			symbol_type_t				symboltype = symbol_type_undef; /* symbol type */
			segment_size_t				symbolsize = segment_size_undef; /* size of symbol (COMDEF) */
			group_ref_t				base_group = group_list_t::undef; /* base group if specified */
			fragment_ref_t				base_fragment = fragment_list_t::undef; /* base segment if specified (as fragment which then refers to segment) */
			segment_ref_t				base_segment = segment_list_t::undef; /* base segment (should be resolved to fragment) */
			segment_frame_t				base_frame_number = segment_frame_undef; /* base frame number if set */
			segment_offset_t			symbol_offset = segment_offset_undef; /* symbol offset within fragment */
	};

	class linkenv {
		public:
			source_list_t				sources;
			string_table_t				strings;
			segment_list_t				segments;
			fragment_list_t				fragments;
			symbol_list_t				symbols;
			fixup_list_t				fixups;
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
			uint32_t gcomdeflen(void);
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

			typedef _base_handlearray<symbol_ref_t> EXTDEF_list_t; /* and COMDEF */
			typedef EXTDEF_list_t::ref_type EXTDEF_ref_t;

			typedef _base_handlearray<symbol_ref_t> PUBDEF_list_t;
			typedef EXTDEF_list_t::ref_type PUBDEF_ref_t;
		public:
			fragment_ref_t			LEDATA_last_fragment = fragment_list_t::undef;
			segment_offset_t		LEDATA_last_offset = segment_offset_undef;
			source_ref_t			current_source = source_list_t::undef;
			std::vector<LIDATA_heap_t>	LIDATA_records; /* LIDATA records converted to vector for FIXUPP later on */
			size_t				LIDATA_expanded_at; /* offset within fragment LIDATA was expanded to */
			bool				LIDATA_last_entry = false;
			LNAMES_list_t			LNAMES;
			SEGDEF_list_t			SEGDEF;
			GRPDEF_list_t			GRPDEF;
			EXTDEF_list_t			EXTDEF;
			PUBDEF_list_t			PUBDEF;
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
			bool				read_rec_EXTDEF(linkenv &lenv,OMF_record &rec);
			bool				read_rec_COMDEF(linkenv &lenv,OMF_record &rec);
			bool				read_rec_PUBDEF(linkenv &lenv,OMF_record &rec);
			bool				read_rec_FIXUPP(linkenv &lenv,OMF_record &rec);
		private:
			fragment_ref_t			segdef_offset_to_fragment(linkenv &lenv,const segment_ref_t sref,const segment_offset_t sofs);
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

	uint32_t OMF_record_data::gcomdeflen(void) {
		/* COMDEFs encode symbol size in a very odd variable length manner */
		uint32_t r = gb();

		if (r == 0x81) { // 0x81 <16-bit WORD>
			r = gw();
		}
		else if (r == 0x84) { // 0x84 <24-bit WORD>
			r = uint32_t(gb()); /* little endian, correct? */
			r += uint32_t(gw()) << uint32_t(8u);
		}
		else if (r == 0x88) { // 0x88 <32-bit DWORD>
			r = gd();
		}
		else if (r > 0x80) {
			r = 0;
		}

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

	fragment_ref_t OMF_reader::segdef_offset_to_fragment(linkenv &lenv,const segment_ref_t sref,const segment_offset_t sofs) {
		if (lenv.segments.exists(sref)) {
			const auto &sg = lenv.segments.get(sref);

			for (auto fi=sg.fragments.begin();fi!=sg.fragments.end();fi++) {
				const auto &fr = lenv.fragments.get(*fi);
				if (fr.org_offset == segment_offset_undef) continue; /* this OMF reader ALWAYS fills this in */

				segment_size_t frsz = segment_size_t(fr.data.size());
				if (frsz < fr.fragmentsize) frsz = fr.fragmentsize;

				if (sofs >= fr.org_offset && sofs < (sofs+segment_offset_t(frsz)))
					return *fi;
			}
		}

		return fragment_list_t::undef;
	}

	void OMF_reader::clear(void) {
		special_FLAT_GRPDEF = GRPDEF_list_t::undef;
		LIDATA_last_entry = false;
		LIDATA_records.clear();
		SEGDEF.clear();
		GRPDEF.clear();
		LNAMES.clear();
		EXTDEF.clear();
		PUBDEF.clear();
		recrdr.clear();

		current_source = source_list_t::undef;
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
		LIDATA_last_entry = true;
		LEDATA_last_offset = segment_offset_undef;
		LEDATA_last_fragment = fragment_list_t::undef;

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
				const auto &fr = lenv.fragments.get(LEDATA_last_fragment=sego.fragments.back());
				if ((fr.org_offset+segment_offset_t(fr.data.size())) == dataoffset) newfrag = false;
			}

			if (newfrag) {
				fragment_ref_t frref = LEDATA_last_fragment = lenv.fragments.allocate();
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
			LEDATA_last_offset = (segment_offset_t)dataoffset - (segment_offset_t)cfr.org_offset;

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

			if (cfr.fragmentsize < segment_offset_t(cfr.data.size()))
				cfr.fragmentsize = segment_offset_t(cfr.data.size());
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
		LIDATA_last_entry = false;
		LEDATA_last_offset = segment_offset_undef;
		LEDATA_last_fragment = fragment_list_t::undef;

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
				const auto &fr = lenv.fragments.get(LEDATA_last_fragment=sego.fragments.back());
				if ((fr.org_offset+segment_offset_t(fr.data.size())) == dataoffset) newfrag = false;
			}

			if (newfrag) {
				fragment_ref_t frref = LEDATA_last_fragment = lenv.fragments.allocate();
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
			LEDATA_last_offset = (segment_offset_t)dataoffset - (segment_offset_t)cfr.org_offset;

			const uint8_t *data = (const uint8_t*)rec.data.data()+rec.data.offset();
			const size_t datalen = rec.data.remains();
			assert((data+datalen) == (const uint8_t*)rec.data.data()+rec.data.size());
			assert(datalen != size_t(0));

			const size_t copyto = cfr.data.size();
			cfr.data.resize(cfr.data.size()+datalen);
			assert((cfr.org_offset+segment_offset_t(cfr.data.size())) == (dataoffset+segment_offset_t(datalen)));

			assert((copyto+datalen) == cfr.data.size());
			memcpy((uint8_t*)cfr.data.data()+copyto,data,datalen);

			if (cfr.fragmentsize < segment_offset_t(cfr.data.size()))
				cfr.fragmentsize = segment_offset_t(cfr.data.size());
		}

		return true;
	}

	bool OMF_reader::read_rec_PUBDEF(linkenv &lenv,OMF_record &rec) {
		const bool local = (rec.type == 0xB6/*LPUBDEF*/ || rec.type == 0xB7/*LPUBDEF*/);
		const bool use32 = (rec.type == 0x91/*PUBDEF*/ || rec.type == 0xB7/*LPUBDEF*/);

		/* <base group index> <base segment index> [ <base frame if group==0 and segment==0> ] */
		const uint16_t basegroupidx = rec.data.gidx();
		const uint16_t basesegidx = rec.data.gidx();
		uint16_t baseframe = 0;

		if (basegroupidx == 0 && basesegidx == 0)
			baseframe = rec.data.gw();

		/* <public name string> <offset> <type index> */
		while (!rec.data.eof()) {
			const std::string namestr = rec.data.glenstr();
			if (namestr.empty()) {
				lenv.log.log(LNKLOG_ERROR,"PUBDEF with null name");
				return false;
			}
			const string_ref_t nameref = lenv.strings.add(namestr);

			const uint32_t offset = use32 ? rec.data.gd() : rec.data.gw();

			const uint16_t typeindex = rec.data.gidx();
			(void)typeindex;//ignored

			/* allocate a new symbol */
			const symbol_ref_t newsymref = lenv.symbols.allocate();
			symbol_t &newsym = lenv.symbols.get(newsymref);

			/* keep track of this module's PUBDEFs vs the global linker state */
			PUBDEF.allocate(newsymref);

			if (local)
				newsym.flags |= SYMFLAG_LOCAL;

			newsym.symbolname = nameref;
			newsym.symboltype = symbol_type_public;
			newsym.symbol_offset = segment_offset_t(offset);
			newsym.source = current_source;

			if (basegroupidx == 0 && basesegidx == 0) {
				newsym.base_frame_number = segment_frame_t(baseframe);
			}
			else {
				if (basegroupidx != 0) {
					if (!GRPDEF.exists(from1based(basegroupidx))) {
						lenv.log.log(LNKLOG_ERROR,"PUBDEF refers to non-existent GRPDEF");
						return false;
					}
					newsym.base_group = GRPDEF.get(from1based(basegroupidx));
				}
				if (basesegidx != 0) {
					if (!SEGDEF.exists(from1based(basesegidx))) {
						lenv.log.log(LNKLOG_ERROR,"PUBDEF refers to non-existent SEGDEF");
						return false;
					}
					newsym.base_segment = SEGDEF.get(from1based(basesegidx));
					/* NTS: This code will match to SEGDEF fragments later. Compilers and assemblers can and often will
					 *      emit PUBDEFs for a segment prior to any LEDATA/LIDATA chunks. However, the SEGDEF does list
					 *      the size of the segment and we should worry about whether the offset extends past the end
					 *      of the segment! At this stage in the OMF loader SEGDEFs have not been combined. */
					if (newsym.symbol_offset >= lenv.segments.get(newsym.base_segment).segmentsize) {
						lenv.log.log(LNKLOG_ERROR,"PUBDEF symbol offset lies outside the SEGDEF it belongs to");
						return false;
					}
				}
			}
		}

		return true;
	}

	bool OMF_reader::read_rec_EXTDEF(linkenv &lenv,OMF_record &rec) {
		const bool local = (rec.type == 0xB4/*LEXTDEF*/ || rec.type == 0xB5/*LEXTDEF*/);

		/* extdef = <symbol name> <type index>
		 * <extdef> [ <extdef> ... ] */
		while (!rec.data.eof()) {
			const std::string namestr = rec.data.glenstr();
			if (namestr.empty()) {
				lenv.log.log(LNKLOG_ERROR,"EXTDEF with null name");
				return false;
			}
			const string_ref_t nameref = lenv.strings.add(namestr);

			const uint16_t typeindex = rec.data.gidx();
			(void)typeindex;//ignored

			/* allocate a new symbol */
			const symbol_ref_t newsymref = lenv.symbols.allocate();
			symbol_t &newsym = lenv.symbols.get(newsymref);

			/* keep track of this module's EXTDEFs vs the global linker state */
			EXTDEF.allocate(newsymref);

			if (local)
				newsym.flags |= SYMFLAG_LOCAL;

			newsym.symbolname = nameref;
			newsym.symboltype = symbol_type_extern;
			newsym.source = current_source;
		}

		return true;
	}

	bool OMF_reader::read_rec_COMDEF(linkenv &lenv,OMF_record &rec) {
		const bool local = (rec.type == 0xB8/*LCOMDEF*/);

		/* comdef = <symbol name> <type index> <data type> <communal length>
		 * <comdef> [ <comdef> ... ] */
		while (!rec.data.eof()) {
			const std::string namestr = rec.data.glenstr();
			if (namestr.empty()) {
				lenv.log.log(LNKLOG_ERROR,"COMDEF with null name");
				return false;
			}
			const string_ref_t nameref = lenv.strings.add(namestr);

			const uint16_t typeindex = rec.data.gidx();
			(void)typeindex;//ignored

			const uint8_t datatype = rec.data.gb();

			/* allocate a new symbol */
			const symbol_ref_t newsymref = lenv.symbols.allocate();
			symbol_t &newsym = lenv.symbols.get(newsymref);

			/* keep track of this module's EXTDEFs vs the global linker state (COMDEFs are listed the same as EXTDEFs) */
			EXTDEF.allocate(newsymref);

			/* local symbol? */
			if (local) newsym.flags |= SYMFLAG_LOCAL;

			/* and then communal symbol length in a strange extended sort of way */
			uint32_t comlen = 0;
			if (datatype == 0x61/*FAR data*/) {
				uint32_t elemcount = rec.data.gcomdeflen();
				uint32_t elemsize = rec.data.gcomdeflen();

				newsym.flags |= SYMFLAG_FAR;
				if (elemsize > 0x8000u || elemcount > 0x100000 || elemsize == 0 || elemcount == 0) {
					/* looks very sus (picious), maybe we misread it */
					lenv.log.log(LNKLOG_ERROR,"COMDEF with abnormal FAR data type values count=%lu size=%lu",(unsigned long)elemcount,(unsigned long)elemsize);
					return false;
				}

				comlen = elemcount * elemsize;
			}
			else if (datatype == 0x62/*NEAR data*/) {
				comlen = rec.data.gcomdeflen();

				newsym.flags |= SYMFLAG_NEAR;
				if (comlen == 0 || comlen >= 0x100000) {
					/* looks very sus (picious), maybe we misread it */
					lenv.log.log(LNKLOG_ERROR,"COMDEF with abnormal NEAR data type values size=%lu",(unsigned long)comlen);
					return false;
				}
			}
			else {
				/* Anything else? "Borland segment index"? */
				lenv.log.log(LNKLOG_ERROR,"COMDEF with unknown data type 0x%02x",datatype);
				return false;
			}

			newsym.symbolname = nameref;
			newsym.symboltype = symbol_type_common;
			newsym.symbolsize = comlen;
			newsym.source = current_source;
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

	bool OMF_reader::read_rec_FIXUPP(linkenv &lenv,OMF_record &rec) {
		const bool fmt32 = (rec.type == 0x9D/*SEGDEF32*/);

		if (LIDATA_last_entry) {
			lenv.log.log(LNKLOG_WARNING,"FIXUPP: LIDATA is not yet supported");
			return true;
		}

		if (!lenv.fragments.exists(LEDATA_last_fragment) || LEDATA_last_offset == segment_offset_undef) {
			lenv.log.log(LNKLOG_WARNING,"FIXUPP: No prior LEDATA/LIDATA to fixup");
			return true;
		}
		const auto &fr = lenv.fragments.get(LEDATA_last_fragment);

		while (!rec.data.eof()) {
			/* <THREAD>: <0|D|0|METHOD|THREAD> [<index>]
			 * <FIXUP>: <LOCAT=1|M|LOCATION|DATARECORDOFFSET> <FIXDATA=F|FRAME|T|P|TARGET> [<frame>] [<target>] [<target displacement>] */
			unsigned int fb = rec.data.gb();
			if (fb & 0x80) {
				// FIXUP
				fb = (fb << 8u) + rec.data.gb();
				/* [bit 15] = 1
				 * [bit 14] = M 1=segment-relative 0=self-relative
				 * [bit 13:10] = Location
				 * [bit 9:0] = data record offset */
				unsigned char fixdata = rec.data.gb();
				/* [bit 7] = F 1=frame refers to thread 0=frame refers to method
				 * [bit 6:4] = frame code
				 * [bit 3] = T 1=target refers to thread 0=target refers to method
				 * [bit 2] = P 1=no displacement 0=displacement
				 * [bit 1:0] = target code */
				if (fixdata & 0x88) {
					/* bit 7 or 3 set, refers to thread */
					lenv.log.log(LNKLOG_ERROR,"FIXUPP: THREAD refrences not supported");
					return false;
				}

				fixup_ref_t fixupref = lenv.fixups.allocate();
				auto &fixup = lenv.fixups.get(fixupref);
				fixup.target_displacement = 0;

				uint16_t frame_datum = 0,target_datum = 0;
				uint8_t frametype = (fixdata >> 4u) & 7u;
				uint8_t targettype = fixdata & 3u;

				if (frametype <= 3) frame_datum = rec.data.gidx();
				target_datum = rec.data.gidx();
				if ((fixdata & 4) == 0) fixup.target_displacement = fmt32 ? rec.data.gd() : rec.data.gw();

#if 0
				lenv.log.log(LNKLOG_DEBUG,"fb %04x fix %02x ft=%u tt=%u how=%u frame=%04x target=%04x disp=%08x",
					fb,fixdata,frametype,targettype,(fb >> 10u) & 0xFu,frame_datum,target_datum,fixup.target_displacement);
#endif

				if (!(fb & 0x4000)) fixup.flags |= fixup_flag_self_relative;

				fixup.apply_to = LEDATA_last_fragment;
				fixup.apply_at = LEDATA_last_offset + (fb & 0x3FFu);

				switch (targettype) {
					case 0: // SEGDEF
						{
							if (!SEGDEF.exists(from1based(target_datum))) {
								lenv.log.log(LNKLOG_ERROR,"FIXUPP: SEGDEF not found");
								return false;
							}
							fixup.target.type = fixup_t::param_t::TYPE_SEGDEF;
							fixup.target.p.segdef = SEGDEF.get(from1based(target_datum));
						}
						break;
					case 1: // GRPDEF
						{
							if (!GRPDEF.exists(from1based(target_datum))) {
								lenv.log.log(LNKLOG_ERROR,"FIXUPP: GRPDEF not found");
								return false;
							}
							fixup.target.type = fixup_t::param_t::TYPE_GRPDEF;
							fixup.target.p.grpdef = GRPDEF.get(from1based(target_datum));
						}
					break;
					case 2: // EXTDEF
						{
							if (!EXTDEF.exists(from1based(target_datum))) {
								lenv.log.log(LNKLOG_ERROR,"FIXUPP: EXTDEF not found");
								return false;
							}
							fixup.target.type = fixup_t::param_t::TYPE_SYMBOL;
							fixup.target.p.symbol = EXTDEF.get(from1based(target_datum));
						}
						break;
					default:
						lenv.log.log(LNKLOG_ERROR,"FIXUPP: Unknown targettype code %u",targettype);
						return false;
				}

				switch (frametype) {
					case 0: // SEGDEF
						{
							if (!SEGDEF.exists(from1based(frame_datum))) {
								lenv.log.log(LNKLOG_ERROR,"FIXUPP: SEGDEF not found");
								return false;
							}
							fixup.frame.type = fixup_t::param_t::TYPE_SEGDEF;
							fixup.frame.p.segdef = SEGDEF.get(from1based(frame_datum));
						}
						break;
					case 1: // GRPDEF
						{
							if (!GRPDEF.exists(from1based(frame_datum))) {
								lenv.log.log(LNKLOG_ERROR,"FIXUPP: GRPDEF not found");
								return false;
							}
							fixup.frame.type = fixup_t::param_t::TYPE_GRPDEF;
							fixup.frame.p.grpdef = GRPDEF.get(from1based(frame_datum));
						}
					break;
					case 2: // EXTDEF
						{
							if (!EXTDEF.exists(from1based(frame_datum))) {
								lenv.log.log(LNKLOG_ERROR,"FIXUPP: EXTDEF not found");
								return false;
							}
							fixup.frame.type = fixup_t::param_t::TYPE_SYMBOL;
							fixup.frame.p.symbol = EXTDEF.get(from1based(frame_datum));
						}
						break;
					case 4: // Segment index of last LEDATA/LIDATA
						{
							if (!lenv.segments.exists(fr.insegment)) {
								lenv.log.log(LNKLOG_ERROR,"FIXUPP: LEDATA fragment has no segment");
								return false;
							}
							fixup.frame.type = fixup_t::param_t::TYPE_SEGDEF;
							fixup.frame.p.segdef = fr.insegment;
						}
						break;
					case 5: // same as target
						fixup.frame = fixup.target;
						break;
					default:
						lenv.log.log(LNKLOG_ERROR,"FIXUPP: Unknown frametype code %u",frametype);
						return false;
				}

				switch ((fb >> 10u) & 0xFu) {
					case 0: fixup.how = fixup_how_offset8; break;
					case 1: fixup.how = fixup_how_offset16; break;
					case 2: fixup.how = fixup_how_segbase16; break;
					case 3: fixup.how = fixup_how_far1616; break;
					case 4: fixup.how = fixup_how_offset8hi; break;
					case 5: fixup.how = fixup_how_offset16; break;
					case 9: fixup.how = fixup_how_offset32; break;
					case 11:fixup.how = fixup_how_far1632; break;
					case 13:fixup.how = fixup_how_offset32; break;
					default:
						lenv.log.log(LNKLOG_ERROR,"FIXUPP: Unknown location code %u",(fb >> 10u) & 0xFu);
						return false;
				}
			}
			else {
				// THREAD
				// Not supported
				lenv.log.log(LNKLOG_ERROR,"FIXUPP: THREAD records not supported");
				return false;
			}
		}

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
		special_FLAT_GRPDEF = GRPDEF_list_t::undef;
		LIDATA_last_entry = false;
		LIDATA_records.clear();
		SEGDEF.clear();
		GRPDEF.clear();
		LNAMES.clear();
		EXTDEF.clear();
		PUBDEF.clear();

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
			else if (rec.type == 0x8C/*EXTDEF*/ || rec.type == 0xB4/*LEXTDEF*/ || rec.type == 0xB5/*LEXTDEF*/) {
				if (!read_rec_EXTDEF(lenv,rec)) {
					lenv.log.log(LNKLOG_ERROR,"Error reading EXTDEF");
					return false;
				}
			}
			else if (rec.type == 0x90/*PUBDEF*/ || rec.type == 0x91/*PUBDEF*/ || rec.type == 0xB6/*LPUBDEF*/ || rec.type == 0xB7/*LPUBDEF*/) {
				if (!read_rec_PUBDEF(lenv,rec)) {
					lenv.log.log(LNKLOG_ERROR,"Error reading PUBDEF");
					return false;
				}
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
			else if (rec.type == 0x9C/*FIXUPP*/ || rec.type == 0x9D/*FIXUPP32*/) {
				if (!read_rec_FIXUPP(lenv,rec)) {
					lenv.log.log(LNKLOG_ERROR,"Error reading FIXUPP");
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
			else if (rec.type == 0xB0/*COMDEF*/ || rec.type == 0xB8/*LCOMDEF*/) {
				if (!read_rec_COMDEF(lenv,rec)) {
					lenv.log.log(LNKLOG_ERROR,"Error reading COMDEF");
					return false;
				}
			}
		}

		/* All segments are segmented model, unless the FLAT GRPDEF was seen.
		 * If no fragment defined then make one with no data (STACK, BSS)
		 * because the design of this linker expects to arrange and combine
		 * segments through appending fragment references, and the symbols
		 * are expected to reference fragments as well. */
		for (auto si=SEGDEF.ref.begin();si!=SEGDEF.ref.end();si++) {
			auto &segref = lenv.segments.get(*si);

			/* if not already marked as either, decide now */
			if ((segref.flags & (SEGFLAG_FLAT|SEGFLAG_SEGMENTED)) == 0) {
				if (special_FLAT_GRPDEF != group_list_t::undef)
					segref.flags |= SEGFLAG_FLAT;
				else
					segref.flags |= SEGFLAG_SEGMENTED;
			}

			/* if no fragment defined, make one and mark as NOEMIT since the object file did not provide any data for it */
			if (segref.fragments.empty()) {
				segref.flags |= SEGFLAG_NOEMIT;

				const fragment_ref_t fref = lenv.fragments.allocate();
				segref.fragments.push_back(fref);

				auto &fr = lenv.fragments.get(fref);
				fr.org_offset = 0;
				fr.insegment = *si;
				fr.source = current_source;
				fr.fragmentsize = segref.segmentsize;
				fr.flags |= FRAGFLAG_EMPTY;
			}
		}

		/* PUBDEFs need to be matched up to the fragment they refer to, not just the segment,
		 * because fragments make the segment combining easier. At the time of the PUBDEF the
		 * LEDATA/LIDATA probably hadn't occurred yet making the match up at the time impossible,
		 * so do it here. */
		for (auto pi=PUBDEF.ref.begin();pi!=PUBDEF.ref.end();pi++) {
			auto &sym = lenv.symbols.get(*pi);
			if (sym.symboltype == symbol_type_public && sym.base_segment != segment_list_t::undef &&
				sym.base_fragment == fragment_list_t::undef && sym.symbol_offset != segment_offset_undef) {
				sym.base_fragment = segdef_offset_to_fragment(lenv,sym.base_segment,sym.symbol_offset);
				if (sym.base_fragment == fragment_list_t::undef) {
					lenv.log.log(LNKLOG_ERROR,"PUBDEF '%s' unable to match symbol offset 0x%lu to fragment",
						lenv.strings.get(sym.symbolname).c_str(),(unsigned long)sym.symbol_offset);
					return false;
				}
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
	if (!omfr.read(lenv,"/usr/src/doslib/fmt/omf/testfile/0007.obj")) fprintf(stderr,"Failed to read 0007.obj\n");
	if (!omfr.read(lenv,"/usr/src/doslib/fmt/omf/testfile/0009.obj")) fprintf(stderr,"Failed to read 0009.obj\n");
	if (!omfr.read(lenv,"/usr/src/doslib/fmt/omf/testfile/0012.obj")) fprintf(stderr,"Failed to read 0012.obj\n");
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
		if (seg.segment_moved_to != DOSLIBLinker::segment_list_t::undef) {
			fprintf(stderr,"    Moved to: [%lu]'%s'",
				(unsigned long)seg.segment_moved_to,
				lenv.strings.get(lenv.segments.get(seg.segment_moved_to).segmentname).c_str());
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
		if (grp.group_moved_to != DOSLIBLinker::group_list_t::undef) {
			fprintf(stderr,"    Moved to: [%lu]'%s'",
				(unsigned long)grp.group_moved_to,
				lenv.strings.get(lenv.groups.get(grp.group_moved_to).groupname).c_str());
			fprintf(stderr,"\n");
		}
		fprintf(stderr,"    flags:");
		if (grp.flags & DOSLIBLinker::GRPFLAG_DELETED)       fprintf(stderr," DELETED");
		fprintf(stderr,"\n");

	}

	fprintf(stderr,"Strings:\n");
	for (auto si=lenv.strings.ref.begin();si!=lenv.strings.ref.end();si++)
		fprintf(stderr,"  [%lu]: '%s'\n",(unsigned long)(si - lenv.strings.ref.begin()),(*si).c_str());

	fprintf(stderr,"String to ref:\n");
	for (auto si=lenv.strings.ref2rev.begin();si!=lenv.strings.ref2rev.end();si++) {
		fprintf(stderr,"  '%s' -> [%lu]\n",si->first.c_str(),(unsigned long)(si->second));
		if (si->first != lenv.strings.get(si->second)) {
			fprintf(stderr,"!!! ERROR: Rev lookup error: key='%s' ref='%s'\n",si->first.c_str(),lenv.strings.get(si->second).c_str());
			abort();
		}
	}

	fprintf(stderr,"String case-insensitive to ref:\n");
	for (auto si=lenv.strings.ref2revic.begin();si!=lenv.strings.ref2revic.end();si++) {
		const auto &r = si->second;

		fprintf(stderr,"  '%s' ->\n",si->first.c_str());
		for (auto ri=r.begin();ri!=r.end();ri++) {
			fprintf(stderr,"    [%lu] '%s'\n",(unsigned long)(*ri),lenv.strings.get(*ri).c_str());
			if (si->first != lenv.strings.ic(lenv.strings.get(*ri))) {
				fprintf(stderr,"!!! ERROR: Rev lookup error: key='%s' ref='%s'\n",si->first.c_str(),lenv.strings.ic(lenv.strings.get(*ri)).c_str());
				abort();
			}
		}
	}

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
			fprintf(stderr," segmentoffset=0x%lx",(unsigned long)fr.segmentoffset);
		if (fr.fragmentsize != DOSLIBLinker::segment_size_undef)
			fprintf(stderr," fragsize=0x%lx",(unsigned long)fr.fragmentsize);
		if (fr.alignment != DOSLIBLinker::alignment_undef)
			fprintf(stderr," alignmask=0x%lx(%lu bytes)",(unsigned long)fr.alignment,(unsigned long)DOSLIBLinker::alignmask2value(fr.alignment));
		if (fr.flags & DOSLIBLinker::FRAGFLAG_EMPTY)
			fprintf(stderr," EMPTY");
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

	fprintf(stderr,"Fixups:\n");
	for (auto fi=lenv.fixups.ref.begin();fi!=lenv.fixups.ref.end();fi++) {
		const auto &fixup = *fi;

		fprintf(stderr,"  [%lu] ",
			(unsigned long)(fi - lenv.fixups.ref.begin()));
		fprintf(stderr,"frag=%lu at=%lx ",
			(unsigned long)fixup.apply_to,
			(unsigned long)fixup.apply_at);

		const auto &fr = lenv.fragments.get(fixup.apply_to);

		fprintf(stderr,"segment[%lu]'%s' ",
			(unsigned long)(fr.insegment),
			(fr.insegment != DOSLIBLinker::segment_list_t::undef) ? lenv.strings.get(lenv.segments.get(fr.insegment).segmentname).c_str() : "");

		if (fixup.target_displacement != 0)
			fprintf(stderr,"tgdisp=%lx ",(unsigned long)fixup.target_displacement);
		if (fixup.flags & DOSLIBLinker::fixup_flag_self_relative)
			fprintf(stderr,"selfrel ");
		fprintf(stderr,"\n");
		fprintf(stderr,"    ");
		switch (fixup.how) {
			case DOSLIBLinker::fixup_how_offset8:
				fprintf(stderr,"OFFSET8 ");
				break;
			case DOSLIBLinker::fixup_how_offset8hi:
				fprintf(stderr,"OFFSET8HI ");
				break;
			case DOSLIBLinker::fixup_how_offset16:
				fprintf(stderr,"OFFSET16 ");
				break;
			case DOSLIBLinker::fixup_how_segbase16:
				fprintf(stderr,"SEGBASE16 ");
				break;
			case DOSLIBLinker::fixup_how_far1616:
				fprintf(stderr,"FAR1616 ");
				break;
			case DOSLIBLinker::fixup_how_offset32:
				fprintf(stderr,"OFFSET32 ");
				break;
			case DOSLIBLinker::fixup_how_far1632:
				fprintf(stderr,"FAR1632 ");
				break;
			default:
				fprintf(stderr,"?? ");
				break;
		}
		fprintf(stderr,"frame=");
		switch (fixup.frame.type) {
			case DOSLIBLinker::fixup_t::param_t::TYPE_NONE:
				fprintf(stderr,"none ");
				break;
			case DOSLIBLinker::fixup_t::param_t::TYPE_SEGDEF:
				fprintf(stderr,"segdef:");
				fprintf(stderr,"segment[%lu]'%s' ",
					(unsigned long)(fixup.frame.p.segdef),
					(fixup.frame.p.segdef != DOSLIBLinker::segment_list_t::undef) ? lenv.strings.get(lenv.segments.get(fixup.frame.p.segdef).segmentname).c_str() : "");
				break;
			case DOSLIBLinker::fixup_t::param_t::TYPE_GRPDEF:
				fprintf(stderr,"grpdef:");
				fprintf(stderr,"group[%lu]'%s' ",
					(unsigned long)(fixup.frame.p.grpdef),
					(fixup.frame.p.grpdef != DOSLIBLinker::group_list_t::undef) ? lenv.strings.get(lenv.groups.get(fixup.frame.p.grpdef).groupname).c_str() : "");
				break;
			case DOSLIBLinker::fixup_t::param_t::TYPE_SYMBOL:
				fprintf(stderr,"symbol:");
				fprintf(stderr,"sym[%lu]'%s' ",
					(unsigned long)(fixup.frame.p.symbol),
					(fixup.frame.p.symbol != DOSLIBLinker::symbol_list_t::undef) ? lenv.strings.get(lenv.symbols.get(fixup.frame.p.symbol).symbolname).c_str() : "");
				break;
			case DOSLIBLinker::fixup_t::param_t::TYPE_SEGMENTFRAME:
				fprintf(stderr,"segframe:%04lx",(unsigned long)fixup.frame.p.segmentframe);
				break;
			default:
				fprintf(stderr,"?? ");
				break;
		}
		fprintf(stderr,"target=");
		switch (fixup.target.type) {
			case DOSLIBLinker::fixup_t::param_t::TYPE_NONE:
				fprintf(stderr,"none ");
				break;
			case DOSLIBLinker::fixup_t::param_t::TYPE_SEGDEF:
				fprintf(stderr,"segdef:");
				fprintf(stderr,"segment[%lu]'%s' ",
					(unsigned long)(fixup.target.p.segdef),
					(fixup.target.p.segdef != DOSLIBLinker::segment_list_t::undef) ? lenv.strings.get(lenv.segments.get(fixup.target.p.segdef).segmentname).c_str() : "");
				break;
			case DOSLIBLinker::fixup_t::param_t::TYPE_GRPDEF:
				fprintf(stderr,"grpdef:");
				fprintf(stderr,"group[%lu]'%s' ",
					(unsigned long)(fixup.target.p.grpdef),
					(fixup.target.p.grpdef != DOSLIBLinker::group_list_t::undef) ? lenv.strings.get(lenv.groups.get(fixup.target.p.grpdef).groupname).c_str() : "");
				break;
			case DOSLIBLinker::fixup_t::param_t::TYPE_SYMBOL:
				fprintf(stderr,"symbol:");
				fprintf(stderr,"sym[%lu]'%s' ",
					(unsigned long)(fixup.target.p.symbol),
					(fixup.target.p.symbol != DOSLIBLinker::symbol_list_t::undef) ? lenv.strings.get(lenv.symbols.get(fixup.target.p.symbol).symbolname).c_str() : "");
				break;
			case DOSLIBLinker::fixup_t::param_t::TYPE_SEGMENTFRAME:
				fprintf(stderr,"segframe:%04lx",(unsigned long)fixup.target.p.segmentframe);
				break;
			default:
				fprintf(stderr,"?? ");
				break;
		}
		fprintf(stderr,"\n");

		if (fr.source != DOSLIBLinker::source_list_t::undef) {
			const auto &src = lenv.sources.get(fr.source);
			fprintf(stderr,"    source: path='%s' name='%s' index=%ld offset=%ld\n",
				src.path.c_str(),src.name.c_str(),(signed long)src.index,(signed long)src.file_offset);
		}
	}

	fprintf(stderr,"Symbols:\n");
	for (auto si=lenv.symbols.ref.begin();si!=lenv.symbols.ref.end();si++) {
		const auto &sym = *si;

		fprintf(stderr,"  [%lu] name='%s'",
			(unsigned long)(si - lenv.symbols.ref.begin()),
			(sym.symbolname != DOSLIBLinker::symbol_list_t::undef) ? lenv.strings.get(sym.symbolname).c_str() : "(undef)");
		if (sym.flags & DOSLIBLinker::SYMFLAG_DELETED) fprintf(stderr," DELETED");
		if (sym.flags & DOSLIBLinker::SYMFLAG_LOCAL) fprintf(stderr," LOCAL");
		if (sym.flags & DOSLIBLinker::SYMFLAG_NEAR) fprintf(stderr," NEAR");
		if (sym.flags & DOSLIBLinker::SYMFLAG_FAR) fprintf(stderr," FAR");
		fprintf(stderr,"\n");

		if (sym.symbol_moved_to != DOSLIBLinker::symbol_list_t::undef) {
			const auto &msym = lenv.symbols.get(sym.symbol_moved_to);
			fprintf(stderr,"  moved to [%lu] '%s'\n",
				(unsigned long)(sym.symbol_moved_to),
				(msym.symbolname != DOSLIBLinker::symbol_list_t::undef) ? lenv.strings.get(msym.symbolname).c_str() : "(undef)");
		}

		if (sym.source != DOSLIBLinker::source_list_t::undef) {
			const auto &src = lenv.sources.get(sym.source);
			fprintf(stderr,"  source: path='%s' name='%s' index=%ld offset=%ld\n",
				src.path.c_str(),src.name.c_str(),(signed long)src.index,(signed long)src.file_offset);
		}

		if (sym.symboltype != DOSLIBLinker::symbol_type_undef) {
			fprintf(stderr,"  symbol type 0x%x",(unsigned int)sym.symboltype);
			switch (sym.symboltype) {
				case DOSLIBLinker::symbol_type_extern: fprintf(stderr," EXTERN"); break;
				case DOSLIBLinker::symbol_type_common: fprintf(stderr," COMMON"); break;
				case DOSLIBLinker::symbol_type_public: fprintf(stderr," PUBLIC"); break;
				default: break;
			}
			if (sym.flags & DOSLIBLinker::SYMFLAG_LOCAL) fprintf(stderr," LOCAL");
			fprintf(stderr,"\n");
		}

		if (sym.symbolsize != DOSLIBLinker::segment_size_undef)
			fprintf(stderr,"  symbol size: %lu\n",(unsigned long)sym.symbolsize);

		if (sym.symbol_offset != DOSLIBLinker::segment_offset_undef)
			fprintf(stderr,"  symbol offset: 0x%lx\n",(unsigned long)sym.symbol_offset);

		if (sym.base_group != DOSLIBLinker::group_list_t::undef) {
			const auto &grp = lenv.groups.get(sym.base_group);
			fprintf(stderr,"  base group: [%lu] '%s'\n",(unsigned long)sym.base_group,lenv.strings.get(grp.groupname).c_str());
		}

		if (sym.base_fragment != DOSLIBLinker::fragment_list_t::undef)
			fprintf(stderr,"  base fragment: [%lu]\n",(unsigned long)sym.base_fragment);

		if (sym.base_segment != DOSLIBLinker::segment_list_t::undef) {
			const auto &src = lenv.segments.get(sym.base_segment);
			fprintf(stderr,"  base segment: [%lu] '%s'\n",(unsigned long)sym.base_segment,lenv.strings.get(src.segmentname).c_str());
		}

		if (sym.base_frame_number != DOSLIBLinker::segment_frame_undef)
			fprintf(stderr,"  base frame number: 0x%lx\n",(unsigned long)sym.base_frame_number);
	}

	return 0;
}

