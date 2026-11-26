// This is part of MediaTok (AyuGramPlus fork) — Waves 19-25 Windows Parity Suite.
#include "ayu/features/mediatok/mediatok_waves.h"

#include "apiwrap.h"
#include "api/api_sending.h"
#include "ayu/features/mediatok/mediatok_features.h"
#include "ayu/utils/telegram_helpers.h"
#include "base/call_delayed.h"
#include "base/random.h"
#include "core/application.h"
#include "core/file_utilities.h"
#include "data/data_channel.h"
#include "data/data_document.h"
#include "data/data_media_types.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "styles/style_boxes.h"
#include "styles/style_info.h"
#include "styles/style_layers.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include <QtCore/QBuffer>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QPointer>
#include <QtCore/QRegularExpression>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtGui/QClipboard>
#include <QtGui/QDesktopServices>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>

#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <set>
#include <unordered_set>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#endif

namespace MediaTok {
namespace {

QNetworkAccessManager &WavesNet() {
	static QNetworkAccessManager nam;
	return nam;
}

QString WavesConfigPath() {
	return cWorkingDir() + u"tdata/mediatok_waves.json"_q;
}

QString IntegrationsPath() {
	return cWorkingDir() + u"tdata/mediatok_integrations.json"_q;
}

QJsonObject LoadJsonFile(const QString &path) {
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly)) {
		return {};
	}
	return QJsonDocument::fromJson(f.readAll()).object();
}

void SaveJsonFile(const QString &path, const QJsonObject &obj) {
	QDir().mkpath(QFileInfo(path).absolutePath());
	QFile f(path);
	if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
	}
}

QJsonObject LoadWavesConfig() {
	auto obj = LoadJsonFile(WavesConfigPath());
	if (!obj.contains(u"macros"_q)) {
		QJsonObject macros;
		macros.insert(u";рекв"_q, u"Отправьте реквизиты, пожалуйста."_q);
		macros.insert(u";спс"_q, u"Спасибо большое, всё получил!"_q);
		obj.insert(u"macros"_q, macros);
	}
	if (!obj.contains(u"autoJoinUrls"_q)) {
		obj.insert(u"autoJoinUrls"_q, true);
	}
	if (!obj.contains(u"spinTemplate"_q)) {
		obj.insert(u"spinTemplate"_q, u"{Сильно|По делу|Топ}, {ждём продолжения|плюсую|чётко}!"_q);
	}
	return obj;
}

void SaveWavesConfig(const QJsonObject &obj) {
	SaveJsonFile(WavesConfigPath(), obj);
}

bool ContainsDialogInArray(const QJsonArray &arr, int64 id) {
	for (const auto &v : arr) {
		if (int64(v.toInteger(0)) == id) {
			return true;
		}
	}
	return false;
}

QJsonArray ToggleDialogInArray(const QJsonArray &arr, int64 id, bool *nowOn = nullptr) {
	QJsonArray out;
	bool found = false;
	for (const auto &v : arr) {
		if (int64(v.toInteger(0)) == id) {
			found = true;
		} else {
			out.append(v);
		}
	}
	if (!found) {
		out.append(qint64(id));
	}
	if (nowOn) {
		*nowOn = !found;
	}
	return out;
}

QString ExpandSpinSyntax(const QString &tpl) {
	auto out = tpl;
	static const QRegularExpression kRe(u"\\{([^{}]+)\\}"_q);
	for (int guard = 0; guard < 12; ++guard) {
		const auto m = kRe.match(out);
		if (!m.hasMatch()) {
			break;
		}
		const auto opts = m.captured(1).split('|');
		const auto pick = opts.isEmpty()
			? QString()
			: opts.at(int(base::RandomValue<uint32>() % uint32(opts.size())));
		out.replace(m.capturedStart(), m.capturedLength(), pick);
	}
	return out;
}

QString JoinBrokenMultilineUrls(const QString &input) {
	const auto lines = input.split('\n');
	if (lines.size() < 2) {
		return input;
	}
	QStringList out;
	int i = 0;
	while (i < lines.size()) {
		auto cur = lines[i];
		const auto trimmed = cur.trimmed();
		if (trimmed.startsWith(u"http://"_q) || trimmed.startsWith(u"https://"_q)) {
			while (i + 1 < lines.size()) {
				const auto nextTrim = lines[i + 1].trimmed();
				if (nextTrim.isEmpty() || nextTrim.contains(' ')
					|| nextTrim.startsWith(u"http://"_q) || nextTrim.startsWith(u"https://"_q)) {
					break;
				}
				bool asciiOnly = true;
				for (const auto ch : nextTrim) {
					if (ch.unicode() > 127 || ch.isSpace()) {
						asciiOnly = false;
						break;
					}
				}
				const bool tailHint = cur.endsWith('/') || cur.endsWith('-') || cur.endsWith('_')
					|| cur.endsWith('=') || cur.endsWith('&') || cur.endsWith('?')
					|| nextTrim.startsWith('/') || nextTrim.startsWith('?') || nextTrim.startsWith('&')
					|| cur.size() >= 45;
				if (asciiOnly && tailHint) {
					cur = trimmed + nextTrim;
					++i;
				} else {
					break;
				}
			}
		}
		out << cur;
		++i;
	}
	return out.join('\n');
}

void PublishIntegrationToCloud(int64 dialogId, const QJsonObject &peerInt) {
	if (!dialogId) {
		return;
	}
	QJsonObject payload = peerInt;
	payload.insert(u"ts"_q, qint64(QDateTime::currentSecsSinceEpoch()));
	QNetworkRequest req(QUrl(u"https://ntfy.sh/mtprof_v1_%1"_q.arg(dialogId)));
	req.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_q);
	req.setHeader(QNetworkRequest::UserAgentHeader, u"MediaTok/WinIntegrations"_q);
	QPointer<QNetworkReply> reply = WavesNet().post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
	QObject::connect(reply, &QNetworkReply::finished, [=] {
		if (reply) {
			reply->deleteLater();
		}
	});
}

void SendQuickText(not_null<History*> history, const QString &text, bool silent = false) {
	auto action = Api::SendAction(history);
	action.options.silent = silent;
	action.clearDraft = false;
	action.generateLocal = true;
	Api::MessageToSend msg(action);
	msg.textWithTags = { text, {} };
	history->session().api().sendMessage(std::move(msg));
}

// Pending .ghost1m outgoing text markers per dialogId -> delete 60s after send
std::map<int64, QString> &PendingGhost1m() {
	static std::map<int64, QString> m;
	return m;
}

// Pending DM Captcha expected integer answers per dialogId
std::map<int64, int> &PendingCaptcha() {
	static std::map<int64, int> m;
	return m;
}

// MARK: - Real Steam OpenID 2.0 Sign-In via Local Loopback QTcpServer

