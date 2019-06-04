#pragma once

#include "Common.h"

struct Famicom;
struct Platform;
struct Texture;

struct Debugger // tag = debug
{
					Debugger()
					:m_fShowDisasmWindow(true)
					,m_fShowRegisterWindow(true)
					,m_fShowChrWindow(true)
					,m_pTexChr(nullptr)
					,m_pTexNametable(nullptr)
						{ ; }

	bool			m_fShowDisasmWindow;
	bool			m_fShowRegisterWindow;
	bool			m_fShowChrWindow;
	Texture *		m_pTexChr;
	Texture *		m_pTexNametable;
};

void InitDebugger(Debugger * pDebug, Platform * pPlat);
void UpdateDebugger(Debugger * pDebug, Platform * pPlat, Famicom * pFam);

void UpdateDisassemblyWindow(Famicom * pFam, bool * pFShowWindow);
void UpdateRegisterWindow(Famicom * pFam, bool * pFShowWindow);
void UpdateChrWindow(Debugger * pDebug, Famicom * pFam, bool * pFShowWindow);