/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Define if building universal (internal helper macro) */
#define AC_APPLE_UNIVERSAL_BUILD 1

#if defined(__BIG_ENDIAN__)
 #define CPU_IS_BIG_ENDIAN 1
 #define CPU_IS_LITTLE_ENDIAN 0
#else
 #define CPU_IS_LITTLE_ENDIAN 1
 #define CPU_IS_BIG_ENDIAN 0
#endif

/* Target processor is big endian. */
/* #undef CPU_IS_BIG_ENDIAN */

/* Target processor is little endian. */
/* #undef CPU_IS_LITTLE_ENDIAN */

/* Set FLAC__BYTES_PER_WORD to 8 (4 is the default) */
/* #undef ENABLE_64_BIT_WORDS */

/* define to align allocated memory on 32-byte boundaries */
/* #undef FLAC__ALIGN_MALLOC_DATA */

#if defined(__aarch64__)
 #define FLAC__CPU_ARM64 1
 #define FLAC__HAS_NEONINTRIN 1
 #define FLAC__HAS_A64NEONINTRIN 1
 #define ENABLE_64_BIT_WORDS 1
 #define HAVE_ARM_NEON_H 1
 #define HAVE_BSWAP16 1
 #define HAVE_BSWAP32 1
 #define SIZEOF_OFF_T 8
 #define SIZEOF_VOIDP 8
#elif defined(__x86_64__)
 #define FLAC__CPU_X86_64 1
 #define FLAC__HAS_X86INTRIN 1
 #define WITH_AVX 1
 #define ENABLE_64_BIT_WORDS 1
 #define HAVE_BSWAP16 1
 #define HAVE_BSWAP32 1
 #define SIZEOF_OFF_T 8
 #define SIZEOF_VOIDP 8
#elif defined(__i386__)
 #define FLAC__CPU_IA32 1
 #define FLAC__HAS_X86INTRIN 1
 #define WITH_AVX 1
 #define HAVE_BSWAP16 1
 #define HAVE_BSWAP32 1
 #define SIZEOF_OFF_T 4
 #define SIZEOF_VOIDP 4
// #define FLAC__HAS_NASM 1
#endif

/* define if building for ia32/i386 */
/* #undef FLAC__CPU_IA32 */

/* define if building for PowerPC */
/* #undef FLAC__CPU_PPC */

/* define if building for PowerPC64 */
/* #undef FLAC__CPU_PPC64 */

/* define if building for SPARC */
/* #undef FLAC__CPU_SPARC */

/* define if building for x86_64 */
/* #undef FLAC__CPU_X86_64 */

/* define if building for arm64 */
/* #undef FLAC__CPU_ARM64 */

/* define if you have docbook-to-man or docbook2man */
/* #undef FLAC__HAS_DOCBOOK_TO_MAN */

/* define if you are compiling for x86 and have the NASM assembler */
/* #undef FLAC__HAS_NASM */

/* define if you have the ogg library */
#define OGG_FOUND 0
#define FLAC__HAS_OGG OGG_FOUND

/* define if you have pandoc */
/* #undef FLAC__HAS_PANDOC */

/* define if compiler has __attribute__((target("cpu=power8"))) support */
/* #undef FLAC__HAS_TARGET_POWER8 */

/* define if compiler has __attribute__((target("cpu=power9"))) support */
/* #undef FLAC__HAS_TARGET_POWER9 */

/* Set to 1 if <x86intrin.h> is available. */
/* #undef FLAC__HAS_X86INTRIN */

/* Set to 1 if <arm_neon.h> is available. */
/* #undef FLAC__HAS_NEONINTRIN */

/* Set to 1 if <arm_neon.h> contains A64 intrinsics */
/* #undef FLAC__HAS_A64NEONINTRIN */

/* define to disable use of assembly code */
/* #undef FLAC__NO_ASM */

/* define if building for Darwin / MacOS X */
#define FLAC__SYS_DARWIN 1

/* define if building for Linux */
/* #undef FLAC__SYS_LINUX */

/* define to enable use of Altivec instructions */
/* #undef FLAC__USE_ALTIVEC */

/* define to enable use of AVX instructions */
/* #undef WITH_AVX */
#ifdef WITH_AVX
  #define FLAC__USE_AVX
#endif

/* define to enable use of VSX instructions */
/* #undef FLAC__USE_VSX */

/* "Define to the commit date of the current git HEAD" */
/* #undef GIT_COMMIT_DATE */

/* "Define to the short hash of the current git HEAD" */
/* #undef GIT_COMMIT_HASH */

/* "Define to the tag of the current git HEAD" */
/* #undef GIT_COMMIT_TAG */

/* Compiler has the __builtin_bswap16 intrinsic */
/* #undef HAVE_BSWAP16 */

