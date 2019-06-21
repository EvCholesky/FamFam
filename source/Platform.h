#pragma once

#include "Common.h"

#define EMU_WINDOWS 1
#ifdef EMU_WINDOWS
	#define FF_FOPEN(PFILE, FILENAME, FLAGS)	fopen_s(&PFILE, FILENAME, FLAGS)
#else
	#define FF_FOPEN(PFILE, FILENAME, FLAGS)	PFILE = fopen(FILENAME, FLAGS)
#endif

struct Famicom;
struct GLFWwindow;

enum KEYCODE : s16
{
    KEYCODE_Unknown = 0,
    KEYCODE_ArrowLeft = 1,
    KEYCODE_ArrowRight = 2,
    KEYCODE_ArrowUp = 3,
    KEYCODE_ArrowDown = 4,
    KEYCODE_Shift = 5,
    KEYCODE_Escape = 6,
    KEYCODE_MouseButtonLeft = 7,
    KEYCODE_MouseButtonRight = 8,

    KEYCODE_Enter = 10,

    KEYCODE_F1 =  11,
    KEYCODE_F2 =  12,
    KEYCODE_F3 =  13,
    KEYCODE_F4 =  14,
    KEYCODE_F5 =  15,
    KEYCODE_F6 =  16,
    KEYCODE_F7 =  17,
    KEYCODE_F8 =  18,
    KEYCODE_F9 =  19,
    KEYCODE_F10 = 20,
    KEYCODE_F11 = 21,
    KEYCODE_F12 = 22,

	KEYCODE_Space = 30,
	KEYCODE_Apostrophe = 31,
	KEYCODE_Comma = 32,
	KEYCODE_Minus = 33,
	KEYCODE_Period = 34,
	KEYCODE_Slash = 35,
	KEYCODE_0 = 36,
	KEYCODE_1 = 37,
	KEYCODE_2 = 38,
	KEYCODE_3 = 39,
	KEYCODE_4 = 40,
	KEYCODE_5 = 41,
	KEYCODE_6 = 42,
	KEYCODE_7 = 43,
	KEYCODE_8 = 44,
	KEYCODE_9 = 45,
	KEYCODE_Semicolon = 46,
	KEYCODE_Equal = 47,
	KEYCODE_A = 48,
	KEYCODE_B = 49,
	KEYCODE_C = 50,
	KEYCODE_D = 51,
	KEYCODE_E = 52,
	KEYCODE_F = 53,
	KEYCODE_G = 54,
	KEYCODE_H = 55,
	KEYCODE_I = 56,
	KEYCODE_J = 57,
	KEYCODE_K = 58,
	KEYCODE_L = 59,
	KEYCODE_M = 60,
	KEYCODE_N = 61,
	KEYCODE_O = 62,
	KEYCODE_P = 63,
	KEYCODE_Q = 64,
	KEYCODE_R = 65,
	KEYCODE_S = 66,
	KEYCODE_T = 67,
	KEYCODE_U = 68,
	KEYCODE_V = 69,
	KEYCODE_W = 70,
	KEYCODE_X = 71,
	KEYCODE_Y = 72,
	KEYCODE_Z = 73,
	KEYCODE_LeftBracket = 74,
	KEYCODE_Backslash = 75,
	KEYCODE_RightBracket = 76,
	KEYCODE_GraveAccent = 77,

	KEYCODE_JoypadButton1 = 80,
	KEYCODE_JoypadButton2 = 81,
	KEYCODE_JoypadButton3 = 82,
	KEYCODE_JoypadButton4 = 83,
	KEYCODE_JoypadButton5 = 84,
	KEYCODE_JoypadButton6 = 85,
	KEYCODE_JoypadButton7 = 86,
	KEYCODE_JoypadButton8 = 87,
	KEYCODE_JoypadButton9 = 88,
	KEYCODE_JoypadButton10 = 89,
	KEYCODE_JoypadButton11 = 90,
	KEYCODE_JoypadButton12 = 91,
	KEYCODE_JoypadButton13 = 92,
	KEYCODE_JoypadButton14 = 93,
	KEYCODE_JoypadButton15 = 94,
	KEYCODE_JoypadButton16 = 95,
	KEYCODE_JoypadButton17 = 96,
	KEYCODE_JoypadButton18 = 97,
	KEYCODE_JoypadButton19 = 98,
	KEYCODE_JoypadButton20 = 99,

	KEYCODE_Max,
	KEYCODE_Min = 0,
	KEYCODE_Nil = -1,
};

struct KeyPressState // tag = keys
{
						KeyPressState();

	EDGES				m_mpKeycodeEdges[KEYCODE_Max];
};

struct Platform // tag = plat
{
						Platform()
						:m_pGlfwin(nullptr)
							{ ; }
	
	GLFWwindow *		m_pGlfwin;
};

bool FTryInitPlatform(Platform * pPlat);
void SetupDefaultInputMapping(Famicom * pFam);
void ShutdownPlatform(Platform * pPlat);

bool FTryCreateWindow(Platform * pPlat, int dX, int dY, const char* pChzTitle);
bool FShouldWindowClose(Platform * pPlat); 

void ClearScreen(Platform * pPlat);

void SwapBuffers(Platform * pPlat);
void PollInput(Platform * pPlat, Famicom * pFam);

struct Texture
{
	short		m_dX;
	short		m_dY;
	s32			m_nId;
	u8 *		m_pB;
};

Texture * PTexCreate(Platform * pPlat, s16 dX, s16 dY);
void UploadTexture(Platform * pPlat, Texture * pTex);