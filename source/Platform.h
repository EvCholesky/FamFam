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

struct IXAudio2SourceVoice;
class VoiceCallback;

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

	KEYCODE_JoypadButtonMin = 80, 
	KEYCODE_JoypadButton0 = KEYCODE_JoypadButtonMin,
	KEYCODE_JoypadButton1 = 81,
	KEYCODE_JoypadButton2 = 82,
	KEYCODE_JoypadButton3 = 83,
	KEYCODE_JoypadButton4 = 84,
	KEYCODE_JoypadButton5 = 85,
	KEYCODE_JoypadButton6 = 86,
	KEYCODE_JoypadButton7 = 87,
	KEYCODE_JoypadButton8 = 88,
	KEYCODE_JoypadButton9 = 89,
	KEYCODE_JoypadButton10 = 90,
	KEYCODE_JoypadButton11 = 91,
	KEYCODE_JoypadButton12 = 92,
	KEYCODE_JoypadButton13 = 93,
	KEYCODE_JoypadButton14 = 94,
	KEYCODE_JoypadButton15 = 95,
	KEYCODE_JoypadButton16 = 96,
	KEYCODE_JoypadButton17 = 97,
	KEYCODE_JoypadButton18 = 98,
	KEYCODE_JoypadButton19 = 99,

	KEYCODE_AxisNegMin = 100,
	KEYCODE_JoypadAxisNeg0 = KEYCODE_AxisNegMin,
	KEYCODE_JoypadAxisNeg1 = 101,
	KEYCODE_JoypadAxisNeg2 = 102,
	KEYCODE_JoypadAxisNeg3 = 103,
	KEYCODE_JoypadAxisNeg4 = 104,
	KEYCODE_JoypadAxisNeg5 = 105,
	KEYCODE_AxisNegMax = 106,

	KEYCODE_AxisPosMin = KEYCODE_AxisNegMax,
	KEYCODE_JoypadAxisPos0 = KEYCODE_AxisPosMin,
	KEYCODE_JoypadAxisPos1 = 107,
	KEYCODE_JoypadAxisPos2 = 108,
	KEYCODE_JoypadAxisPos3 = 109,
	KEYCODE_JoypadAxisPos4 = 110,
	KEYCODE_JoypadAxisPos5 = 111,
	KEYCODE_AxisPosMax = 112,

	KEYCODE_Max,
	KEYCODE_Min = 0,
	KEYCODE_Nil = -1,
};

struct KeyPressState // tag = keys
{
						KeyPressState();

	EDGES				m_mpKeycodeEdges[KEYCODE_Max];
};

struct PlatformTime // tag = pltime
{
				PlatformTime()
				:m_nHzDesired(60)
				,m_dTFrameTarget(1.0f/60.0f)
				,m_rTickToSecond(0.001f)
				,m_cTickPerSecond(1000)
				,m_fHasMsSleep(false)
					{ ; }

	s32			m_nHzDesired;
	f32			m_dTFrameTarget;	
	f32			m_rTickToSecond;		// 1.0f / cTickPerSecond;
	s64			m_cTickPerSecond;
	bool		m_fHasMsSleep;
};

enum TVAL // Timer record 
{
	TVAL_FamicomUpdate,
	TVAL_UpdateApu,
	TVAL_DebuggerUpdate,
	TVAL_ImguiRender,
	TVAL_TotalFrame,

	TVAL_Max,
	TVAL_Min = 0,
	TVAL_Nil = -1,
};

enum JCONS // Joystick CONnection State
{
	JCONS_Disconnected,
	JCONS_Connected,

	JCONS_Nil = -1
};

struct PlatformJoystick // tag = pljoy
{
					PlatformJoystick()
					:m_jcons(JCONS_Disconnected)
					,m_iPljoy(-1)
					,m_cGAxis(0)
					,m_cBButton(0)
					,m_aGAxis(nullptr)
					,m_aBButton(nullptr)
					,m_aGAxisPrev(nullptr)
					,m_aBButtonPrev(nullptr)
						{ ; }

	JCONS			m_jcons;

	int				m_iPljoy;		// GLFW joystick token GLFW_JOYSTICK_1 .. GLFW_JOYSTICK_LAST
	int				m_cGAxis;
	int				m_cBButton;
	const f32 *		m_aGAxis;		// allocated by GLFL, scope valid until controller disconnect
	const u8 *		m_aBButton;		// allocated by GLFL, scope valid until controller disconnect

	f32 *			m_aGAxisPrev;	// allocted locally, delete upon controller disconnect
	u8 *			m_aBButtonPrev;	// allocted locally, delete upon controller disconnect
};



struct PlatformAudio // tag = plaud
{
				PlatformAudio()
				:m_pVoicecb(nullptr)
				,m_pSrcvoice(nullptr)
				,m_iASamp(0)
					{ ; }

    static const int s_cBitSample = 16;
	static const u32 s_cSampBuffer = 32000;
	static const u32 s_cAudioBuffer = 3;

    VoiceCallback *  		m_pVoicecb;
    IXAudio2SourceVoice *	m_pSrcvoice;
    s16 					m_aSamp[s_cAudioBuffer * s_cSampBuffer + 8];
    u8						m_iASamp;	// which is the current buffer (in the triple buffer)
};



static const int s_cJoystickMax = 16;
struct Platform // tag = plat
{
						Platform();
	
	GLFWwindow *		m_pGlfwin;
	PlatformTime		m_pltime;
	s64					m_mpTvalCTickStart[TVAL_Max];	// when was StartTimer called for a given TVAL
	s64					m_mpTvalCTick[TVAL_Max];	// total ticks this frame
	float				m_mpTvalDTFrame[TVAL_Max];		// last frame's timings - used for reporting

	PlatformAudio		m_plaud;
	PlatformJoystick	m_aPljoy[s_cJoystickMax];
};
extern Platform g_plat;

bool FTryInitPlatform(Platform * pPlat, int nHzTarget);
void SetupDefaultInputMapping(Famicom * pFam);
void ShutdownPlatform(Platform * pPlat);

void InitPlatformTime(PlatformTime * pPltime, int nHzTarget);
void WaitUntilFrameEnd(PlatformTime * pPltime, s64 cTickStart);

bool FTrySetTimerResolution(u32 msResolution);
s64 CTickPerSecond();
s64 CTickWallClock();

bool FTryCreateWindow(Platform * pPlat, int dX, int dY, const char* pChzTitle);
bool FShouldWindowClose(Platform * pPlat); 

void ClearScreen(Platform * pPlat);

void SwapBuffers(Platform * pPlat);
void PollInput(Platform * pPlat, Famicom * pFam);

void StartTimer(TVAL tval);
void EndTimer(TVAL tval);
f32 DTFrame(Platform * pPlat, TVAL tval);
f32 DMsecFrame(Platform * pPlat, TVAL tval);
void FrameEndTimers(Platform * pPlat);
void ClearTimers(Platform * pPlat);

void InitPlatformAudio(Famicom * pFam, PlatformAudio * pPlaud);
void ShutdownPlatformAudio(PlatformAudio * pPlaud);
void UpdatePlatformAudio(Famicom * pFam, PlatformAudio * pPlaud);

struct Texture
{
	short		m_dX;
	short		m_dY;
	s32			m_nId;
	u8 *		m_pB;
};

Texture * PTexCreate(Platform * pPlat, s16 dX, s16 dY);
void UploadTexture(Platform * pPlat, Texture * pTex);