/* Compiler has the __builtin_bswap32 intrinsic */
/* #undef HAVE_BSWAP32 */

/* Define to 1 if you have the <byteswap.h> header file. */
/* #undef HAVE_BYTESWAP_H */

/* define if you have clock_gettime */
/* #undef HAVE_CLOCK_GETTIME */

/* Define to 1 if you have the <cpuid.h> header file. */
/* #undef HAVE_CPUID_H */

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if fseeko (and presumably ftello) exists and is declared. */
#define HAVE_FSEEKO 1

/* Define to 1 if you have the `getauxval' function. */
/* #undef HAVE_GETAUXVAL */

/* Define to 1 if you have the `getopt_long' function. */
#define HAVE_GETOPT_LONG 1

/* Define if you have the iconv() function and it works. */
#define HAVE_ICONV 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define if you have <langinfo.h> and nl_langinfo(CODESET). */
#define HAVE_LANGINFO_CODESET 1

/* lround support */
#define HAVE_LROUND 1

/* Define to 1 if you have the <minix/config.h> header file. */
/* #undef HAVE_MINIX_CONFIG_H */

/* Define to 1 if the system has the type `socklen_t'. */
/* #undef HAVE_SOCKLEN_T */

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/auxv.h> header file. */
/* #undef HAVE_SYS_AUXV_H */

/* Define to 1 if you have the <sys/ioctl.h> header file. */
#define HAVE_SYS_IOCTL_H 1

/* Define to 1 if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <termios.h> header file. */
#define HAVE_TERMIOS_H 1

/* Define to 1 if typeof works with your compiler. */
#define HAVE_TYPEOF 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the <wchar.h> header file. */
#define HAVE_WCHAR_H 1

/* Define to 1 if you have the <x86intrin.h> header file. */
/* #undef HAVE_X86INTRIN_H */

/* Define as const if the declaration of iconv() needs const. */
#define ICONV_CONST 

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Define if debugging is disabled */
/* #undef NDEBUG */

/* Name of package */
#define PACKAGE "flac"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "flac-dev@xiph.org"

/* Define to the full name of this package. */
#define PACKAGE_NAME "flac"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "flac 1.4.2"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "flac"

/* Define to the home page for this package. */
#define PACKAGE_URL "https://www.xiph.org/flac/"

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.4.2"

/* The size of `off_t', as computed by sizeof. */
/* #undef SIZEOF_OFF_T */

/* The size of `void*', as computed by sizeof. */
/* #undef SIZEOF_VOIDP */

/* Define to 1 if all of the C90 standard headers exist (not just the ones
   required in a freestanding environment). This macro is provided for
   backward compatibility; new code need not use it. */
#define STDC_HEADERS 1

/* Enable extensions on AIX 3, Interix.  */
#ifndef _ALL_SOURCE
# define _ALL_SOURCE 1
#endif
/* Enable general extensions on macOS.  */
#ifndef _DARWIN_C_SOURCE
# define _DARWIN_C_SOURCE 1
#endif
/* Enable general extensions on Solaris.  */
#ifndef __EXTENSIONS__
# define __EXTENSIONS__ 1
#endif
/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif
/* Enable threading extensions on Solaris.  */
#ifndef _POSIX_PTHREAD_SEMANTICS
# define _POSIX_PTHREAD_SEMANTICS 1
#endif
/* Enable extensions on HP NonStop.  */
#ifndef _TANDEM_SOURCE
# define _TANDEM_SOURCE 1
#endif
/* Enable general extensions on Solaris.  */
#ifndef __EXTENSIONS__
# define __EXTENSIONS__ 1
#endif


/* Version number of package */
#define VERSION "1.4.2"

/* Target processor is big endian. */
#define WORDS_BIGENDIAN CPU_IS_BIG_ENDIAN

/* Enable large inode numbers on Mac OS X 10.5.  */
#ifndef _DARWIN_USE_64_BIT_INODE
# define _DARWIN_USE_64_BIT_INODE 1
#endif

/* Number of bits in a file offset, on hosts where this is settable. */
#ifndef _FILE_OFFSET_BITS
# define _FILE_OFFSET_BITS 64
#endif

/* Define to 1 to make fseeko visible on some hosts (e.g. glibc 2.2). */
#ifndef _LARGEFILE_SOURCE
# define _LARGEFILE_SOURCE 1
#endif

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */

/* Define to 1 if on MINIX. */
/* #undef _MINIX */

/* Define to 2 if the system does not provide POSIX.1 features except with
   this defined. */
/* #undef _POSIX_1_SOURCE */

/* Define to 1 if you need to in order for `stat' and other things to work. */
/* #undef _POSIX_SOURCE */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif

/* Define to __typeof__ if your compiler spells it that way. */
/* #undef typeof */
