#ifndef UI_WINDOWS_H
#define UI_WINDOWS_H

#include "app_context.h"

// dashboard
void Dashboard_Open(HINSTANCE hi);

// janela flutuante
void FloatingWindow_StartForTarget(const std::string& host);

// janela de logs
void LogWindow_Open(HINSTANCE hi);

#endif
