#include "common.h"

#include <cstdarg>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Buffer::~Buffer()
{
	ResizeBuffer(this, 0);
}

void ResizeBuffer(Buffer * pBuf, int cB)
{
	u8 * pBPrev = pBuf->m_pB;
	int cBPrev = pBuf->m_cB;

	if (cB)
	{
		pBuf->m_pB = (u8 *)malloc(cB);
	}

	int cBCopy = ffMin(cBPrev, cB);
	if (cBCopy)
	{
		memcpy(pBuf->m_pB, pBPrev, cBCopy);
	}

	if (pBPrev)
	{
		free(pBPrev);
	}

	pBuf->m_cB = cBCopy;
	pBuf->m_cBMax = cB;
}

void FFAssertHandler(const char* pChzFile, u32 line, const char* pChzCondition, const char* pChzMessage, ... )
{
	printf("Assertion failed: \"%s\" at %s:%u\n", pChzCondition, pChzFile, line);

	if (pChzMessage)
	{
		va_list ap;
		va_start(ap, pChzMessage);
		vprintf(pChzMessage, ap);
		va_end(ap);
		printf("\n");
	}
}

void ZeroAB(void * pDest, size_t cB)
{
	memset(pDest, 0, cB);
}

void FillAB(u8 b, void * pDest, size_t cB)
{
	memset(pDest, b, cB);
}

void CopyAB(const void * pSource, void * pDest, size_t cB)
{
	memcpy(pDest, pSource, cB);
}

void ShowErrorV(const char * pChzFormat, va_list ap)
{
	printf("Error: ");
	vprintf(pChzFormat, ap);
}

void ShowError(const char* pChzFormat, ...)
{
	va_list ap;
	va_start(ap, pChzFormat);
	ShowErrorV(pChzFormat, ap);
	va_end(ap);
}

void ShowInfoV(const char * pChzFormat, va_list ap)
{
	printf("Error: ");
	vprintf(pChzFormat, ap);
}

void ShowInfo(const char * pChzFormat, ...)
{
	va_list ap;
	va_start(ap, pChzFormat);
	ShowInfoV(pChzFormat, ap);
	va_end(ap);
}
// simple hash function with meh avalanching, works for now. replace with MUM hash or xxHash
u32 HvFromPBFVN(const void * pV, size_t cB)
{
	auto pB = (u8*)pV;
    u32 hv = 2166136261;

	for (size_t iB=0; iB < cB; ++iB)
    {
        hv = (hv * 16777619) ^ pB[iB];
    }

    return hv;
}

u32 HvConcatPBFVN(u32 hv, const void * pV, size_t cB)
{
	auto pB = (u8*)pV;
	for (size_t iB=0; iB < cB; ++iB)
    {
        hv = (hv * 16777619) ^ pB[iB];
    }

    return hv;
}

inline size_t CBCodepoint(const char * pCoz)
{
	if ((*pCoz & 0xF8) == 0xF0)	return 4;
	if ((*pCoz & 0xF0) == 0xE0)	return 3;
	if ((*pCoz & 0xE0) == 0xC0)	return 2;
	return 1;
}

void SplitFilename(const char * pChzFilename, size_t * piBFile, size_t * piBExtension, size_t * piBEnd)
{
	ptrdiff_t iBFile = 0;
	ptrdiff_t iBExtension = -1;
	const char * pChIt = pChzFilename;
	for ( ; *pChIt != '\0'; pChIt += CBCodepoint(pChIt))
	{
		if (*pChIt == '\\')
		{
			iBFile = (pChIt+1) - pChzFilename;
			iBExtension = -1;	// only acknowledge the first '.' after the last directory
		}
		else if (iBExtension < 0 && *pChIt == '.')
		{
			iBExtension = pChIt - pChzFilename;
		}
	}

	*piBFile = iBFile;
	*piBEnd = pChIt - pChzFilename;
	*piBExtension = (iBExtension < 0) ? *piBEnd : iBExtension;
}


