/**
 ****************************************************************************************
 *
 * @file libsodium_config.h
 *
 * @brief libsodium configuration flags.
 *
 * Copyright (C) 2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef LIBSODIUM_CONFIG_H_
#define LIBSODIUM_CONFIG_H_

#if  __MINGW32__

#define PACKAGE_NAME "libsodium"
#define PACKAGE_TARNAME "libsodium"
#define PACKAGE_VERSION "1.0.13"
#define PACKAGE_STRING "libsodium\\1.0.13"
#define PACKAGE_BUGREPORT "https://github.com/jedisct1/libsodium/issues"
#define PACKAGE_URL "https://github.com/jedisct1/libsodium"
#define PACKAGE "libsodium"
#define VERSION "1.0.13"
#define HAVE_PTHREAD_PRIO_INHERIT 1
#define HAVE_PTHREAD 1
#define STDC_HEADERS 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_MEMORY_H 1
#define HAVE_STRINGS_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_UNISTD_H 1
#define __EXTENSIONS__ 1
#define _ALL_SOURCE 1
#define _GNU_SOURCE 1
#define _POSIX_PTHREAD_SEMANTICS 1

#define _TANDEM_SOURCE 1

#define LT_OBJDIR ".libs/"
#define HAVE_MMINTRIN_H 1
#define HAVE_EMMINTRIN_H 1
#define HAVE_PMMINTRIN_H 1
#define HAVE_TMMINTRIN_H 1
#define HAVE_SMMINTRIN_H 1
#define HAVE_AVXINTRIN_H 1
#define HAVE_AVX2INTRIN_H 1
#define HAVE_WMMINTRIN_H 1
#define NATIVE_LITTLE_ENDIAN 1
#define HAVE_CPUID 1
#define CPU_UNALIGNED_ACCESS 1
#define HAVE_ATOMIC_OPS 1
#define HAVE_MPROTECT 1
#define HAVE_NANOSLEEP 1
#define HAVE_GETPID 1
#define CONFIGURED 1
#define HAVE_INTRIN_H 1

#define _FORTIFY_SOURCE 2

#else

#define PACKAGE_NAME "libsodium"
#define PACKAGE_TARNAME "libsodium"
#define PACKAGE_VERSION "1.0.13"
#define PACKAGE_STRING "libsodium\\1.0.13"
#define PACKAGE_BUGREPORT "https://github.com/jedisct1/libsodium/issues"
#define PACKAGE_URL "https://github.com/jedisct1/libsodium"
#define PACKAGE "libsodium"
#define VERSION "1.0.13"
#define HAVE_PTHREAD_PRIO_INHERIT 1
#define HAVE_PTHREAD 1
#define STDC_HEADERS 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_MEMORY_H 1
#define HAVE_STRINGS_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_UNISTD_H 1
#define __EXTENSIONS__ 1
#define _ALL_SOURCE 1
#define _GNU_SOURCE 1
#define _POSIX_PTHREAD_SEMANTICS 1
#define _TANDEM_SOURCE 1
#define HAVE_CATCHABLE_SEGV 1
#define HAVE_DLFCN_H 1
#define LT_OBJDIR ".libs/"
#define HAVE_MMINTRIN_H 1
#define HAVE_EMMINTRIN_H 1
#define HAVE_PMMINTRIN_H 1
#define HAVE_TMMINTRIN_H 1
#define HAVE_SMMINTRIN_H 1
#define HAVE_AVXINTRIN_H 1
#define HAVE_AVX2INTRIN_H 1
#define HAVE_WMMINTRIN_H 1
#define HAVE_SYS_MMAN_H 1
#define NATIVE_LITTLE_ENDIAN 1
#define HAVE_AMD64_ASM 1
#define HAVE_AVX_ASM 1
#define HAVE_TI_MODE 1
#define HAVE_CPUID 1
#define ASM_HIDE_SYMBOL .hidden
#define HAVE_WEAK_SYMBOLS 1
#define CPU_UNALIGNED_ACCESS 1
#define HAVE_ATOMIC_OPS 1
#define HAVE_MMAP 1
#define HAVE_MLOCK 1
#define HAVE_MADVISE 1
#define HAVE_MPROTECT 1
#define HAVE_NANOSLEEP 1
#define HAVE_POSIX_MEMALIGN 1
#define HAVE_GETPID 1
#define CONFIGURED 1

#endif

#endif /* LIBSODIUM_CONFIG_H_ */

