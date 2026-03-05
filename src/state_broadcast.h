#pragma once

void stateBroadcastBegin(const char* unicastTarget = nullptr);
void stateBroadcastTick(int state, int frame, const char* mode,
                        float normX = 0.5f, float normY = 0.5f, int direction = 0);
