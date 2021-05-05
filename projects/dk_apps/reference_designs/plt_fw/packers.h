/*
 * packers.h
 *
 *  Created on: Sep 14, 2015
 *      Author: akostop
 */

#ifndef PACKERS_H_
#define PACKERS_H_

static inline uint8_t r8le(uint8_t *p)
{
	return (uint8_t) p[0];
}

static inline uint16_t r16le(uint8_t *p)
{
	return (uint16_t) (p[0] | (p[1] << 8));
}

static inline void w8le(uint8_t *p, uint8_t v)
{
	p[0] = v;
}

static inline void w16le(uint8_t *p, uint16_t v )
{
	p[0] = v & 0xff;
	p[1] = (v >> 8) & 0xff;
}

#define padvN(p, N) \
	p += (N)

#define padv(ptr, type) \
	padvN(ptr, sizeof(type))


#endif /* PACKERS_H_ */