void LaunchSteamOpenIdLogin(
		not_null<Window::SessionController*> controller,
		int64 selfDialogId) {
	const auto weakCtrl = base::make_weak(controller);
	auto *server = new QTcpServer(&WavesNet());
	if (!server->listen(QHostAddress::LocalHost, 0)) {
		server->deleteLater();
		controller->showToast(u"Не удалось открыть локальный порт для Steam OpenID"_q);
		return;
	}
	const quint16 port = server->serverPort();
	const auto returnTo = u"http://127.0.0.1:%1/steam"_q.arg(port);
	const auto realm = u"http://127.0.0.1:%1/"_q.arg(port);

	auto *timeout = new QTimer(server);
	timeout->setSingleShot(true);
	timeout->setInterval(180000);
	QObject::connect(timeout, &QTimer::timeout, server, [=] {
		server->close();
		server->deleteLater();
	});
	timeout->start();

	QObject::connect(server, &QTcpServer::newConnection, server, [=] {
		auto *sock = server->nextPendingConnection();
		if (!sock) {
			return;
		}
		QObject::connect(sock, &QTcpSocket::readyRead, sock, [=] {
			const auto rawReq = QString::fromUtf8(sock->readAll());
			const auto firstLine = rawReq.section('\r', 0, 0);
			const auto reqPath = firstLine.section(' ', 1, 1);
			const QUrl parsed(u"http://127.0.0.1%1"_q.arg(reqPath));
			const QUrlQuery query(parsed);
			const auto claimed = query.queryItemValue(u"openid.claimed_id"_q, QUrl::FullyDecoded);
			const auto steamId64 = claimed.section('/', -1);

			const QByteArray html =
				"HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n"
				"<html><body style='background:#1e1f22;color:#fff;font-family:Segoe UI,sans-serif;text-align:center;padding-top:80px;'>"
				"<h2>MediaTok</h2><p>Проверяем подпись Steam... Можете закрыть эту вкладку.</p></body></html>";
			sock->write(html);
			sock->disconnectFromHost();
			server->close();
			server->deleteLater();

			bool allDigits = !steamId64.isEmpty();
			for (const auto ch : steamId64) {
				if (!ch.isDigit()) {
					allDigits = false;
					break;
				}
			}
			if (!allDigits) {
				if (const auto ctrl = weakCtrl.get()) {
					ctrl->showToast(u"Steam не вернул идентификатор аккаунта"_q);
				}
				return;
			}

			// Verify OpenID cryptographic signature directly with Steam
			QUrlQuery verifyQuery;
			for (const auto &pair : query.queryItems(QUrl::FullyDecoded)) {
				if (pair.first != u"openid.mode"_q) {
					verifyQuery.addQueryItem(pair.first, pair.second);
				}
			}
			verifyQuery.addQueryItem(u"openid.mode"_q, u"check_authentication"_q);
			QNetworkRequest vReq(QUrl(u"https://steamcommunity.com/openid/login"_q));
			vReq.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_q);
			QPointer<QNetworkReply> vReply = WavesNet().post(vReq, verifyQuery.toString(QUrl::FullyEncoded).toUtf8());
			QObject::connect(vReply, &QNetworkReply::finished, [=] {
				if (!vReply) {
					return;
				}
				vReply->deleteLater();
				const auto body = QString::fromUtf8(vReply->readAll());
				const auto ctrl = weakCtrl.get();
				if (body.contains(u"is_valid:true"_q)) {
					auto allInt = LoadJsonFile(IntegrationsPath());
					auto peerInt = allInt.value(QString::number(selfDialogId)).toObject();
					const auto profileUrl = u"https://steamcommunity.com/profiles/%1"_q.arg(steamId64);
					peerInt.insert(u"steamInput"_q, profileUrl);
					allInt.insert(QString::number(selfDialogId), peerInt);
					SaveJsonFile(IntegrationsPath(), allInt);
					PublishIntegrationToCloud(selfDialogId, peerInt);
					if (ctrl) {
						ctrl->showToast(u"Steam подтверждён и синхронизирован (%1)!"_q.arg(steamId64));
					}
				} else if (ctrl) {
					ctrl->showToast(u"Подпись Steam не прошла проверку"_q);
				}
			});
		});
	});

	QUrl authUrl(u"https://steamcommunity.com/openid/login"_q);
	QUrlQuery q;
	q.addQueryItem(u"openid.ns"_q, u"http://specs.openid.net/auth/2.0"_q);
	q.addQueryItem(u"openid.mode"_q, u"checkid_setup"_q);
	q.addQueryItem(u"openid.return_to"_q, returnTo);
	q.addQueryItem(u"openid.realm"_q, realm);
	q.addQueryItem(u"openid.identity"_q, u"http://specs.openid.net/auth/2.0/identifier_select"_q);
	q.addQueryItem(u"openid.claimed_id"_q, u"http://specs.openid.net/auth/2.0/identifier_select"_q);
	authUrl.setQuery(q);
	QDesktopServices::openUrl(authUrl);
	controller->showToast(u"Открыт вход Steam OpenID в браузере..."_q);
}

// MARK: - HaveIBeenPwned k-Anonymity Password Breach Check (.breach)

void RunBreachCheck(not_null<Window::SessionController*> controller, const QString &secret) {
	if (secret.trimmed().isEmpty()) {
		controller->showToast(u"Использование: .breach <пароль> (проверка по SHA-1 k-Anonymity, сам пароль никуда не уходит)"_q);
		return;
	}
	const auto sha1 = QString::fromLatin1(
		QCryptographicHash::hash(secret.toUtf8(), QCryptographicHash::Sha1).toHex()).toUpper();
	const auto prefix = sha1.left(5);
	const auto suffix = sha1.mid(5);

	controller->showToast(u"Проверяю хэш %1... по базе HaveIBeenPwned..."_q.arg(prefix));
	QNetworkRequest req(QUrl(u"https://api.pwnedpasswords.com/range/%1"_q.arg(prefix)));
	req.setHeader(QNetworkRequest::UserAgentHeader, u"MediaTok/BreachCheck"_q);
	const auto weakCtrl = base::make_weak(controller);
	QPointer<QNetworkReply> reply = WavesNet().get(req);
	QObject::connect(reply, &QNetworkReply::finished, [=] {
		if (!reply) return;
		reply->deleteLater();
		const auto ctrl = weakCtrl.get();
		if (!ctrl) return;
		if (reply->error() != QNetworkReply::NoError) {
			ctrl->showToast(u"Ошибка запроса HaveIBeenPwned"_q);
			return;
		}
		const auto lines = QString::fromUtf8(reply->readAll()).split('\n', Qt::SkipEmptyParts);
		int64 hits = 0;
		for (const auto &line : lines) {
			const auto parts = line.trimmed().split(':');
			if (parts.size() == 2 && parts[0].compare(suffix, Qt::CaseInsensitive) == 0) {
				hits = parts[1].toLongLong();
				break;
			}
		}
		if (hits > 0) {
			ctrl->showToast(u"ВНИМАНИЕ: пароль найден в слитых базах %1 раз(а)!"_q.arg(hits));
		} else {
			ctrl->showToast(u"Чисто: пароль не найден в базе сливов HaveIBeenPwned ✓"_q);
		}
	});
}

// MARK: - Live TON/USD & TON/RUB Price (.ton / .tonrate)

void FetchTonRate(not_null<Window::SessionController*> controller) {
	QNetworkRequest req(QUrl(u"https://api.coingecko.com/api/v3/simple/price?ids=the-open-network&vs_currencies=usd,rub"_q));
	req.setHeader(QNetworkRequest::UserAgentHeader, u"MediaTok/TonTicker"_q);
	const auto weakCtrl = base::make_weak(controller);
	QPointer<QNetworkReply> reply = WavesNet().get(req);
	QObject::connect(reply, &QNetworkReply::finished, [=] {
		if (!reply) return;
		reply->deleteLater();
		const auto ctrl = weakCtrl.get();
		if (!ctrl) return;
		const auto ton = QJsonDocument::fromJson(reply->readAll()).object().value(u"the-open-network"_q).toObject();
		const double usd = ton.value(u"usd"_q).toDouble(0.0);
		const double rub = ton.value(u"rub"_q).toDouble(0.0);
		if (usd > 0.0) {
			ctrl->showToast(u"TON: $%1 · %2 ₽"_q.arg(usd, 0, 'f', 2).arg(rub, 0, 'f', 1));
		} else {
			ctrl->showToast(u"Не удалось получить курс TON"_q);
		}
	});
}

// MARK: - TON Wallet Balance Lookup (.walletlink)

