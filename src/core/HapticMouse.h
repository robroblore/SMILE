#pragma once

#include "core/Tip.h"

namespace HapticMouse {
void requestPulse(TipCategory category);
void playPulse(TipCategory category, int strength, bool logRequest = true);
}
