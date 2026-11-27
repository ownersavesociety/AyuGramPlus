// This is part of MediaTok (AyuGramPlus fork).
#include "ayu/ui/settings/settings_mediatok.h"

#include "ayu/ayu_settings.h"
#include "ayu/features/mediatok/mediatok_features.h"
#include "ayu/features/mediatok/mediatok_fonts.h"
#include "ayu/features/mediatok/mediatok_langs.h"
#include "ayu/features/mediatok/mediatok_waves.h"
#include "ayu/ui/boxes/edit_mark_box.h"
#include "ayu/ui/settings/ayu_builder.h"
#include "ayu/ui/settings/settings_main.h"
#include "base/weak_ptr.h"
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
			const auto clean = m.trimmed();
			return (clean.isEmpty() || clean.compare(u"gemini-4-hydrogen"_q, Qt::CaseInsensitive) == 0)
				? u"gemini-2.5-flash"_q
				: clean;
		}),
		.onClick = [=] {
			if (!controller) {
				return;
			}
			const auto models = std::vector<QString>{
				u"gemini-2.5-flash"_q,
				u"gemini-2.5-pro"_q,
				u"gemini-2.5-flash-lite"_q,
				u"gemini-3-flash-preview"_q,
				u"gemini-3.1-pro-preview"_q,
				u"Ввести вручную..."_q,
			};
			const int presetCount = int(models.size()) - 1;
			auto current = AyuSettings::getInstance().geminiModel().trimmed();
			if (current.compare(u"gemini-4-hydrogen"_q, Qt::CaseInsensitive) == 0) {
				current = u"gemini-2.5-flash"_q;
			}
			int initialIndex = 0;
			bool matched = false;
			for (int i = 0; i < presetCount; ++i) {
				if (current.compare(models[i], Qt::CaseInsensitive) == 0) {
					initialIndex = i;
					matched = true;
					break;
				}
			}
			if (!matched && !current.isEmpty()) {
				initialIndex = presetCount;
			}
			const auto weak = base::make_weak(controller);
			controller->show(Box([=](not_null<Ui::GenericBox*> box) {
				SingleChoiceBox(box, {
					.title = rpl::single(u"Выбор модели Gemini"_q),
					.options = models,
					.initialSelection = initialIndex,
					.callback = [=](int index) {
						if (index >= 0 && index < presetCount) {
							AyuSettings::getInstance().setGeminiModel(models[index]);
						} else if (const auto strong = weak.get()) {
							strong->show(Box<EditMarkBox>(
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

	builder.addButton({
		.id = u"mediatok/fetchGeminiModels"_q,
		.title = rpl::single(u"Загрузить модели из Gemini API (по ключу)"_q),
		.icon = { &st::menuIconBot },
		.onClick = [=] {
			if (!controller) {
				return;
			}
			const auto weak = base::make_weak(controller);
			controller->showToast(u"Gemini: загружаю список моделей..."_q);
			MediaTok::FetchGeminiModels(controller, [=](
					std::vector<QString> fetched,
					QString error) {
				const auto strong = weak.get();
				if (!strong) {
					return;
				}
				if (!error.isEmpty()) {
					strong->showToast(u"Gemini: %1"_q.arg(error));
					return;
				}
				const auto current = AyuSettings::getInstance().geminiModel().trimmed();
				int initialIndex = 0;
				for (int i = 0; i < int(fetched.size()); ++i) {
					if (current.compare(fetched[i], Qt::CaseInsensitive) == 0) {
						initialIndex = i;
						break;
					}
				}
				strong->show(Box([=](not_null<Ui::GenericBox*> box) {
					SingleChoiceBox(box, {
						.title = rpl::single(u"Модели из вашего Gemini API"_q),
						.options = fetched,
						.initialSelection = initialIndex,
						.callback = [=](int index) {
							if (index >= 0 && index < int(fetched.size())) {
								AyuSettings::getInstance().setGeminiModel(fetched[index]);
								if (const auto s = weak.get()) {
									s->showToast(
										u"MediaTok: выбрана модель %1"_q.arg(fetched[index]));
								}
							}
						},
					});
				}));
			});
		},
	});

	builder.addButton({
		.id = u"mediatok/geminiSystemPrompt"_q,
		.title = rpl::single(u"Системный промпт Gemini (роль/стиль)"_q),
		.icon = { &st::menuIconEdit },
		.label = settings->geminiSystemPromptValue() | rpl::map([](const QString &p) {
			const auto t = p.trimmed();
			return t.isEmpty() ? u"По умолчанию"_q : (t.size() > 24 ? t.left(24) + u"..."_q : t);
		}),
		.onClick = [=] {
			if (!controller) {
				return;
			}
			controller->show(Box<EditMarkBox>(
				rpl::single(u"Системный промпт для Gemini"_q),
				AyuSettings::getInstance().geminiSystemPrompt(),
				QString(),
				[=](const QString &value) {
					AyuSettings::getInstance().setGeminiSystemPrompt(value.trimmed());
				}));
		},
	});

	builder.addButton({
		.id = u"mediatok/testGemini"_q,
		.title = rpl::single(u"Проверить Gemini API Key"_q),
		.icon = { &st::menuIconShowAll },
		.onClick = [=] {
			if (controller) {
				MediaTok::TestGeminiApi(controller);
			}
		},
	});

	builder.addSkip();
	builder.addDividerText(rpl::single(
		u"В любом чате: .ai / .ai! (видит цепочку реплаев, фото и аудио), .tldr [N] (выжимка), .tr <язык>, .fix, .tone <стиль>. На ГС/кружках работает «Расшифровать (AI)»."_q));
	builder.addSkip();
}

void BuildMediaAndSpeedSection(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	const auto controller = builder.controller();
	const auto toolsRefresh = std::make_shared<rpl::event_stream<>>();

	builder.addSubsectionTitle(rpl::single(u"Скорость, видео и кружки"_q));

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

	builder.addButton({
		.id = u"mediatok/downloadVideoTools"_q,
		.title = rpl::single(u"Установить yt-dlp + ffmpeg (для 1080p и кружков)"_q),
		.icon = { &st::menuIconRestore },
		.label = toolsRefresh->events_starting_with({}) | rpl::map([] {
			const auto yt = MediaTok::HasYtDlpTool() ? u"OK"_q : u"Нет"_q;
			const auto ff = MediaTok::HasFfmpegTool() ? u"OK"_q : u"Нет"_q;
			return u"yt-dlp: %1 | ffmpeg: %2"_q.arg(yt, ff);
		}),
		.onClick = [=] {
			if (controller) {
				MediaTok::DownloadVideoTools(controller, [=] {
					toolsRefresh->fire({});
				});
			}
		},
	});

	builder.addSkip();
	builder.addDividerText(rpl::single(
		u"Turbo разгоняет параллельные MTProto-потоки до 16 DC-сессий. С ffmpeg видео качается в 1080p HD и работает .circle <ссылка> (видео -> кружок)."_q));
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
			options.reserve(MediaTok::kVoicemodPresetCount);
			for (int i = 0; i < MediaTok::kVoicemodPresetCount; ++i) {
				options.push_back(MediaTok::VoicemodPresetName(i));
			}
			const auto initial = std::clamp(
				AyuSettings::getInstance().voicemodPreset(),
				0,
				MediaTok::kVoicemodPresetCount - 1);
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
		u"Обработка в реальном времени (окно Ханна, без щелчков) для ГС и кружков. В чате: .vm <off|deep|helium|robot|radio|echo|anon|bass>."_q));
	builder.addSkip();
}

void BuildCleanupSection(SectionBuilder &builder) {
	const auto controller = builder.controller();
	const auto wipeRefresh = std::make_shared<rpl::event_stream<>>();
	const auto autoDelRefresh = std::make_shared<rpl::event_stream<>>();
	const auto notesRefresh = std::make_shared<rpl::event_stream<>>();

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
		.id = u"mediatok/clearAutoDelList"_q,
		.title = rpl::single(u"Сбросить таймеры .autodel"_q),
		.icon = { &st::menuIconTimer },
		.label = autoDelRefresh->events_starting_with({}) | rpl::map([] {
			return u"%1 чатов"_q.arg(
				AyuSettings::getInstance().autoDeleteOutgoingPeers().size());
		}),
		.onClick = [=] {
			AyuSettings::getInstance().clearAutoDeleteOutgoingPeers();
			autoDelRefresh->fire({});
			if (controller) {
				controller->showToast(u"MediaTok: все таймеры .autodel сброшены"_q);
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

	builder.addButton({
		.id = u"mediatok/clearPeerNotes"_q,
		.title = rpl::single(u"Очистить личные заметки о контактах"_q),
		.icon = { &st::menuIconEdit },
		.label = notesRefresh->events_starting_with({}) | rpl::map([] {
			return u"%1 заметок"_q.arg(
				AyuSettings::getInstance().peerNotes().size());
		}),
		.onClick = [=] {
			AyuSettings::getInstance().clearPeerNotes();
			notesRefresh->fire({});
			if (controller) {
				controller->showToast(u"MediaTok: личные заметки очищены"_q);
			}
		},
	});

	builder.addButton({
		.id = u"mediatok/commandPalette"_q,
		.title = rpl::single(u"Палитра команд по категориям (.cmd)"_q),
		.icon = { &st::menuIconShowAll },
		.onClick = [=] {
			if (controller) {
				MediaTok::ShowCommandPaletteBox(controller);
			}
		},
	});

	builder.addButton({
		.id = u"mediatok/onlineUpscaler"_q,
		.title = rpl::single(u"Онлайн AI-апскейл фото (Real-ESRGAN / iLoveIMG / Bigjpg)"_q),
		.icon = { &st::menuIconPhoto },
		.onClick = [=] {
			if (controller) {
				MediaTok::ShowOnlineUpscalerBox(controller);
			}
		},
	});

	builder.addButton({
		.id = u"mediatok/afkResponder"_q,
		.title = rpl::single(u"AFK-автоответчик в ЛС (.afk)"_q),
		.icon = { &st::menuIconTimer },
		.onClick = [=] {
			if (controller) {
				MediaTok::ShowAfkConfigBox(controller);
			}
		},
	});

	builder.addButton({
		.id = u"mediatok/wavesHub"_q,
		.title = rpl::single(u"Инструменты Waves 19–25 (.waves)"_q),
		.icon = { &st::menuIconBot },
		.onClick = [=] {
			if (controller) {
				MediaTok::ShowWavesHubBox(controller);
			}
		},
	});

	builder.addButton({
		.id = u"mediatok/showHelp"_q,
		.title = rpl::single(u"Шпаргалка по всем командам (.mt)"_q),
		.icon = { &st::menuIconInfo },
		.onClick = [=] {
			if (controller) {
				MediaTok::ShowHelpBox(controller);
			}
		},
	});

	builder.addSkip();
	builder.addDividerText(rpl::single(
		u"Команды (.mt): .ai/.ai!, .tldr [N], .tr <яз>, .fix, .tone <стиль>, .circle/.gif/.voice/.mp3/.dl [таймкод] [скорость x], .delme [N|период], .autodel <время>, .wipe/.unwipe, .vm."_q));
	builder.addSkip();
}

void BuildFontsSection(SectionBuilder &builder) {
	const auto controller = builder.controller();

	builder.addSubsectionTitle(rpl::single(u"Шрифты клиента и текста сообщений"_q));

	builder.addButton({
		.id = u"mediatok/fontsStudio"_q,
		.title = rpl::single(u"Студия шрифтов MediaTok (80+ клиента • .ttf редактор • 32 стиля чата)"_q),
		.icon = { &st::menuIconFont },
		.label = rpl::single(MediaTok::ActiveChatTextStyleId().isEmpty()
			? u"80+ шрифтов"_q
			: MediaTok::ActiveChatTextStyleName()),
		.onClick = [=] {
			if (controller) {
				MediaTok::ShowFontsStudioBox(controller, 0);
			}
		},
	});

	builder.addButton({
		.id = u"mediatok/ttfCreator"_q,
		.title = rpl::single(u"Создать свой шрифт (.ttf) — рисовать буквы"_q),
		.icon = { &st::menuIconEdit },
		.label = rpl::single(u"Редактор .ttf"_q),
		.onClick = [=] {
			if (controller) {
				MediaTok::ShowTtfCreatorBox(controller);
			}
		},
	});

	builder.addButton({
		.id = u"mediatok/chatTextFonts"_q,
		.title = rpl::single(u"Стили текста сообщений (видят все в Telegram)"_q),
		.icon = { &st::menuIconEdit },
		.label = rpl::single(MediaTok::ActiveChatTextStyleName()),
		.onClick = [=] {
			if (controller) {
				MediaTok::ShowFontsStudioBox(controller, 1);
			}
		},
	});

	builder.addButton({
		.id = u"mediatok/langStudio"_q,
		.title = rpl::single(u"Языки MediaTok (66 паков ТГ + фулл-редактор ВСЕХ 8669 фраз)"_q),
		.icon = { &st::menuIconShowAll },
		.label = rpl::single(MediaTok::LangOverrideCount() > 0
			? u"Правок: %1"_q.arg(MediaTok::LangOverrideCount())
			: u"66 паков + Фулл"_q),
		.onClick = [=] {
			if (controller) {
				MediaTok::ShowLanguageStudioBox(controller, 0);
			}
		},
	});

	builder.addSkip();
	builder.addDividerText(rpl::single(
		u"80 встроенных кириллических шрифтов + свой редактор .ttf + импорт .ttf/.otf + .color (цветной текст-стикер) + каталог 66 языков ТГ и полный редактор всех 8669 фраз (.font, .f, .color, .lang)."_q));
	builder.addSkip();
}

const auto kMeta = BuildHelper({
	.id = AyuMediaTok::Id(),
	.parentId = AyuMain::Id(),
	.title = u"MediaTok (AI, Шрифты, Видео, Войсмод)"_q,
	.icon = &st::menuIconPremium,
}, [](SectionBuilder &builder) {
	auto ayu = AyuSectionBuilder(builder);

	builder.addSkip();
	BuildFontsSection(builder);
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
