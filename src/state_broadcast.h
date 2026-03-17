#pragma once
#include <Arduino.h>

void stateBroadcastBegin(const char* unicastTarget = nullptr);
void stateBroadcastTick(int state, int frame, const char* mode,
                        float normX = 0.5f, float normY = 0.5f, int direction = 0,
                        int weatherType = -1, float temperature = -999,
                        int moisture = -1, int humidity = -1);

// One-shot broadcasts for desktop sync
void stateBroadcastPixelArt(int size, const char* rows[], int rowCount);
void stateBroadcastChatMsg(const char* role, const char* text);
