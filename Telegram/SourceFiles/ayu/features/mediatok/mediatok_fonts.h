// This is part of MediaTok (AyuGramPlus fork).
#pragma once

#include "api/api_common.h"
#include "base/basic_types.h"

#include <QtCore/QString>

namespace Window {
class SessionController;
} // namespace Window

namespace MediaTok {

void RegisterBundledAndCustomFonts();

[[nodiscard]] QString ActiveChatTextStyleId();
[[nodiscard]] QString ActiveChatTextStyleName();
void SetActiveChatTextStyleId(const QString &styleId);

[[nodiscard]] QString TransformChatText(
	const QString &text,
	const QString &styleId);

void TransformOutgoingMessage(Api::MessageToSend &message);

[[nodiscard]] bool HandleFontCommand(
	not_null<Window::SessionController*> controller,
	Api::MessageToSend &message,
	const QString &cmdLower,
	const QString &rest);

void ShowFontsStudioBox(
	not_null<Window::SessionController*> controller,
	int initialTab = 0);

} // namespace MediaTok
