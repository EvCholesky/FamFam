#pragma once

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