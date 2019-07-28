#pragma once
#include "Common.h"
#include <cstdlib>
#include <string.h>

// base array template
template <typename T>
struct Ary // tag=ary
{
public:
	typedef T Type;

	class Iterator // tag=iter
	{
	public:
				Iterator(Ary<T> * pAry)
				:m_pCurrent(pAry->m_a)
				,m_pEnd(&pAry->m_a[pAry->C()])
					{ ; }
								
		T *		Next() 
					{
						if (m_pCurrent == m_pEnd)
							return nullptr;

						return m_pCurrent++;
					}

		T * m_pCurrent;
		T * m_pEnd;
	};


				Ary()
				:m_a(nullptr)
				,m_c(0)
				,m_cMax(0)
					{ ; }

				Ary(T * a, size_t c, size_t cMax)
				:m_a(a)
				,m_c(c)
				,m_cMax(cMax)
					{ ; }

				Ary(const Ary&) = delete;
	Ary &		operator=(const Ary&) = delete;

	const T &	operator[](size_t i) const	{ FF_ASSERT((i>=0) & (i<m_c), "array overflow"); return m_a[i]; }
	T &			operator[](size_t i)		{ FF_ASSERT((i>=0) & (i<m_c), "array overflow"); return m_a[i]; }

	T *			A()							{ return m_a; }
	const T*	A() const					{ return m_a; }
	size_t		C() const					{ return m_c; }
	size_t		CMax() const				{ return m_cMax; }
	bool		FIsEmpty() const			{ return m_c == 0; }
	T *			PMax()						{ return &m_a[m_c]; }
	const T *	PMax() const				{ return &m_a[m_c]; }
	T *			PLast()						
					{ 
						if (FF_FVERIFY(m_c > 0, "using PLast on empty Ary"))
							return &m_a[m_c-1];
						return nullptr;
					}
	T 			Last() const			
					{ 
						if (FF_FVERIFY(m_c > 0, "using Last on empty Ary"))
							return m_a[m_c-1];
						return T();
					}
	const T *	PLast() const
					{ 
						if (FF_FVERIFY(m_c > 0, "using PLast on empty Ary"))
							return &m_a[m_c-1];
						return nullptr;
					}
	size_t		IFromP(const T * pT) const
					{ 
						size_t iT = ((uintptr_t)pT - (uintptr_t)m_a) / sizeof(T); 
						FF_ASSERT((iT >= 0) & (iT < m_c), "pointer not contained within array bounds");
						return iT; 
					}

	void		Clear()
					{
						if (m_c)
							DestructN(m_a, m_c);
						m_c = 0;
					}

	void		Reverse()
					{
						if (FIsEmpty())
							return;

						size_t iEnd = C() -1;
						for (size_t i = 0; i < iEnd; ++i, --iEnd)
						{
							T t = m_a[i];
							m_a[i] = m_a[iEnd];
							m_a[iEnd] = t;
						}
					}

	// Note - These allocation routines are here so that we can operate on FixAry without passing the template argument C_MAX
	void		Append(const Type t)
					{
						FF_ASSERT(m_c + 1 <= m_cMax, "fixed array overflow, %d + 1 > %d", m_c, m_cMax);
						T * retValue = &m_a[m_c++];
						CopyConstruct(retValue, t);
					}
	void		Append(const Type * pTArray, size_t cT)
					{
						if (!cT)
							return;

						FF_ASSERT(m_c + cT <= m_cMax, "fixed array overflow. %d + %d > %d", m_c, cT, m_cMax);
						T * pTEnd = &m_a[m_c];
						CopyConstructArray(pTEnd, cT, pTArray);
						m_c += cT;
					}

	void		AppendFill(size_t c, const Type t)
					{
						FF_ASSERT(m_c + c <= m_cMax, "fixed array overflow");
						CopyConstructN(m_a, c, t);
						m_c += c;
					}

	T *			AppendNew()
					{
						FF_ASSERT(m_c + 1 <= m_cMax, "fixed array overflow");
						T * retValue = &m_a[m_c++];
						Construct(retValue);
						return retValue;
					}

	void		AppendNew(size_t cNew)
					{
						FF_ASSERT(m_c + cNew <= m_cMax, "fixed array overflow");
						T * retValue = &m_a[m_c];
						ConstructN(retValue, cNew);
						m_c += cNew;
					}

	void		PopLast()
					{
						if (FF_FVERIFY(m_c > 0, "array underflow"))
						{
							--m_c;
							Destruct(&m_a[m_c]);
						}
					}

	T			TPopLast()
					{
						T last = Last();
						PopLast();
						return last;
					}

	void		PopToSize(size_t cNew)
					{
						while (m_c > cNew)
						{
							PopLast();
						}
					}

	void		Swap(Ary<T> * paryTOther)
					{
						T * m_aTemp    = m_a;
						size_t m_cTemp    = m_c;
						size_t m_cMaxTemp = m_cMax;

						m_a    = paryTOther->m_a;
						m_c    = paryTOther->m_c;
						m_cMax = paryTOther->m_cMax;

						paryTOther->m_a    = m_aTemp;
						paryTOther->m_c    = m_cTemp;
						paryTOther->m_cMax = m_cMaxTemp;
					}

