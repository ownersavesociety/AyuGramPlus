// This is part of MediaTok (AyuGramPlus fork).
#include "ayu/features/mediatok/mediatok_langs.h"

#include "core/application.h"
#include "lang/lang_cloud_manager.h"
#include "lang/lang_instance.h"
#include "lang_auto_counts.h"
#include "settings/settings_common.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>

#include <array>
#include <map>
#include <vector>

namespace MediaTok {
namespace {

struct PublicLangPack {
	const char *slug;
	const char *title;
	const char *desc;
	const char *category;
};

constexpr std::array<PublicLangPack, 66> kPublicPacks = {{
	// Мемы и сленг
	{ "therussianslang", "Сленг", "Разговорный молодёжный сленг", "Мемы и сленг" },
	{ "idontno1", "Смешной без мата", "Мега смешной перевод без мата", "Мемы и сленг" },
	{ "userfriendly", "Несерьёзный", "Меньше официальности, больше кайфа", "Мемы и сленг" },
	{ "rusmltrs", "всё с маленькой буквы", "Эстетичный минимализм без заглавных", "Мемы и сленг" },
	{ "mymylang", "вацап.", "Маленькими буквами и с точкой.", "Мемы и сленг" },
	{ "iseeyouforevernow", "оно говорит со мной?", "Криповый сломанный стиль", "Мемы и сленг" },
	{ "sadnesssad", "Печальный язык", "Грустный вайб для всего ТГ", "Мемы и сленг" },
	{ "shizoid", "Шизоид", "Ироничный мемный интерфейс", "Мемы и сленг" },
	{ "abobusgram", "Абобусграм", "Мемный стиль Абобус", "Мемы и сленг" },
	{ "draining", "Drain / Дед-инсайд", "Мрачный дрейн-стиль", "Мемы и сленг" },
	{ "tokianghoul", "Ghoul (Дед инсайд)", "Для настоящих гулей 1000-7", "Мемы и сленг" },
	{ "piratesss", "PiRaTeS", "Пиратский стиль интерфейса", "Мемы и сленг" },
	{ "goblincoore", "Goblincore", "Лёгкий язык с гоблинами без мата", "Мемы и сленг" },
	{ "akademiad", "Dark Academia", "Тёмная академия и эстетика", "Мемы и сленг" },
	{ "churchwex", "Церковный", "Церковно-духовный стиль", "Мемы и сленг" },
	{ "udiikwoheh", "Нефорский", "Альтернативный сленг", "Мемы и сленг" },
	{ "syrncorelanguage", "Русский + секунды", "Точное время с секундами", "Мемы и сленг" },
	{ "teamgram", "Tgram (уютный + сек.)", "Уютный перевод с секундами", "Мемы и сленг" },
	{ "tmngram", "ТюменьГрам", "Шуточный региональный перевод", "Мемы и сленг" },
	{ "russianbyg02m0n1k", "Шуточный от G02m0n1k", "Известные фразы и шутки", "Мемы и сленг" },

	// Котики и милые
	{ "kiska", "Киска / Кошачий", "Мяу-интерфейс и котики", "Котики и милые" },
	{ "catttish", "Catttish", "Кошачий сленг", "Котики и милые" },
	{ "meowtg", "MeowTG", "Мурлыкающий Telegram", "Котики и милые" },
	{ "liffan", "Kotiki", "Язык с котиками от Liffan", "Котики и милые" },
	{ "krutyshka", "Крутышка и котики", "Милый стиль с котиками", "Котики и милые" },
	{ "fluffyru", "Пуфыстенький", "Фуррёвый мягкий перевод", "Котики и милые" },
	{ "burviss", "Nyan", "Котики и лёгкий сленг :з", "Котики и милые" },
	{ "ruwussian", "РУвУсский (UwU)", "Милый UwU / аниме-стиль", "Котики и милые" },
	{ "angeltoangels", "Angel Tongue", "Ангелочки, котики, каомодзи", "Котики и милые" },
	{ "sad0vira1", "Садовый рай", "Цветочная тематика без мата", "Котики и милые" },
	{ "krasotcka", "Tigers", "Для любителей тигров", "Котики и милые" },

	// Игры
	{ "stalkeryazik", "S.T.A.L.K.E.R.", "Call of The Zone (Зона)", "Игры" },
	{ "atomicheart20", "Atomic Heart", "Вселенная игры Atomic Heart", "Игры" },
	{ "modernwarfare", "Call of Duty: MW", "Стиль оригинальной трилогии MW", "Игры" },
	{ "borderland", "Borderlands", "Язык Пандоры и Искателей", "Игры" },
	{ "binding-of-isaac", "The Binding of Isaac", "Абадоооон / стиль Айзека", "Игры" },
	{ "genshin-8", "Genshin Impact", "Перевод по Геншину", "Игры" },
	{ "ultimatehope228", "Danganronpa", "Отчаяние и надежда", "Игры" },

	// Аниме и кино
	{ "naturo", "Наруто (Синоби)", "Перевод по вселенной Наруто", "Аниме и кино" },
	{ "evang3li0n", "Evangelion", "По Евангелиону без мата", "Аниме и кино" },
	{ "jjba", "JoJo's Bizarre Adv.", "Аниме-стиль ДжоДжо", "Аниме и кино" },
	{ "bleach", "Bleach", "Аниме-стиль Блич", "Аниме и кино" },
	{ "tokiiiio", "Токийские мстители", "Перевод по Tokyo Revengers", "Аниме и кино" },
	{ "bungoustraydogslang", "Бродячие псы", "Bungou Stray Dogs", "Аниме и кино" },
	{ "middle-earths", "Средиземье", "Властелин колец и Хоббит", "Аниме и кино" },
	{ "marvel-ru", "Marvel", "Вселенная супергероев Marvel", "Аниме и кино" },
	{ "fightclublmf", "Бойцовский клуб", "Первое правило клуба...", "Аниме и кино" },
	{ "223222", "Пираты Карибского моря", "Йо-хо-хо и ром", "Аниме и кино" },
	{ "supernaturallanguage", "Сверхъестественное", "Для фанатов Винчестеров", "Аниме и кино" },
	{ "katamaranovian", "Внутри Лапенко", "Катамарановский стиль", "Аниме и кино" },
	{ "metal-femily", "Metal Family", "Перевод по Metal Family", "Аниме и кино" },

	// Исторические и коды
	{ "oldrus111", "Дореформенный Русскій", "ѣ, ѳ, і и твёрдые знаки", "Исторические и коды" },
	{ "classicrussian", "Русскiй (царский)", "Дореволюционная стилистика", "Исторические и коды" },
	{ "vahtang", "Старославянский", "Древнерусский слог", "Исторические и коды" },
	{ "glagol", "Глаголица", "Первый славянский алфавит", "Исторические и коды" },
	{ "aprelteam", "Советский (СССР)", "Товарищи и комсомол", "Исторические и коды" },
	{ "ilovecp1251", "бНОПНЯ (CP1251)", "Сломанная кодировка из 2000-х", "Исторические и коды" },
	{ "koi7codemap", "KOI-7 H1", "Ретро-транслит кодировка", "Исторические и коды" },

	// 18+ приколы
	{ "dblogrm", "DebiloGram (18+)", "Мемный перевод с матом", "18+ приколы" },
	{ "ebllogram-ru", "Еблограм (18+)", "Легендарный Еблограм", "18+ приколы" },
	{ "dlgram", "Долбограм (18+)", "Жёсткий сленг и угар", "18+ приколы" },
	{ "piizdosgram", "Пиздосграм (18+)", "Поэзия из матов", "18+ приколы" },
	{ "dpxlwop", "Ахуительный язык (18+)", "Прикольчики и мат", "18+ приколы" },
	{ "govor", "ТоксикРашн (18+)", "Максимальная токсичность", "18+ приколы" },
	{ "chillish", "Chill language (18+)", "Шуточный перевод на чилле", "18+ приколы" },
	{ "poshlatina343", "Премиум телега (18+)", "Забавный перевод + секунды", "18+ приколы" },
}};

struct PopularKey {
	const char *key;
	const char *label;
};

constexpr std::array<PopularKey, 42> kPopularKeys = {{
	{ "lng_status_online", "В сети" },
	{ "lng_status_recently", "Был(а) недавно" },
	{ "lng_status_last_week", "Был(а) на этой неделе" },
	{ "lng_status_last_month", "Был(а) в этом месяце" },
	{ "lng_status_offline", "Был(а) давно" },
	{ "lng_status_lastseen_now", "Был(а) только что" },
	{ "lng_status_lastseen_today", "Был(а) сегодня в {time}" },
	{ "lng_status_lastseen_yesterday", "Был(а) вчера в {time}" },
	{ "lng_status_lastseen_date", "Был(а) {date}" },
	{ "lng_typing", "Печатает" },
	{ "lng_user_typing", "{user} печатает" },
	{ "lng_send_action_record_audio", "Записывает голосовое" },
	{ "lng_send_action_record_round", "Записывает видеосообщение" },
	{ "lng_send_action_upload_photo", "Отправляет фото" },
	{ "lng_send_action_upload_video", "Отправляет видео" },
	{ "lng_saved_messages", "Избранное" },
	{ "lng_filters_all", "Все чаты" },
	{ "lng_message_ph", "Написать сообщение..." },
	{ "lng_send_button", "Отправить" },
	{ "lng_context_reply_msg", "Ответить" },
	{ "lng_context_forward_msg", "Переслать" },
	{ "lng_context_copy_text", "Копировать текст" },
	{ "lng_context_edit_msg", "Изменить" },
	{ "lng_context_delete_msg", "Удалить" },
	{ "lng_context_delete_selected", "Удалить выбранные" },
	{ "lng_context_pin_msg", "Закрепить" },
	{ "lng_pinned_message", "Закреплённое сообщение" },
	{ "lng_edited", "Изменено" },
	{ "lng_from_draft", "Черновик" },
	{ "lng_from_you", "Вы" },
	{ "lng_menu_settings", "Настройки" },
	{ "lng_menu_contacts", "Контакты" },
	{ "lng_menu_calls", "Звонки" },
	{ "lng_menu_night_mode", "Ночной режим" },
	{ "lng_dlg_search_in", "Поиск сообщений" },
	{ "lng_deleted", "Удалённый аккаунт" },
	{ "lng_status_bot", "Бот" },
	{ "lng_profile_block_user", "Заблокировать" },
	{ "lng_profile_unblock_user", "Разблокировать" },
	{ "lng_create_group_title", "Новая группа" },
	{ "lng_create_channel_title", "Новый канал" },
	{ "lng_save_file", "Сохранить файл" },
}};

struct PresetEntry {
	const char *name;
	std::vector<std::pair<QString, QString>> items;
};

[[nodiscard]] const std::vector<PresetEntry> &Presets() {
	static const std::vector<PresetEntry> list = {
		{
			"Живой / Умер 💀",
			{
				{ u"lng_status_online"_q, u"живой 🟢"_q },
				{ u"lng_status_recently"_q, u"умер 💀"_q },
				{ u"lng_status_last_week"_q, u"сдох на неделе 💀"_q },
				{ u"lng_status_last_month"_q, u"разлагается месяц 🪦"_q },
				{ u"lng_status_offline"_q, u"давно в гробу ⚰️"_q },
				{ u"lng_status_lastseen_now"_q, u"только что дышал"_q },
				{ u"lng_status_lastseen_today"_q, u"был живой сегодня в {time}"_q },
				{ u"lng_status_lastseen_yesterday"_q, u"умер вчера в {time}"_q },
				{ u"lng_status_lastseen_date"_q, u"умер {date}"_q },
				{ u"lng_typing"_q, u"строчит бред"_q },
				{ u"lng_user_typing"_q, u"{user} строчит бред"_q },
				{ u"lng_send_action_record_audio"_q, u"ноет в микро"_q },
				{ u"lng_send_action_record_round"_q, u"кривляется на камеру"_q },
				{ u"lng_send_action_upload_photo"_q, u"кидает фотку"_q },
				{ u"lng_send_action_upload_video"_q, u"кидает видос"_q },
				{ u"lng_saved_messages"_q, u"Тайник"_q },
				{ u"lng_filters_all"_q, u"Все тёрки"_q },
				{ u"lng_message_ph"_q, u"Чиркани чё-нить..."_q },
				{ u"lng_send_button"_q, u"Кинуть"_q },
				{ u"lng_context_reply_msg"_q, u"Ответить по факту"_q },
				{ u"lng_context_forward_msg"_q, u"Слить"_q },
				{ u"lng_context_copy_text"_q, u"Скопипастить"_q },
				{ u"lng_context_edit_msg"_q, u"Переобуться"_q },
				{ u"lng_context_delete_msg"_q, u"Сжечь"_q },
				{ u"lng_context_pin_msg"_q, u"Прибить"_q },
				{ u"lng_pinned_message"_q, u"Прибитое"_q },
				{ u"lng_edited"_q, u"переобулся"_q },
				{ u"lng_from_draft"_q, u"Недописал"_q },
				{ u"lng_menu_settings"_q, u"Крутилки"_q },
				{ u"lng_menu_contacts"_q, u"Кенты"_q },
				{ u"lng_menu_calls"_q, u"Звонилка"_q },
				{ u"lng_deleted"_q, u"Призрак 👻"_q },
				{ u"lng_status_bot"_q, u"железяка 🤖"_q },
				{ u"lng_profile_block_user"_q, u"В бан"_q },
				{ u"lng_profile_unblock_user"_q, u"Помиловать"_q },
			}
		},
		{
			"Киберпанк 2077 ⚡",
			{
				{ u"lng_status_online"_q, u"[ONLINE // ACTIVE]"_q },
				{ u"lng_status_recently"_q, u"[OFFLINE // SIGNAL LOST]"_q },
				{ u"lng_status_last_week"_q, u"[OFFLINE > 7D]"_q },
				{ u"lng_status_last_month"_q, u"[OFFLINE > 30D]"_q },
				{ u"lng_status_offline"_q, u"[NODE TERMINATED]"_q },
				{ u"lng_typing"_q, u"[INJECTING PAYLOAD]"_q },
				{ u"lng_send_action_record_audio"_q, u"[AUDIO STREAM]"_q },
				{ u"lng_send_action_record_round"_q, u"[VIDEO FEED]"_q },
				{ u"lng_saved_messages"_q, u"VAULT // LOCAL"_q },
				{ u"lng_filters_all"_q, u"ALL NODES"_q },
				{ u"lng_message_ph"_q, u"> input_command..."_q },
				{ u"lng_send_button"_q, u"TRANSMIT"_q },
				{ u"lng_context_delete_msg"_q, u"PURGE"_q },
				{ u"lng_edited"_q, u"patched"_q },
				{ u"lng_menu_settings"_q, u"SYS_CONFIG"_q },
				{ u"lng_menu_contacts"_q, u"NET_PEERS"_q },
				{ u"lng_deleted"_q, u"GHOST_NODE"_q },
				{ u"lng_status_bot"_q, u"AI_DAEMON"_q },
			}
		},
		{
			"Царскій 👑",
			{
				{ u"lng_status_online"_q, u"въ сѣти"_q },
				{ u"lng_status_recently"_q, u"почивалъ намедни"_q },
				{ u"lng_status_last_week"_q, u"не видать съ седмицы"_q },
				{ u"lng_status_offline"_q, u"канулъ въ Лету"_q },
				{ u"lng_typing"_q, u"изволитъ писать"_q },
				{ u"lng_send_action_record_audio"_q, u"глаголетъ въ трубу"_q },
				{ u"lng_saved_messages"_q, u"Сундукъ"_q },
				{ u"lng_message_ph"_q, u"Начертать грамоту..."_q },
				{ u"lng_send_button"_q, u"Отправити"_q },
				{ u"lng_context_reply_msg"_q, u"Отвѣтствовать"_q },
				{ u"lng_context_delete_msg"_q, u"Предати огню"_q },
				{ u"lng_menu_settings"_q, u"Уложеніе"_q },
				{ u"lng_menu_contacts"_q, u"Знакомцы"_q },
				{ u"lng_deleted"_q, u"Усопшій аккъ"_q },
			}
		},
		{
			"Кошачий / Мяу 🐾",
			{
				{ u"lng_status_online"_q, u"мурлычет в сети 🐱"_q },
				{ u"lng_status_recently"_q, u"спит на батарее 💤"_q },
				{ u"lng_status_last_week"_q, u"ушёл гулять по крышам"_q },
				{ u"lng_status_offline"_q, u"убежал в которай"_q },
				{ u"lng_typing"_q, u"тыгыдыкает по клаве"_q },
				{ u"lng_send_action_record_audio"_q, u"мяукает в микро"_q },
				{ u"lng_saved_messages"_q, u"Коробка 📦"_q },
				{ u"lng_message_ph"_q, u"Мяукнуть..."_q },
				{ u"lng_send_button"_q, u"Мяу!"_q },
				{ u"lng_context_delete_msg"_q, u"Закопать"_q },
				{ u"lng_edited"_q, u"перемяукал"_q },
				{ u"lng_menu_settings"_q, u"Когтеточка"_q },
				{ u"lng_menu_contacts"_q, u"Котики 🐾"_q },
				{ u"lng_deleted"_q, u"Призрачный кот"_q },
			}
		},
	};
	return list;
}

std::map<QString, QString> gOverrides;
bool gLoaded = false;
bool gSubscribed = false;
bool gApplying = false;
std::vector<QString> gAllKeys;

[[nodiscard]] QString LangFilePath() {
	return cWorkingDir() + u"tdata/MediaTok_Fonts/lang_overrides.json"_q;
}

void EnsureAllKeysLoaded() {
	if (!gAllKeys.empty()) return;
	QFile f(u":/mediatok/fonts/mt_lang_keys.txt"_q);
	if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
		const auto all = QString::fromUtf8(f.readAll()).split(u'\n', Qt::SkipEmptyParts);
		gAllKeys.reserve(size_t(all.size()));
		for (const auto &line : all) {
			const auto k = line.trimmed();
			if (!k.isEmpty()) gAllKeys.push_back(k);
		}
	}
}

void LoadOverridesFromDisk() {
	if (gLoaded) return;
	gLoaded = true;
	QFile f(LangFilePath());
	if (!f.open(QIODevice::ReadOnly)) return;
	const auto doc = QJsonDocument::fromJson(f.readAll());
	if (!doc.isObject()) return;
	const auto obj = doc.object();
	for (auto it = obj.begin(); it != obj.end(); ++it) {
		const auto val = it.value().toString();
		if (!it.key().isEmpty() && !val.isEmpty()) {
			gOverrides[it.key()] = val;
		}
	}
}

void SaveOverridesToDisk() {
	QDir().mkpath(cWorkingDir() + u"tdata/MediaTok_Fonts"_q);
	QJsonObject obj;
	for (const auto &[k, v] : gOverrides) {
		obj.insert(k, v);
	}
	QFile f(LangFilePath());
	if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
	}
}

