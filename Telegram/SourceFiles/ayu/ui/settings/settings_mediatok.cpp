// This is part of MediaTok (AyuGramPlus fork).
#include "ayu/ui/settings/settings_mediatok.h"

#include "ayu/ayu_settings.h"
#include "ayu/features/mediatok/mediatok_features.h"
#include "ayu/ui/boxes/edit_mark_box.h"
#include "ayu/ui/settings/ayu_builder.h"
#include "ayu/ui/settings/settings_main.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "ui/boxes/single_choice_box.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include <algorithm>
#include <array>

namespace Settings {

using namespace Builder;
using namespace AyuBuilder;

namespace {

[[nodiscard]] QString MaskGeminiKey(const QString &key) {
	const auto trimmed = key.trimmed();
	if (trimmed.isEmpty()) {
		return u"Не задан"_q;
	}
	if (trimmed.size() <= 10) {
		return u"••••••••"_q;
	}
	return trimmed.left(4) + u"..."_q + trimmed.right(4);
}

void BuildGeminiSection(SectionBuilder &builder) {
	auto *settings = &AyuSettings::getInstance();
	const auto controller = builder.controller();

	builder.addSubsectionTitle(rpl::single(u"Gemini API (.ai)"_q));

	builder.addButton({
		.id = u"mediatok/geminiApiKey"_q,
		.title = rpl::single(u"Gemini API Key"_q),
		.icon = { &st::menuIconBot },
		.label = settings->geminiApiKeyValue() | rpl::map(MaskGeminiKey),
		.onClick = [=] {
			if (!controller) {
				return;
			}
			controller->show(Box<EditMarkBox>(
				rpl::single(u"Gemini API Key"_q),
				AyuSettings::getInstance().geminiApiKey(),
				QString(),
				[=](const QString &value) {
					AyuSettings::getInstance().setGeminiApiKey(value.trimmed());
					controller->showToast(u"MediaTok: Gemini API Key сохранён"_q);
				}));
		},
	});

	builder.addButton({
		.id = u"mediatok/geminiModel"_q,
		.title = rpl::single(u"Модель Gemini"_q),
		.icon = { &st::menuIconEdit },
		.label = settings->geminiModelValue() | rpl::map([](const QString &m) {
			return m.trimmed().isEmpty() ? u"gemini-2.5-flash"_q : m;
		}),
		.onClick = [=] {
			if (!controller) {
				return;
			}
			const auto models = std::vector<QString>{
				u"gemini-2.5-flash"_q,
				u"gemini-2.5-pro"_q,
				u"gemini-3-flash-preview"_q,
				u"gemini-3.1-pro-preview"_q,
				u"gemini-2.0-flash"_q,
				u"Ввести вручную..."_q,
			};
			const auto current = AyuSettings::getInstance().geminiModel().trimmed();
			int initialIndex = 0;
			bool matched = false;
			for (int i = 0; i < 5; ++i) {
				if (current.compare(models[i], Qt::CaseInsensitive) == 0) {
					initialIndex = i;
					matched = true;
					break;
				}
			}
			if (!matched && !current.isEmpty()) {
				initialIndex = 5;
			}
			controller->show(Box([=](not_null<Ui::GenericBox*> box) {
				SingleChoiceBox(box, {
					.title = rpl::single(u"Выбор модели Gemini"_q),
					.options = models,
					.initialSelection = initialIndex,
					.callback = [=](int index) {
						if (index >= 0 && index < 5) {
							AyuSettings::getInstance().setGeminiModel(models[index]);
						} else {
							controller->show(Box<EditMarkBox>(
								rpl::single(u"Модель Gemini (id)"_q),
								AyuSettings::getInstance().geminiModel(),
								u"gemini-2.5-flash"_q,
								[=](const QString &value) {
									const auto trimmed = value.trimmed();
									AyuSettings::getInstance().setGeminiModel(
										trimmed.isEmpty()
											? u"gemini-2.5-flash"_q
											: trimmed);
								}));
						}
					},
				});
			}));
		},
	});

	builder.addSkip();
	builder.addDividerText(rpl::single(
		u"В любом чате: .ai <вопрос> (отправит ответ в чат, можно с реплаем) или .ai! <вопрос> (покажет ответ только вам)."_q));
	builder.addSkip();
}

void BuildMediaAndSpeedSection(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(rpl::single(u"Скорость и видео по ссылкам"_q));

	ayu.addSettingToggle({
		.id = u"mediatok/turboSpeed"_q,
		.title = rpl::single(u"Макс. скорость загрузки и отправки (Turbo)"_q),
		.getter = &AyuSettings::turboSpeed,
		.setter = &AyuSettings::setTurboSpeed,
		.icon = { &st::menuIconRestore },
	});

	ayu.addSettingToggle({
		.id = u"mediatok/autoVideoFromLinks"_q,
		.title = rpl::single(u"Скачивать видео при отправке ссылки"_q),
		.getter = &AyuSettings::autoVideoFromLinks,
		.setter = &AyuSettings::setAutoVideoFromLinks,
		.icon = { &st::menuIconLink },
	});

	builder.addSkip();
	builder.addDividerText(rpl::single(
		u"Turbo разгоняет параллельные MTProto-потоки до максимума (16 DC-сессий). Авто-видео качает ролик через yt-dlp вместо ссылки (для текста используйте .raw <ссылка>)."_q));
	builder.addSkip();
}

void BuildVoicemodSection(SectionBuilder &builder) {
	auto *settings = &AyuSettings::getInstance();
	const auto controller = builder.controller();

	builder.addSubsectionTitle(rpl::single(u"Voicemod (Голосовые и кружки)"_q));

	builder.addButton({
		.id = u"mediatok/voicemodPreset"_q,
		.title = rpl::single(u"Эффект голоса"_q),
		.icon = { &st::menuIconPremium },
		.label = settings->voicemodPresetValue() | rpl::map([](int p) {
			return MediaTok::VoicemodPresetName(p);
		}),
		.onClick = [=] {
			if (!controller) {
				return;
			}
			std::vector<QString> options;
			options.reserve(6);
			for (int i = 0; i < 6; ++i) {
				options.push_back(MediaTok::VoicemodPresetName(i));
			}
			const auto initial = std::clamp(
				AyuSettings::getInstance().voicemodPreset(),
				0,
				5);
			controller->show(Box([=](not_null<Ui::GenericBox*> box) {
				SingleChoiceBox(box, {
					.title = rpl::single(u"Эффект Voicemod"_q),
					.options = options,
					.initialSelection = initial,
					.callback = [=](int index) {
						AyuSettings::getInstance().setVoicemodPreset(index);
					},
				});
			}));
		},
	});

	builder.addSkip();
	builder.addDividerText(rpl::single(
		u"Обработка в реальном времени для ГС и видеосообщений (кружков). Быстрая смена в чате: .vm <off|deep|helium|robot|radio|echo>."_q));
	builder.addSkip();
}

void BuildCleanupSection(SectionBuilder &builder) {
	const auto controller = builder.controller();
	const auto wipeRefresh = std::make_shared<rpl::event_stream<>>();

	builder.addSubsectionTitle(rpl::single(u"Очистка и авто-удаление в ЛС"_q));

	builder.addButton({
		.id = u"mediatok/deleteAllMyMessages"_q,
		.title = rpl::single(u"Удалить все мои сообщения везде"_q),
		.icon = { &st::menuIconDelete },
		.onClick = [=] {
			if (controller) {
				MediaTok::DeleteMyMessagesEverywhere(controller);
			}
		},
	});

	builder.addButton({
		.id = u"mediatok/clearWipeList"_q,
		.title = rpl::single(u"Очистить список .wipe (ЛС)"_q),
		.icon = { &st::menuIconClear },
		.label = wipeRefresh->events_starting_with({}) | rpl::map([] {
			return u"%1 чатов"_q.arg(
				AyuSettings::getInstance().autoDeleteIncomingPeers().size());
		}),
		.onClick = [=] {
			AyuSettings::getInstance().clearAutoDeleteIncomingPeers();
			wipeRefresh->fire({});
			if (controller) {
				controller->showToast(u"MediaTok: авто-удаление входящих выключено для всех ЛС"_q);
			}
		},
	});

	builder.addSkip();
	builder.addDividerText(rpl::single(
		u"Команды в чатах (.mt для справки):\n"
		"• .delme / .delmeall — удалить ваши сообщения (в чате / во всех чатах)\n"
		"• .wipe / .wipe all / .unwipe — мгновенно сносить входящие от собеседника в ЛС для обоих\n"
		"• .dl <ссылка> — принудительно скачать и отправить видео вместо ссылки"_q));
	builder.addSkip();
}

const auto kMeta = BuildHelper({
	.id = AyuMediaTok::Id(),
	.parentId = AyuMain::Id(),
	.title = u"MediaTok (AI, Видео, Войсмод)"_q,
	.icon = &st::menuIconPremium,
}, [](SectionBuilder &builder) {
	auto ayu = AyuSectionBuilder(builder);

	builder.addSkip();
	BuildGeminiSection(builder);
	BuildMediaAndSpeedSection(builder, ayu);
	BuildVoicemodSection(builder);
	BuildCleanupSection(builder);
});

} // namespace

rpl::producer<QString> AyuMediaTok::title() {
	return rpl::single(u"MediaTok: AI и функции"_q);
}

AyuMediaTok::AyuMediaTok(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

void AyuMediaTok::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMeta.build);
	Ui::ResizeFitChild(this, content);
}

Type AyuMediaTokId() {
	return AyuMediaTok::Id();
}

} // namespace Settings