	T *			m_a;
	size_t		m_c;
	size_t		m_cMax;
};



struct FF_ALIGN(16)	AlignedQword  					{ u8 m_a[16]; };
template <int ALIGNMENT> struct AlignedBlock		{ AlignedBlock() {FF_CASSERT(sizeof(*this), "unknown alignment in SAlignedBlock");}  };
template <> struct AlignedBlock<0>					{ u8 m_b; };
template <> struct AlignedBlock<1>					{ u8 m_b1; };
template <> struct AlignedBlock<2>					{ u16 m_b2; };
template <> struct AlignedBlock<4>					{ u32 m_b4; };
template <> struct AlignedBlock<8>					{ u64 m_b8; };
template <> struct AlignedBlock<16>					{ AlignedQword m_b16; };

template <int CB, int ALIGNMENT>
struct AlignedBytes // tag=alby
{
	void *	A()		{ return m_aB; }
	size_t	CBMax()	{ return CB; }

	AlignedBlock<ALIGNMENT> m_aB[(CB + (ALIGNMENT-1)) / ALIGNMENT];
};



// fixed sized array container template
template <typename T, int C_MAX>
class FixAry : public Ary<T> // tag=ary
{
public:
	typedef T Type;
	using Ary<T>::m_a;		// workaround for templated base class dependent names 
	using Ary<T>::m_c;	
	using Ary<T>::m_cMax;	

				FixAry()
				:Ary<T>()
					{ 
						m_a = (T *)m_alby.A(); 
						m_c = 0;
						m_cMax = C_MAX;
					}

				~FixAry()
					{ Clear(); }

				FixAry(const FixAry & rhs)
					{
						const T * pMac = rhs.PMac();
						for (const T * pT = rhs.A(); pT != pMac; ++pT)
						{
							Append(*pT);
						}
					}

	FixAry& operator=(const FixAry & rhs)
					{
						Clear();

						const T * pMac = rhs.PMac();
						for (const T * pT = rhs.A(); pT != pMac; ++pT)
						{
							Append(*pT);
						}
						return *this;
					}

	void		Append(const Type t)
					{
						FF_ASSERT(m_c < m_cMax, "FixAry overflow");
						T * retValue = &m_a[m_c++];
						CopyConstruct(retValue, t);
					}

	void		Append(const Type * pTArray, size_t cT)
					{
						if (!cT)
							return;

						FF_ASSERT(m_c + cT < m_cMax, "FixAry overflow");
						T * pTEnd = &m_a[m_c];
						CopyConstructArray(pTEnd, cT, pTArray);
						m_c += cT;
					}

	void		AppendFill(size_t c, const Type t)
					{
						FF_ASSERT(m_c < m_cMax, "FixAry overflow");
						CopyConstructN(&m_a[m_c], c, t);
						m_c += c;
					}

	T *			AppendNew()
					{
						FF_ASSERT(m_c < m_cMax, "FixAry overflow");
						T * retValue = &m_a[m_c++];
						Construct(retValue);
						return retValue;
					}

	void		Remove(const Type t)
					{
						T * pEnd = &m_a[m_c];
						for (T * pSrc = m_a, * pDst = m_a; pSrc != pEnd; ++pSrc)
						{
							if(t == *pSrc)
							{
								Destruct(pSrc);
								--m_c;
							}
							else
							{
								*pDst++ = *pSrc;
							}
						}
					}
	void		RemoveSpan(int iMin, int iMax)
					{
						T * pEnd = &m_a[m_c];
						int i = iMin;
						for (T * pSrc = &m_a[i], * pDst = &m_a[iMax]; pSrc != pEnd; ++pSrc, ++i)
						{
							if (i < iMax)
							{
								Destruct(pSrc);
								--m_c;
							}
							else
							{
								*pDst++ = *pSrc;
							}
						}
					}
		
	void			Clear()
					{
						T * pTMac = &m_a[m_c];
						for (T * pT = m_a; pT != pTMac; ++pT)
						{
							Destruct(pT);
						}
						m_c = 0;
					}
				

	AlignedBytes<sizeof(T) * C_MAX, FF_ALIGN_OF(T)>	m_alby;
};


// resizable array (aka std::vector)
template <typename T>
class DynAry : public Ary<T> //tag=ary
{
public:
	typedef T Type;
	using Ary<T>::m_a;		// workaround for templated base class dependent names 
	using Ary<T>::m_c;	
	using Ary<T>::m_cMax;	

				DynAry(size_t cMaxStarting)
				:Ary<T>(nullptr, 0, 0)
					{ Resize(cMaxStarting); }

				DynAry()
				:Ary<T>(nullptr, 0, 0)
					{ ; }

				DynAry(const DynAry & rhs)
					{
						EnsureSize(rhs.C());

						const T * pMac = rhs.PMac();
						for (const T * pT = rhs.A(); pT != pMac; ++pT)
						{
							Append(*pT);
						}
					}

				~DynAry()
					{ Clear(); }