void ApplyCurrentOverridesToLangPack() {
	if (gApplying || gOverrides.empty()) return;
	gApplying = true;
	auto &inst = Lang::GetInstance();
	for (const auto &[k, v] : gOverrides) {
		inst.applyValue(k.toUtf8(), v.toUtf8());
	}
	gApplying = false;
}

[[nodiscard]] QString CurrentLangValue(const QString &key) {
	using namespace Lang;
	const auto keyLatin = key.toLatin1();
	const auto idx = GetKeyIndex(QLatin1String(keyLatin.constData(), keyLatin.size()));
	if (idx != kKeysCount) {
		return GetInstance().getValue(idx);
	}
	return QString();
}

void PromptRestartForLang(not_null<Window::SessionController*> controller) {
	controller->show(Ui::MakeConfirmBox({
		.text = u"Фразы применены! Перезапустить MediaTok сейчас, чтобы обновились все открытые кнопки и вкладки?"_q,
		.confirmed = [=](Fn<void()> &&close) {
			close();
			Core::Restart();
		},
		.confirmText = u"Перезапустить"_q,
		.cancelText = u"Позже"_q,
	}));
}

} // namespace

void ApplySavedLangOverrides() {
	LoadOverridesFromDisk();
	ApplyCurrentOverridesToLangPack();
	if (!gSubscribed) {
		gSubscribed = true;
		static rpl::lifetime life;
		Lang::GetInstance().updated() | rpl::on_next([] {
			ApplyCurrentOverridesToLangPack();
		}, life);
	}
}

