/*
===================================================================================================

	C string manipulation tools

===================================================================================================
*/

#pragma once

#include <cstdarg>
#include <cstring>

#include "sys_types.h"

// ASCII only tolower/toupper
#define Q_tolower_fast(c) ( ( ( (c) >= 'A' ) && ( (c) <= 'Z' ) ) ? ( (c) + 32 ) : (c) )
#define Q_toupper_fast(c) ( ( ( (c) >= 'a' ) && ( (c) <= 'z' ) ) ? ( (c) - 32 ) : (c) )

/*
===================================================================================================

	Standard library string function replacements

===================================================================================================
*/

using strlen_t = uint32;

#define Q_strlen(a) static_cast<strlen_t>(strlen(a))

/*
=======================================
	String modification
=======================================
*/

void Q_strcpy_s( _Post_z_ char *pDest, strlen_t nDestSize, const char *pSrc );

template< strlen_t nDestSize >
inline void Q_strcpy_s( _Post_z_ char( &pDest )[nDestSize], const char *pSrc )
{
	Q_strcpy_s( pDest, nDestSize, pSrc );
}

int Q_vsprintf_s( _Post_z_ char *pDest, strlen_t nDestSize, _Printf_format_string_ const char *pFmt, va_list args );

template< strlen_t nDestSize >
inline int Q_vsprintf_s( _Post_z_ char( &pDest )[nDestSize], _Printf_format_string_ const char *pFmt, va_list args )
{
	return Q_vsprintf_s( pDest, nDestSize, pFmt, args );
}

inline int Q_sprintf_s( _Post_z_ char *pDest, strlen_t nDestSize, _Printf_format_string_ const char *pFmt, ... )
{
	int result;
	va_list args;

	va_start( args, pFmt );
	result = Q_vsprintf_s( pDest, nDestSize, pFmt, args );
	va_end( args );

	return result;
}

template< strlen_t nDestSize >
inline int Q_sprintf_s( _Post_z_ char( &pDest )[nDestSize], _Printf_format_string_ const char *pFmt, ... )
{
	int result;
	va_list args;

	va_start( args, pFmt );
	result = Q_vsprintf_s( pDest, nDestSize, pFmt, args );
	va_end( args );

	return result;
}

int Q_vsprintf( _Post_z_ char *pDest, _Printf_format_string_ const char *pFmt, va_list args );

inline int Q_sprintf( _Post_z_ char *pDest, _Printf_format_string_ const char *pFmt, ... )
{
	int result;
	va_list args;

	va_start( args, pFmt );
	result = Q_vsprintf( pDest, pFmt, args );
	va_end( args );
	
	return result;
}

/*
=======================================
	String comparisons
=======================================
*/

// cmp

#if 0

// Probably not worth it...

inline int Q_strcmp( const char *s1, const char *s2 ) {
	return s1 == s2 ? 0 : strcmp( s1, s2 );
}

inline int Q_strncmp( const char *s1, const char *s2, strlen_t maxcount ) {
	return s1 == s2 ? 0 : strncmp( s1, s2, maxcount );
}

#else

#define Q_strcmp strcmp
#define Q_strncmp strncmp

#endif

int Q_strcasecmp( const char *s1, const char *s2 );
int Q_strncasecmp( const char *s1, const char *s2, strlen_t n );

#define Q_stricmp Q_strcasecmp
#define Q_strnicmp Q_strncasecmp

// strstr

#define Q_strstr strstr

const char *Q_stristr( const char *str, const char *substr );

inline char *Q_stristr( char *str, const char *substr ) {
	return const_cast<char *>( Q_stristr( str, substr ) );
}

/*
=======================================
	String to number
=======================================
*/

int64	Q_atoi64( const char *str );
uint64	Q_atoui64( const char *str );

int32	Q_atoi32( const char *str );
uint32	Q_atoui32( const char *str );

double	Q_atod( const char *str );
float	Q_atof( const char *str );

#define Q_atoi Q_atoi32
#define Q_atoui Q_atoui32

/*
=======================================
	String to upper / lower case
=======================================
*/

inline constexpr int Q_tolower( int ch ) {
	return ( ch <= 'Z' && ch >= 'A' ) ? ( ch + ( 'a' - 'A' ) ) : ch;
}

inline constexpr int Q_toupper( int ch ) {
	return ( ch >= 'a' && ch <= 'z' ) ? ( ch - ( 'a' - 'A' ) ) : ch;
}

inline void Q_strlwr( char *dest ) {
	while ( *dest )
	{
		*dest = Q_tolower_fast( *dest );
		++dest;
	}
}

inline void Q_strupr( char *dest ) {
	while ( *dest )
	{
		*dest = Q_toupper_fast( *dest );
		++dest;
	}
}

/*
===================================================================================================

	Our string functions

===================================================================================================
*/

//
// Hashing
//

inline uint32 HashString( const char* s )
{
	uint32 h = 2166136261u;
	for ( ; *s; ++s )
	{
		uint32 c = (unsigned char)*s;
		h = (h ^ c) * 16777619;
	}
	return (h ^ (h << 17)) + (h >> 21);
}

inline uint32 HashStringInsensitive( const char* s )
{
	uint32 h = 2166136261u;
	for ( ; *s; ++s )
	{
		uint32 c = (unsigned char)*s;
		c += (((('A'-1) - c) & (c - ('Z'+1))) >> 26) & 32;
		h = (h ^ c) * 16777619;
	}
	return (h ^ (h << 17)) + (h >> 21);
}

//
// Consteval hashing
//

template< strlen_t cnt >
inline consteval uint32 ConstHashString( const char( &s )[cnt] )
{
	uint32 h = 2166136261u;
	for ( uint32 i = 0; i < cnt-1; ++i )
	{
		uint32 c = (unsigned char)s[i];
		h = (h ^ c) * 16777619;
	}
	return (h ^ (h << 17)) + (h >> 21);
}

template< strlen_t cnt >
inline consteval uint32 ConstHashStringInsensitive( const char( &s )[cnt] )
{
	uint32 h = 2166136261u;
	for ( uint32 i = 0; i < cnt-1; ++i )
	{
		uint32 c = (unsigned char)s[i];
		c += (((('A'-1) - c) & (c - ('Z'+1))) >> 26) & 32;
		h = (h ^ c) * 16777619;
	}
	return (h ^ (h << 17)) + (h >> 21);
}

//
// Misc
//

inline void Str_Substitute( char *pStr, int a, int b )
{
	for ( ; *pStr; ++pStr )
	{
		if ( *pStr == a )
		{
			*pStr = (char)b;
		}
	}
}

inline void Str_FixSlashes( char *pStr ) {
	Str_Substitute( pStr, '\\', '/' );
}

// Widens ASCII input
void Str_Widen( const char *pNarrow, wchar_t *pWide, strlen_t maxCount );
