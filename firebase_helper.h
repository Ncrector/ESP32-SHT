#pragma once

String makeSlotName(int slot);
bool writeToFirebasePath(const String& path, const String& json);
bool writeLatestReading(float temperatureF, float humidity, int slot);
bool writeHistoryReading(int slot, float temperatureF, float humidity);
bool writeCurrentSlotMeta(int slot);
int loadCurrentSlotFromFirebase();