int LangOverrideCount() {
	LoadOverridesFromDisk();
	return int(gOverrides.size());
}

void ShowLanguageStudioBox(
		not_null<Window::SessionController*> controller,
		int initialTab) {
	LoadOverridesFromDisk();
	EnsureAllKeysLoaded();
	const auto weak = base::make_weak(controller);

	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setWidth(st::boxWideWidth);
		box->setTitle(rpl::single(u"Языки MediaTok (66 паков • Фулл-редактор 8669 фраз)"_q));

		struct State {
			int tab = 0;      // 0 = Catalog+Presets, 1 = Full Editor, 2 = Export/Import
			int filter = 0;   // 0 = Popular, 1 = ALL 8669 keys, 2 = Modified only
			QString query;
		};
		const auto state = box->lifetime().make_state<State>();
		state->tab = std::clamp(initialTab, 0, 2);

		const auto topWrap = box->setPinnedToTopContent(
			object_ptr<Ui::VerticalLayout>(box));

		const auto modeLabel = [=] {
			if (state->tab == 0) return u"Каталог ТГ (66) + Пресеты  →  нажми для Фулл-редактора"_q;
			if (state->tab == 1) return u"Фулл-редактор (все %1 фраз)  →  нажми для Экспорта/Импорта"_q.arg(int(gAllKeys.size()));
			return u"Экспорт / Импорт (.mtlang)  →  нажми для Каталога"_q;
		};
		const auto modeVar = box->lifetime().make_state<rpl::variable<QString>>(modeLabel());
		const auto modeBtn = Settings::AddButtonWithLabel(
			topWrap,
			rpl::single(u"Раздел"_q),
			modeVar->value(),
			st::settingsButton,
			{ &st::menuIconEdit });

		const auto filterLabel = [=] {
			if (state->filter == 0) return u"Хиты (42) → переключить на ВСЕ %1 фраз"_q.arg(int(gAllKeys.size()));
			if (state->filter == 1) return u"ВСЕ %1 фраз → переключить на Изменённые (%2)"_q.arg(int(gAllKeys.size())).arg(int(gOverrides.size()));
			return u"Только изменённые (%1) → переключить на Хиты"_q.arg(int(gOverrides.size()));
		};
		const auto filterVar = box->lifetime().make_state<rpl::variable<QString>>(filterLabel());
		const auto filterBtn = Settings::AddButtonWithLabel(
			topWrap,
			rpl::single(u"Фильтр фраз"_q),
			filterVar->value(),
			st::settingsButton);

		const auto searchField = topWrap->add(
			object_ptr<Ui::InputField>(
				topWrap,
				st::defaultInputField,
				rpl::single(u"Поиск языка или ЛЮБОЙ фразы (в сети, удалить, архив...)"_q)),
			QMargins(16, 4, 16, 8));

		const auto listWrap = box->verticalLayout()->add(
			object_ptr<Ui::VerticalLayout>(box->verticalLayout()));

		const auto rebuild = std::make_shared<Fn<void()>>();

		const auto openEditPhraseBox = [=](const QString &key, const QString &hintLabel) {
			const auto c = weak.get();
			if (!c) return;
			const auto curVal = CurrentLangValue(key);
			const auto existingOverride = gOverrides.contains(key) ? gOverrides[key] : QString();
			c->show(Box([=](not_null<Ui::GenericBox*> ebox) {
				ebox->setTitle(rpl::single(u"Изменить фразу"_q));
				const auto container = ebox->verticalLayout();
				const auto info = u"Ключ: %1\nТекущее: «%2»%3"_q
					.arg(key, curVal,
						curVal.contains(u'{') ? u"\n⚠ Сохраняй теги вроде {time}, {user}, {date}!"_q : QString());
				ebox->addRow(object_ptr<Ui::FlatLabel>(container, info, st::boxLabel));
				Ui::AddSkip(container);
				const auto field = ebox->addRow(object_ptr<Ui::InputField>(
					container,
					st::defaultInputField,
					rpl::single(hintLabel.isEmpty() ? key : hintLabel),
					existingOverride.isEmpty() ? curVal : existingOverride));
				ebox->addButton(rpl::single(u"Сохранить"_q), [=] {
					const auto nv = field->getLastText();
					if (nv.isEmpty()) {
						gOverrides.erase(key);
						Lang::GetInstance().resetValue(key.toUtf8());
					} else {
						gOverrides[key] = nv;
						Lang::GetInstance().applyValue(key.toUtf8(), nv.toUtf8());
					}
					SaveOverridesToDisk();
					*filterVar = filterLabel();
					ebox->closeBox();
					(*rebuild)();
					if (const auto s = weak.get()) {
						s->showToast(u"Фраза «%1» обновлена!"_q.arg(key));
					}
				});
				ebox->addLeftButton(rpl::single(u"Сбросить"_q), [=] {
					gOverrides.erase(key);
					Lang::GetInstance().resetValue(key.toUtf8());
					SaveOverridesToDisk();
					*filterVar = filterLabel();
					ebox->closeBox();
					(*rebuild)();
				});
				ebox->addButton(rpl::single(u"Отмена"_q), [=] { ebox->closeBox(); });
			}));
		};

		*rebuild = [=] {
			while (listWrap->count() > 0) {
				delete listWrap->widgetAt(0);
			}
			filterBtn->setVisible(state->tab == 1);
			searchField->setVisible(state->tab != 2);

			const auto q = state->query.trimmed().toLower();
			if (state->tab == 0) {
				if (q.isEmpty()) {
					Ui::AddSubsectionTitle(listWrap, rpl::single(u"Мгновенные пресеты MediaTok (офлайн, в 1 клик)"_q));
					for (const auto &pr : Presets()) {
						const auto pName = QString::fromUtf8(pr.name);
						Settings::AddButtonWithLabel(
							listWrap,
							rpl::single(pName),
							rpl::single(u"%1 фраз"_q.arg(int(pr.items.size()))),
							st::settingsButton
						)->setClickedCallback([=, items = pr.items] {
							for (const auto &[k, v] : items) {
								gOverrides[k] = v;
								Lang::GetInstance().applyValue(k.toUtf8(), v.toUtf8());
							}
							SaveOverridesToDisk();
							*filterVar = filterLabel();
							if (const auto s = weak.get()) {
								s->showToast(u"Пресет «%1» применён!"_q.arg(pName));
								PromptRestartForLang(s);
							}
						});
					}
					if (!gOverrides.empty()) {
						Settings::AddButtonWithLabel(
							listWrap,
							rpl::single(u"Сбросить все правки MediaTok"_q),
							rpl::single(u"%1 фраз"_q.arg(int(gOverrides.size()))),
							st::settingsButton
						)->setClickedCallback([=] {
							for (const auto &[k, _] : gOverrides) {
								Lang::GetInstance().resetValue(k.toUtf8());
							}
							gOverrides.clear();
							SaveOverridesToDisk();
							*filterVar = filterLabel();
							(*rebuild)();
							if (const auto s = weak.get()) {
								s->showToast(u"Все правки языка сброшены"_q);
							}
						});
					}
				}

				QString lastCat;
				for (const auto &pk : kPublicPacks) {
					const auto slug = QString::fromLatin1(pk.slug);
					const auto title = QString::fromUtf8(pk.title);
					const auto desc = QString::fromUtf8(pk.desc);
					const auto cat = QString::fromUtf8(pk.category);
					if (!q.isEmpty()
						&& !title.toLower().contains(q)
						&& !desc.toLower().contains(q)
						&& !cat.toLower().contains(q)
						&& !slug.toLower().contains(q)) {
						continue;
					}
					if (cat != lastCat) {
						lastCat = cat;
						Ui::AddSkip(listWrap);
						Ui::AddSubsectionTitle(listWrap, rpl::single(u"Telegram-паки: "_q + lastCat));
					}
					Settings::AddButtonWithLabel(
						listWrap,
						rpl::single(title),
						rpl::single(desc),
						st::settingsButton
					)->setClickedCallback([=] {
						Lang::CurrentCloudManager().switchWithWarning(slug);
					});
				}
			} else if (state->tab == 1) {
				constexpr int kMaxRows = 120;
				int shown = 0;
				if (state->filter == 0 && q.isEmpty()) {
					Ui::AddSubsectionTitle(listWrap, rpl::single(u"Популярные фразы (переключи фильтр на «ВСЕ» или введи слово в поиск)"_q));
					for (const auto &pk : kPopularKeys) {
						const auto key = QString::fromLatin1(pk.key);
						const auto label = QString::fromUtf8(pk.label);
						const auto cur = CurrentLangValue(key);
						const bool mod = gOverrides.contains(key);
						Settings::AddButtonWithLabel(
							listWrap,
							rpl::single(mod ? (label + u"  ✎"_q) : label),
							rpl::single(cur.left(42)),
							st::settingsButton
						)->setClickedCallback([=] {
							openEditPhraseBox(key, label);
						});
						++shown;
					}
				} else if (state->filter == 2) {
					Ui::AddSubsectionTitle(listWrap, rpl::single(u"Изменённые фразы (%1)"_q.arg(int(gOverrides.size()))));
					for (const auto &[key, val] : gOverrides) {
						if (!q.isEmpty() && !key.toLower().contains(q) && !val.toLower().contains(q)) {
							continue;
						}
						Settings::AddButtonWithLabel(
							listWrap,
							rpl::single(key + u"  ✎"_q),
							rpl::single(val.left(42)),
							st::settingsButton
						)->setClickedCallback([=] {
							openEditPhraseBox(key, key);
						});
						if (++shown >= kMaxRows) break;
					}
				} else {
					Ui::AddSubsectionTitle(listWrap, rpl::single(q.isEmpty()
						? u"ВСЕ %1 фраз Telegram Desktop (показаны первые %2 — ищи в поиске сверху!)"_q.arg(int(gAllKeys.size())).arg(kMaxRows)
						: u"Поиск «%1» по всем %2 фразам Telegram:"_q.arg(q).arg(int(gAllKeys.size()))));
					for (const auto &key : gAllKeys) {
						const auto cur = CurrentLangValue(key);
						if (!q.isEmpty() && !key.toLower().contains(q) && !cur.toLower().contains(q)) {
							continue;
						}
						const bool mod = gOverrides.contains(key);
						Settings::AddButtonWithLabel(
							listWrap,
							rpl::single(mod ? (key + u"  ✎"_q) : key),
							rpl::single(cur.left(42)),
							st::settingsButton
						)->setClickedCallback([=] {
							openEditPhraseBox(key, key);
						});
						if (++shown >= kMaxRows) {
							Ui::AddDividerText(listWrap, rpl::single(u"Показаны первые 120 совпадений — уточните поиск."_q));
							break;
						}
					}
				}
			} else {
				Ui::AddSubsectionTitle(listWrap, rpl::single(u"Экспорт и импорт своего языка (.mtlang JSON)"_q));
				Settings::AddButtonWithLabel(
					listWrap,
					rpl::single(u"Скопировать мой язык в буфер обмена"_q),
					rpl::single(u"%1 фраз"_q.arg(int(gOverrides.size()))),
					st::settingsButton
				)->setClickedCallback([=] {
					QJsonObject obj;
					for (const auto &[k, v] : gOverrides) obj.insert(k, v);
					if (auto *cb = QGuiApplication::clipboard()) {
						cb->setText(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Indented)));
					}
					if (const auto s = weak.get()) {
						s->showToast(u"Язык скопирован в буфер (%1 фраз)"_q.arg(int(gOverrides.size())));
					}
				});
				Settings::AddButtonWithLabel(
					listWrap,
					rpl::single(u"Импортировать язык из буфера обмена"_q),
					rpl::single(u"Вставить JSON"_q),
					st::settingsButton
				)->setClickedCallback([=] {
					const auto raw = QGuiApplication::clipboard() ? QGuiApplication::clipboard()->text() : QString();
					const auto doc = QJsonDocument::fromJson(raw.toUtf8());
					if (!doc.isObject()) {
						if (const auto s = weak.get()) s->showToast(u"В буфере нет корректного JSON языка"_q);
						return;
					}
					int n = 0;
					const auto obj = doc.object();
					for (auto it = obj.begin(); it != obj.end(); ++it) {
						const auto v = it.value().toString();
						if (!it.key().isEmpty() && !v.isEmpty()) {
							gOverrides[it.key()] = v;
							Lang::GetInstance().applyValue(it.key().toUtf8(), v.toUtf8());
							++n;
						}
					}
					SaveOverridesToDisk();
					*filterVar = filterLabel();
					if (const auto s = weak.get()) {
						s->showToast(u"Импортировано %1 фраз!"_q.arg(n));
						PromptRestartForLang(s);
					}
				});
				Settings::AddButtonWithLabel(
					listWrap,
					rpl::single(u"Сбросить все правки языка"_q),
					rpl::single(u"Очистить"_q),
					st::settingsButton
				)->setClickedCallback([=] {
					for (const auto &[k, _] : gOverrides) {
						Lang::GetInstance().resetValue(k.toUtf8());
					}
					gOverrides.clear();
					SaveOverridesToDisk();
					*filterVar = filterLabel();
					if (const auto s = weak.get()) {
						s->showToast(u"Язык сброшен к оригиналу"_q);
						PromptRestartForLang(s);
					}
				});
			}
			listWrap->resizeToWidth(box->width());
		};

		modeBtn->setClickedCallback([=] {
			state->tab = (state->tab + 1) % 3;
			*modeVar = modeLabel();
			(*rebuild)();
		});
		filterBtn->setClickedCallback([=] {
			state->filter = (state->filter + 1) % 3;
			*filterVar = filterLabel();
			(*rebuild)();
		});
		searchField->changes() | rpl::on_next([=] {
			const auto txt = searchField->getLastText();
			if (txt != state->query) {
				state->query = txt;
				(*rebuild)();
			}
		}, searchField->lifetime());

		(*rebuild)();
		box->addButton(rpl::single(u"Закрыть"_q), [=] { box->closeBox(); });
	}));
}

} // namespace MediaTok
