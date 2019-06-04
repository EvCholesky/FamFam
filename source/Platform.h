#pragma once

#include "Common.h"

#define EMU_WINDOWS 1
#ifdef EMU_WINDOWS
	#define FF_FOPEN(PFILE, FILENAME, FLAGS)	fopen_s(&PFILE, FILENAME, FLAGS)
#else
	#define FF_FOPEN(PFILE, FILENAME, FLAGS)	PFILE = fopen(FILENAME, FLAGS)
#endif

struct GLFWwindow;

struct Platform // tag = plat
{
	
	GLFWwindow *		m_pGlfwin;
};

bool FTryInitPlatform(Platform * pPlat);
void ShutdownPlatform(Platform * pPlat);

bool FTryCreateWindow(Platform * pPlat, int dX, int dY, const char* pChzTitle);
bool FShouldWindowClose(Platform * pPlat); 

void ClearScreen(Platform * pPlat);

void SwapBuffers(Platform * pPlat);
void PollInput(Platform * pPlat);

struct Texture
{
	short		m_dX;
	short		m_dY;
	s32			m_nId;
	s8 *		m_pB;
};

Texture * PTexCreate(Platform * pPlat, s16 dX, s16 dY);
void UploadTexture(Platform * pPlat, Texture * pTex);