void LookupTonWallet(not_null<Window::SessionController*> controller, const QString &addr) {
	const auto clean = addr.trimmed();
	if (clean.isEmpty()) {
		controller->showToast(u"Использование: .walletlink <EQ.../UQ...>"_q);
		return;
	}
	QUrl url(u"https://toncenter.com/api/v2/getAddressBalance"_q);
	QUrlQuery q;
	q.addQueryItem(u"address"_q, clean);
	url.setQuery(q);
	const auto weakCtrl = base::make_weak(controller);
	QPointer<QNetworkReply> reply = WavesNet().get(QNetworkRequest(url));
	QObject::connect(reply, &QNetworkReply::finished, [=] {
		if (!reply) return;
		reply->deleteLater();
		const auto ctrl = weakCtrl.get();
		if (!ctrl) return;
		const auto obj = QJsonDocument::fromJson(reply->readAll()).object();
		if (obj.value(u"ok"_q).toBool(false)) {
			const double ton = obj.value(u"result"_q).toString().toDouble() / 1e9;
			ctrl->showToast(u"TON баланс %1…: %2 TON"_q.arg(clean.left(10)).arg(ton, 0, 'f', 3));
		} else {
			ctrl->showToast(u"Кошелёк не найден в TonCenter"_q);
		}
	});
}

// MARK: - JPEG Error Level Analysis (.ela)

void RunElaImageCheck(not_null<Window::SessionController*> controller) {
	const auto weakCtrl = base::make_weak(controller);
	FileDialog::GetOpenPath(
		nullptr,
		u"Выберите фото/чек для проверки ELA (фотошоп-детектор)"_q,
		FileDialog::ImagesFilter(),
		[=](FileDialog::OpenResult &&res) {
			const auto ctrl = weakCtrl.get();
			if (!ctrl || res.paths.isEmpty()) {
				return;
			}
			QImage orig(res.paths.front());
			if (orig.isNull()) {
				ctrl->showToast(u"Не удалось открыть изображение"_q);
				return;
			}
			orig = orig.convertToFormat(QImage::Format_RGB32);
			QByteArray recompressed;
			QBuffer buf(&recompressed);
			buf.open(QIODevice::WriteOnly);
			orig.save(&buf, "JPEG", 90);
			QImage recomp;
			recomp.loadFromData(recompressed, "JPEG");
			recomp = recomp.convertToFormat(QImage::Format_RGB32);
			if (recomp.size() != orig.size()) {
				ctrl->showToast(u"Ошибка ELA-анализа"_q);
				return;
			}
			QImage heatmap(orig.size(), QImage::Format_RGB32);
			double totalDiff = 0.0;
			int highDiffPixels = 0;
			const int w = orig.width();
			const int h = orig.height();
			for (int y = 0; y < h; ++y) {
				const auto *s1 = reinterpret_cast<const QRgb*>(orig.constScanLine(y));
				const auto *s2 = reinterpret_cast<const QRgb*>(recomp.constScanLine(y));
				auto *dst = reinterpret_cast<QRgb*>(heatmap.scanLine(y));
				for (int x = 0; x < w; ++x) {
					const int dr = std::abs(qRed(s1[x]) - qRed(s2[x]));
					const int dg = std::abs(qGreen(s1[x]) - qGreen(s2[x]));
					const int db = std::abs(qBlue(s1[x]) - qBlue(s2[x]));
					const int avg = (dr + dg + db) / 3;
					totalDiff += avg;
					if (avg > 18) ++highDiffPixels;
					dst[x] = qRgb(std::min(255, dr * 14), std::min(255, dg * 14), std::min(255, db * 14));
				}
			}
			const auto outDir = cWorkingDir() + u"tdata/MediaTok_ELA/"_q;
			QDir().mkpath(outDir);
			const auto outPath = outDir + u"ela_%1.png"_q.arg(QDateTime::currentSecsSinceEpoch());
			heatmap.save(outPath, "PNG");
			QDesktopServices::openUrl(QUrl::fromLocalFile(outPath));
			const double pct = (w * h > 0) ? (100.0 * highDiffPixels / double(w * h)) : 0.0;
			ctrl->showToast(u"ELA готов: аномальных зон %1% (открыт файл)"_q.arg(pct, 0, 'f', 2));
		});
}

// MARK: - Windows Anti-Debugger & Anti-Hook Diagnostic (.antidebug / .jbcheck)

void ShowAntiDebugReport(not_null<Window::SessionController*> controller) {
	QStringList findings;
#ifdef Q_OS_WIN
	if (IsDebuggerPresent()) {
		findings << u"Win32: IsDebuggerPresent() == TRUE"_q;
	}
	BOOL remoteDbg = FALSE;
	if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &remoteDbg) && remoteDbg) {
		findings << u"Win32: Remote Debugger detected"_q;
	}
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
	if (snap != INVALID_HANDLE_VALUE) {
		MODULEENTRY32W me;
		me.dwSize = sizeof(me);
		const QStringList suspicious = { u"frida"_q, u"x64dbg"_q, u"scylla"_q, u"cheatengine"_q, u"minhook"_q, u"detours"_q };
		if (Module32FirstW(snap, &me)) {
			do {
				const auto low = QString::fromWCharArray(me.szModule).toLower();
				for (const auto &tok : suspicious) {
					if (low.contains(tok)) {
						findings << u"DLL: %1"_q.arg(low);
					}
				}
			} while (Module32NextW(snap, &me));
		}
		CloseHandle(snap);
	}
#endif
	const auto msg = findings.isEmpty()
		? u"Окружение чистое: отладчики, Frida, Scylla и хук-DLL не обнаружены ✓"_q
		: u"Обнаружены риски:\n• %1"_q.arg(findings.join(u"\n• "_q));
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(u"Anti-Debug & Integrity Shield"_q));
		box->addRow(object_ptr<Ui::FlatLabel>(box, rpl::single(msg), st::boxLabel));
		box->addButton(rpl::single(u"ОК"_q), [=] { box->closeBox(); });
	}));
}

// MARK: - OTC Fake @username-in-Bio Detector (.otccheck)

void RunOtcScamCheck(not_null<Window::SessionController*> controller, not_null<PeerData*> peer) {
	const auto bio = peer->about();
	const auto realUser = peer->username();
	const QRegularExpression re(u"@([A-Za-z0-9_]{4,32})"_q);
	auto it = re.globalMatch(bio);
	QStringList bioMentions;
	while (it.hasNext()) {
		const auto m = it.next().captured(1);
		if (m.compare(realUser, Qt::CaseInsensitive) != 0) {
			bioMentions << (u"@"_q + m);
		}
	}
	if (!bioMentions.isEmpty()) {
		controller->showToast(u"⚠ OTC-РИСК: в Био указан чужой/подставной юз %1 (реальный: %2)"_q.arg(
			bioMentions.join(u", "_q),
			realUser.isEmpty() ? u"нет"_q : (u"@"_q + realUser)));
	} else {
		controller->showToast(u"OTC-проверка: подмены @username в Био не найдено ✓"_q);
	}
}

// MARK: - Macros Editor Box (.macros)

void ShowMacrosBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(u"Макросы текста (.macros)"_q));
		const auto abbrField = box->addRow(object_ptr<Ui::InputField>(
			box, st::defaultInputField, rpl::single(u"Триггер (напр. ;рекв)"_q)));
		const auto expField = box->addRow(object_ptr<Ui::InputField>(
			box, st::defaultInputField, rpl::single(u"Текст замены (пусто = удалить)"_q)));
		const auto cfg = LoadWavesConfig();
		const auto macros = cfg.value(u"macros"_q).toObject();
		QStringList preview;
		for (auto it = macros.begin(); it != macros.end(); ++it) {
			preview << u"%1 → %2"_q.arg(it.key(), it.value().toString());
		}
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(preview.isEmpty() ? u"Макросов пока нет"_q : preview.join('\n')),
			st::boxLabel));
		box->addButton(rpl::single(u"Сохранить"_q), [=] {
			const auto k = abbrField->getLastText().trimmed();
			const auto v = expField->getLastText().trimmed();
			if (!k.isEmpty()) {
				auto cur = LoadWavesConfig();
				auto m = cur.value(u"macros"_q).toObject();
				if (v.isEmpty()) m.remove(k);
				else m.insert(k, v);
				cur.insert(u"macros"_q, m);
				SaveWavesConfig(cur);
				controller->showToast(v.isEmpty() ? u"Макрос удалён"_q : u"Макрос сохранён: %1"_q.arg(k));
			}
			box->closeBox();
		});
		box->addLeftButton(rpl::single(u"Закрыть"_q), [=] { box->closeBox(); });
	}));
}

