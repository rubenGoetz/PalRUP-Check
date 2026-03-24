
#pragma once

// UTILS

#define LIKELY(cond) __builtin_expect(cond, 1)
#define UNLIKELY(cond) __builtin_expect(cond, 0)

#define MIN(X,Y) (X < Y ? X : Y)
#define MAX(X,Y) (X > Y ? X : Y)

typedef unsigned long u64;
typedef unsigned int u32;
typedef unsigned char u8;

#define SIG_SIZE_BYTES 16
typedef u8 signature[SIG_SIZE_BYTES];

#if /* glibc >= 2.19: */ _DEFAULT_SOURCE || /* glibc <= 2.19: */ _SVID_SOURCE || _BSD_SOURCE
#define UNLOCKED_IO(fun) fun##_unlocked
#else
#define UNLOCKED_IO(fun) fun
#endif

// PalRUP constants
#define TRUSTED_CHK_CLS_PRODUCE 'a'
#define TRUSTED_CHK_CLS_IMPORT 'i'
#define TRUSTED_CHK_CLS_DELETE 'd'

// Checker constants
#define EMPTY_ID (u64)-1
