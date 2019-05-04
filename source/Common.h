#pragma once

#include <stdint.h>

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef size_t uSize;
typedef ptrdiff_t sSize;

template <typename T> T min(T a, T b)						{ return a < b ? a : b; }
template <typename T> T max(T a, T b)						{ return a > b ? a : b; }

// simple buffer
struct Buffer // tag = buf
{
			Buffer()
			:m_pB(nullptr)
			,m_cB(0)
			,m_cBMax(0)
				{ ; }

			~Buffer();

	u8 *	m_pB;
	int		m_cB;
	int		m_cBMax;
};

void ResizeBuffer(Buffer * pBuf, int cB);

#if defined( __GNUC__ )
	#define		FF_FORCE_INLINE	inline __attribute__((always_inline))
	#define		FF_ALIGN(CB)		__attribute__((aligned(CB)))
	#define 	FF_ALIGN_OF(T) 	__alignof__(T)
	#define		FF_DEBUG_BREAK()	asm ("int $3")
#elif defined( _MSC_VER )
	#define		FF_FORCE_INLINE	__forceinline
	#define		FF_ALIGN(CB)		__declspec(align(CB))
	#define 	FF_ALIGN_OF(T) 	__alignof(T)
	#define		FF_DEBUG_BREAK()	__debugbreak()
#elif defined( __clang__)
	#define		FF_FORCE_INLINE	inline __attribute__((always_inline))
	#define		FF_ALIGN(CB)		__attribute__((aligned(CB)))
	#define 	FF_ALIGN_OF(T) 	__alignof__(T)
	#define		FF_DEBUG_BREAK()   asm("int $3")
#endif



void FFAssertHandler( const char* pChzFile, u32 line, const char* pChzCondition, const char* pChzMessage = 0, ...);

#define FF_VERIFY( PREDICATE, ... ) \
	do { if (!(PREDICATE)) { \
		FFAssertHandler(__FILE__, __LINE__, #PREDICATE, __VA_ARGS__); \
		FF_DEBUG_BREAK(); \
	} } while (0)

#define FF_ASSERT( PREDICATE, ... ) FF_VERIFY(PREDICATE, __VA_ARGS__);


#if defined( _MSC_VER )
#define FF_FVERIFY_PROC( PREDICATE, ASSERTPROC, FILE, LINE, ... )\
(\
  ( ( PREDICATE ) ? \
	true :\
	(\
	  ASSERTPROC( FILE, LINE, #PREDICATE, __VA_ARGS__ ),\
	  FF_DEBUG_BREAK(), \
	  false\
	)\
  )\
)
#else
// use a goofy expression statement to play nice with clang
#define FF_FVERIFY( PREDICATE, ... )\
(\
  ( ( PREDICATE ) ? \
	true :\
	({\
	  ASSERTPROC( FILE, LINE, #PREDICATE, __VA_ARGS__ );\
	  FF_DEBUG_BREAK(); \
	  false;\
	})\
  )\
)
#endif

#define FF_FVERIFY(PREDICATE, ...) \
	FF_FVERIFY_PROC (PREDICATE, FFAssertHandler, __FILE__, __LINE__, __VA_ARGS__ )

#define FF_FASSERT( PREDICATE, ... ) FF_FVERIFY(PREDICATE, __VA_ARGS__)

#define FF_TRACE(PREDICATE, ...) \
do { if (PREDICATE) \
	{ printf(__VA_ARGS__); } \
} while (0)



inline s32 S32Coerce(s64 n)		{ s32 nRet = (s32)n;	FF_ASSERT((s64)nRet == n, "S32Coerce failure"); return nRet; }
inline s16 S16Coerce(s64 n)		{ s16 nRet = (s16)n;	FF_ASSERT((s64)nRet == n, "S16Coerce failure"); return nRet; }
inline s8 S8Coerce(s64 n)		{ s8 nRet = (s8)n;		FF_ASSERT((s64)nRet == n, "S8Coerce failure");  return nRet; }
inline u32 U32Coerce(u64 n)		{ u32 nRet = (u32)n;	FF_ASSERT((u64)nRet == n, "u32Coerce failure"); return nRet; }
inline u16 U16Coerce(u64 n)		{ u16 nRet = (u16)n;	FF_ASSERT((u64)nRet == n, "u16Coerce failure"); return nRet; }
inline u8 U8Coerce(u64 n)		{ u8 nRet = (u8)n;		FF_ASSERT((u64)nRet == n, "u8Coerce failure");  return nRet; }


