// This is part of MediaTok (AyuGramPlus fork).
#pragma once

#include "base/basic_types.h"

#include <QtCore/QString>

namespace Window {
class SessionController;
} // namespace Window

namespace MediaTok {

void ApplySavedLangOverrides();
[[nodiscard]] int LangOverrideCount();

void ShowLanguageStudioBox(
	not_null<Window::SessionController*> controller,
	int initialTab = 0);

} // namespace MediaTok