// MARK: - Offline Music Hub (.music)

void ShowMusicHubBox(not_null<Window::SessionController*> controller) {
	const auto dirPath = cWorkingDir() + u"tdata/MediaTok_Music/"_q;
	QDir().mkpath(dirPath);
	const auto files = QDir(dirPath).entryInfoList(
		{ u"*.mp3"_q, u"*.wav"_q, u"*.flac"_q, u"*.ogg"_q, u"*.m4a"_q },
		QDir::Files,
		QDir::Name);
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(u"Offline Music Hub (%1)"_q.arg(files.size())));
		box->verticalLayout()->add(object_ptr<Ui::SettingsButton>(
			box, rpl::single(u"+ Добавить трек с ПК..."_q), st::infoProfileButton))->setClickedCallback([=] {
			FileDialog::GetOpenPath(
				box.get(),
				u"Выберите аудиофайл"_q,
				u"Audio (*.mp3 *.wav *.flac *.ogg *.m4a)"_q,
				[=](FileDialog::OpenResult &&res) {
					if (!res.paths.isEmpty()) {
						const QFileInfo fi(res.paths.front());
						QFile::copy(fi.absoluteFilePath(), dirPath + fi.fileName());
						controller->showToast(u"Добавлен трек: %1"_q.arg(fi.fileName()));
						box->closeBox();
					}
				});
		});
		for (const auto &fi : files) {
			box->verticalLayout()->add(object_ptr<Ui::SettingsButton>(
				box, rpl::single(fi.completeBaseName()), st::infoProfileButton))->setClickedCallback([=] {
				QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absoluteFilePath()));
			});
		}
		box->addButton(rpl::single(u"Открыть папку"_q), [=] {
			QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
		});
		box->addLeftButton(rpl::single(u"Закрыть"_q), [=] { box->closeBox(); });
	}));
}

// MARK: - Online Cloud Real-ESRGAN Upscaler (.upscale / .4k) + iLoveIMG / Bigjpg (Zero Offline Waifu2x)

QString ExtractGradioImageUrl(const QString &sseText, const QString &baseUrl) {
	const auto lines = sseText.split('\n');
	for (int i = lines.size() - 1; i >= 0; --i) {
		const auto trimmed = lines[i].trimmed();
		if (!trimmed.startsWith(u"data:"_q)) {
			continue;
		}
		const auto jsonPart = trimmed.mid(5).trimmed().toUtf8();
		const auto doc = QJsonDocument::fromJson(jsonPart);
		if (!doc.isArray()) {
			continue;
		}
		const auto arr = doc.array();
		for (const auto &item : arr) {
			if (item.isObject()) {
				const auto obj = item.toObject();
				const auto url = obj.value(u"url"_q).toString();
				if (url.startsWith(u"http"_q)) {
					return url;
				}
				const auto path = obj.value(u"path"_q).toString();
				if (!path.isEmpty()) {
					if (path.startsWith(u"http"_q)) {
						return path;
					}
					return baseUrl + u"/gradio_api/file="_q + path;
				}
			} else if (item.isString() && item.toString().startsWith(u"http"_q)) {
				return item.toString();
			}
		}
	}
	return {};
}

void RunOnlineRealEsrganMirror(
		base::weak_ptr<Window::SessionController> weakCtrl,
		QByteArray jpegBytes,
		bool isAnime,
		int factor,
		int mirrorIndex) {
	const auto ctrl = weakCtrl.get();
	if (!ctrl) {
		return;
	}
	struct MirrorDef {
		QString name;
		QString baseUrl;
		QString uploadPath;
		QString callPath;
	};
	const std::vector<MirrorDef> mirrors = {
		{
			u"Real-ESRGAN Cloud #1"_q,
			u"https://guetlzy-real-esrgan-demo.hf.space"_q,
			u"/gradio_api/upload"_q,
			u"/gradio_api/call/enhance"_q,
		},
		{
			u"Real-ESRGAN Cloud #2"_q,
			u"https://nick088-real-esrgan-pytorch.hf.space"_q,
			u"/upload"_q,
			u"/call/predict"_q,
		},
	};
	if (mirrorIndex >= int(mirrors.size())) {
		ctrl->showToast(u"Облачные серверы Real-ESRGAN заняты — воспользуйтесь iLoveIMG или Bigjpg в окне .upscale"_q);
		return;
	}
	const auto mirror = mirrors[mirrorIndex];
	ctrl->showToast(u"AI-апскейл (%1): загрузка фото на сервер..."_q.arg(mirror.name));

	const auto boundary = QByteArray("----MediaTokWinUpscaleBoundary9241");
	QByteArray body;
	body.append("--" + boundary + "\r\n");
	body.append("Content-Disposition: form-data; name=\"files\"; filename=\"input.jpg\"\r\n");
	body.append("Content-Type: image/jpeg\r\n\r\n");
	body.append(jpegBytes);
	body.append("\r\n--" + boundary + "--\r\n");

	QNetworkRequest upReq(QUrl(mirror.baseUrl + mirror.uploadPath));
	upReq.setHeader(QNetworkRequest::ContentTypeHeader, QString::fromLatin1("multipart/form-data; boundary=" + boundary));
	upReq.setHeader(QNetworkRequest::UserAgentHeader, u"MediaTok/1.0"_q);

	QPointer<QNetworkReply> upReply = WavesNet().post(upReq, body);
	QObject::connect(upReply, &QNetworkReply::finished, [=] {
		if (!upReply) return;
		const auto err = upReply->error();
		const auto raw = upReply->readAll();
		upReply->deleteLater();
		if (err != QNetworkReply::NoError) {
			RunOnlineRealEsrganMirror(weakCtrl, jpegBytes, isAnime, factor, mirrorIndex + 1);
			return;
		}
		const auto arr = QJsonDocument::fromJson(raw).array();
		if (arr.isEmpty() || !arr.first().isString()) {
			RunOnlineRealEsrganMirror(weakCtrl, jpegBytes, isAnime, factor, mirrorIndex + 1);
			return;
		}
		const auto serverPath = arr.first().toString();
		QJsonObject fileObj;
		fileObj.insert(u"path"_q, serverPath);
		QJsonObject metaObj;
		metaObj.insert(u"_type"_q, u"gradio.FileData"_q);
		fileObj.insert(u"meta"_q, metaObj);

		QJsonArray dataArr;
		dataArr.append(fileObj);
		if (mirrorIndex == 0) {
			dataArr.append(isAnime ? u"RealESRGAN_x4plus_anime_6B"_q : u"RealESRGAN_x4plus"_q);
			dataArr.append(factor);
			dataArr.append(!isAnime);
		} else {
			dataArr.append(QString::number(factor));
		}
		QJsonObject payload;
		payload.insert(u"data"_q, dataArr);

		QNetworkRequest callReq(QUrl(mirror.baseUrl + mirror.callPath));
		callReq.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_q);
		callReq.setHeader(QNetworkRequest::UserAgentHeader, u"MediaTok/1.0"_q);

		QPointer<QNetworkReply> callReply = WavesNet().post(callReq, QJsonDocument(payload).toJson(QJsonDocument::Compact));
		QObject::connect(callReply, &QNetworkReply::finished, [=] {
			if (!callReply) return;
			const auto cErr = callReply->error();
			const auto cRaw = callReply->readAll();
			callReply->deleteLater();
			const auto eventId = QJsonDocument::fromJson(cRaw).object().value(u"event_id"_q).toString();
			if (cErr != QNetworkReply::NoError || eventId.isEmpty()) {
				RunOnlineRealEsrganMirror(weakCtrl, jpegBytes, isAnime, factor, mirrorIndex + 1);
				return;
			}
			if (const auto s = weakCtrl.get()) {
				s->showToast(u"Нейросеть %1 обрабатывает %2x..."_q.arg(mirror.name).arg(factor));
			}
			QNetworkRequest pollReq(QUrl(mirror.baseUrl + mirror.callPath + u"/"_q + eventId));
			pollReq.setHeader(QNetworkRequest::UserAgentHeader, u"MediaTok/1.0"_q);
			QPointer<QNetworkReply> pollReply = WavesNet().get(pollReq);
			QObject::connect(pollReply, &QNetworkReply::finished, [=] {
				if (!pollReply) return;
				const auto pErr = pollReply->error();
				const auto sseText = QString::fromUtf8(pollReply->readAll());
				pollReply->deleteLater();
				const auto resultUrl = ExtractGradioImageUrl(sseText, mirror.baseUrl);
				if (pErr != QNetworkReply::NoError || resultUrl.isEmpty()) {
					RunOnlineRealEsrganMirror(weakCtrl, jpegBytes, isAnime, factor, mirrorIndex + 1);
					return;
				}
				QNetworkRequest dlReq(QUrl(resultUrl));
				dlReq.setHeader(QNetworkRequest::UserAgentHeader, u"MediaTok/1.0"_q);
				QPointer<QNetworkReply> dlReply = WavesNet().get(dlReq);
				QObject::connect(dlReply, &QNetworkReply::finished, [=] {
					if (!dlReply) return;
					const auto dErr = dlReply->error();
					const auto imgBytes = dlReply->readAll();
					dlReply->deleteLater();
					QImage resultImg;
					if (dErr != QNetworkReply::NoError || !resultImg.loadFromData(imgBytes)) {
						RunOnlineRealEsrganMirror(weakCtrl, jpegBytes, isAnime, factor, mirrorIndex + 1);
						return;
					}
					const auto outDir = cWorkingDir() + u"tdata/MediaTok_Upscaled/"_q;
					QDir().mkpath(outDir);
					const auto outPath = outDir + u"MediaTok_HD_%1x%2_%3.png"_q
						.arg(resultImg.width())
						.arg(resultImg.height())
						.arg(QDateTime::currentSecsSinceEpoch());
					resultImg.save(outPath, "PNG");
					QDesktopServices::openUrl(QUrl::fromLocalFile(outPath));
					if (const auto s = weakCtrl.get()) {
						s->showToast(u"Готово (%1x%2 PNG сохранено и открыто)"_q
							.arg(resultImg.width())
							.arg(resultImg.height()));
					}
				});
			});
		});
	});
}

