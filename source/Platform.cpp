#include "Cpu.h"
#include "imgui.h"
#include "Platform.h"
#include "glfw/include/GLFW/glfw3.h"
#include "glfw/deps/GL/glext.h"

KeyPressState g_keyps;

KEYCODE KeycodeFromGlfwKey(int nKey)
{
	struct SKeyPair // tag = keyp 
	{
		int			m_nKeyGlfw;
		KEYCODE		m_keycode;
	};

	SKeyPair s_aKeyp [] = 
	{
		{ GLFW_KEY_LEFT,	KEYCODE_ArrowLeft},
		{ GLFW_KEY_RIGHT,	KEYCODE_ArrowRight},
		{ GLFW_KEY_UP,		KEYCODE_ArrowUp},
		{ GLFW_KEY_DOWN,	KEYCODE_ArrowDown},
		{ GLFW_KEY_ESCAPE,	KEYCODE_Escape},

		{ GLFW_KEY_ENTER,	KEYCODE_Enter},

		{ GLFW_KEY_F1,		KEYCODE_F1},
		{ GLFW_KEY_F2,		KEYCODE_F2},
		{ GLFW_KEY_F3,		KEYCODE_F3},
		{ GLFW_KEY_F4,		KEYCODE_F4},
		{ GLFW_KEY_F5,		KEYCODE_F5},
		{ GLFW_KEY_F6,		KEYCODE_F6},
		{ GLFW_KEY_F7,		KEYCODE_F7},
		{ GLFW_KEY_F8,		KEYCODE_F8},
		{ GLFW_KEY_F9,		KEYCODE_F9},
		{ GLFW_KEY_F10,		KEYCODE_F10},
		{ GLFW_KEY_F11,		KEYCODE_F11},
		{ GLFW_KEY_F12,		KEYCODE_F12},

		{ GLFW_KEY_SPACE,	KEYCODE_Space },
		{ GLFW_KEY_APOSTROPHE,	KEYCODE_Apostrophe },
		{ GLFW_KEY_COMMA,	KEYCODE_Comma },
		{ GLFW_KEY_MINUS,	KEYCODE_Minus },
		{ GLFW_KEY_PERIOD,	KEYCODE_Period },
		{ GLFW_KEY_SLASH,	KEYCODE_Slash },
		{ GLFW_KEY_0,	KEYCODE_0 },
		{ GLFW_KEY_1,	KEYCODE_1 },
		{ GLFW_KEY_2,	KEYCODE_2 },
		{ GLFW_KEY_3,	KEYCODE_3 },
		{ GLFW_KEY_4,	KEYCODE_4 },
		{ GLFW_KEY_5,	KEYCODE_5 },
		{ GLFW_KEY_6,	KEYCODE_6 },
		{ GLFW_KEY_7,	KEYCODE_7 },
		{ GLFW_KEY_8,	KEYCODE_8 },
		{ GLFW_KEY_9,	KEYCODE_9 },
		{ GLFW_KEY_SEMICOLON,	KEYCODE_Semicolon },
		{ GLFW_KEY_EQUAL,	KEYCODE_Equal },
		{ GLFW_KEY_A,	KEYCODE_A },
		{ GLFW_KEY_B,	KEYCODE_B },
		{ GLFW_KEY_C,	KEYCODE_C },
		{ GLFW_KEY_D,	KEYCODE_D },
		{ GLFW_KEY_E,	KEYCODE_E },
		{ GLFW_KEY_F,	KEYCODE_F },
		{ GLFW_KEY_G,	KEYCODE_G },
		{ GLFW_KEY_H,	KEYCODE_H },
		{ GLFW_KEY_I,	KEYCODE_I },
		{ GLFW_KEY_J,	KEYCODE_J },
		{ GLFW_KEY_K,	KEYCODE_K },
		{ GLFW_KEY_L,	KEYCODE_L },
		{ GLFW_KEY_M,	KEYCODE_M },
		{ GLFW_KEY_N,	KEYCODE_N },
		{ GLFW_KEY_O,	KEYCODE_O },
		{ GLFW_KEY_P,	KEYCODE_P },
		{ GLFW_KEY_Q,	KEYCODE_Q },
		{ GLFW_KEY_R,	KEYCODE_R },
		{ GLFW_KEY_S,	KEYCODE_S },
		{ GLFW_KEY_T,	KEYCODE_T },
		{ GLFW_KEY_U,	KEYCODE_U },
		{ GLFW_KEY_V,	KEYCODE_V },
		{ GLFW_KEY_W,	KEYCODE_W },
		{ GLFW_KEY_X,	KEYCODE_X },
		{ GLFW_KEY_Y,	KEYCODE_Y },
		{ GLFW_KEY_Z,	KEYCODE_Z },
		{ GLFW_KEY_LEFT_BRACKET,	KEYCODE_LeftBracket },
		{ GLFW_KEY_BACKSLASH,	KEYCODE_Backslash },			// '\'
		{ GLFW_KEY_RIGHT_BRACKET,	KEYCODE_RightBracket },
		{ GLFW_KEY_GRAVE_ACCENT,	KEYCODE_GraveAccent },		// `
	};

	SKeyPair * pKeypMax = FF_PMAX(s_aKeyp);
	for (SKeyPair * pKeyp = s_aKeyp; pKeyp != pKeypMax; ++pKeyp)
	{
		if (pKeyp->m_nKeyGlfw == nKey)
			return pKeyp->m_keycode;
	}

	return KEYCODE_Unknown;
}

