#pragma once

#include "Common.h"

struct Famicom;

void UpdateDisassemblyWindow(Famicom * pFam, bool * pFShowDisasm);
void UpdateRegisterWindow(Famicom * pFam, bool * pFShowDisasm);