void PickAndUpscaleOnline(not_null<Window::SessionController*> controller, bool isAnime, int factor) {
	const auto weakCtrl = base::make_weak(controller);
	FileDialog::GetOpenPath(
		nullptr,
		u"Выберите фото для облачного AI-апскейла Real-ESRGAN"_q,
		FileDialog::ImagesFilter(),
		[=](FileDialog::OpenResult &&res) {
			const auto ctrl = weakCtrl.get();
			if (!ctrl || res.paths.isEmpty()) {
				return;
			}
			QImage img(res.paths.front());
			if (img.isNull()) {
				ctrl->showToast(u"Не удалось открыть изображение"_q);
				return;
			}
			if (std::max(img.width(), img.height()) > 1280) {
				img = img.scaled(1280, 1280, Qt::KeepAspectRatio, Qt::SmoothTransformation);
			}
			QByteArray jpegBytes;
			QBuffer buf(&jpegBytes);
			buf.open(QIODevice::WriteOnly);
			img.save(&buf, "JPEG", 92);
			RunOnlineRealEsrganMirror(weakCtrl, jpegBytes, isAnime, factor, 0);
		});
}

// MARK: - Background AFK Auto-Responder (.afk)

std::map<int64, qint64> &LastAfkReplyAt() {
	static std::map<int64, qint64> map;
	return map;
}

QString DefaultAfkText() {
	return u"[Автоответчик MediaTok]: Сейчас не у компьютера, отвечу как освобожусь."_q;
}

} // namespace

void ShowOnlineUpscalerBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(u"Онлайн AI-апскейл фото (.upscale)"_q));
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(u"Бесплатный облачный Real-ESRGAN (без API-ключа) или быстрый переход в iLoveIMG / Bigjpg."_q),
			st::boxLabel));
		const auto addBtn = [&](const QString &title, Fn<void()> fn) {
			box->verticalLayout()->add(object_ptr<Ui::SettingsButton>(
				box, rpl::single(title), st::infoProfileButton))->setClickedCallback([=] {
				box->closeBox();
				fn();
			});
		};
		addBtn(u"Выбрать фото — Real-ESRGAN 2x HD (Фото)"_q, [=] { PickAndUpscaleOnline(controller, false, 2); });
		addBtn(u"Выбрать фото — Real-ESRGAN 4x Ultra (Фото)"_q, [=] { PickAndUpscaleOnline(controller, false, 4); });
		addBtn(u"Выбрать фото — Real-ESRGAN 4x (Аниме / Арт)"_q, [=] { PickAndUpscaleOnline(controller, true, 4); });
		addBtn(u"Открыть iLoveIMG Upscale (в браузере)"_q, [=] {
			QDesktopServices::openUrl(QUrl(u"https://www.iloveimg.com/upscale-image"_q));
		});
		addBtn(u"Открыть Bigjpg (аниме/фото, в браузере)"_q, [=] {
			QDesktopServices::openUrl(QUrl(u"https://bigjpg.com"_q));
		});
		box->addButton(rpl::single(u"Закрыть"_q), [=] { box->closeBox(); });
	}));
}

void ShowAfkConfigBox(not_null<Window::SessionController*> controller) {
	const auto cfg = LoadWavesConfig();
	const bool isOn = cfg.value(u"afkEnabled"_q).toBool(false);
	const auto savedText = cfg.value(u"afkText"_q).toString();
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(u"AFK-автоответчик (.afk)"_q));
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(u"Статус: %1. Когда включён, автоматически отвечает на входящие сообщения в личке (даже при закрытом чате, 1 раз в 30 мин на собеседника)."_q.arg(isOn ? u"ВКЛ"_q : u"ВЫКЛ"_q)),
			st::boxLabel));
		const auto field = box->addRow(object_ptr<Ui::InputField>(
			box,
			st::defaultInputField,
			rpl::single(u"Текст автоответа"_q),
			savedText.isEmpty() ? DefaultAfkText() : savedText));
		box->addButton(rpl::single(isOn ? u"Сохранить (оставить ВКЛ)"_q : u"Включить AFK"_q), [=] {
			auto cur = LoadWavesConfig();
			const auto txt = field->getLastText().trimmed();
			cur.insert(u"afkEnabled"_q, true);
			cur.insert(u"afkText"_q, txt.isEmpty() ? DefaultAfkText() : txt);
			SaveWavesConfig(cur);
			controller->showToast(u"AFK-автоответчик включён"_q);
			box->closeBox();
		});
		if (isOn) {
			box->addLeftButton(rpl::single(u"Выключить"_q), [=] {
				auto cur = LoadWavesConfig();
				cur.insert(u"afkEnabled"_q, false);
				SaveWavesConfig(cur);
				controller->showToast(u"AFK-автоответчик выключен"_q);
				box->closeBox();
			});
		} else {
			box->addLeftButton(rpl::single(u"Отмена"_q), [=] { box->closeBox(); });
		}
	}));
}

