#pragma once

#include "Common.h"

void reset(s32 width, s32 height);

void on_mouse_button_down_event(int button);
void on_mouse_button_up_event(int button);
void on_mouse_move_event(double x, double y);
void on_arrow_key_down_event(int direction);
void on_arrow_key_up_event(int direction);
