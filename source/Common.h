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

#define FF_DIM(arr) (sizeof(arr) / sizeof(*arr))
#define FF_PMAC(arr) &arr[EWC_DIM(arr)]

#if defined( __GNUC__ )
	#define		FF_FORCE_INLINE	inline __attribute__((always_inline))
	#define		FF_ALIGN(CB)		__attribute__((aligned(CB)))
	#define 	FF_ALIGN_OF(T) 	__alignof__(T)
	#define		FF_IS_ENUM(T)		__is_enum(T)
	#define		FF_DEBUG_BREAK()	asm ("int $3")
#elif defined( _MSC_VER )
	#define		FF_FORCE_INLINE	__forceinline
	#define		FF_ALIGN(CB)		__declspec(align(CB))
	#define 	FF_ALIGN_OF(T) 	__alignof(T)
	#define		FF_IS_ENUM(T)		__is_enum(T)
	#define		FF_DEBUG_BREAK()	__debugbreak()
#elif defined( __clang__)
	#define		FF_FORCE_INLINE	inline __attribute__((always_inline))
	#define		FF_ALIGN(CB)		__attribute__((aligned(CB)))
	#define 	FF_ALIGN_OF(T) 	__alignof__(T)
	#define		FF_IS_ENUM(T)		__is_enum(T)
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


// template traits
template <typename T> struct SStripConst				{ typedef T Type;	enum { F_STRIPPED= false }; };
template <typename T> struct SStripConst<const T>		{ typedef T Type;	enum { F_STRIPPED= true }; };

template <typename T> struct SStripReference			{ typedef T Type; 	enum { F_STRIPPED= false }; };
template <typename T> struct SStripReference<T&>		{ typedef T Type; 	enum { F_STRIPPED= true }; };

template <typename T> struct SStripPointer				{ typedef T Type; 	enum { F_STRIPPED= false }; };
template <typename T> struct SStripPointer<T*>			{ typedef T Type; 	enum { F_STRIPPED= true }; };

template <typename T> struct SIsReference				{ enum { V = false }; };
template <typename T> struct SIsReference<T&>			{ enum { V = true }; };

template <typename T> struct SIsPointer					{ enum { V = false }; };
template <typename T> struct SIsPointer<T&>				{ enum { V = true }; };

template <typename T> struct SIsSignedInt				{ enum { V = false }; };
template <typename T> struct SIsUnsignedInt						{ enum { V = false }; };

template <> struct SIsUnsignedInt<u8>							{ enum { V = true }; };
template <> struct SIsUnsignedInt<u16>							{ enum { V = true }; };
template <> struct SIsUnsignedInt<u32>							{ enum { V = true }; };
template <> struct SIsUnsignedInt<u64>							{ enum { V = true }; };
 
template <typename T> struct SIsInt								{ enum { V  = SIsSignedInt<T>::V  || SIsUnsignedInt<T>::V  }; };

template <typename T> struct SIsFloat							{ enum { V = false }; };
template <> struct SIsFloat<f32>								{ enum { V = true }; };
template <> struct SIsFloat<f64>								{ enum { V = true }; };

template <typename T> struct SIsBool							{ enum { V = false }; };
template <> struct SIsBool<bool>								{ enum { V = true }; };

template <typename T> struct SIsVoid							{ enum { V = false }; };
template <> struct SIsVoid<void>								{ enum { V = true }; };

template <typename T> struct SVoidSafeSizeof					{ enum { V = sizeof(T) }; };
template <> struct SVoidSafeSizeof<void>						{ enum { V = 0 }; };

// NOTE: can't just check static_cast<T>(-1) because it doesn't work for custom types
template <typename T, bool IS_ENUM> struct SIsSignedSelector	{ enum { V = SIsFloat<T>::V || SIsSignedInt<T>::V }; };
template <typename T> struct SIsSignedSelector<T, true>			{ enum { V = static_cast<T>(-1) < 0 }; };
template <typename T> struct SIsSigned							{ enum { V = SIsSignedSelector<T, FF_IS_ENUM(T)>::V }; };

template <typename T> struct SIsFundamentalType					{ enum { V  = 
																		SIsPointer<T>::V  || 
																		SIsReference<T>::V  || 
																		SIsInt<T>::V  || 
																		SIsFloat<T>::V  || 
																		SIsBool<T>::V  || 
																		FF_IS_ENUM(T) 
																	}; 
															};
template <typename T>
struct SArrayTraits
{
	typedef T Element;
	enum { C_ELEMENTS = -1 };
	enum { F_IS_ARRAY = false };
};