static void GlfwKeyCallback(GLFWwindow * pGlfwin, int key, int scancode, int action, int mods)
{
	auto keycode = KeycodeFromGlfwKey(key);
    ImGuiIO& io = ImGui::GetIO();
    if (action == GLFW_PRESS)
	{
        io.KeysDown[key] = true;
		g_keyps.m_mpKeycodeEdges[keycode] = EDGES_Press;
	}
    if (action == GLFW_RELEASE)
	{
        io.KeysDown[key] = false;
		g_keyps.m_mpKeycodeEdges[keycode] = EDGES_Release;
	}

    // Modifiers are not reliable across systems
    io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
    io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
    io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
    io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
}

static void GlfwMouseButtonCallback(GLFWwindow * pGlfwin, int nButton,int nAction, int mods)
{
}

static void GlfwMouseCursorCallback(GLFWwindow * pGlfwin, double xCursor, double yCursor)
{
}

static void GlfwCharCallback(GLFWwindow * pGlfwin, unsigned int c)
{
    ImGuiIO& io = ImGui::GetIO();
    if (c > 0 && c < 0x10000)
        io.AddInputCharacter((unsigned short)c);
}

static void GlfwScrollCallback(GLFWwindow * pGlfwin, double dX, double dY)
{
    ImGuiIO& io = ImGui::GetIO();
    io.MouseWheelH += (float)dX;
    io.MouseWheel += (float)dY;
}

FamicomInput::FamicomInput()
: m_nControllerLatch(0)
{
	ZeroAB(m_aGpad, sizeof(m_aGpad));
}

KeyPressState::KeyPressState()
{
	ZeroAB(m_mpKeycodeEdges, sizeof(m_mpKeycodeEdges));
}

void SetupDefaultInputMapping(Famicom * pFam)
{
	pFam->m_pKeyps = &g_keyps;
	FamicomInput * pFamin = &pFam->m_famin;
	for (int iGpad = 0; iGpad < s_cGamepadMax; ++iGpad)
	{
		auto pGpad = &pFamin->m_aGpad[iGpad];
		for (int butk = 0; butk < BUTK_Max; ++butk)
		{
			pGpad->m_mpButkKeycode[butk] = KEYCODE_Nil;
		}
	}

	Gamepad * pGpad0 = &pFamin->m_aGpad[0];
	pGpad0->m_fIsConnected = true;

	pGpad0->m_mpButkKeycode[BUTK_A] = KEYCODE_A;
	pGpad0->m_mpButkKeycode[BUTK_B] = KEYCODE_S;
	pGpad0->m_mpButkKeycode[BUTK_Select] = KEYCODE_Apostrophe;
	pGpad0->m_mpButkKeycode[BUTK_Start] = KEYCODE_Enter;
	pGpad0->m_mpButkKeycode[BUTK_Up] = KEYCODE_ArrowUp;
	pGpad0->m_mpButkKeycode[BUTK_Down] = KEYCODE_ArrowDown;
	pGpad0->m_mpButkKeycode[BUTK_Left] = KEYCODE_ArrowLeft;
	pGpad0->m_mpButkKeycode[BUTK_Right] = KEYCODE_ArrowRight;
}

bool FTryInitPlatform(Platform * pPlat)
{
	if (!glfwInit())
		return false;
	return true;
}

bool FTryCreateWindow(Platform * pPlat, int dX, int dY, const char* pChzTitle)
{
	auto pGlfwin = glfwCreateWindow(dX, dY, pChzTitle, nullptr, nullptr);
	
	if (!pGlfwin)
	{
		glfwTerminate();
		return false;
	}

	pPlat->m_pGlfwin = pGlfwin;
	glfwMakeContextCurrent(pGlfwin);
	glfwSwapInterval(0);

	glfwSetKeyCallback(pGlfwin, GlfwKeyCallback);
	glfwSetMouseButtonCallback(pGlfwin, GlfwMouseButtonCallback);
	glfwSetCursorPosCallback(pGlfwin, GlfwMouseCursorCallback);
    glfwSetScrollCallback(pGlfwin, GlfwScrollCallback);
    glfwSetCharCallback(pGlfwin, GlfwCharCallback);

	return true;
}

bool FShouldWindowClose(Platform * pPlat)
{
	return glfwWindowShouldClose(pPlat->m_pGlfwin) != 0;
}

void ClearScreen(Platform * pPlat)
{
    int dX, dY;
    glfwGetFramebufferSize(pPlat->m_pGlfwin, &dX, &dY);

	glfwGetFramebufferSize(pPlat->m_pGlfwin, &dX, &dY);
    glViewport(0, 0, dX, dY);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void SwapBuffers(Platform * pPlat)
{
	glfwSwapBuffers(pPlat->m_pGlfwin);
}

void PollInput(Platform * pPlat, Famicom * pFam)
{
	glfwPollEvents();
}

void ShutdownPlatform(Platform * pPlat)
{
	glfwTerminate();
}

Texture * PTexCreate(Platform * pPlat, s16 dX, s16 dY)
{
	auto pTex = new Texture;
	pTex->m_dX = dX;
	pTex->m_dY = dY;
	pTex->m_pB = new u8[dX * dY * 3];

	glGenTextures(1, (GLuint*)&pTex->m_nId);
	glBindTexture(GL_TEXTURE_2D, pTex->m_nId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	return pTex;
}

void UploadTexture(Platform * pPlat, Texture * pTex)
{
	glBindTexture(GL_TEXTURE_2D, pTex->m_nId);
	glTexImage2D(
		GL_TEXTURE_2D,			// target	
		0,						// level
		GL_RGBA,				// internal format
		pTex->m_dX,				// width
		pTex->m_dY,				// height
		0,						// border
		GL_RGB,					// format
		GL_UNSIGNED_BYTE,		// type
		pTex->m_pB);
}
