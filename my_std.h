#ifndef D_MY_STD_H
#define D_MY_STD_H

#include <stdio.h>
#include <Windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <windows.h>
#include <usp10.h>

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef int8_t    s8;
typedef int16_t   s16;
typedef int32_t   s32;
typedef int64_t   s64;
typedef uint8_t   b8;
typedef uint16_t  b16;
typedef uint32_t  b32;
typedef uint64_t  b64;


#define arr_count(arr) (sizeof(arr) / sizeof(arr[0]))

int strlen(const wchar_t *string){
  int len = 0;
  while(*string++)len++;
  return len;
}

#endif