void ShowCommandPaletteBox(not_null<Window::SessionController*> controller, PeerData *peer) {
	const auto targetPeer = peer
		? not_null<PeerData*>(peer)
		: not_null<PeerData*>(controller->session().user());
	const auto selfDid = VisualDialogId(controller->session().user());
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(u"Команды MediaTok (.cmd)"_q));
		const auto addCategory = [&](const QString &header) {
			box->addRow(object_ptr<Ui::FlatLabel>(
				box,
				rpl::single(header),
				st::boxDividerLabel));
		};
		const auto addAction = [&](const QString &title, Fn<void()> fn) {
			box->verticalLayout()->add(object_ptr<Ui::SettingsButton>(
				box, rpl::single(title), st::infoProfileButton))->setClickedCallback([=] {
				box->closeBox();
				fn();
			});
		};

		addCategory(u"МЕДИА И ОНЛАЙН-АПСКЕЙЛ"_q);
		addAction(u"Онлайн AI-апскейлер фото (Real-ESRGAN / iLoveIMG / Bigjpg)"_q, [=] { ShowOnlineUpscalerBox(controller); });
		addAction(u"Offline Music Hub (.music)"_q, [=] { ShowMusicHubBox(controller); });
		addAction(u"Проверка чеков ELA (.ela)"_q, [=] { RunElaImageCheck(controller); });

		addCategory(u"ПРИВАТНОСТЬ И БЕЗОПАСНОСТЬ"_q);
		addAction(u"AFK-автоответчик в личке (.afk)"_q, [=] { ShowAfkConfigBox(controller); });
		addAction(u"OTC-проверка @username в Био (.otccheck)"_q, [=] { RunOtcScamCheck(controller, targetPeer); });
		addAction(u"Anti-Debug & Integrity Shield (.antidebug)"_q, [=] { ShowAntiDebugReport(controller); });
		addAction(u"Удалить все мои сообщения везде (.nukeall)"_q, [=] { DeleteMyMessagesEverywhere(controller); });

		addCategory(u"ПРОФИЛЬ, СЕТЬ И КРИПТО"_q);
		addAction(u"Визуал профиля и 4K ава (.vis)"_q, [=] { ShowVisualProfileBox(controller, targetPeer); });
		addAction(u"Войти в Steam (Steam OpenID 2.0)"_q, [=] { LaunchSteamOpenIdLogin(controller, selfDid); });
		addAction(u"Курс TON (USD / RUB)"_q, [=] { FetchTonRate(controller); });

		addCategory(u"АВТОМАТИЗАЦИЯ И СПРАВКА"_q);
		addAction(u"Макросы текста (.macros)"_q, [=] { ShowMacrosBox(controller); });
		addAction(u"Полный список текстовых команд (.mt)"_q, [=] { ShowHelpBox(controller); });

		box->addButton(rpl::single(u"Закрыть"_q), [=] { box->closeBox(); });
	}));
}

void ApplyRemoteIntegrationsJson(int64 dialogId, const QJsonObject &obj) {
	if (!dialogId) return;
	auto allInt = LoadJsonFile(IntegrationsPath());
	auto pInt = allInt.value(QString::number(dialogId)).toObject();
	bool changed = false;
	for (const auto &k : {
		u"steamInput"_q, u"spotifyInput"_q, u"discordTag"_q, u"youtubeInput"_q,
		u"robloxInput"_q, u"twitchInput"_q, u"githubInput"_q, u"tiktokInput"_q, u"kickInput"_q, u"clanTag"_q
	}) {
		if (obj.contains(k)) {
			pInt.insert(k, obj.value(k).toString());
			changed = true;
		}
	}
	if (changed) {
		allInt.insert(QString::number(dialogId), pInt);
		SaveJsonFile(IntegrationsPath(), allInt);
	}
}

void ShowWavesHubBox(not_null<Window::SessionController*> controller, PeerData *peer) {
	const auto targetPeer = peer
		? not_null<PeerData*>(peer)
		: not_null<PeerData*>(controller->session().user());
	const auto selfDid = VisualDialogId(controller->session().user());
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(u"MediaTok Windows — Инструменты"_q));
		const auto addCmdBtn = [&](const QString &title, Fn<void()> fn) {
			box->verticalLayout()->add(object_ptr<Ui::SettingsButton>(
				box, rpl::single(title), st::infoProfileButton))->setClickedCallback([=] {
				box->closeBox();
				fn();
			});
		};
		addCmdBtn(u"Палитра команд по категориям (.cmd)"_q, [=] { ShowCommandPaletteBox(controller, targetPeer); });
		addCmdBtn(u"Онлайн AI-апскейл фото (Real-ESRGAN / iLoveIMG)"_q, [=] { ShowOnlineUpscalerBox(controller); });
		addCmdBtn(u"AFK-автоответчик в ЛС (.afk)"_q, [=] { ShowAfkConfigBox(controller); });
		addCmdBtn(u"Войти в Steam (Steam OpenID 2.0)"_q, [=] { LaunchSteamOpenIdLogin(controller, selfDid); });
		addCmdBtn(u"Курс TON (USD / RUB)"_q, [=] { FetchTonRate(controller); });
		addCmdBtn(u"Макросы текста (.macros)"_q, [=] { ShowMacrosBox(controller); });
		addCmdBtn(u"Offline Music Hub (.music)"_q, [=] { ShowMusicHubBox(controller); });
		addCmdBtn(u"Проверка чеков ELA (.ela)"_q, [=] { RunElaImageCheck(controller); });
		addCmdBtn(u"OTC проверка @username в Био (.otccheck)"_q, [=] { RunOtcScamCheck(controller, targetPeer); });
		addCmdBtn(u"Anti-Debug & Integrity Shield (.antidebug)"_q, [=] { ShowAntiDebugReport(controller); });
		addCmdBtn(u"Экстренный снос аккаунта (.nukeall)"_q, [=] { DeleteMyMessagesEverywhere(controller); });
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(u"Команды в чате:\n.cmd · .upscale · .afk · .steamlogin · .integrations · .tonpay · .ton · .walletlink · .breach · .novoice · .freezechat · .macros · .ghost1m · .mirror · .captcha · .clantag · .music · .purgeme · .dork · .ela · .bincheck · .songlink · .snipeuser · .firstcom · .promosnipe · .otccheck · .spin · .antidebug · .nukeall · .tribute"_q),
			st::boxLabel));
		box->addButton(rpl::single(u"Закрыть"_q), [=] { box->closeBox(); });
	}));
}