	DynAry<T> & operator= (const DynAry & rhs)
					{
						Clear();
						EnsureSize(rhs.C());

						const T * pMac = rhs.PMax();
						for (const T * pT = rhs.A(); pT != pMac; ++pT)
						{
							Append(*pT);
						}
						return *this;
					}

	void		Clear()
					{ Resize(0); }

	void		Append(const Type t)
					{
						EnsureSize(m_c+1);
						T * pTEnd = &m_a[m_c++];
						CopyConstruct(pTEnd, t);
					}

	void		Append(const Type * pTArray, size_t cT)
					{
						if (!cT)
							return;

						EnsureSize(m_c + cT);
						T * pTEnd = &m_a[m_c];
						CopyConstructArray(pTEnd, cT, pTArray);
						m_c += cT;
					}

	void		AppendFill(size_t c, const Type t)
					{
						EnsureSize(m_c + c);
						CopyConstructN(&m_a[m_c], c, t);
						m_c += c;
					}

	T *			AppendNew()
					{
						EnsureSize(m_c+1);
						T * retValue = &m_a[m_c++];
						Construct(retValue);
						return retValue;
					}

	void		AppendNew(size_t cNew)
					{
						EnsureSize(m_c+cNew);
						T * retValue = &m_a[m_c];
						ConstructN(retValue, cNew);
						m_c += cNew;
					}

	void		EnsureSize(size_t cSize)
					{
						size_t c = cSize;
						if (c > m_cMax) 
						{
							size_t cNew = ffMax(m_cMax * 2, c); 
							Resize(cNew);
						}
					}

	void		Remove(const Type t)
					{
						T * pEnd = &m_a[m_c];
						for (T * pSrc = m_a, * pDst = m_a; pSrc != pEnd; ++pSrc)
						{
							if(t == *pSrc)
							{
								--m_c;
							}
							else
							{
								*pDst++ = *pSrc;
							}
						}

						// BB - doesn't destruct the object until it resizes!
						size_t cResize = (m_c < 8) ? 0 : m_cMax / 2;
						if(m_c < cResize)
							Resize(cResize);
					}

	void		RemoveSpan(size_t iMin, size_t iMax)
					{
						if (iMin >= iMax)
							return;

						T * pEnd = &m_a[m_c];
						size_t i = iMin;
						for (T * pSrc = &m_a[i], * pDst = &m_a[iMax]; pSrc != pEnd; ++pSrc, ++i)
						{
							if (i < iMax)
							{
								Destruct(pSrc);
								--m_c;
							}
							else
							{
								*pDst++ = *pSrc;
							}
						}
					}

	void		RemoveFastByI(size_t iT)
					{
						if (FF_FVERIFY((iT >= 0) & (iT < m_c), "bad element index" ))
						{
							--m_c;
							Destruct(&m_a[iT]);
							if (iT != m_c)
								m_a[iT] = m_a[m_c];

							size_t cResize = (m_c < 8) ? 0 : m_cMax / 2;
							if(m_c <= cResize)
								Resize(cResize);
						}
					}

	void		PopLast()
					{
						if (FF_FVERIFY(m_c > 0, "array underflow"))
						{
							--m_c;
							Destruct(&m_a[m_c]);

							size_t cResize = (m_c < 8) ? 0 : m_cMax / 2;
							if(m_c <= cResize)
								Resize(cResize);
						}
					}
		
	void		Resize(size_t cMax)
					{
						size_t cNewMax = cMax;
						FF_ASSERT(cNewMax == static_cast<s32>(cNewMax), "size overflow in DynAry");
						if (cNewMax == m_cMax)
							return;

						T * aOld = m_a;
						if (cNewMax < m_cMax)
						{
							if (cNewMax < m_c)
							{
								DestructN(&m_a[cNewMax], m_c - cNewMax);
								m_c = cNewMax;
							}
						}

						if (cNewMax > 0)
						{
							size_t cB = sizeof(T) * cNewMax;
							//m_a = (T *)malloc(cB, FF_ALIGN_OF(T), m_bk);
							//m_a = (T *)std::aligned_alloc(FF_ALIGN_OF(T), cB);
							m_a = (T *)_aligned_malloc(cB, FF_ALIGN_OF(T));

							if (aOld)
							{
								auto cMin = (m_cMax < cNewMax) ? m_cMax : cNewMax;
								memcpy(m_a, aOld, sizeof(T) * cMin);
							}
						}
						else
						{
							m_a = nullptr;
						}

						if (aOld)
							_aligned_free(aOld);
						m_cMax = cNewMax;
					}

	void		Swap(DynAry<T> * paryTOther)
					{
						T * m_aTemp    = m_a;
						size_t m_cTemp    = m_c;
						size_t m_cMaxTemp = m_cMax;

						m_a    = paryTOther->m_a;
						m_c    = paryTOther->m_c;
						m_cMax = paryTOther->m_cMax;

						paryTOther->m_a    = m_aTemp;
						paryTOther->m_c    = m_cTemp;
						paryTOther->m_cMax = m_cMaxTemp;
					}
};
