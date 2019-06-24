#pragma once

#include "Common.h"

struct Famicom;
struct Platform;
struct Texture;

struct PpuWindow // tag = ppuwin
{
					PpuWindow()
					:m_fShowWindow(false)
					,m_fChrNeedsRefresh(true)
					,m_fUse8x16Mode(false)
					,m_pTexChr(nullptr)
						{ ; }

	bool			m_fShowWindow;
	bool			m_fChrNeedsRefresh;
	bool			m_fUse8x16Mode;
	Texture *		m_pTexChr;

};

struct NameTableWindow // tag = ntwin
{
					NameTableWindow()
					:m_fShowWindow(false)
					,m_cFrameRefresh(2)
					,m_pTex(nullptr)
						{ ; }

	bool			m_fShowWindow;
	int				m_cFrameRefresh;
	Texture *		m_pTex;
};

struct Debugger // tag = debug
{
					Debugger()
					:m_fShowDisasmWindow(true)
					,m_fShowRegisterWindow(true)
					,m_pTexNametable(nullptr)
						{ ; }

	bool			m_fShowDisasmWindow;
	bool			m_fShowRegisterWindow;
	Texture *		m_pTexNametable;
	PpuWindow		m_ppuwin;
	NameTableWindow	m_ntwin;
};

void InitDebugger(Debugger * pDebug, Platform * pPlat);
void UpdateDebugger(Debugger * pDebug, Platform * pPlat, Famicom * pFam);

void UpdateDisassemblyWindow(Famicom * pFam, bool * pFShowWindow);
void UpdateRegisterWindow(Famicom * pFam, bool * pFShowWindow);
void UpdateChrWindow(Debugger * pDebug, Famicom * pFam, Platform * pPlat);
void UpdateNameTableWindow(Debugger * pDebug, Famicom * pFam, Platform * pPlat);
void UpdateScreenWindow(Debugger * pDebug, Platform * pPlat, Famicom * pFam);