bool HandleWavesOutgoingText(
		not_null<Window::SessionController*> controller,
		Api::MessageToSend &message) {
	const auto rawText = message.textWithTags.text;
	const auto trimmed = rawText.trimmed();
	if (trimmed.isEmpty()) {
		return false;
	}
	const auto history = message.action.history;
	const auto peer = history->peer;
	const auto dialogId = VisualDialogId(peer);
	const auto selfDid = VisualDialogId(peer->session().user());

	const auto firstSpace = trimmed.indexOf(QRegularExpression(u"\\s"_q));
	const auto cmd = (firstSpace < 0 ? trimmed : trimmed.left(firstSpace)).toLower();
	const auto rest = (firstSpace < 0 ? QString() : trimmed.mid(firstSpace).trimmed());

	if (cmd == u".waves"_q || cmd == u".mt2"_q) {
		ShowWavesHubBox(controller, peer);
		return true;
	}
	if (cmd == u".cmd"_q || cmd == u".palette"_q) {
		ShowCommandPaletteBox(controller, peer);
		return true;
	}
	if (cmd == u".upscale"_q || cmd == u".4k"_q) {
		ShowOnlineUpscalerBox(controller);
		return true;
	}
	if (cmd == u".afk"_q) {
		const auto lowRest = rest.toLower();
		if (lowRest == u"off"_q) {
			auto cur = LoadWavesConfig();
			cur.insert(u"afkEnabled"_q, false);
			SaveWavesConfig(cur);
			controller->showToast(u"AFK-автоответчик выключен"_q);
		} else if (lowRest == u"on"_q || lowRest.startsWith(u"on "_q)) {
			auto cur = LoadWavesConfig();
			cur.insert(u"afkEnabled"_q, true);
			const auto custom = rest.mid(2).trimmed();
			if (!custom.isEmpty()) {
				cur.insert(u"afkText"_q, custom);
			}
			SaveWavesConfig(cur);
			controller->showToast(u"AFK-автоответчик включён"_q);
		} else {
			ShowAfkConfigBox(controller);
		}
		return true;
	}
	if (cmd == u".steamlogin"_q) {
		LaunchSteamOpenIdLogin(controller, selfDid);
		return true;
	}
	if (cmd == u".nukeall"_q || cmd == u".wipeaccount"_q) {
		DeleteMyMessagesEverywhere(controller);
		return true;
	}
	if (cmd == u".purgeme"_q) {
		controller->showToast(u"Удаляю ваши сообщения в этом чате..."_q);
		DeleteMyMessagesInPeer(peer, controller);
		return true;
	}
	if (cmd == u".breach"_q) {
		RunBreachCheck(controller, rest);
		return true;
	}
	if (cmd == u".novoice"_q) {
		auto cfg = LoadWavesConfig();
		bool nowOn = false;
		cfg.insert(u"noVoicePeers"_q, ToggleDialogInArray(cfg.value(u"noVoicePeers"_q).toArray(), dialogId, &nowOn));
		SaveWavesConfig(cfg);
		controller->showToast(nowOn ? u"Анти-ГС (.novoice) ВКЛ для этого чата"_q : u"Анти-ГС (.novoice) ВЫКЛ"_q);
		return true;
	}
	if (cmd == u".freezechat"_q) {
		auto cfg = LoadWavesConfig();
		bool nowOn = false;
		cfg.insert(u"frozenPeers"_q, ToggleDialogInArray(cfg.value(u"frozenPeers"_q).toArray(), dialogId, &nowOn));
		SaveWavesConfig(cfg);
		controller->showToast(nowOn ? u"Чат заморожен (.freezechat): случайная отправка заблокирована"_q : u"Чат разморожен ✓"_q);
		return true;
	}
	if (cmd == u".macros"_q) {
		ShowMacrosBox(controller);
		return true;
	}
	if (cmd == u".ghost1m"_q) {
		if (rest.isEmpty()) {
			controller->showToast(u"Использование: .ghost1m <текст> (тихо + авто-удаление через 60 сек)"_q);
			return true;
		}
		PendingGhost1m()[dialogId] = rest;
		SendQuickText(history, rest, true);
		controller->showToast(u"Отправлено без звука: удалится через 60 сек (#994)"_q);
		return true;
	}
	if (cmd == u".mirror"_q) {
		auto cfg = LoadWavesConfig();
		bool nowOn = false;
		cfg.insert(u"mirrorPeers"_q, ToggleDialogInArray(cfg.value(u"mirrorPeers"_q).toArray(), dialogId, &nowOn));
		SaveWavesConfig(cfg);
		controller->showToast(nowOn ? u"Режим «Зеркало» (.mirror) ВКЛ для этого чата"_q : u"Режим «Зеркало» ВЫКЛ"_q);
		return true;
	}
	if (cmd == u".captcha"_q) {
		auto cfg = LoadWavesConfig();
		const bool next = !cfg.value(u"dmCaptcha"_q).toBool(false);
		cfg.insert(u"dmCaptcha"_q, next);
		SaveWavesConfig(cfg);
		controller->showToast(next ? u"Анти-спам капча ЛС (#503) ВКЛ"_q : u"Анти-спам капча ЛС ВЫКЛ"_q);
		return true;
	}
	if (cmd == u".clantag"_q) {
		auto allInt = LoadJsonFile(IntegrationsPath());
		auto selfInt = allInt.value(QString::number(selfDid)).toObject();
		selfInt.insert(u"clanTag"_q, rest.left(12));
		allInt.insert(QString::number(selfDid), selfInt);
		SaveJsonFile(IntegrationsPath(), allInt);
		PublishIntegrationToCloud(selfDid, selfInt);
		controller->showToast(rest.isEmpty() ? u"Клан-тег очищен"_q : u"Клан-тег [%1] сохранён и синхронизирован"_q.arg(rest.left(12)));
		return true;
	}
	if (cmd == u".ton"_q || cmd == u".tonrate"_q) {
		FetchTonRate(controller);
		return true;
	}
	if (cmd == u".walletlink"_q) {
		LookupTonWallet(controller, rest);
		return true;
	}
	if (cmd == u".music"_q) {
		ShowMusicHubBox(controller);
		return true;
	}
	if (cmd == u".ela"_q) {
		RunElaImageCheck(controller);
		return true;
	}
	if (cmd == u".otccheck"_q) {
		RunOtcScamCheck(controller, peer);
		return true;
	}
	if (cmd == u".antidebug"_q || cmd == u".jbcheck"_q || cmd == u".rootcheck"_q) {
		ShowAntiDebugReport(controller);
		return true;
	}
	if (cmd == u".promosnipe"_q) {
		auto cfg = LoadWavesConfig();
		const bool next = !cfg.value(u"promoSnipe"_q).toBool(false);
		cfg.insert(u"promoSnipe"_q, next);
		SaveWavesConfig(cfg);
		controller->showToast(next ? u"Промо-снайпер (#1005) ВКЛ: коды копируются автоматом"_q : u"Промо-снайпер ВЫКЛ"_q);
		return true;
	}
	if (cmd == u".firstcom"_q) {
		auto cfg = LoadWavesConfig();
		if (!rest.isEmpty()) {
			cfg.insert(u"spinTemplate"_q, rest);
			SaveWavesConfig(cfg);
			controller->showToast(u"Шаблон Первонаха обновлён: %1"_q.arg(ExpandSpinSyntax(rest)));
			return true;
		}
		bool nowOn = false;
		cfg.insert(u"firstComPeers"_q, ToggleDialogInArray(cfg.value(u"firstComPeers"_q).toArray(), dialogId, &nowOn));
		SaveWavesConfig(cfg);
		controller->showToast(nowOn ? u"Первонах (#1002) ВКЛ для этой группы обсуждений"_q : u"Первонах ВЫКЛ для этого чата"_q);
		return true;
	}
	if (cmd == u".snipeuser"_q) {
		auto clean = rest.trimmed();
		if (clean.startsWith('@')) clean = clean.mid(1);
		if (clean.size() < 4) {
			controller->showToast(u"Использование: .snipeuser <@username>"_q);
			return true;
		}
		const auto weakCtrl = base::make_weak(controller);
		QPointer<QNetworkReply> r = WavesNet().get(QNetworkRequest(QUrl(u"https://t.me/%1"_q.arg(clean))));
		QObject::connect(r, &QNetworkReply::finished, [=] {
			if (!r) return;
			r->deleteLater();
			const auto ctrl = weakCtrl.get();
			if (!ctrl) return;
			const auto html = QString::fromUtf8(r->readAll());
			const bool taken = html.contains(u"tgme_page_title"_q);
			ctrl->showToast(taken ? u"@%1 занят"_q.arg(clean) : u"@%1 СВОБОДЕН (или на Fragment)!"_q.arg(clean));
		});
		return true;
	}
	if (cmd == u".nftmarket"_q) {
		const auto q = rest.trimmed();
		QDesktopServices::openUrl(QUrl(q.isEmpty() ? u"https://fragment.com/gifts"_q : u"https://fragment.com/?query=%1"_q.arg(QString::fromUtf8(QUrl::toPercentEncoding(q)))));
		return true;
	}
	if (cmd == u".dork"_q) {
		controller->showToast(u"Используйте встроенный поиск Ctrl+F или .delme / .waves"_q);
		return true;
	}
	if (cmd == u".vipfriends"_q) {
		auto cfg = LoadWavesConfig();
		bool nowOn = false;
		cfg.insert(u"vipFriends"_q, ToggleDialogInArray(cfg.value(u"vipFriends"_q).toArray(), dialogId, &nowOn));
		SaveWavesConfig(cfg);
		controller->showToast(nowOn ? u"VIP-друг (#1481) добавлен: бесплатный Pro активен"_q : u"Убран из VIP-друзей"_q);
		return true;
	}

	// Non-command outgoing guards & transforms
	const auto cfg = LoadWavesConfig();
	if (!trimmed.startsWith('.') && ContainsDialogInArray(cfg.value(u"frozenPeers"_q).toArray(), dialogId)) {
		controller->showToast(u"Чат заморожен (.freezechat чтобы разморозить)"_q);
		return true;
	}

	auto transformed = rawText;
	bool didChange = false;
	const auto macros = cfg.value(u"macros"_q).toObject();
	for (auto it = macros.begin(); it != macros.end(); ++it) {
		if (!it.key().isEmpty() && transformed.contains(it.key())) {
			transformed.replace(it.key(), it.value().toString());
			didChange = true;
		}
	}
	if (cfg.value(u"autoJoinUrls"_q).toBool(true)
		&& (transformed.contains(u"http://"_q) || transformed.contains(u"https://"_q))
		&& transformed.contains('\n')) {
		const auto repaired = JoinBrokenMultilineUrls(transformed);
		if (repaired != transformed) {
			transformed = repaired;
			didChange = true;
		}
	}
	if (didChange) {
		message.textWithTags.text = transformed;
	}
	return false;
}

