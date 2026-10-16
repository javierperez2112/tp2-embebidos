/***************************************************************************//**
  @file     strutil.h
  @brief    funciones como string.h necesarias en App.c
  @author   grupo 2
 ******************************************************************************/

#ifndef STRUTIL_H_
#define STRUTIL_H_

#include <stddef.h>

void  *memCopy (void *dst, const void *src, size_t n);
void  *memFill (void *dst, unsigned char value, size_t n);
int    memCompare (const void *a, const void *b, size_t n);
int    strCompare (const char *a, const char *b);
size_t strLength (const char *s);

#endif /* STRUTIL_H_ */
