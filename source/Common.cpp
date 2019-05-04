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

	int cBCopy = min(cBPrev, cB);
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
		printf("\n");
	}
}
