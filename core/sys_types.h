/*
===================================================================================================

	Basic types

===================================================================================================
*/

#pragma once

#include <cstddef>
#include <cstdint>

using int8 = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;

using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

using byte = uint8;
using uint = unsigned int;

using qboolean = int32;		// obsolete

// Platform specific character types
// minimise usage, only for code that touches platform APIs!

#ifdef _WIN32

// UTF-16 on Windows

using pchar_t = wchar_t;

#define PLATTEXT_( quote ) L##quote
#define PLATTEXT( quote ) PLATTEXT_( quote )

#else

// UTF-8 on Linux

using pchar_t = char;

#define PLATTEXT( quote ) ( quote )

#endif
