/***************************************************************************//**
  @file     strutil.c
  @brief    funciones como string.h necesarias en App.c
  @author   grupo 2
 ******************************************************************************/

#include "strutil.h"

void *memCopy (void *dst, const void *src, size_t n)
{
	unsigned char *d = dst;
	const unsigned char *s = src;
	while (n--) {
		*d++ = *s++;
	}
	return dst;
}

void *memFill (void *dst, unsigned char value, size_t n)
{
	unsigned char *d = dst;
	while (n--) {
		*d++ = value;
	}
	return dst;
}

int memCompare (const void *a, const void *b, size_t n)
{
	const unsigned char *pa = a;
	const unsigned char *pb = b;
	while (n--) {
		if (*pa != *pb) {
			return (int)*pa - (int)*pb;
		}
		pa++;
		pb++;
	}
	return 0;
}

int strCompare (const char *a, const char *b)
{
	while (*a && (*a == *b)) {
		a++;
		b++;
	}
	return (unsigned char)*a - (unsigned char)*b;
}

size_t strLength (const char *s)
{
	size_t n = 0;
	while (s[n]) {
		n++;
	}
	return n;
}
