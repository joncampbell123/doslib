
#ifndef __ISP_UTILS_MISC_RAWINT_H
#define __ISP_UTILS_MISC_RAWINT_H

#include <stdint.h>

static inline uint16_t __le_u16(const void *p) {
	return *((uint16_t*)(p));
}

static inline int16_t __le_s16(const void *p) {
	return *((int16_t*)(p));
}

static inline void __w_le_u16(const void *p,const uint16_t val) {
	*((uint16_t*)(p)) = val;
}

static inline uint32_t __le_u24(const void *p) {
	return *((uint32_t*)(p)) & 0xFFFFFF;
}

static inline int32_t __le_s24(const void *p) {
	return ((int32_t)(*((uint32_t*)(p)) << 8)) >> 8; /* NTS: unsigned shift to move low 24 bits over, then signed shift to stretch it down */
}

static inline uint32_t __le_u32(const void *p) {
	return *((uint32_t*)(p));
}

static inline int32_t __le_s32(const void *p) {
	return *((int32_t*)(p));
}

static inline void __w_le_u32(const void *p,const uint32_t val) {
	*((uint32_t*)(p)) = val;
}

static inline uint64_t __le_u64(const void *p) {
	return *((uint64_t*)(p));
}

static inline void __w_le_u64(const void *p,const uint64_t val) {
	*((uint64_t*)(p)) = val;
}

static inline uint16_t __be_u16(void *p) {
	const unsigned char *c = (const unsigned char *)p;
	return	(((uint16_t)c[0]) <<  8U) |
		(((uint16_t)c[1]) <<  0U);
}

static inline int16_t __be_s16(void *p) {
	return (int16_t)__be_u16(p);
}

static inline uint32_t __be_u24(const void *p) {
	const unsigned char *c = (const unsigned char *)p;
	return	(((uint32_t)c[0]) << 16U) |
		(((uint32_t)c[1]) <<  8U) |
		(((uint32_t)c[2]) <<  0U);
}

static inline int32_t __be_s24(const void *p) {
	return ((int32_t)(__be_u24(p) << 8)) >> 8; /* NTS: unsigned shift to move low 24 bits over, then signed shift to stretch it down */
}

static inline uint32_t __be_u32(void *p) {
	const unsigned char *c = (const unsigned char *)p;
	return	(((uint32_t)c[0]) << 24U) |
		(((uint32_t)c[1]) << 16U) |
		(((uint32_t)c[2]) <<  8U) |
		(((uint32_t)c[3]) <<  0U);
}

static inline int32_t __be_s32(void *p) {
	return (int32_t)__be_u32(p);
}

static inline uint64_t __be_u64(void *p) {
	const unsigned char *c = (const unsigned char *)p;
	return	(((uint64_t)c[0]) << 56ULL) |
		(((uint64_t)c[1]) << 48ULL) |
		(((uint64_t)c[2]) << 40ULL) |
		(((uint64_t)c[3]) << 32ULL) |
		(((uint64_t)c[4]) << 24ULL) |
		(((uint64_t)c[5]) << 16ULL) |
		(((uint64_t)c[6]) <<  8ULL) |
		(((uint64_t)c[7]) <<  0ULL);
}

#if defined(__cplusplus)
extern "C" {
#endif

double ConvertFromBeIeeeExtended(unsigned char *bytes);

#if defined(__cplusplus)
}
#endif

#endif /* __ISP_UTILS_MISC_RAWINT_H */

