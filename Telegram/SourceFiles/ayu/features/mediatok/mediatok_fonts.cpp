// This is part of MediaTok (AyuGramPlus fork).
#include "ayu/features/mediatok/mediatok_fonts.h"

#include "apiwrap.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "core/file_utilities.h"
#include "data/data_user.h"
#include "history/history.h"
#include "main/main_session.h"
#include "settings/settings_common.h"
#include "storage/localstorage.h"
#include "storage/storage_media_prepare.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "ui/boxes/choose_font_box.h"
#include "ui/boxes/confirm_box.h"
#include "ui/chat/attach/attach_prepare.h"
#include "ui/chat/chat_theme.h"
#include "ui/layers/generic_box.h"
#include "ui/painter.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/section_widget.h"
#include "window/themes/window_theme.h"
#include "window/window_session_controller.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QRegularExpression>
#include <QtGui/QBrush>
#include <QtGui/QClipboard>
#include <QtGui/QColor>
#include <QtGui/QFont>
#include <QtGui/QFontDatabase>
#include <QtGui/QFontMetrics>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtGui/QLinearGradient>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPen>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <tuple>
#include <vector>

namespace MediaTok {
namespace {

struct BundledFontInfo {
	const char *slug;
	const char *displayName;
	const char *category;
	const char *resourcePath;
};

constexpr std::array<BundledFontInfo, 80> kBundledFonts = {{
	// Современные (26)
	{ "inter", "Inter", "Современные", ":/mediatok/fonts/mt_inter.ttf" },
	{ "montserrat", "Montserrat", "Современные", ":/mediatok/fonts/mt_montserrat.ttf" },
	{ "manrope", "Manrope", "Современные", ":/mediatok/fonts/mt_manrope.ttf" },
	{ "nunito", "Nunito", "Современные", ":/mediatok/fonts/mt_nunito.ttf" },
	{ "comfortaa", "Comfortaa", "Современные", ":/mediatok/fonts/mt_comfortaa.ttf" },
	{ "rubik", "Rubik", "Современные", ":/mediatok/fonts/mt_rubik.ttf" },
	{ "onest", "Onest", "Современные", ":/mediatok/fonts/mt_onest.ttf" },
	{ "golos_text", "Golos Text", "Современные", ":/mediatok/fonts/mt_golos_text.ttf" },
	{ "raleway", "Raleway", "Современные", ":/mediatok/fonts/mt_raleway.ttf" },
	{ "mulish", "Mulish", "Современные", ":/mediatok/fonts/mt_mulish.ttf" },
	{ "jost", "Jost", "Современные", ":/mediatok/fonts/mt_jost.ttf" },
	{ "ubuntu", "Ubuntu", "Современные", ":/mediatok/fonts/mt_ubuntu.ttf" },
	{ "pt_sans", "PT Sans", "Современные", ":/mediatok/fonts/mt_pt_sans.ttf" },
	{ "fira_sans", "Fira Sans", "Современные", ":/mediatok/fonts/mt_fira_sans.ttf" },
	{ "exo_2", "Exo 2", "Современные", ":/mediatok/fonts/mt_exo_2.ttf" },
	{ "oswald", "Oswald", "Современные", ":/mediatok/fonts/mt_oswald.ttf" },
	{ "sofia_sans", "Sofia Sans", "Современные", ":/mediatok/fonts/mt_sofia_sans.ttf" },
	{ "commissioner", "Commissioner", "Современные", ":/mediatok/fonts/mt_commissioner.ttf" },
	{ "cuprum", "Cuprum", "Современные", ":/mediatok/fonts/mt_cuprum.ttf" },
	{ "yanone_kaffeesatz", "Yanone Kaffeesatz", "Современные", ":/mediatok/fonts/mt_yanone_kaffeesatz.ttf" },
	{ "didact_gothic", "Didact Gothic", "Современные", ":/mediatok/fonts/mt_didact_gothic.ttf" },
	{ "tenor_sans", "Tenor Sans", "Современные", ":/mediatok/fonts/mt_tenor_sans.ttf" },
	{ "el_messiri", "El Messiri", "Современные", ":/mediatok/fonts/mt_el_messiri.ttf" },
	{ "alumni_sans", "Alumni Sans", "Современные", ":/mediatok/fonts/mt_alumni_sans.ttf" },
	{ "bellota_text", "Bellota Text", "Современные", ":/mediatok/fonts/mt_bellota_text.ttf" },
	{ "wix_madefor_display", "Wix Madefor Display", "Современные", ":/mediatok/fonts/mt_wix_madefor_display.ttf" },

	// Кибер / Пиксель / Арт (29)
	{ "unbounded", "Unbounded", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_unbounded.ttf" },
	{ "russo_one", "Russo One", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_russo_one.ttf" },
	{ "tektur", "Tektur", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_tektur.ttf" },
	{ "jura", "Jura", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_jura.ttf" },
	{ "press_start_2p", "Press Start 2P", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_press_start_2p.ttf" },
	{ "pixelify_sans", "Pixelify Sans", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_pixelify_sans.ttf" },
	{ "tiny5", "Tiny5", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_tiny5.ttf" },
	{ "rubik_glitch", "Rubik Glitch", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_rubik_glitch.ttf" },
	{ "rubik_bubbles", "Rubik Bubbles", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_rubik_bubbles.ttf" },
	{ "rubik_mono_one", "Rubik Mono One", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_rubik_mono_one.ttf" },
	{ "kelly_slab", "Kelly Slab", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_kelly_slab.ttf" },
	{ "underdog", "Underdog", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_underdog.ttf" },
	{ "poiret_one", "Poiret One", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_poiret_one.ttf" },
	{ "ruslan_display", "Ruslan Display", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_ruslan_display.ttf" },
	{ "seymour_one", "Seymour One", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_seymour_one.ttf" },
	{ "stalinist_one", "Stalinist One", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_stalinist_one.ttf" },
	{ "kablammo", "Kablammo", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_kablammo.ttf" },
	{ "rubik_wet_paint", "Rubik Wet Paint", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_rubik_wet_paint.ttf" },
	{ "rubik_spray_paint", "Rubik Spray Paint", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_rubik_spray_paint.ttf" },
	{ "rubik_vinyl", "Rubik Vinyl", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_rubik_vinyl.ttf" },
	{ "rubik_iso", "Rubik Iso", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_rubik_iso.ttf" },
	{ "rubik_broken_fax", "Rubik Broken Fax", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_rubik_broken_fax.ttf" },
	{ "rubik_burned", "Rubik Burned", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_rubik_burned.ttf" },
	{ "rubik_distressed", "Rubik Distressed", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_rubik_distressed.ttf" },
	{ "rubik_marker_hatch", "Rubik Marker Hatch", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_rubik_marker_hatch.ttf" },
	{ "rubik_maze", "Rubik Maze", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_rubik_maze.ttf" },
	{ "rubik_moonrocks", "Rubik Moonrocks", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_rubik_moonrocks.ttf" },
	{ "rubik_puddles", "Rubik Puddles", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_rubik_puddles.ttf" },
	{ "rubik_scribble", "Rubik Scribble", "Кибер / Пиксель / Арт", ":/mediatok/fonts/mt_rubik_scribble.ttf" },

	// С засечками (10)
	{ "playfair_display", "Playfair Display", "С засечками (Serif)", ":/mediatok/fonts/mt_playfair_display.ttf" },
	{ "cormorant_garamond", "Cormorant Garamond", "С засечками (Serif)", ":/mediatok/fonts/mt_cormorant_garamond.ttf" },
	{ "lora", "Lora", "С засечками (Serif)", ":/mediatok/fonts/mt_lora.ttf" },
	{ "yeseva_one", "Yeseva One", "С засечками (Serif)", ":/mediatok/fonts/mt_yeseva_one.ttf" },
	{ "philosopher", "Philosopher", "С засечками (Serif)", ":/mediatok/fonts/mt_philosopher.ttf" },
	{ "alice", "Alice", "С засечками (Serif)", ":/mediatok/fonts/mt_alice.ttf" },
	{ "merriweather", "Merriweather", "С засечками (Serif)", ":/mediatok/fonts/mt_merriweather.ttf" },
	{ "vollkorn", "Vollkorn", "С засечками (Serif)", ":/mediatok/fonts/mt_vollkorn.ttf" },
	{ "forum", "Forum", "С засечками (Serif)", ":/mediatok/fonts/mt_forum.ttf" },
	{ "prata", "Prata", "С засечками (Serif)", ":/mediatok/fonts/mt_prata.ttf" },

	// Рукописные и Фан (11)
	{ "caveat", "Caveat", "Рукописные и Фан", ":/mediatok/fonts/mt_caveat.ttf" },
	{ "pacifico", "Pacifico", "Рукописные и Фан", ":/mediatok/fonts/mt_pacifico.ttf" },
	{ "marck_script", "Marck Script", "Рукописные и Фан", ":/mediatok/fonts/mt_marck_script.ttf" },
	{ "bad_script", "Bad Script", "Рукописные и Фан", ":/mediatok/fonts/mt_bad_script.ttf" },
	{ "lobster", "Lobster", "Рукописные и Фан", ":/mediatok/fonts/mt_lobster.ttf" },
	{ "amatic_sc", "Amatic SC", "Рукописные и Фан", ":/mediatok/fonts/mt_amatic_sc.ttf" },
	{ "neucha", "Neucha", "Рукописные и Фан", ":/mediatok/fonts/mt_neucha.ttf" },
	{ "balsamiq_sans", "Balsamiq Sans", "Рукописные и Фан", ":/mediatok/fonts/mt_balsamiq_sans.ttf" },
	{ "shantell_sans", "Shantell Sans", "Рукописные и Фан", ":/mediatok/fonts/mt_shantell_sans.ttf" },
	{ "pangolin", "Pangolin", "Рукописные и Фан", ":/mediatok/fonts/mt_pangolin.ttf" },
	{ "comforter_brush", "Comforter Brush", "Рукописные и Фан", ":/mediatok/fonts/mt_comforter_brush.ttf" },

	// Моноширинные (4)
	{ "jetbrains_mono", "JetBrains Mono", "Моноширинные", ":/mediatok/fonts/mt_jetbrains_mono.ttf" },
	{ "fira_code", "Fira Code", "Моноширинные", ":/mediatok/fonts/mt_fira_code.ttf" },
	{ "ibm_plex_mono", "IBM Plex Mono", "Моноширинные", ":/mediatok/fonts/mt_ibm_plex_mono.ttf" },
	{ "anonymous_pro", "Anonymous Pro", "Моноширинные", ":/mediatok/fonts/mt_anonymous_pro.ttf" },
}};

struct ChatStyleInfo {
	const char *id;
	const char *name;
	const char *category;
};

constexpr std::array<ChatStyleInfo, 32> kChatStyles = {{
	// Кириллица + Латиница (21)
	{ "smallcaps", "Маленькие капсы", "Кириллица + Латиница" },
	{ "wide", "Широкий Vaporwave", "Кириллица + Латиница" },
	{ "spaced", "Разрядка (А Э С Т Е Т И К А)", "Кириллица + Латиница" },
	{ "ancient", "Старославянский", "Кириллица + Латиница" },
	{ "cyber", "Киберпанк", "Кириллица + Латиница" },
	{ "runes", "Рунический / Греч.", "Кириллица + Латиница" },
	{ "bubble", "В кружочках", "Кириллица + Латиница" },
	{ "square", "В квадратах", "Кириллица + Латиница" },
	{ "underline", "Подчёркнутый", "Кириллица + Латиница" },
	{ "double_line", "Двойная линия", "Кириллица + Латиница" },
	{ "strike", "Зачёркнутый", "Кириллица + Латиница" },
	{ "slash", "Перечёркнутый", "Кириллица + Латиница" },
	{ "overline", "Надчёркнутый", "Кириллица + Латиница" },
	{ "wave", "Волнистый", "Кириллица + Латиница" },
	{ "glitch", "Глитч / Zalgo", "Кириллица + Латиница" },
	{ "stars", "Эстетик ✦", "Кириллица + Латиница" },
	{ "brackets", "Японские рамки 『 』", "Кириллица + Латиница" },
	{ "leet", "Хакер 1337", "Кириллица + Латиница" },
	{ "fence", "Заборчик", "Кириллица + Латиница" },
	{ "upside", "Перевёрнутый", "Кириллица + Латиница" },
	{ "mirror", "Зеркальный", "Кириллица + Латиница" },

	// Латиница + Цифры Unicode Math (11)
	{ "bold", "Bold Serif", "Латиница + Цифры (Unicode)" },
	{ "italic", "Italic Serif", "Латиница + Цифры (Unicode)" },
	{ "bold_italic", "Bold Italic", "Латиница + Цифры (Unicode)" },
	{ "sans_bold", "Sans Bold", "Латиница + Цифры (Unicode)" },
	{ "sans_italic", "Sans Italic", "Латиница + Цифры (Unicode)" },
	{ "script", "Calligraphy Script", "Латиница + Цифры (Unicode)" },
	{ "bold_script", "Bold Script", "Латиница + Цифры (Unicode)" },
	{ "fraktur", "Gothic / Fraktur", "Латиница + Цифры (Unicode)" },
	{ "bold_fraktur", "Bold Gothic", "Латиница + Цифры (Unicode)" },
	{ "double_struck", "Double-Struck", "Латиница + Цифры (Unicode)" },
	{ "mono", "Monospace Unicode", "Латиница + Цифры (Unicode)" },
}};

struct CustomFontRecord {
	QString filePath;
	QString displayName;
	QString qtFamily;
};

bool gFontsRegistered = false;
QHash<QString, QString> gBundledSlugToFamily;
std::vector<CustomFontRecord> gCustomFonts;
bool gChatStyleLoaded = false;
QString gActiveChatStyle;

[[nodiscard]] QString FontsDirPath() {
	return cWorkingDir() + u"tdata/MediaTok_Fonts"_q;
}

[[nodiscard]] QString ChatStyleFilePath() {
	return FontsDirPath() + u"/chat_font.txt"_q;
}

[[nodiscard]] bool IsValidFontMagic(const QString &path) {
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly)) {
		return false;
	}
	const auto header = f.read(4);
	if (header.size() < 4) {
		return false;
	}
	const auto b0 = uchar(header[0]);
	const auto b1 = uchar(header[1]);
	const auto b2 = uchar(header[2]);
	const auto b3 = uchar(header[3]);
	const bool isTtf = (b0 == 0x00 && b1 == 0x01 && b2 == 0x00 && b3 == 0x00);
	const bool isOtf = (b0 == 'O' && b1 == 'T' && b2 == 'T' && b3 == 'O');
	const bool isTrue = (b0 == 't' && b1 == 'r' && b2 == 'u' && b3 == 'e');
	const bool isTtc = (b0 == 't' && b1 == 't' && b2 == 'c' && b3 == 'f');
	return isTtf || isOtf || isTrue || isTtc;
}

void RegisterSingleCustomFont(const QString &filePath) {
	const int id = QFontDatabase::addApplicationFont(filePath);
	if (id < 0) {
		return;
	}
	const auto families = QFontDatabase::applicationFontFamilies(id);
	if (families.isEmpty()) {
		return;
	}
	const auto fam = families.first();
	const auto baseName = QFileInfo(filePath).completeBaseName()
		.replace('_', ' ')
		.replace('-', ' ')
		.trimmed();
	for (auto &existing : gCustomFonts) {
		if (existing.filePath == filePath) {
			existing.qtFamily = fam;
			existing.displayName = baseName.isEmpty() ? fam : baseName;
			return;
		}
	}
	gCustomFonts.push_back({
		filePath,
		baseName.isEmpty() ? fam : baseName,
		fam,
	});
}

void EnsureChatStyleLoaded() {
	if (gChatStyleLoaded) {
		return;
	}
	gChatStyleLoaded = true;
	QFile f(ChatStyleFilePath());
	if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
		const auto id = QString::fromUtf8(f.readAll()).trimmed().toLower();
		for (const auto &s : kChatStyles) {
			if (id == QLatin1String(s.id)) {
				gActiveChatStyle = id;
				break;
			}
		}
	}
}

[[nodiscard]] QString FromUcs4(char32_t cp) {
	return QString::fromUcs4(&cp, 1);
}

[[nodiscard]] bool IsCyrillicChar(QChar ch) {
	const auto u = ch.unicode();
	return (u >= 0x0410 && u <= 0x044F) || u == 0x0401 || u == 0x0451;
}

[[nodiscard]] QString MapMath(QChar ch, char32_t upperStart, char32_t lowerStart, char32_t digitStart) {
	const auto u = ch.unicode();
	if (u >= 'A' && u <= 'Z') {
		return FromUcs4(upperStart + char32_t(u - 'A'));
	}
	if (u >= 'a' && u <= 'z') {
		return FromUcs4(lowerStart + char32_t(u - 'a'));
	}
	if (digitStart != 0 && u >= '0' && u <= '9') {
		return FromUcs4(digitStart + char32_t(u - '0'));
	}
	return QString(ch);
}

[[nodiscard]] const QHash<QChar, QString> &SmallCapsMap() {
	static const QHash<QChar, QString> map = {
		{ u'a', u"ᴀ"_q }, { u'b', u"ʙ"_q }, { u'c', u"ᴄ"_q }, { u'd', u"ᴅ"_q },
		{ u'e', u"ᴇ"_q }, { u'f', u"ғ"_q }, { u'g', u"ɢ"_q }, { u'h', u"ʜ"_q },
		{ u'i', u"ɪ"_q }, { u'j', u"ᴊ"_q }, { u'k', u"ᴋ"_q }, { u'l', u"ʟ"_q },
		{ u'm', u"ᴍ"_q }, { u'n', u"ɴ"_q }, { u'o', u"ᴏ"_q }, { u'p', u"ᴘ"_q },
		{ u'q', u"ǫ"_q }, { u'r', u"ʀ"_q }, { u's', u"s"_q }, { u't', u"ᴛ"_q },
		{ u'u', u"ᴜ"_q }, { u'v', u"ᴠ"_q }, { u'w', u"ᴡ"_q }, { u'x', u"x"_q },
		{ u'y', u"ʏ"_q }, { u'z', u"ᴢ"_q },
		{ u'а', u"ᴀ"_q }, { u'в', u"ʙ"_q }, { u'е', u"ᴇ"_q }, { u'ё', u"ᴇ\u0308"_q },
		{ u'к', u"ᴋ"_q }, { u'м', u"ᴍ"_q }, { u'н', u"ʜ"_q }, { u'о', u"ᴏ"_q },
		{ u'р', u"ᴘ"_q }, { u'с', u"ᴄ"_q }, { u'т', u"ᴛ"_q }, { u'у', u"ʏ"_q },
		{ u'А', u"ᴀ"_q }, { u'В', u"ʙ"_q }, { u'Е', u"ᴇ"_q }, { u'Ё', u"ᴇ\u0308"_q },
		{ u'К', u"ᴋ"_q }, { u'М', u"ᴍ"_q }, { u'Н', u"ʜ"_q }, { u'О', u"ᴏ"_q },
		{ u'Р', u"ᴘ"_q }, { u'С', u"ᴄ"_q }, { u'Т', u"ᴛ"_q }, { u'У', u"ʏ"_q },
	};
	return map;
}

[[nodiscard]] const QHash<QChar, QString> &AncientMap() {
	static const QHash<QChar, QString> map = {
		{ u'Е', u"Ѣ"_q }, { u'Ё', u"Ѥ"_q }, { u'З', u"Ꙁ"_q }, { u'И', u"І"_q },
		{ u'Й', u"Ї"_q }, { u'О', u"Ѻ"_q }, { u'С', u"Ѕ"_q }, { u'У', u"Ѹ"_q },
		{ u'Ф', u"Ѳ"_q }, { u'Э', u"Є"_q }, { u'Я', u"Ѧ"_q },
		{ u'е', u"ѣ"_q }, { u'ё', u"ѥ"_q }, { u'з', u"ꙁ"_q }, { u'и', u"і"_q },
		{ u'й', u"ї"_q }, { u'о', u"ѻ"_q }, { u'с', u"ѕ"_q }, { u'у', u"ѹ"_q },
		{ u'ф', u"ѳ"_q }, { u'э', u"є"_q }, { u'я', u"ѧ"_q },
	};
	return map;
}

[[nodiscard]] const QHash<QChar, QString> &CyberMap() {
	static const QHash<QChar, QString> map = {
		{ u'А', u"卂"_q }, { u'Б', u"Ƃ"_q }, { u'В', u"乃"_q }, { u'Г', u"厂"_q },
		{ u'Е', u"乇"_q }, { u'Ё', u"乇"_q }, { u'З', u"Ӡ"_q }, { u'К', u"长"_q },
		{ u'Л', u"爪"_q }, { u'М', u"爪"_q }, { u'Н', u"卄"_q }, { u'О', u"口"_q },
		{ u'Р', u"尺"_q }, { u'С', u"匚"_q }, { u'Т', u"匕"_q }, { u'У', u"ㄚ"_q },
		{ u'Ф', u"中"_q }, { u'Х', u"乂"_q }, { u'Ш', u"山"_q }, { u'Щ', u"山"_q },
		{ u'а', u"卂"_q }, { u'б', u"Ƃ"_q }, { u'в', u"乃"_q }, { u'г', u"厂"_q },
		{ u'е', u"乇"_q }, { u'ё', u"乇"_q }, { u'к', u"长"_q }, { u'м', u"爪"_q },
		{ u'н', u"卄"_q }, { u'о', u"口"_q }, { u'р', u"尺"_q }, { u'с', u"匚"_q },
		{ u'т', u"匕"_q }, { u'у', u"ㄚ"_q }, { u'ф', u"中"_q }, { u'х', u"乂"_q },
		{ u'ш', u"山"_q }, { u'щ', u"山"_q },
		{ u'A', u"卂"_q }, { u'B', u"乃"_q }, { u'C', u"匚"_q }, { u'D', u"刀"_q },
		{ u'E', u"乇"_q }, { u'F', u"下"_q }, { u'G', u"ム"_q }, { u'H', u"卄"_q },
		{ u'I', u"工"_q }, { u'J', u"丁"_q }, { u'K', u"长"_q }, { u'L', u"乚"_q },
		{ u'M', u"爪"_q }, { u'N', u"几"_q }, { u'O', u"口"_q }, { u'P', u"尸"_q },
		{ u'Q', u"Ɋ"_q }, { u'R', u"尺"_q }, { u'S', u"丂"_q }, { u'T', u"匕"_q },
		{ u'U', u"凵"_q }, { u'V', u"リ"_q }, { u'W', u"山"_q }, { u'X', u"乂"_q },
		{ u'Y', u"ㄚ"_q }, { u'Z', u"乙"_q },
		{ u'a', u"卂"_q }, { u'b', u"乃"_q }, { u'c', u"匚"_q }, { u'd', u"刀"_q },
		{ u'e', u"乇"_q }, { u'f', u"下"_q }, { u'g', u"ム"_q }, { u'h', u"卄"_q },
		{ u'i', u"工"_q }, { u'j', u"丁"_q }, { u'k', u"长"_q }, { u'l', u"乚"_q },
		{ u'm', u"爪"_q }, { u'n', u"几"_q }, { u'o', u"口"_q }, { u'p', u"尸"_q },
		{ u'q', u"Ɋ"_q }, { u'r', u"尺"_q }, { u's', u"丂"_q }, { u't', u"匕"_q },
		{ u'u', u"凵"_q }, { u'v', u"リ"_q }, { u'w', u"山"_q }, { u'x', u"乂"_q },
		{ u'y', u"ㄚ"_q }, { u'z', u"乙"_q },
	};
	return map;
}

[[nodiscard]] const QHash<QChar, QString> &RunesMap() {
	static const QHash<QChar, QString> map = {
		{ u'А', u"Λ"_q }, { u'Б', u"Ƃ"_q }, { u'В', u"Β"_q }, { u'Г', u"Γ"_q },
		{ u'Д', u"Δ"_q }, { u'Е', u"Σ"_q }, { u'Ё', u"Σ"_q }, { u'З', u"Ζ"_q },
		{ u'К', u"Κ"_q }, { u'Л', u"Λ"_q }, { u'М', u"Μ"_q }, { u'Н', u"Η"_q },
		{ u'О', u"Θ"_q }, { u'П', u"Π"_q }, { u'Р', u"Ρ"_q }, { u'С', u"Ϲ"_q },
		{ u'Т', u"Τ"_q }, { u'У', u"Υ"_q }, { u'Ф', u"Φ"_q }, { u'Х', u"Χ"_q },
		{ u'Ц', u"Џ"_q }, { u'Ш', u"Ψ"_q }, { u'Щ', u"Ψ"_q }, { u'Э', u"Ξ"_q },
		{ u'а', u"α"_q }, { u'б', u"ɓ"_q }, { u'в', u"β"_q }, { u'г', u"γ"_q },
		{ u'д', u"δ"_q }, { u'е', u"ε"_q }, { u'ё', u"ε"_q }, { u'з', u"ζ"_q },
		{ u'и', u"ι"_q }, { u'к', u"κ"_q }, { u'л', u"λ"_q }, { u'м', u"μ"_q },
		{ u'н', u"η"_q }, { u'о', u"σ"_q }, { u'п', u"π"_q }, { u'р', u"ρ"_q },
		{ u'с', u"ς"_q }, { u'т', u"τ"_q }, { u'у', u"υ"_q }, { u'ф', u"φ"_q },
		{ u'х', u"χ"_q }, { u'ш', u"ω"_q }, { u'щ', u"ψ"_q },
		{ u'A', u"Λ"_q }, { u'B', u"Β"_q }, { u'C', u"Ϲ"_q }, { u'D', u"Δ"_q },
		{ u'E', u"Σ"_q }, { u'F', u"Ғ"_q }, { u'G', u"Ǥ"_q }, { u'H', u"Η"_q },
		{ u'I', u"Ι"_q }, { u'K', u"Κ"_q }, { u'L', u"Λ"_q }, { u'M', u"Μ"_q },
		{ u'N', u"Ν"_q }, { u'O', u"Θ"_q }, { u'P', u"Ρ"_q }, { u'R', u"Я"_q },
		{ u'S', u"Ѕ"_q }, { u'T', u"Τ"_q }, { u'U', u"Ц"_q }, { u'W', u"Ψ"_q },
		{ u'X', u"Χ"_q }, { u'Y', u"Υ"_q }, { u'Z', u"Ζ"_q },
		{ u'a', u"α"_q }, { u'b', u"β"_q }, { u'c', u"ς"_q }, { u'd', u"δ"_q },
		{ u'e', u"ε"_q }, { u'g', u"γ"_q }, { u'h', u"η"_q }, { u'i', u"ι"_q },
		{ u'k', u"κ"_q }, { u'l', u"λ"_q }, { u'm', u"μ"_q }, { u'n', u"η"_q },
		{ u'o', u"σ"_q }, { u'p', u"ρ"_q }, { u'r', u"г"_q }, { u's', u"ѕ"_q },
		{ u't', u"τ"_q }, { u'u', u"υ"_q }, { u'v', u"ν"_q }, { u'w', u"ω"_q },
		{ u'x', u"χ"_q }, { u'y', u"γ"_q }, { u'z', u"ζ"_q },
	};
	return map;
}

[[nodiscard]] const QHash<QChar, QString> &LeetMap() {
	static const QHash<QChar, QString> map = {
		{ u'А', u"4"_q }, { u'а', u"4"_q }, { u'В', u"8"_q }, { u'в', u"8"_q },
		{ u'Е', u"3"_q }, { u'е', u"3"_q }, { u'Ё', u"3"_q }, { u'ё', u"3"_q },
		{ u'З', u"3"_q }, { u'з', u"3"_q }, { u'И', u"1"_q }, { u'и', u"1"_q },
		{ u'О', u"0"_q }, { u'о', u"0"_q }, { u'С', u"("_q }, { u'с', u"("_q },
		{ u'Т', u"7"_q }, { u'т', u"7"_q }, { u'Ч', u"4"_q }, { u'ч', u"4"_q },
		{ u'A', u"4"_q }, { u'a', u"4"_q }, { u'B', u"8"_q }, { u'b', u"8"_q },
		{ u'E', u"3"_q }, { u'e', u"3"_q }, { u'G', u"6"_q }, { u'g', u"9"_q },
		{ u'I', u"1"_q }, { u'i', u"1"_q }, { u'L', u"1"_q }, { u'l', u"1"_q },
		{ u'O', u"0"_q }, { u'o', u"0"_q }, { u'S', u"5"_q }, { u's', u"5"_q },
		{ u'T', u"7"_q }, { u't', u"7"_q }, { u'Z', u"2"_q }, { u'z', u"2"_q },
	};
	return map;
}

[[nodiscard]] const QHash<QChar, QString> &UpsideMap() {
	static const QHash<QChar, QString> map = {
		{ u'a', u"ɐ"_q }, { u'b', u"q"_q }, { u'c', u"ɔ"_q }, { u'd', u"p"_q },
		{ u'e', u"ǝ"_q }, { u'f', u"ɟ"_q }, { u'g', u"ƃ"_q }, { u'h', u"ɥ"_q },
		{ u'i', u"ᴉ"_q }, { u'j', u"ɾ"_q }, { u'k', u"ʞ"_q }, { u'l', u"ן"_q },
		{ u'm', u"ɯ"_q }, { u'n', u"u"_q }, { u'o', u"o"_q }, { u'p', u"d"_q },
		{ u'q', u"b"_q }, { u'r', u"ɹ"_q }, { u's', u"s"_q }, { u't', u"ʇ"_q },
		{ u'u', u"n"_q }, { u'v', u"ʌ"_q }, { u'w', u"ʍ"_q }, { u'x', u"x"_q },
		{ u'y', u"ʎ"_q }, { u'z', u"z"_q },
		{ u'A', u"∀"_q }, { u'C', u"Ɔ"_q }, { u'D', u"◖"_q }, { u'E', u"Ǝ"_q },
		{ u'F', u"Ⅎ"_q }, { u'G', u"⅁"_q }, { u'J', u"ſ"_q }, { u'K', u"⋊"_q },
		{ u'L', u"˥"_q }, { u'M', u"W"_q }, { u'P', u"Ԁ"_q }, { u'Q', u"Ό"_q },
		{ u'R', u"ᴚ"_q }, { u'T', u"⊥"_q }, { u'U', u"∩"_q }, { u'V', u"Λ"_q },
		{ u'W', u"M"_q }, { u'Y', u"⅄"_q },
		{ u'а', u"ɐ"_q }, { u'б', u"ƍ"_q }, { u'в', u"ʚ"_q }, { u'г', u"ɹ"_q },
		{ u'д', u"ɓ"_q }, { u'е', u"ǝ"_q }, { u'ё', u"ǝ"_q }, { u'з', u"ε"_q },
		{ u'к', u"ʞ"_q }, { u'л', u"v"_q }, { u'м', u"w"_q }, { u'п', u"u"_q },
		{ u'р', u"d"_q }, { u'с', u"ɔ"_q }, { u'т', u"ɯ"_q }, { u'у', u"ʎ"_q },
		{ u'ц', u"ǹ"_q }, { u'ч', u"Һ"_q }, { u'ш', u"m"_q }, { u'щ', u"m"_q },
		{ u'ы', u"ıq"_q }, { u'э', u"є"_q }, { u'ю', u"oı"_q }, { u'я', u"ʁ"_q },
		{ u'А', u"∀"_q }, { u'В', u"ꓭ"_q }, { u'Г', u"⅃"_q }, { u'Е', u"Ǝ"_q },
		{ u'З', u"Ɛ"_q }, { u'К', u"⋊"_q }, { u'Л', u"V"_q }, { u'М', u"W"_q },
		{ u'П', u"U"_q }, { u'Р', u"Ԁ"_q }, { u'С', u"Ɔ"_q }, { u'Т', u"⊥"_q },
		{ u'У', u"⅄"_q }, { u'Ш', u"M"_q }, { u'Я', u"ʁ"_q },
		{ u'!', u"¡"_q }, { u'?', u"¿"_q }, { u'.', u"˙"_q }, { u',', u"'"_q },
		{ u'(', u")"_q }, { u')', u"("_q },
	};
	return map;
}

[[nodiscard]] QString TransformSingleChar(QChar ch, const QString &styleId, int index) {
	if (styleId == u"smallcaps"_q) {
		return SmallCapsMap().value(ch, QString(ch));
	}
	if (styleId == u"wide"_q) {
		const auto u = ch.unicode();
		if (u >= '!' && u <= '~') {
			return QString(QChar(ushort(u + 0xFEE0)));
		}
		if (IsCyrillicChar(ch)) {
			return QString(ch) + QChar(0x2009);
		}
		return QString(ch);
	}
	if (styleId == u"spaced"_q) {
		return ch.isLetterOrNumber() ? (QString(ch) + u' ') : QString(ch);
	}
	if (styleId == u"ancient"_q) {
		if (const auto it = AncientMap().constFind(ch); it != AncientMap().constEnd()) {
			return it.value();
		}
		if (ch >= 'A' && ch <= 'Z') {
			return TransformSingleChar(ch, u"fraktur"_q, index);
		}
		if (ch >= 'a' && ch <= 'z') {
			return TransformSingleChar(ch, u"fraktur"_q, index);
		}
		return QString(ch);
	}
	if (styleId == u"cyber"_q) {
		return CyberMap().value(ch, QString(ch));
	}
	if (styleId == u"runes"_q) {
		return RunesMap().value(ch, QString(ch));
	}
	if (styleId == u"bubble"_q) {
		const auto u = ch.unicode();
		if (u >= 'A' && u <= 'Z') return QString(QChar(ushort(0x24B6 + (u - 'A'))));
		if (u >= 'a' && u <= 'z') return QString(QChar(ushort(0x24D0 + (u - 'a'))));
		if (u == '0') return QString(QChar(0x24EA));
		if (u >= '1' && u <= '9') return QString(QChar(ushort(0x2460 + (u - '1'))));
		if (ch.isLetterOrNumber()) return QString(ch) + QChar(0x20DD);
		return QString(ch);
	}
	if (styleId == u"square"_q) {
		const auto u = ch.unicode();
		if (u >= 'A' && u <= 'Z') return FromUcs4(0x1F130 + char32_t(u - 'A'));
		if (u >= 'a' && u <= 'z') return FromUcs4(0x1F130 + char32_t(u - 'a'));
		if (ch.isLetterOrNumber()) return QString(ch) + QChar(0x20DE);
		return QString(ch);
	}
	if (styleId == u"underline"_q) return !ch.isSpace() ? (QString(ch) + QChar(0x0332)) : QString(ch);
	if (styleId == u"double_line"_q) return !ch.isSpace() ? (QString(ch) + QChar(0x0333)) : QString(ch);
	if (styleId == u"strike"_q) return !ch.isSpace() ? (QString(ch) + QChar(0x0336)) : QString(ch);
	if (styleId == u"slash"_q) return !ch.isSpace() ? (QString(ch) + QChar(0x0337)) : QString(ch);
	if (styleId == u"overline"_q) return !ch.isSpace() ? (QString(ch) + QChar(0x0305)) : QString(ch);
	if (styleId == u"wave"_q) return !ch.isSpace() ? (QString(ch) + QChar(0x0330)) : QString(ch);
	if (styleId == u"glitch"_q) {
		if (!ch.isLetterOrNumber()) return QString(ch);
		static constexpr ushort kMid[] = { 0x0334, 0x0335, 0x0336, 0x0337, 0x0338 };
		static constexpr ushort kUp[] = { 0x030D, 0x030E, 0x0304, 0x0305, 0x0311, 0x0315, 0x031A };
		QString r;
		r.reserve(3);
		r.append(ch);
		r.append(QChar(kMid[index % 5]));
		r.append(QChar(kUp[(index * 3 + ch.unicode()) % 7]));
		return r;
	}
	if (styleId == u"leet"_q) return LeetMap().value(ch, QString(ch));
	if (styleId == u"fence"_q) {
		if (!ch.isLetter()) return QString(ch);
		return QString((index % 2 == 0) ? ch.toUpper() : ch.toLower());
	}
	if (styleId == u"bold"_q) return MapMath(ch, 0x1D400, 0x1D41A, 0x1D7CE);
	if (styleId == u"italic"_q) {
		if (ch == u'h') return QString(QChar(0x210E));
		return MapMath(ch, 0x1D434, 0x1D44E, 0);
	}
	if (styleId == u"bold_italic"_q) return MapMath(ch, 0x1D468, 0x1D482, 0x1D7CE);
	if (styleId == u"sans_bold"_q) return MapMath(ch, 0x1D5D4, 0x1D5EE, 0x1D7EC);
	if (styleId == u"sans_italic"_q) return MapMath(ch, 0x1D608, 0x1D622, 0);
	if (styleId == u"script"_q) {
		switch (ch.unicode()) {
		case 'B': return QString(QChar(0x212C));
		case 'E': return QString(QChar(0x2130));
		case 'F': return QString(QChar(0x2131));
		case 'H': return QString(QChar(0x210B));
		case 'I': return QString(QChar(0x2110));
		case 'L': return QString(QChar(0x2112));
		case 'M': return QString(QChar(0x2133));
		case 'R': return QString(QChar(0x211B));
		case 'e': return QString(QChar(0x212F));
		case 'g': return QString(QChar(0x210A));
		case 'o': return QString(QChar(0x2134));
		default: return MapMath(ch, 0x1D49C, 0x1D4B6, 0);
		}
	}
	if (styleId == u"bold_script"_q) return MapMath(ch, 0x1D4D0, 0x1D4EA, 0x1D7CE);
	if (styleId == u"fraktur"_q) {
		switch (ch.unicode()) {
		case 'C': return QString(QChar(0x212D));
		case 'H': return QString(QChar(0x210C));
		case 'I': return QString(QChar(0x2111));
		case 'R': return QString(QChar(0x211C));
		case 'Z': return QString(QChar(0x2128));
		default: return MapMath(ch, 0x1D504, 0x1D51E, 0);
		}
	}
	if (styleId == u"bold_fraktur"_q) return MapMath(ch, 0x1D56C, 0x1D586, 0x1D7CE);
	if (styleId == u"double_struck"_q) {
		switch (ch.unicode()) {
		case 'C': return QString(QChar(0x2102));
		case 'H': return QString(QChar(0x210D));
		case 'N': return QString(QChar(0x2115));
		case 'P': return QString(QChar(0x2119));
		case 'Q': return QString(QChar(0x211A));
		case 'R': return QString(QChar(0x211D));
		case 'Z': return QString(QChar(0x2124));
		default: return MapMath(ch, 0x1D538, 0x1D552, 0x1D7D8);
		}
	}
	if (styleId == u"mono"_q) return MapMath(ch, 0x1D670, 0x1D68A, 0x1D7F6);
	return QString(ch);
}

[[nodiscard]] bool ContainsSpecialToken(const QString &text) {
	const auto lower = text.toLower();
	return lower.contains(u"http://"_q)
		|| lower.contains(u"https://"_q)
		|| lower.contains(u"t.me/"_q)
		|| text.contains(u'@')
		|| text.contains(u'#');
}

[[nodiscard]] bool IsProtectedToken(const QString &token) {
	if (token.isEmpty()) return true;
	const auto lower = token.toLower();
	if (lower.startsWith(u"http://"_q)
		|| lower.startsWith(u"https://"_q)
		|| lower.startsWith(u"t.me/"_q)
		|| lower.startsWith(u"www."_q)) {
		return true;
	}
	if ((token.startsWith(u'@') || token.startsWith(u'#') || token.startsWith(u'/'))
		&& token.size() > 1) {
		return true;
	}
	return false;
}

[[nodiscard]] QString ReverseCodePoints(const QString &text) {
	const auto ucs4 = text.toUcs4();
	QString out;
	out.reserve(text.size());
	for (qsizetype i = ucs4.size() - 1; i >= 0; --i) {
		out.append(FromUcs4(ucs4[i]));
	}
	return out;
}

[[nodiscard]] QString FlipUpsideDown(const QString &text) {
	const auto ucs4 = text.toUcs4();
	QString out;
	out.reserve(text.size());
	const auto &map = UpsideMap();
	for (qsizetype i = ucs4.size() - 1; i >= 0; --i) {
		const auto cp = ucs4[i];
		if (cp <= 0xFFFF) {
			const auto it = map.constFind(QChar(ushort(cp)));
			if (it != map.constEnd()) {
				out.append(it.value());
				continue;
			}
		}
		out.append(FromUcs4(cp));
	}
	return out;
}

[[nodiscard]] QString TransformTokens(const QString &text, const QString &styleId) {
	QString out;
	out.reserve(text.size() * 2);
	qsizetype i = 0;
	int fenceIdx = 0;
	while (i < text.size()) {
		if (text[i].isSpace()) {
			out.append(text[i]);
			++i;
			continue;
		}
		qsizetype end = i;
		while (end < text.size() && !text[end].isSpace()) {
			++end;
		}
		const auto token = text.mid(i, end - i);
		if (IsProtectedToken(token)) {
			out.append(token);
		} else if (styleId == u"mirror"_q) {
			out.append(ReverseCodePoints(token));
		} else if (styleId == u"upside"_q) {
			out.append(FlipUpsideDown(token));
		} else {
			qsizetype k = 0;
			while (k < token.size()) {
				const auto ch = token[k];
				if (ch.isHighSurrogate() && k + 1 < token.size()) {
					out.append(ch);
					out.append(token[k + 1]);
					k += 2;
					continue;
				}
				out.append(TransformSingleChar(ch, styleId, fenceIdx));
				if (ch.isLetter()) {
					++fenceIdx;
				}
				++k;
			}
		}
		i = end;
	}
	return out;
}

[[nodiscard]] const ChatStyleInfo *FindChatStyle(const QString &query) {
	const auto q = query.trimmed().toLower();
	if (q.isEmpty()) return nullptr;
	for (const auto &s : kChatStyles) {
		if (q == QLatin1String(s.id)) return &s;
	}
	for (const auto &s : kChatStyles) {
		if (QString::fromLatin1(s.id).startsWith(q)
			|| QString::fromUtf8(s.name).toLower().startsWith(q)) {
			return &s;
		}
	}
	for (const auto &s : kChatStyles) {
		if (QString::fromLatin1(s.id).contains(q)
			|| QString::fromUtf8(s.name).toLower().contains(q)) {
			return &s;
		}
	}
	return nullptr;
}

void ApplyClientFontAndPromptRestart(
		not_null<Window::SessionController*> controller,
		const QString &qtFamily,
		const QString &displayName) {
	Core::App().settings().setCustomFontFamily(qtFamily);
	Local::writeSettings();
	const auto weak = base::make_weak(controller);
	controller->show(Ui::MakeConfirmBox({
		.text = qtFamily.isEmpty()
			? u"Выбран системный шрифт по умолчанию. Перезапустить MediaTok сейчас, чтобы применить?"_q
			: u"Выбран шрифт клиента «%1». Перезапустить MediaTok сейчас, чтобы применить везде?"_q.arg(displayName),
		.confirmed = [=](Fn<void()> &&close) {
			close();
			Core::Restart();
		},
		.confirmText = u"Перезапустить"_q,
		.cancelText = u"Позже"_q,
	}));
	if (const auto strong = weak.get()) {
		strong->showToast(qtFamily.isEmpty()
			? u"MediaTok: шрифт сброшен (требуется перезапуск)"_q
			: u"MediaTok: шрифт «%1» сохранён"_q.arg(displayName));
	}
}

void OpenNativeChooseFontBox(not_null<Window::SessionController*> controller) {
	const auto family = Core::App().settings().customFontFamily();
	const auto save = [=](QString chosen) {
		Core::App().settings().setCustomFontFamily(chosen);
		Local::writeSettings();
		Core::Restart();
	};
	auto lifetime = std::make_shared<rpl::lifetime>();
	const auto theme = std::shared_ptr<Ui::ChatTheme>(
		Window::Theme::DefaultChatThemeOn(*lifetime));
	const auto generateBg = [theme, lifetime] {
		const auto size = st::boxWidth;
		const auto ratio = style::DevicePixelRatio();
		auto result = QImage(
			QSize(size, size) * ratio,
			QImage::Format_ARGB32_Premultiplied);
		auto p = QPainter(&result);
		Window::SectionWidget::PaintBackground(
			p,
			theme.get(),
			QSize(size, size * 3),
			QRect(0, 0, size, size));
		p.end();
		return result;
	};
	controller->show(Box(Ui::ChooseFontBox, generateBg, family, save));
}

class FontPreviewRow final : public Ui::AbstractButton {
public:
	FontPreviewRow(
		QWidget *parent,
		const QString &title,
		const QString &preview,
		const QString &qtFamily,
		bool selected)
	: AbstractButton(parent)
	, _title(title)
	, _preview(preview)
	, _qtFamily(qtFamily)
	, _selected(selected) {
		setFixedHeight(58);
		setCursor(style::cur_pointer);
	}

	void setSelected(bool selected) {
		if (_selected != selected) {
			_selected = selected;
			update();
		}
	}

	void setPreviewText(const QString &preview) {
		if (_preview != preview) {
			_preview = preview;
			update();
		}
	}

protected:
	void paintEvent(QPaintEvent *e) override {
		auto p = QPainter(this);
		p.setRenderHint(QPainter::Antialiasing, true);

		const auto r = rect().adjusted(12, 3, -12, -3);
		QPainterPath path;
		path.addRoundedRect(r, 8, 8);
		if (_selected) {
			auto bg = st::windowBgActive->c;
			bg.setAlpha(36);
			p.fillPath(path, bg);
			p.setPen(QPen(st::windowActiveTextFg->c, 1.5));
			p.drawPath(path);
		} else if (isOver()) {
			p.fillPath(path, st::windowBgOver->c);
		} else {
			auto bg = st::windowBgOver->c;
			bg.setAlpha(110);
			p.fillPath(path, bg);
		}

		const auto titleRect = QRect(r.left() + 12, r.top() + 5, r.width() - 24, 18);
		p.setFont(st::normalFont);
		p.setPen(_selected ? st::windowActiveTextFg : st::windowSubTextFg);
		const auto titleText = _selected ? (_title + u"  ✓"_q) : _title;
		p.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
			QFontMetrics(p.font()).elidedText(titleText, Qt::ElideRight, titleRect.width()));

		const auto previewRect = QRect(r.left() + 12, r.top() + 23, r.width() - 24, 26);
		QFont previewFont = _qtFamily.isEmpty() ? QGuiApplication::font() : QFont(_qtFamily);
		previewFont.setPixelSize(15);
		p.setFont(previewFont);
		p.setPen(st::windowFg);
		p.drawText(previewRect, Qt::AlignLeft | Qt::AlignVCenter,
			QFontMetrics(previewFont).elidedText(_preview, Qt::ElideRight, previewRect.width()));
	}

private:
	QString _title;
	QString _preview;
	QString _qtFamily;
	bool _selected = false;
};

} // namespace

void RegisterBundledAndCustomFonts() {
	if (!gFontsRegistered) {
		gFontsRegistered = true;
		for (const auto &info : kBundledFonts) {
			const auto resPath = QString::fromLatin1(info.resourcePath);
			const int id = QFontDatabase::addApplicationFont(resPath);
			if (id >= 0) {
				const auto families = QFontDatabase::applicationFontFamilies(id);
				if (!families.isEmpty()) {
					gBundledSlugToFamily.insert(
						QString::fromLatin1(info.slug),
						families.first());
				}
			}
		}
	}

	const QDir dir(FontsDirPath());
	if (dir.exists()) {
		const auto entries = dir.entryInfoList(
			QStringList{ u"*.ttf"_q, u"*.otf"_q },
			QDir::Files | QDir::Readable,
			QDir::Name);
		for (const auto &fi : entries) {
			RegisterSingleCustomFont(fi.absoluteFilePath());
		}
	}
	EnsureChatStyleLoaded();
}

QString ActiveChatTextStyleId() {
	EnsureChatStyleLoaded();
	return gActiveChatStyle;
}

QString ActiveChatTextStyleName() {
	EnsureChatStyleLoaded();
	if (gActiveChatStyle.isEmpty()) {
		return u"Выкл"_q;
	}
	for (const auto &s : kChatStyles) {
		if (gActiveChatStyle == QLatin1String(s.id)) {
			return QString::fromUtf8(s.name);
		}
	}
	return u"Выкл"_q;
}

void SetActiveChatTextStyleId(const QString &styleId) {
	EnsureChatStyleLoaded();
	const auto clean = styleId.trimmed().toLower();
	QString validated;
	for (const auto &s : kChatStyles) {
		if (clean == QLatin1String(s.id)) {
			validated = clean;
			break;
		}
	}
	gActiveChatStyle = validated;
	QDir().mkpath(FontsDirPath());
	QFile f(ChatStyleFilePath());
	if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
		f.write(gActiveChatStyle.toUtf8());
	}
}

QString TransformChatText(const QString &text, const QString &styleId) {
	if (styleId.isEmpty() || text.isEmpty()) {
		return text;
	}
	if (styleId == u"stars"_q) {
		return u"✦ "_q + TransformTokens(text, u"smallcaps"_q) + u" ✦"_q;
	}
	if (styleId == u"brackets"_q) {
		return u"『"_q + text + u"』"_q;
	}
	if (styleId == u"mirror"_q) {
		return ContainsSpecialToken(text)
			? TransformTokens(text, styleId)
			: ReverseCodePoints(text);
	}
	if (styleId == u"upside"_q) {
		return ContainsSpecialToken(text)
			? TransformTokens(text, styleId)
			: FlipUpsideDown(text);
	}
	return TransformTokens(text, styleId);
}

void TransformOutgoingMessage(Api::MessageToSend &message) {
	EnsureChatStyleLoaded();
	if (gActiveChatStyle.isEmpty()) {
		return;
	}
	const auto &src = message.textWithTags.text;
	const auto trimmed = src.trimmed();
	if (trimmed.isEmpty()
		|| trimmed.startsWith(u'.')
		|| trimmed.startsWith(u'/')) {
		return;
	}

	if (message.textWithTags.tags.isEmpty()) {
		message.textWithTags.text = TransformChatText(src, gActiveChatStyle);
		return;
	}

	if (gActiveChatStyle == u"mirror"_q || gActiveChatStyle == u"upside"_q) {
		return;
	}

	std::vector<bool> protectedMask(size_t(src.size()), false);
	for (const auto &tag : message.textWithTags.tags) {
		const bool keepRaw = tag.id.startsWith(u"custom-emoji"_q)
			|| tag.id.startsWith(u"mention"_q)
			|| tag.id.startsWith(u"code"_q)
			|| tag.id.startsWith(u"pre"_q)
			|| tag.id.startsWith(u"http"_q);
		if (keepRaw) {
			const auto start = std::clamp(tag.offset, 0, int(src.size()));
			const auto end = std::clamp(tag.offset + tag.length, start, int(src.size()));
			for (int p = start; p < end; ++p) {
				protectedMask[size_t(p)] = true;
			}
		}
	}

	QString out;
	out.reserve(src.size() * 2);
	std::vector<int> indexMap(size_t(src.size()) + 1, 0);
	int i = 0;
	int fenceIdx = 0;
	while (i < src.size()) {
		indexMap[size_t(i)] = int(out.size());
		const auto ch = src[i];
		if (ch.isHighSurrogate() && i + 1 < src.size()) {
			out.append(ch);
			out.append(src[i + 1]);
			indexMap[size_t(i + 1)] = int(out.size());
			i += 2;
			continue;
		}
		if (protectedMask[size_t(i)]) {
			out.append(ch);
			++i;
			continue;
		}
		out.append(TransformSingleChar(ch, gActiveChatStyle, fenceIdx));
		if (ch.isLetter()) {
			++fenceIdx;
		}
		++i;
	}
	indexMap[size_t(src.size())] = int(out.size());

	for (auto &tag : message.textWithTags.tags) {
		const auto oldStart = std::clamp(tag.offset, 0, int(src.size()));
		const auto oldEnd = std::clamp(tag.offset + tag.length, oldStart, int(src.size()));
		tag.offset = indexMap[size_t(oldStart)];
		tag.length = std::max(0, indexMap[size_t(oldEnd)] - indexMap[size_t(oldStart)]);
	}
	message.textWithTags.text = out;
}

bool HandleFontCommand(
		not_null<Window::SessionController*> controller,
		Api::MessageToSend &message,
		const QString &cmdLower,
		const QString &rest) {
	RegisterBundledAndCustomFonts();

	if (cmdLower == u".font"_q || cmdLower == u".fonts"_q) {
		const auto clean = rest.trimmed();
		if (clean.isEmpty()) {
			ShowFontsStudioBox(controller, 0);
			return true;
		}
		const auto lower = clean.toLower();
		if (lower == u"off"_q || lower == u"0"_q || lower == u"выкл"_q || lower == u"сброс"_q || lower == u"reset"_q) {
			SetActiveChatTextStyleId(QString());
			controller->showToast(u"MediaTok: авто-шрифт сообщений ВЫКЛ"_q);
			return true;
		}
		if (lower == u"list"_q || lower == u"стили"_q || lower == u"список"_q) {
			ShowFontsStudioBox(controller, 1);
			return true;
		}
		if (const auto *style = FindChatStyle(lower)) {
			const auto id = QString::fromLatin1(style->id);
			SetActiveChatTextStyleId(id);
			controller->showToast(
				u"MediaTok: авто-шрифт «%1» ВКЛ (%2)"_q
					.arg(QString::fromUtf8(style->name))
					.arg(TransformChatText(u"Привет / Hello"_q, id)));
			return true;
		}
		for (const auto &bf : kBundledFonts) {
			const auto slug = QString::fromLatin1(bf.slug);
			const auto name = QString::fromUtf8(bf.displayName);
			if (slug == lower || name.compare(clean, Qt::CaseInsensitive) == 0 || slug.startsWith(lower)) {
				const auto fam = gBundledSlugToFamily.value(slug, name);
				ApplyClientFontAndPromptRestart(controller, fam, name);
				return true;
			}
		}
		controller->showToast(u"Стиль «%1» не найден. Введите .font для Студии Шрифтов"_q.arg(clean));
		return true;
	}

	if (cmdLower == u".f"_q) {
		const auto clean = rest.trimmed();
		if (clean.isEmpty()) {
			ShowFontsStudioBox(controller, 1);
			return true;
		}
		const auto sp = clean.indexOf(QRegularExpression(u"\\s"_q));
		if (sp < 0) {
			controller->showToast(u"Формат: .f <стиль> <текст> (напр. .f cyber Привет!)"_q);
			return true;
		}
		const auto styleQuery = clean.left(sp).trimmed().toLower();
		const auto body = clean.mid(sp).trimmed();
		const auto *style = FindChatStyle(styleQuery);
		if (!style) {
			controller->showToast(u"Стиль «%1» не найден. Введите .font list"_q.arg(styleQuery));
			return true;
		}
		auto styledMsg = message;
		styledMsg.textWithTags.text = TransformChatText(body, QString::fromLatin1(style->id));
		styledMsg.textWithTags.tags.clear();
		styledMsg.action.clearDraft = false;
		styledMsg.action.generateLocal = true;
		message.action.history->session().api().sendMessage(std::move(styledMsg));
		return true;
	}

	if (cmdLower == u".color"_q || cmdLower == u".col"_q) {
		const auto clean = rest.trimmed();
		const auto sp = clean.indexOf(QRegularExpression(u"\\s"_q));
		if (clean.isEmpty() || sp < 0) {
			controller->showToast(u"Формат: .color <neon|gold|fire|cyber|ice|toxic|sunset|rainbow|red|pink|blue|green|#HEX,#HEX> <текст>"_q);
			return true;
		}
		const auto spec = clean.left(sp).trimmed().toLower();
		const auto body = clean.mid(sp).trimmed();
		std::vector<QColor> colors;
		if (spec == u"neon"_q || spec == u"неон"_q) colors = { QColor(0xFF,0x00,0xCC), QColor(0x33,0x33,0xFF), QColor(0x00,0xF5,0xFF) };
		else if (spec == u"gold"_q || spec == u"золото"_q) colors = { QColor(0xFF,0xF3,0xB0), QColor(0xFF,0xB7,0x03), QColor(0xFB,0x85,0x00) };
		else if (spec == u"fire"_q || spec == u"огонь"_q) colors = { QColor(0xFF,0xEA,0x00), QColor(0xFF,0x6D,0x00), QColor(0xD5,0x00,0x00) };
		else if (spec == u"cyber"_q || spec == u"кибер"_q) colors = { QColor(0x00,0xF5,0xD4), QColor(0x00,0xBB,0xF9), QColor(0x9B,0x5D,0xE5) };
		else if (spec == u"ice"_q || spec == u"лёд"_q || spec == u"лед"_q) colors = { QColor(0xE0,0xFB,0xFC), QColor(0x48,0xCA,0xE4), QColor(0x00,0x77,0xB6) };
		else if (spec == u"toxic"_q || spec == u"токсик"_q) colors = { QColor(0xCC,0xFF,0x00), QColor(0x00,0xE6,0x76), QColor(0x00,0xBF,0xA5) };
		else if (spec == u"sunset"_q || spec == u"закат"_q) colors = { QColor(0xFF,0x5F,0x6D), QColor(0xFF,0xC3,0x71) };
		else if (spec == u"rainbow"_q || spec == u"радуга"_q) colors = { QColor(0xFF,0x3B,0x30), QColor(0xFF,0x95,0x00), QColor(0xFF,0xCC,0x00), QColor(0x34,0xC7,0x59), QColor(0x00,0x7A,0xFF), QColor(0xAF,0x52,0xDE) };
		else if (spec == u"red"_q || spec == u"красный"_q) colors = { QColor(0xFF,0x4D,0x6D), QColor(0xC9,0x18,0x4A) };
		else if (spec == u"pink"_q || spec == u"розовый"_q) colors = { QColor(0xFF,0x99,0xC8), QColor(0xFF,0x00,0x6E) };
		else if (spec == u"blue"_q || spec == u"синий"_q) colors = { QColor(0x4C,0xC9,0xF0), QColor(0x43,0x61,0xEE) };
		else if (spec == u"green"_q || spec == u"зелёный"_q || spec == u"зеленый"_q) colors = { QColor(0x80,0xED,0x99), QColor(0x38,0xB0,0x00) };
		else if (spec == u"purple"_q || spec == u"фиолетовый"_q) colors = { QColor(0xC7,0x7D,0xFF), QColor(0x7B,0x2C,0xBF) };
		else if (spec == u"white"_q || spec == u"белый"_q) colors = { QColor(0xFF,0xFF,0xFF), QColor(0xE2,0xE8,0xF0) };
		else {
			for (const auto &part : spec.split(u',')) {
				const auto s = part.trimmed();
				QColor c(s.startsWith(u'#') ? s : (u"#"_q + s));
				if (c.isValid()) colors.push_back(c);
			}
			if (colors.empty()) colors = { QColor(0xFF,0x00,0xCC), QColor(0x00,0xF5,0xFF) };
			if (colors.size() == 1) colors.push_back(colors[0]);
		}

		const auto fam = Core::App().settings().customFontFamily();
		QFont font(fam.isEmpty() ? gBundledSlugToFamily.value(u"unbounded"_q, u"Segoe UI"_q) : fam);
		font.setBold(true);
		int pxSize = (body.size() <= 10) ? 84 : (body.size() <= 24 ? 64 : (body.size() <= 50 ? 48 : 36));
		constexpr int kW = 512;
		constexpr int kPad = 28;
		QRect textRect;
		while (pxSize >= 22) {
			font.setPixelSize(pxSize);
			textRect = QFontMetrics(font).boundingRect(QRect(0, 0, kW - kPad * 2, 1000), Qt::AlignCenter | Qt::TextWordWrap, body);
			if (textRect.height() <= 440) break;
			pxSize -= 6;
		}
		const int h = std::clamp(textRect.height() + kPad * 2, 120, 512);
		QImage img(kW, h, QImage::Format_ARGB32_Premultiplied);
		img.fill(Qt::transparent);
		{
			QPainter p(&img);
			p.setRenderHint(QPainter::Antialiasing, true);
			p.setRenderHint(QPainter::TextAntialiasing, true);
			p.setFont(font);
			const QRect drawRect(kPad, (h - textRect.height()) / 2, kW - kPad * 2, textRect.height());
			// Soft neon glow + dark outline
			QColor glow = colors.front();
			glow.setAlpha(90);
			p.setPen(glow);
			for (int dx = -4; dx <= 4; dx += 2) {
				for (int dy = -4; dy <= 4; dy += 2) {
					p.drawText(drawRect.translated(dx, dy), Qt::AlignCenter | Qt::TextWordWrap, body);
				}
			}
			p.setPen(QColor(0, 0, 0, 220));
			for (int dx = -2; dx <= 2; ++dx) {
				for (int dy = -2; dy <= 2; ++dy) {
					if (dx != 0 || dy != 0) p.drawText(drawRect.translated(dx, dy), Qt::AlignCenter | Qt::TextWordWrap, body);
				}
			}
			QLinearGradient grad(drawRect.topLeft(), drawRect.bottomRight());
			for (size_t i = 0; i < colors.size(); ++i) {
				grad.setColorAt(colors.size() == 1 ? 0.0 : double(i) / double(colors.size() - 1), colors[i]);
			}
			p.setPen(QPen(QBrush(grad), 1.0));
			p.drawText(drawRect, Qt::AlignCenter | Qt::TextWordWrap, body);
		}

		QDir().mkpath(FontsDirPath() + u"/stickers"_q);
		const auto outPath = FontsDirPath() + u"/stickers/mt_col_%1.webp"_q.arg(QDateTime::currentMSecsSinceEpoch());
		if (!img.save(outPath, "WEBP", 95)) {
			controller->showToast(u"MediaTok: не удалось сохранить цветной стикер"_q);
			return true;
		}
		const auto session = &message.action.history->session();
		const auto premium = session->user()->isPremium();
		auto list = Storage::PrepareMediaList(QStringList{ outPath }, st::sendMediaPreviewSize, premium);
		if (list.error == Ui::PreparedList::Error::None && !list.files.empty()) {
			auto sendAction = message.action;
			sendAction.clearDraft = false;
			sendAction.generateLocal = true;
			session->api().sendFiles(std::move(list), SendMediaType::File, nullptr, sendAction);
		}
		return true;
	}

	return false;
}

void ShowFontsStudioBox(
		not_null<Window::SessionController*> controller,
		int initialTab) {
	RegisterBundledAndCustomFonts();
	const auto weak = base::make_weak(controller);

	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setWidth(st::boxWideWidth);
		box->setTitle(rpl::single(u"Шрифты MediaTok (80+ клиента • .ttf редактор • 32 стиля чата)"_q));

		struct State {
			int tab = 0;
			QString sampleText = u"Привет, MediaTok! Hello 123"_q;
			QString searchQuery;
		};
		const auto state = box->lifetime().make_state<State>();
		state->tab = (initialTab == 1) ? 1 : 0;

		const auto topWrap = box->setPinnedToTopContent(
			object_ptr<Ui::VerticalLayout>(box));

		const auto tabSwitcherLabel = [=] {
			return (state->tab == 0)
				? u"Переключить на: Стили текста сообщений (32, видят все)"_q
				: u"Переключить на: Шрифты клиента (%1 + свой .ttf/.otf)"_q
					.arg(int(kBundledFonts.size() + gCustomFonts.size()));
		};
		const auto switchVar = box->lifetime().make_state<rpl::variable<QString>>(
			tabSwitcherLabel());

		const auto switchBtn = Settings::AddButtonWithLabel(
			topWrap,
			rpl::single(u"Режим студии"_q),
			switchVar->value(),
			st::settingsButton,
			{ &st::menuIconFont });

		const auto filterInput = topWrap->add(
			object_ptr<Ui::InputField>(
				topWrap,
				st::defaultInputField,
				rpl::single(state->tab == 0
					? u"Поиск шрифта (Montserrat, Pixel, Mono...)"_q
					: u"Текст для живого предпросмотра стилей..."_q)),
			QMargins(16, 4, 16, 8));
		if (state->tab == 1) {
			filterInput->setText(state->sampleText);
		}

		const auto listWrap = box->verticalLayout()->add(
			object_ptr<Ui::VerticalLayout>(box->verticalLayout()));

		const QPointer<Ui::GenericBox> weakBox = box.get();
		const auto rebuild = std::make_shared<Fn<void()>>();
		box->lifetime().add([=] {
			*rebuild = nullptr;
		});
		*rebuild = [=] {
			if (!weakBox) {
				return;
			}
			while (listWrap->count() > 0) {
				delete listWrap->widgetAt(0);
			}
			const auto strong = weak.get();
			if (!strong) {
				return;
			}

			if (state->tab == 0) {
				const auto currentFam = Core::App().settings().customFontFamily();

				Settings::AddButtonWithLabel(
					listWrap,
					rpl::single(u"✦ Создать свой шрифт (.ttf) — рисовать буквы"_q),
					rpl::single(u"Конструктор"_q),
					st::settingsButton,
					{ &st::menuIconEdit }
				)->setClickedCallback([=] {
					if (const auto c = weak.get()) {
						ShowTtfCreatorBox(c);
					}
				});

				Settings::AddButtonWithLabel(
					listWrap,
					rpl::single(u"+ Загрузить свой .ttf / .otf шрифт..."_q),
					rpl::single(u"Импорт"_q),
					st::settingsButton,
					{ &st::menuIconEdit }
				)->setClickedCallback([=] {
					if (!weakBox) {
						return;
					}
					FileDialog::GetOpenPath(
						weakBox.data(),
						u"Выберите файл шрифта (.ttf или .otf)"_q,
						u"Font Files (*.ttf *.otf)"_q,
						[=](FileDialog::OpenResult &&res) {
							if (res.paths.isEmpty()) {
								return;
							}
							const auto srcPath = res.paths.front();
							if (!IsValidFontMagic(srcPath)) {
								if (const auto c = weak.get()) {
									c->showToast(u"Это не корректный .ttf / .otf файл"_q);
								}
								return;
							}
							QDir().mkpath(FontsDirPath());
							const auto fi = QFileInfo(srcPath);
							const auto dstPath = FontsDirPath() + u"/"_q + fi.fileName();
							QFile::remove(dstPath);
							if (!QFile::copy(srcPath, dstPath)) {
								if (const auto c = weak.get()) {
									c->showToast(u"Не удалось скопировать шрифт в tdata/MediaTok_Fonts"_q);
								}
								return;
							}
							const int fid = QFontDatabase::addApplicationFont(dstPath);
							const auto fams = (fid >= 0) ? QFontDatabase::applicationFontFamilies(fid) : QStringList();
							if (fams.isEmpty()) {
								QFile::remove(dstPath);
								if (const auto c = weak.get()) {
									c->showToast(u"Qt не смог загрузить этот шрифт"_q);
								}
								return;
							}
							RegisterSingleCustomFont(dstPath);
							if (const auto c = weak.get()) {
								ApplyClientFontAndPromptRestart(c, fams.first(), fi.completeBaseName());
							}
							if (weakBox && *rebuild) {
								(*rebuild)();
							}
						});
				});

				Settings::AddButtonWithLabel(
					listWrap,
					rpl::single(u"Системный выбор шрифта (с предпросмотром чата)"_q),
					rpl::single(currentFam.isEmpty() ? u"Стандартный"_q : currentFam),
					st::settingsButton,
					{ &st::menuIconFont }
				)->setClickedCallback([=] {
					if (const auto c = weak.get()) {
						OpenNativeChooseFontBox(c);
					}
				});

				if (!currentFam.isEmpty()) {
					Settings::AddButtonWithLabel(
						listWrap,
						rpl::single(u"Сбросить шрифт клиента"_q),
						rpl::single(u"Стандартный"_q),
						st::settingsButton
					)->setClickedCallback([=] {
						if (const auto c = weak.get()) {
							ApplyClientFontAndPromptRestart(c, QString(), u"Стандартный"_q);
						}
					});
				}

				const auto query = state->searchQuery.trimmed().toLower();

				if (!gCustomFonts.empty()) {
					bool addedHeader = false;
					for (const auto &cf : gCustomFonts) {
						if (!query.isEmpty()
							&& !cf.displayName.toLower().contains(query)
							&& !cf.qtFamily.toLower().contains(query)) {
							continue;
						}
						if (!addedHeader) {
							addedHeader = true;
							Ui::AddSkip(listWrap);
							Ui::AddSubsectionTitle(listWrap, rpl::single(u"Мои импортированные шрифты"_q));
						}
						const bool selected = (currentFam.compare(cf.qtFamily, Qt::CaseInsensitive) == 0);
						const auto row = listWrap->add(object_ptr<FontPreviewRow>(
							listWrap,
							cf.displayName,
							u"Привет, MediaTok! AaBbCc 0123"_q,
							cf.qtFamily,
							selected));
						row->setClickedCallback([=, fam = cf.qtFamily, name = cf.displayName] {
							if (const auto c = weak.get()) {
								ApplyClientFontAndPromptRestart(c, fam, name);
							}
						});
					}
				}

				QString lastCat;
				for (const auto &bf : kBundledFonts) {
					const auto slug = QString::fromLatin1(bf.slug);
					const auto name = QString::fromUtf8(bf.displayName);
					const auto cat = QString::fromUtf8(bf.category);
					if (!query.isEmpty()
						&& !name.toLower().contains(query)
						&& !cat.toLower().contains(query)
						&& !slug.contains(query)) {
						continue;
					}
					if (cat != lastCat) {
						lastCat = cat;
						Ui::AddSkip(listWrap);
						Ui::AddSubsectionTitle(listWrap, rpl::single(cat));
					}
					const auto qtFam = gBundledSlugToFamily.value(slug, name);
					const bool selected = (currentFam.compare(qtFam, Qt::CaseInsensitive) == 0);
					const auto row = listWrap->add(object_ptr<FontPreviewRow>(
						listWrap,
						name,
						u"Привет, MediaTok! AaBbCc 0123"_q,
						qtFam,
						selected));
					row->setClickedCallback([=] {
						if (const auto c = weak.get()) {
							ApplyClientFontAndPromptRestart(c, qtFam, name);
						}
					});
				}
			} else {
				const auto activeId = ActiveChatTextStyleId();
				const auto sample = state->sampleText.trimmed().isEmpty()
					? u"Привет, MediaTok! Hello 123"_q
					: state->sampleText;

				Settings::AddButtonWithLabel(
					listWrap,
					rpl::single(u"Авто-стиль исходящих сообщений"_q),
					rpl::single(activeId.isEmpty() ? u"Выкл"_q : ActiveChatTextStyleName() + u" (нажмите чтобы выкл)"_q),
					st::settingsButton,
					{ &st::menuIconFont }
				)->setClickedCallback([=] {
					SetActiveChatTextStyleId(QString());
					if (const auto c = weak.get()) {
						c->showToast(u"MediaTok: авто-стиль сообщений выключен"_q);
					}
					(*rebuild)();
				});

				Ui::AddDividerText(
					listWrap,
					rpl::single(u"Нажмите на стиль, чтобы включить авто-стилизацию всех исходящих сообщений (видят ВСЕ в Telegram). Разовая отправка в чате: .f <стиль> <текст>"_q));

				QString lastCat;
				for (const auto &stInfo : kChatStyles) {
					const auto id = QString::fromLatin1(stInfo.id);
					const auto name = QString::fromUtf8(stInfo.name);
					const auto cat = QString::fromUtf8(stInfo.category);
					if (cat != lastCat) {
						lastCat = cat;
						Ui::AddSkip(listWrap);
						Ui::AddSubsectionTitle(listWrap, rpl::single(cat));
					}
					const bool selected = (activeId == id);
					const auto rendered = TransformChatText(sample, id);
					const auto row = listWrap->add(object_ptr<FontPreviewRow>(
						listWrap,
						u"%1  (.f %2)"_q.arg(name, id),
						rendered,
						QString(),
						selected));
					row->setClickedCallback([=] {
						if (ActiveChatTextStyleId() == id) {
							SetActiveChatTextStyleId(QString());
							if (const auto c = weak.get()) {
								c->showToast(u"Авто-стиль выключен"_q);
							}
						} else {
							SetActiveChatTextStyleId(id);
							if (auto *cb = QGuiApplication::clipboard()) {
								cb->setText(rendered);
							}
							if (const auto c = weak.get()) {
								c->showToast(u"Авто-стиль «%1» ВКЛ (пример скопирован)"_q.arg(name));
							}
						}
						(*rebuild)();
					});
				}
			}
			listWrap->resizeToWidth(box->width());
		};

		switchBtn->setClickedCallback([=] {
			state->tab = (state->tab == 0) ? 1 : 0;
			*switchVar = tabSwitcherLabel();
			if (state->tab == 0) {
				filterInput->setPlaceholder(rpl::single(u"Поиск шрифта (Montserrat, Pixel, Mono...)"_q));
				filterInput->setText(state->searchQuery);
			} else {
				filterInput->setPlaceholder(rpl::single(u"Текст для живого предпросмотра стилей..."_q));
				filterInput->setText(state->sampleText);
			}
			(*rebuild)();
		});

		filterInput->changes() | rpl::on_next([=] {
			const auto txt = filterInput->getLastText();
			if (state->tab == 0) {
				if (txt != state->searchQuery) {
					state->searchQuery = txt;
					(*rebuild)();
				}
			} else {
				if (txt != state->sampleText) {
					state->sampleText = txt;
					(*rebuild)();
				}
			}
		}, filterInput->lifetime());

		(*rebuild)();

		box->addButton(rpl::single(u"Закрыть"_q), [=] {
			box->closeBox();
		});
	}));
}

namespace {

constexpr int kGridW = 10;
constexpr int kGridH = 12;

struct TtfRectI {
	int x1, y1, x2, y2;
};

void WriteU16BE(QByteArray &b, uint16_t v) {
	b.append(char((v >> 8) & 0xFF));
	b.append(char(v & 0xFF));
}

void WriteS16BE(QByteArray &b, int16_t v) {
	WriteU16BE(b, uint16_t(v));
}

void WriteU32BE(QByteArray &b, uint32_t v) {
	b.append(char((v >> 24) & 0xFF));
	b.append(char((v >> 16) & 0xFF));
	b.append(char((v >> 8) & 0xFF));
	b.append(char(v & 0xFF));
}

uint32_t CalcTtfChecksum(const QByteArray &data) {
	uint32_t sum = 0;
	const qsizetype n = data.size();
	for (qsizetype i = 0; i < n; i += 4) {
		uint32_t v = (uint32_t(uchar(data[i])) << 24);
		if (i + 1 < n) v |= (uint32_t(uchar(data[i + 1])) << 16);
		if (i + 2 < n) v |= (uint32_t(uchar(data[i + 2])) << 8);
		if (i + 3 < n) v |= uint32_t(uchar(data[i + 3]));
		sum += v;
	}
	return sum;
}

QByteArray BuildSimpleGlyphBytes(const std::vector<TtfRectI> &rects) {
	if (rects.empty()) return {};
	int xMin = 32767, yMin = 32767, xMax = -32768, yMax = -32768;
	for (const auto &r : rects) {
		xMin = std::min({ xMin, r.x1, r.x2 });
		xMax = std::max({ xMax, r.x1, r.x2 });
		yMin = std::min({ yMin, r.y1, r.y2 });
		yMax = std::max({ yMax, r.y1, r.y2 });
	}
	QByteArray out;
	WriteS16BE(out, int16_t(rects.size()));
	WriteS16BE(out, int16_t(xMin));
	WriteS16BE(out, int16_t(yMin));
	WriteS16BE(out, int16_t(xMax));
	WriteS16BE(out, int16_t(yMax));
	for (size_t i = 0; i < rects.size(); ++i) {
		WriteU16BE(out, uint16_t(i * 4 + 3));
	}
	WriteU16BE(out, 0);
	const size_t nPts = rects.size() * 4;
	out.append(qsizetype(nPts), char(1));
	int prevX = 0;
	for (const auto &r : rects) {
		const int xs[4] = { r.x1, r.x1, r.x2, r.x2 };
		for (int x : xs) {
			WriteS16BE(out, int16_t(x - prevX));
			prevX = x;
		}
	}
	int prevY = 0;
	for (const auto &r : rects) {
		const int ys[4] = { r.y1, r.y2, r.y2, r.y1 };
		for (int y : ys) {
			WriteS16BE(out, int16_t(y - prevY));
			prevY = y;
		}
	}
	while (out.size() % 4 != 0) out.append('\0');
	return out;
}

QByteArray CompileCustomTtf(
		const QString &fontNameRaw,
		const std::map<char16_t, std::array<bool, kGridW * kGridH>> &grids,
		int pixelStyle) {
	const auto fontName = fontNameRaw.trimmed().isEmpty() ? u"MediaTok Custom"_q : fontNameRaw.trimmed();
	const int cellW = (pixelStyle == 2) ? 82 : 76;
	const int cellH = 82;
	const int gap = (pixelStyle == 1) ? 10 : 0;
	const int slantPerRow = (pixelStyle == 3) ? 14 : 0;
	constexpr int kBaselineRow = 9;

	std::map<uint16_t, std::vector<TtfRectI>> charRects;
	charRects[32] = {};
	for (const auto &[ch, grid] : grids) {
		if (ch == 32) continue;
		std::vector<TtfRectI> rects;
		for (int row = 0; row < kGridH; ++row) {
			const int yBottom = (kBaselineRow - row) * cellH + gap / 2;
			const int yTop = (kBaselineRow - row + 1) * cellH - gap / 2;
			const int slant = (kBaselineRow - row) * slantPerRow;
			int col = 0;
			while (col < kGridW) {
				if (!grid[size_t(row * kGridW + col)]) { ++col; continue; }
				const int startCol = col;
				if (pixelStyle == 1) {
					rects.push_back({ 40 + startCol * cellW + gap / 2 + slant, yBottom, 40 + (startCol + 1) * cellW - gap / 2 + slant, yTop });
					++col;
				} else {
					while (col < kGridW && grid[size_t(row * kGridW + col)]) ++col;
					rects.push_back({ 40 + startCol * cellW + slant, yBottom, 40 + col * cellW + slant, yTop });
				}
			}
		}
		charRects[uint16_t(ch)] = std::move(rects);
	}

	std::vector<uint16_t> cps;
	for (const auto &[cp, _] : charRects) {
		if (cp != 32) cps.push_back(cp);
	}
	std::sort(cps.begin(), cps.end());

	std::vector<uint16_t> glyphOrder;
	glyphOrder.push_back(0);
	glyphOrder.push_back(32);
	glyphOrder.insert(glyphOrder.end(), cps.begin(), cps.end());

	std::map<uint16_t, uint16_t> cpToGid;
	cpToGid[32] = 1;
	for (size_t i = 0; i < cps.size(); ++i) cpToGid[cps[i]] = uint16_t(i + 2);
	const uint16_t numGlyphs = uint16_t(glyphOrder.size());

	QByteArray glyf, loca, hmtx;
	uint16_t maxPoints = 0, maxContours = 0;
	for (uint16_t gid = 0; gid < numGlyphs; ++gid) {
		WriteU32BE(loca, uint32_t(glyf.size()));
		std::vector<TtfRectI> rects;
		uint16_t adv = 400;
		int16_t lsb = 0;
		if (gid == 0) {
			rects = { { 80, 0, 520, 720 } };
			adv = 600; lsb = 80;
		} else if (glyphOrder[gid] != 32) {
			rects = charRects[glyphOrder[gid]];
			if (!rects.empty()) {
				int xMin = 32767, xMax = -32768;
				for (const auto &r : rects) {
					xMin = std::min({ xMin, r.x1, r.x2 });
					xMax = std::max({ xMax, r.x1, r.x2 });
				}
				adv = uint16_t(std::max(440, xMax + 70));
				lsb = int16_t(xMin);
			}
		}
		glyf.append(BuildSimpleGlyphBytes(rects));
		WriteU16BE(hmtx, adv);
		WriteS16BE(hmtx, lsb);
		maxPoints = std::max(maxPoints, uint16_t(rects.size() * 4));
		maxContours = std::max(maxContours, uint16_t(rects.size()));
	}
	WriteU32BE(loca, uint32_t(glyf.size()));

	struct CmapSeg { uint16_t start, end, delta; };
	std::vector<CmapSeg> segs;
	for (const auto &[cp, gid] : cpToGid) {
		const uint16_t delta = uint16_t((int(gid) - int(cp)) & 0xFFFF);
		if (!segs.empty() && cp == segs.back().end + 1 && delta == segs.back().delta) {
			segs.back().end = cp;
		} else {
			segs.push_back({ cp, cp, delta });
		}
	}
	segs.push_back({ 0xFFFF, 0xFFFF, 1 });
	const uint16_t segCount = uint16_t(segs.size());
	uint16_t pow2 = 1, es = 0;
	while (pow2 * 2 <= segCount) { pow2 *= 2; ++es; }
	const uint16_t sr = pow2 * 2;
	const uint16_t rs = segCount * 2 - sr;

	QByteArray fmt4;
	WriteU16BE(fmt4, 4);
	WriteU16BE(fmt4, uint16_t(16 + segCount * 8));
	WriteU16BE(fmt4, 0);
	WriteU16BE(fmt4, segCount * 2);
	WriteU16BE(fmt4, sr);
	WriteU16BE(fmt4, es);
	WriteU16BE(fmt4, rs);
	for (const auto &s : segs) WriteU16BE(fmt4, s.end);
	WriteU16BE(fmt4, 0);
	for (const auto &s : segs) WriteU16BE(fmt4, s.start);
	for (const auto &s : segs) WriteU16BE(fmt4, s.delta);
	for (size_t i = 0; i < segs.size(); ++i) WriteU16BE(fmt4, 0);

	QByteArray cmap;
	WriteU16BE(cmap, 0); WriteU16BE(cmap, 2);
	WriteU16BE(cmap, 0); WriteU16BE(cmap, 3); WriteU32BE(cmap, 20);
	WriteU16BE(cmap, 3); WriteU16BE(cmap, 1); WriteU32BE(cmap, 20);
	cmap.append(fmt4);

	QString safePs;
	for (const auto ch : fontName) {
		if (ch.unicode() < 128 && ch.isLetterOrNumber()) safePs.append(ch);
	}
	if (safePs.isEmpty()) safePs = u"MediaTokCustom"_q;
	const std::array<std::pair<uint16_t, QString>, 6> namePairs = {{
		{ 1, fontName }, { 2, u"Regular"_q }, { 3, u"MediaTok-"_q + safePs },
		{ 4, fontName }, { 5, u"Version 1.0"_q }, { 6, safePs },
	}};
	struct NameRec { uint16_t pid, eid, lid, nid, len, off; };
	std::vector<NameRec> recs;
	QByteArray strBlob;
	for (const auto &[nid, s] : namePairs) {
		QByteArray b;
		for (const auto ch : s) b.append((ch.unicode() >= 32 && ch.unicode() <= 126) ? char(ch.unicode()) : '?');
		recs.push_back({ 1, 0, 0, nid, uint16_t(b.size()), uint16_t(strBlob.size()) });
		strBlob.append(b);
	}
	for (const auto &[nid, s] : namePairs) {
		QByteArray b;
		for (const auto ch : s) WriteU16BE(b, ch.unicode());
		recs.push_back({ 3, 1, 0x0409, nid, uint16_t(b.size()), uint16_t(strBlob.size()) });
		strBlob.append(b);
	}
	std::sort(recs.begin(), recs.end(), [](const NameRec &a, const NameRec &b) {
		return std::tie(a.pid, a.eid, a.lid, a.nid) < std::tie(b.pid, b.eid, b.lid, b.nid);
	});
	QByteArray nameTbl;
	WriteU16BE(nameTbl, 0);
	WriteU16BE(nameTbl, uint16_t(recs.size()));
	WriteU16BE(nameTbl, uint16_t(6 + recs.size() * 12));
	for (const auto &r : recs) {
		WriteU16BE(nameTbl, r.pid); WriteU16BE(nameTbl, r.eid); WriteU16BE(nameTbl, r.lid);
		WriteU16BE(nameTbl, r.nid); WriteU16BE(nameTbl, r.len); WriteU16BE(nameTbl, r.off);
	}
	nameTbl.append(strBlob);

	QByteArray head;
	WriteU32BE(head, 0x00010000); WriteU32BE(head, 0x00010000);
	WriteU32BE(head, 0); WriteU32BE(head, 0x5F0F3CF5);
	WriteU16BE(head, 0x000B); WriteU16BE(head, 1000);
	WriteU32BE(head, 0); WriteU32BE(head, 0); WriteU32BE(head, 0); WriteU32BE(head, 0);
	WriteS16BE(head, 0); WriteS16BE(head, -200); WriteS16BE(head, 1000); WriteS16BE(head, 900);
	WriteU16BE(head, 0); WriteU16BE(head, 8); WriteS16BE(head, 2); WriteS16BE(head, 1); WriteS16BE(head, 0);

	QByteArray hhea;
	WriteU32BE(hhea, 0x00010000);
	WriteS16BE(hhea, 850); WriteS16BE(hhea, -200); WriteS16BE(hhea, 50); WriteU16BE(hhea, 1000);
	WriteS16BE(hhea, 0); WriteS16BE(hhea, 0); WriteS16BE(hhea, 1000); WriteS16BE(hhea, 1);
	for (int i = 0; i < 7; ++i) WriteS16BE(hhea, 0);
	WriteU16BE(hhea, numGlyphs);

	QByteArray maxp;
	WriteU32BE(maxp, 0x00010000);
	WriteU16BE(maxp, numGlyphs); WriteU16BE(maxp, maxPoints); WriteU16BE(maxp, maxContours);
	WriteU16BE(maxp, 0); WriteU16BE(maxp, 0); WriteU16BE(maxp, 2);
	for (int i = 0; i < 8; ++i) WriteU16BE(maxp, 0);

	QByteArray os2;
	WriteU16BE(os2, 3); WriteS16BE(os2, 600); WriteU16BE(os2, 400); WriteU16BE(os2, 5); WriteS16BE(os2, 0);
	WriteS16BE(os2, 650); WriteS16BE(os2, 600); WriteS16BE(os2, 0); WriteS16BE(os2, 75);
	WriteS16BE(os2, 650); WriteS16BE(os2, 600); WriteS16BE(os2, 0); WriteS16BE(os2, 350);
	WriteS16BE(os2, 50); WriteS16BE(os2, 300); WriteS16BE(os2, 0);
	const char panose[10] = { 2, 0, 5, 3, 0, 0, 0, 0, 0, 0 };
	os2.append(panose, 10);
	WriteU32BE(os2, 0x00000201); WriteU32BE(os2, 0); WriteU32BE(os2, 0); WriteU32BE(os2, 0);
	os2.append("MTOK", 4);
	WriteU16BE(os2, 0x0040); WriteU16BE(os2, 32);
	WriteU16BE(os2, cpToGid.empty() ? 0x04FF : cpToGid.rbegin()->first);
	WriteS16BE(os2, 850); WriteS16BE(os2, -200); WriteS16BE(os2, 50);
	WriteU16BE(os2, 900); WriteU16BE(os2, 250);
	WriteU32BE(os2, 0x00000005); WriteU32BE(os2, 0);
	WriteS16BE(os2, 500); WriteS16BE(os2, 720);
	WriteU16BE(os2, 0); WriteU16BE(os2, 32); WriteU16BE(os2, 0);

	QByteArray post;
	WriteU32BE(post, 0x00030000); WriteU32BE(post, 0);
	WriteS16BE(post, -100); WriteS16BE(post, 50);
	for (int i = 0; i < 5; ++i) WriteU32BE(post, 0);

	const std::map<QByteArray, QByteArray> tables = {
		{ "OS/2", os2 }, { "cmap", cmap }, { "glyf", glyf }, { "head", head },
		{ "hhea", hhea }, { "hmtx", hmtx }, { "loca", loca }, { "maxp", maxp },
		{ "name", nameTbl }, { "post", post },
	};
	const uint16_t nt = uint16_t(tables.size());
	uint16_t tpow2 = 1, tes = 0;
	while (tpow2 * 2 <= nt) { tpow2 *= 2; ++tes; }
	const uint16_t tsr = tpow2 * 16;
	const uint16_t trs = nt * 16 - tsr;

	QByteArray full;
	WriteU32BE(full, 0x00010000);
	WriteU16BE(full, nt); WriteU16BE(full, tsr); WriteU16BE(full, tes); WriteU16BE(full, trs);

	uint32_t offset = 12 + uint32_t(nt) * 16;
	qsizetype headOffset = 0;
	QByteArray body;
	for (const auto &[tag, data] : tables) {
		full.append(tag);
		WriteU32BE(full, CalcTtfChecksum(data));
		WriteU32BE(full, offset);
		WriteU32BE(full, uint32_t(data.size()));
		if (tag == "head") headOffset = qsizetype(offset);
		body.append(data);
		const int pad = (4 - (int(data.size()) % 4)) % 4;
		body.append(pad, '\0');
		offset += uint32_t(data.size() + pad);
	}
	full.append(body);
	const uint32_t adj = uint32_t(0xB1B0AFBAu - CalcTtfChecksum(full));
	full[headOffset + 8] = char((adj >> 24) & 0xFF);
	full[headOffset + 9] = char((adj >> 16) & 0xFF);
	full[headOffset + 10] = char((adj >> 8) & 0xFF);
	full[headOffset + 11] = char(adj & 0xFF);
	return full;
}

std::vector<char16_t> CreatorCharset() {
	std::vector<char16_t> chars;
	for (char16_t c = 0x0410; c <= 0x042F; ++c) chars.push_back(c); // А-Я
	chars.push_back(0x0401); // Ё
	for (char16_t c = 0x0430; c <= 0x044F; ++c) chars.push_back(c); // а-я
	chars.push_back(0x0451); // ё
	for (char16_t c = 'A'; c <= 'Z'; ++c) chars.push_back(c);
	for (char16_t c = 'a'; c <= 'z'; ++c) chars.push_back(c);
	for (char16_t c = '0'; c <= '9'; ++c) chars.push_back(c);
	const QString punct = u"!?.,:;+-=()[]@#%*\"'/_"_q;
	for (const auto ch : punct) chars.push_back(ch.unicode());
	return chars;
}

std::map<char16_t, std::array<bool, kGridW * kGridH>> RasterizeStarterGrids(const QString &baseFamily) {
	std::map<char16_t, std::array<bool, kGridW * kGridH>> result;
	QImage img(kGridW, kGridH, QImage::Format_ARGB32);
	QFont f = baseFamily.isEmpty() ? QGuiApplication::font() : QFont(baseFamily);
	f.setPixelSize(10);
	for (const auto ch : CreatorCharset()) {
		img.fill(Qt::transparent);
		{
			QPainter p(&img);
			p.setRenderHint(QPainter::TextAntialiasing, true);
			p.setFont(f);
			p.setPen(Qt::white);
			p.drawText(QRect(0, 0, kGridW, kGridH), Qt::AlignHCenter | Qt::AlignVCenter, QString(QChar(ch)));
		}
		std::array<bool, kGridW * kGridH> grid{};
		for (int y = 0; y < kGridH; ++y) {
			for (int x = 0; x < kGridW; ++x) {
				grid[size_t(y * kGridW + x)] = qAlpha(img.pixel(x, y)) >= 90;
			}
		}
		result[ch] = grid;
	}
	return result;
}

class GlyphCanvasWidget final : public Ui::RpWidget {
public:
	GlyphCanvasWidget(
		QWidget *parent,
		std::array<bool, kGridW * kGridH> *grid,
		Fn<void()> onChanged)
	: RpWidget(parent)
	, _grid(grid)
	, _onChanged(std::move(onChanged)) {
		setFixedSize(260, 312);
		setCursor(style::cur_pointer);
	}

	void setGrid(std::array<bool, kGridW * kGridH> *grid) {
		_grid = grid;
		update();
	}

protected:
	void paintEvent(QPaintEvent *) override {
		auto p = QPainter(this);
		p.fillRect(rect(), st::windowBg);
		const double cw = double(width()) / kGridW;
		const double ch = double(height()) / kGridH;
		for (int r = 0; r < kGridH; ++r) {
			for (int c = 0; c < kGridW; ++c) {
				const QRectF cell(c * cw, r * ch, cw, ch);
				if (_grid && (*_grid)[size_t(r * kGridW + c)]) {
					p.fillRect(cell.adjusted(2, 2, -2, -2), st::windowBgActive);
				}
				p.setPen(QPen(QColor(130, 130, 130, 80), 1.0));
				p.drawRect(cell);
			}
		}
		p.setPen(QPen(QColor(255, 70, 70, 170), 2.0));
		p.drawLine(0, int(10 * ch), width(), int(10 * ch));
	}

	void mousePressEvent(QMouseEvent *e) override {
		if (!_grid) return;
		const int c = std::clamp(int(e->pos().x() * kGridW / width()), 0, kGridW - 1);
		const int r = std::clamp(int(e->pos().y() * kGridH / height()), 0, kGridH - 1);
		const size_t idx = size_t(r * kGridW + c);
		_paintValue = !(*_grid)[idx];
		_dragging = true;
		(*_grid)[idx] = _paintValue;
		update();
		if (_onChanged) _onChanged();
	}

	void mouseMoveEvent(QMouseEvent *e) override {
		if (!_dragging || !_grid) return;
		const int c = std::clamp(int(e->pos().x() * kGridW / width()), 0, kGridW - 1);
		const int r = std::clamp(int(e->pos().y() * kGridH / height()), 0, kGridH - 1);
		const size_t idx = size_t(r * kGridW + c);
		if ((*_grid)[idx] != _paintValue) {
			(*_grid)[idx] = _paintValue;
			update();
			if (_onChanged) _onChanged();
		}
	}

	void mouseReleaseEvent(QMouseEvent *) override {
		_dragging = false;
	}

private:
	std::array<bool, kGridW * kGridH> *_grid = nullptr;
	Fn<void()> _onChanged;
	bool _dragging = false;
	bool _paintValue = true;
};

class LivePixelPreview final : public Ui::RpWidget {
public:
	LivePixelPreview(
		QWidget *parent,
		std::map<char16_t, std::array<bool, kGridW * kGridH>> *grids)
	: RpWidget(parent)
	, _grids(grids) {
		setFixedHeight(44);
	}

protected:
	void paintEvent(QPaintEvent *) override {
		auto p = QPainter(this);
		p.fillRect(rect().adjusted(12, 2, -12, -2), st::windowBgOver);
		if (!_grids) return;
		const QString sample = u"Привет MediaTok 123"_q;
		constexpr int cell = 3;
		int x = 20;
		const int y0 = 6;
		for (const auto ch : sample) {
			if (ch == u' ') { x += cell * 4; continue; }
			const auto it = _grids->find(ch.unicode());
			if (it == _grids->end()) continue;
			int maxCol = 0;
			for (int r = 0; r < kGridH; ++r) {
				for (int c = 0; c < kGridW; ++c) {
					if (it->second[size_t(r * kGridW + c)]) {
						p.fillRect(QRect(x + c * cell, y0 + r * cell, cell, cell), st::windowFg);
						if (c > maxCol) maxCol = c;
					}
				}
			}
			x += std::max(4, maxCol + 2) * cell;
			if (x > width() - 24) break;
		}
	}

private:
	std::map<char16_t, std::array<bool, kGridW * kGridH>> *_grids = nullptr;
};

} // namespace

void ShowTtfCreatorBox(not_null<Window::SessionController*> controller) {
	const auto weak = base::make_weak(controller);
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setWidth(st::boxWideWidth);
		box->setTitle(rpl::single(u"Создать свой шрифт (.ttf)"_q));

		struct CreatorState {
			std::map<char16_t, std::array<bool, kGridW * kGridH>> grids;
			std::vector<char16_t> charset;
			int charIndex = 0;
			int pixelStyle = 0; // 0=Плотный, 1=Пиксель, 2=Жирный, 3=Курсив
		};
		const auto st = box->lifetime().make_state<CreatorState>();
		st->charset = CreatorCharset();
		st->grids = RasterizeStarterGrids(Core::App().settings().customFontFamily());

		const auto container = box->verticalLayout();
		const auto nameField = box->addRow(object_ptr<Ui::InputField>(
			container,
			st::defaultInputField,
			rpl::single(u"Название твоего шрифта (напр. МойПиксель)"_q),
			u"Мой Шрифт"_q));

		const auto preview = container->add(
			object_ptr<LivePixelPreview>(container, &st->grids));

		const auto curCharLabel = box->lifetime().make_state<rpl::variable<QString>>(
			u"Буква: «%1» (%2 из %3)"_q.arg(QChar(st->charset[0])).arg(1).arg(int(st->charset.size())));

		const auto styleNames = std::array<QString, 4>{ u"Плотный"_q, u"Пиксель"_q, u"Жирный"_q, u"Курсив"_q };
		const auto styleVar = box->lifetime().make_state<rpl::variable<QString>>(styleNames[0]);

		Settings::AddButtonWithLabel(
			container,
			rpl::single(u"Стиль генерации .ttf"_q),
			styleVar->value(),
			st::settingsButton
		)->setClickedCallback([=] {
			st->pixelStyle = (st->pixelStyle + 1) % 4;
			*styleVar = styleNames[size_t(st->pixelStyle)];
		});

		const auto canvas = container->add(
			object_ptr<GlyphCanvasWidget>(container, &st->grids[st->charset[0]], [=] {
				preview->update();
			}),
			QMargins(80, 6, 80, 6));

		const auto selectIndex = [=](int idx) {
			const int n = int(st->charset.size());
			st->charIndex = ((idx % n) + n) % n;
			const auto ch = st->charset[size_t(st->charIndex)];
			canvas->setGrid(&st->grids[ch]);
			*curCharLabel = u"Буква: «%1» (%2 из %3)"_q.arg(QChar(ch)).arg(st->charIndex + 1).arg(n);
		};

		Settings::AddButtonWithLabel(
			container,
			rpl::single(u"Следующая буква →"_q),
			curCharLabel->value(),
			st::settingsButton
		)->setClickedCallback([=] { selectIndex(st->charIndex + 1); });

		Settings::AddButtonWithLabel(
			container,
			rpl::single(u"← Предыдущая буква"_q),
			rpl::single(u"Назад"_q),
			st::settingsButton
		)->setClickedCallback([=] { selectIndex(st->charIndex - 1); });

		const auto jumpField = box->addRow(object_ptr<Ui::InputField>(
			container,
			st::defaultInputField,
			rpl::single(u"Быстро перейти к букве (введите символ, напр. Ж или A)"_q)));
		jumpField->changes() | rpl::on_next([=] {
			const auto t = jumpField->getLastText();
			if (!t.isEmpty()) {
				const auto target = t[t.size() - 1].unicode();
				for (int i = 0; i < int(st->charset.size()); ++i) {
					if (st->charset[size_t(i)] == target) {
						selectIndex(i);
						break;
					}
				}
			}
		}, jumpField->lifetime());

		Settings::AddButtonWithLabel(
			container,
			rpl::single(u"Очистить текущую букву"_q),
			rpl::single(u"Стереть"_q),
			st::settingsButton
		)->setClickedCallback([=] {
			st->grids[st->charset[size_t(st->charIndex)]].fill(false);
			canvas->update();
			preview->update();
		});

		Settings::AddButtonWithLabel(
			container,
			rpl::single(u"Сделать букву жирнее"_q),
			rpl::single(u"+Толщина"_q),
			st::settingsButton
		)->setClickedCallback([=] {
			auto &g = st->grids[st->charset[size_t(st->charIndex)]];
			const auto copy = g;
			for (int r = 0; r < kGridH; ++r) {
				for (int c = 0; c < kGridW - 1; ++c) {
					if (copy[size_t(r * kGridW + c)]) g[size_t(r * kGridW + c + 1)] = true;
				}
			}
			canvas->update();
			preview->update();
		});

		Ui::AddDividerText(
			container,
			rpl::single(u"Рисуйте мышкой по сетке (красная линия — низ строки). Все 139 букв А–Я, A–Z, 0–9 готовы из шаблона — перерисуйте нужные и нажмите «Собрать .ttf»!"_q));

		box->addButton(rpl::single(u"Собрать .ttf и применить"_q), [=] {
			const auto rawName = nameField->getLastText().trimmed().isEmpty()
				? u"MyFont"_q
				: nameField->getLastText().trimmed();
			QString safeFile;
			for (const auto ch : rawName) {
				if (ch.isLetterOrNumber() || ch == u'_' || ch == u'-') safeFile.append(ch);
				else if (ch == u' ') safeFile.append(u'_');
			}
			if (safeFile.isEmpty()) safeFile = u"custom_font"_q;

			const auto bytes = CompileCustomTtf(rawName, st->grids, st->pixelStyle);
			QDir().mkpath(FontsDirPath());
			const auto outPath = FontsDirPath() + u"/"_q + safeFile + u".ttf"_q;
			QFile f(outPath);
			if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
				if (const auto s = weak.get()) s->showToast(u"Не удалось сохранить .ttf"_q);
				return;
			}
			f.write(bytes);
			f.close();

			const int fid = QFontDatabase::addApplicationFont(outPath);
			const auto fams = (fid >= 0) ? QFontDatabase::applicationFontFamilies(fid) : QStringList();
			if (fams.isEmpty()) {
				if (const auto s = weak.get()) s->showToast(u"Ошибка загрузки собранного .ttf"_q);
				return;
			}
			RegisterSingleCustomFont(outPath);
			box->closeBox();
			if (const auto s = weak.get()) {
				ApplyClientFontAndPromptRestart(s, fams.first(), rawName);
			}
		});
		box->addButton(rpl::single(u"Закрыть"_q), [=] { box->closeBox(); });
	}));
}

} // namespace MediaTok