template <typename A, int C>
struct SArrayTraits<A[C]>
{
	typedef A Element;
	enum { C_ELEMENTS = C };
	enum { F_IS_ARRAY = true };
};

template <typename T> struct SHasTrivialConstructor		{ enum { V  = SIsFundamentalType<T>::V  }; };
template <typename T> struct SHasTrivialCopy			{ enum { V  = SIsFundamentalType<T>::V  }; };
template <typename T> struct SHasTrivialDestructor		{ enum { V  = SIsFundamentalType<T>::V  }; };

template <typename T, bool TRIVIAL_CONSTRUCT>
struct SConstructSelector
{
	static void Construct(T * p)
	{
		FF_ASSERT( ((uintptr_t)p & (FF_ALIGN_OF(T)-1)) == 0, "trying to construct misaligned object" );
		new (p) T;
	}

	static void ConstructN(T * p, size_t c)
	{
		FF_ASSERT( ((uintptr_t)p & (FF_ALIGN_OF(T)-1)) == 0, "trying to construct misaligned object" );
		for (size_t i = 0; i < c; ++i)
			new (p + i) T;
	}
};

template <typename T>
struct SConstructSelector<T, true> // trivial constructor
{
	static void Construct(T * p)					{ }
	static void ConstructN(T * p, size_t c)			{ }
};

template <typename T, bool TRIVIAL_COPY>
struct SCopySelector
{
	static void CopyConstruct(T * p, const T & orig)
	{
		FF_ASSERT( ((uintptr_t)p & (FF_ALIGN_OF(T)-1)) == 0, "trying to copy construct misaligned object" );
		new (p) T(orig);
	}

	static void CopyConstructN(T * p, size_t c, const T & orig)
	{
		FF_ASSERT( ((uintptr_t)p & (FF_ALIGN_OF(T)-1)) == 0, "trying to copy construct misaligned object" );
		for (size_t i = 0; i < c; ++i)
			new (p + i) T(orig);
	}

	static void CopyConstructArray(T * pTDst, size_t cT, const T * pTSrc)
	{
		FF_ASSERT( ((uintptr_t)pTDst & (FF_ALIGN_OF(T)-1)) == 0, "trying to copy construct misaligned object" );
		auto pTDstMax = pTDst + cT;
		for (auto pTDstIt = pTDst; pTDstIt != pTDstMax; ++pTDstIt, ++pTSrc)
			new (pTDstIt) T(*pTSrc);
	}
};

template <typename T>
struct SCopySelector<T, true> // trivial copy constructor
{
	static void CopyConstruct(T * p, const T & orig)					{ *p = orig; }
	static void CopyConstructN(T * p, size_t c, const T & orig)		
	{ 
		for (T * pEnd = &p[c]; p != pEnd; ++p)
			*p = orig;
	}

	static void CopyConstructArray(T * pTDst, size_t cT, const T * pTSrc)		
	{ 
		CopyAB(pTSrc, pTDst, sizeof(T) * cT);
	}
};

template <typename T, bool TRIVIAL_DESTRUCT>
struct SDestructSelector
{
	static void Destruct(T * p)
	{
		p->~T();
	}

	static void DestructN(T * p, size_t c)
	{
		for (size_t i = 0; i < c; ++i)
			(p + i)->~T();
	}
};

template <typename T>
struct SDestructSelector<T, true> // trivial destructor 
{
	static void Destruct(T * p)					{ }
	static void DestructN(T * p, size_t c)		{ }
};

template <typename T> void Construct(T * p)									{ SConstructSelector<T, SHasTrivialConstructor<T>::V >::Construct(p); }
template <typename T> void ConstructN(T * p, size_t c)						{ SConstructSelector<T, SHasTrivialConstructor<T>::V >::ConstructN(p,c); }

template <typename T> void CopyConstruct(T * p, const T & orig)				{ SCopySelector<T, SHasTrivialCopy<T>::V >::CopyConstruct(p, orig); }
template <typename T> void CopyConstructN(T * p, size_t c, const T & orig)	{ SCopySelector<T, SHasTrivialCopy<T>::V >::CopyConstructN(p, c, orig); }
template <typename T> void CopyConstructArray(T * pTDst, size_t cT, const T * pTSrc)	
																			{ SCopySelector<T, SHasTrivialCopy<T>::V >::CopyConstructArray(pTDst, cT, pTSrc); }

template <typename T> void Destruct(T * p)									{ SDestructSelector<T, SHasTrivialDestructor<T>::V >::Destruct(p); }
template <typename T> void DestructN(T * p, size_t c)						{ SDestructSelector<T, SHasTrivialDestructor<T>::V >::DestructN(p,c); }