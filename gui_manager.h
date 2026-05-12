#ifndef GUI_MANAGER_H
#define GUI_MANAGER_H

#include "app_context.h"

void GUIManager_Init(HINSTANCE hi);
void GUIManager_Cleanup();
void GUIManager_UpdateTray();
void GUIManager_ShowStats();
void GUIManager_ToggleFloat();

#endif
