#ifndef RECRAFT_BUILD_PLATFORM_H
#define RECRAFT_BUILD_PLATFORM_H
#if defined(_WIN32)
#define RECRAFT_BUILD_OS "WINDOWS"
#elif defined(__APPLE__)
#define RECRAFT_BUILD_OS "MACOSX"
#elif defined(__linux__)
#define RECRAFT_BUILD_OS "LINUX"
#else
#define RECRAFT_BUILD_OS "UNKNOWN"
#endif
#if defined(__aarch64__) || defined(_M_ARM64)
#define RECRAFT_BUILD_ARCH "ARM64"
#elif defined(__x86_64__) || defined(_M_X64)
#define RECRAFT_BUILD_ARCH "X86-64"
#elif defined(__i386__) || defined(_M_IX86)
#define RECRAFT_BUILD_ARCH "X86"
#elif defined(__ARM_ARCH) && __ARM_ARCH >= 7
#define RECRAFT_BUILD_ARCH "ARMv7"
#elif defined(__arm__)
#define RECRAFT_BUILD_ARCH "ARM"
#else
#define RECRAFT_BUILD_ARCH "UNKNOWN"
#endif
#define RECRAFT_BUILD_PLATFORM RECRAFT_BUILD_OS "_" RECRAFT_BUILD_ARCH
#endif
