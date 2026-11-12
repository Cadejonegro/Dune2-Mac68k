
/* Temporary compatibility symbols for mission-loading tests.
   Replace these with original engine functionality in later stages. */

#include "types.h"

uint8 g_hintsShown1;
uint8 g_hintsShown2;
uint8 g_selectionTypeNew;
uint8 g_starPortEnforceUnitLimit;
uint8 g_viewport_fadein;

void Driver_Sound_Play(void) {}
int Driver_Voice_IsPlaying(void) { return 0; }
void GUI_Production_BuildThis_Click(void) {}
void GUI_Production_Down_Click(void) {}
void GUI_Production_List_Click(void) {}
void GUI_Production_ResumeGame_Click(void) {}
void GUI_Production_Up_Click(void) {}
void GUI_Production_Upgrade_Click(void) {}
void GUI_Purchase_Invoice_Click(void) {}
void GUI_Purchase_Minus_Click(void) {}
void GUI_Purchase_Plus_Click(void) {}
void GUI_Widget_ActionPanel_Draw(void) {}
void GUI_Widget_DrawAll(void) {}
void GUI_Widget_DrawBorder(void) {}
void PrepareEnd(void) {}
void Sound_Output_Feedback(void) {}
void Sound_StartSound(void) {}
void Sound_StartSpeech(void) {}
void Voice_Play(void) {}
void Voice_PlayAtTile(void) {}
