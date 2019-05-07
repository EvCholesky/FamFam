#include "imgui.h"
#include "Platform.h"
#include "glfw/include/GLFW/glfw3.h"


static void GlfwKeyCallback(GLFWwindow * pGlfwin, int key, int scancode, int action, int mods)
{
    ImGuiIO& io = ImGui::GetIO();
    if (action == GLFW_PRESS)
        io.KeysDown[key] = true;
    if (action == GLFW_RELEASE)
        io.KeysDown[key] = false;

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

void PollInput(Platform * pPlat)
{
	glfwPollEvents();
}

void ShutdownPlatform(Platform * pPlat)
{
	glfwTerminate();
}