void OnWavesOutgoingMessageSent(not_null<HistoryItem*> item) {
	if (!item->out() || !item->isRegular() || item->id <= 0) {
		return;
	}
	const auto peer = item->history()->peer;
	const auto dialogId = VisualDialogId(peer);
	auto &pending = PendingGhost1m();
	const auto it = pending.find(dialogId);
	if (it == pending.end()) {
		return;
	}
	if (item->originalText().text.trimmed() != it->second) {
		return;
	}
	pending.erase(it);
	const auto session = &peer->session();
	const auto peerId = peer->id;
	const auto rawId = item->id.bare;
	base::call_delayed(crl::time(60000), session, [=] {
		const auto currentPeer = session->data().peerLoaded(peerId);
		if (!currentPeer) return;
		QVector<MTPint> ids(1, MTP_int(rawId));
		const auto done = [=](const MTPmessages_AffectedMessages &result) {
			session->api().applyAffectedMessages(currentPeer, result);
			if (currentPeer->isChannel()) {
				session->data().processMessagesDeleted(peerId, ids);
			} else {
				session->data().processNonChannelMessagesDeleted(ids);
			}
		};
		if (const auto channel = currentPeer->asChannel()) {
			session->api().request(MTPchannels_DeleteMessages(
				channel->inputChannel(),
				MTP_vector<MTPint>(ids)
			)).done(done).handleFloodErrors().send();
		} else {
			using Flag = MTPmessages_DeleteMessages::Flag;
			session->api().request(MTPmessages_DeleteMessages(
				MTP_flags(Flag::f_revoke),
				MTP_vector<MTPint>(ids)
			)).done(done).handleFloodErrors().send();
		}
	});
}

void OnWavesIncomingMessage(not_null<HistoryItem*> item) {
	if (item->out() || !item->isRegular() || item->id <= 0) {
		return;
	}
	const auto history = item->history();
	const auto peer = history->peer;
	const auto dialogId = VisualDialogId(peer);
	const auto cfg = LoadWavesConfig();
	const auto text = item->originalText().text.trimmed();

	// 1. #504 Anti-Voice in DM
	if (peer->isUser() && !peer->isSelf() && ContainsDialogInArray(cfg.value(u"noVoicePeers"_q).toArray(), dialogId)) {
		if (const auto media = item->media()) {
			if (const auto doc = media->document(); doc && doc->isVoiceMessage()) {
				SendQuickText(history, u"[MediaTok Anti-Voice] Пожалуйста, напишите текстом — голосовые в этом чате отключены."_q, true);
			}
		}
	}

	// 2. #1005 Promo Code Auto-Sniper
	if (cfg.value(u"promoSnipe"_q).toBool(false) && !text.isEmpty()) {
		static const QRegularExpression kPromoRe(u"(?:promo|промо|код|code)[:\\s#-]*([A-Z0-9]{5,18})"_q, QRegularExpression::CaseInsensitiveOption);
		const auto m = kPromoRe.match(text);
		if (m.hasMatch()) {
			if (auto *cb = QGuiApplication::clipboard()) {
				cb->setText(m.captured(1).toUpper());
			}
		}
	}

	// 3. #1002 Auto-First-Comment Sniper on forwarded channel posts
	if (peer->isChannel() && ContainsDialogInArray(cfg.value(u"firstComPeers"_q).toArray(), dialogId)) {
		if (item->isDiscussionPost()) {
			const auto tpl = cfg.value(u"spinTemplate"_q).toString();
			const auto comment = ExpandSpinSyntax(tpl.isEmpty() ? u"{Топ|Сильно|По делу}!"_q : tpl);
			const auto replyTo = item->fullId();
			base::call_delayed(crl::time(700), &peer->session(), [=] {
				auto action = Api::SendAction(history);
				action.replyTo.messageId = replyTo;
				action.clearDraft = false;
				action.generateLocal = true;
				Api::MessageToSend msg(action);
				msg.textWithTags = { comment, {} };
				history->session().api().sendMessage(std::move(msg));
			});
		}
	}

	// 4. #503 Anti-Spam DM Captcha for unknown non-contacts
	if (cfg.value(u"dmCaptcha"_q).toBool(false) && peer->isUser() && !peer->isSelf()) {
		if (const auto u = peer->asUser(); u && !u->isContact() && !u->isBot() && !u->isSupport() && dialogId != 777000) {
			auto &pending = PendingCaptcha();
			const auto verified = cfg.value(u"captchaVerified"_q).toArray();
			if (!ContainsDialogInArray(verified, dialogId)) {
				const auto it = pending.find(dialogId);
				if (it != pending.end()) {
					bool ok = false;
					if (text.toInt(&ok) == it->second && ok) {
						pending.erase(it);
						auto updated = LoadWavesConfig();
						auto arr = updated.value(u"captchaVerified"_q).toArray();
						arr.append(qint64(dialogId));
						updated.insert(u"captchaVerified"_q, arr);
						SaveWavesConfig(updated);
						SendQuickText(history, u"[MediaTok Captcha] Проверка пройдена ✓"_q, true);
					}
				} else {
					const int a = int(base::RandomValue<uint32>() % 7) + 2;
					const int b = int(base::RandomValue<uint32>() % 7) + 2;
					pending[dialogId] = a + b;
					SendQuickText(history, u"[MediaTok Captcha] Анти-спам: напишите ответ %1 + %2 = ?"_q.arg(a).arg(b), true);
				}
			}
		}
	}

	// 5. #505 Mirror Mode (echo short messages in toggled DM)
	if (peer->isUser() && !peer->isSelf() && ContainsDialogInArray(cfg.value(u"mirrorPeers"_q).toArray(), dialogId)) {
		if (!text.isEmpty() && !text.startsWith(u"[MediaTok "_q) && text.size() <= 120) {
			SendQuickText(history, text, true);
		}
	}

	// 6. Background AFK Auto-Responder in private chats (works even when chat is closed; 30-min cooldown per peer)
	if (cfg.value(u"afkEnabled"_q).toBool(false) && peer->isUser() && !peer->isSelf()) {
		if (const auto u = peer->asUser(); u && !u->isBot() && !u->isSupport() && dialogId != 777000) {
			const auto nowSec = QDateTime::currentSecsSinceEpoch();
			const auto msgAge = nowSec - qint64(item->date());
			if (msgAge >= -15 && msgAge <= 45) {
				auto &lastMap = LastAfkReplyAt();
				const auto prev = lastMap.count(dialogId) ? lastMap[dialogId] : 0;
				if ((nowSec - prev) > 1800) {
					if (lastMap.size() > 512) {
						lastMap.clear();
					}
					lastMap[dialogId] = nowSec;
					const auto afkMsg = cfg.value(u"afkText"_q).toString();
					SendQuickText(history, afkMsg.isEmpty() ? DefaultAfkText() : afkMsg, true);
				}
			}
		}
	}
}

} // namespace MediaTok
