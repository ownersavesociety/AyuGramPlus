// This is part of MediaTok (AyuGramPlus fork).
#pragma once

#include "settings/settings_common.h"
#include "settings/settings_common_session.h"

namespace Window {
class SessionController;
} // namespace Window

namespace Settings {

class AyuMediaTok : public Section<AyuMediaTok> {
public:
	AyuMediaTok(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

private:
	void setupContent();
};

[[nodiscard]] Type AyuMediaTokId();

} // namespace Settings
