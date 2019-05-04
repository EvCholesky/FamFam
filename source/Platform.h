#pragma once

#define EMU_WINDOWS 1
#ifdef EMU_WINDOWS
	#define FF_FOPEN(PFILE, FILENAME, FLAGS)	fopen_s(&PFILE, FILENAME, FLAGS)
#else
	#define FF_FOPEN(PFILE, FILENAME, FLAGS)	PFILE = fopen(FILENAME, FLAGS)
#endif