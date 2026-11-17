// This is part of MediaTok (AyuGramPlus fork).
#include "ayu/features/mediatok/mediatok_features.h"

#include "apiwrap.h"
#include "api/api_sending.h"
#include "ayu/ayu_settings.h"
#include "ayu/features/mediatok/mediatok_process.h"
#include "ayu/features/mediatok/mediatok_voicemod.h"
#include "ayu/utils/telegram_helpers.h"
#include "base/call_delayed.h"
#include "base/random.h"
#include "core/application.h"
#include "data/data_channel.h"
#include "data/data_chat.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_folder.h"
#include "data/data_histories.h"
#include "data/data_media_types.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_search_controller.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "dialogs/dialogs_indexed_list.h"
#include "dialogs/dialogs_main_list.h"
#include "dialogs/dialogs_row.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "storage/localimageloader.h"
#include "storage/storage_media_prepare.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "ui/boxes/confirm_box.h"
#include "ui/chat/attach/attach_prepare.h"
#include "ui/image/image.h"
#include "ui/text/text_utilities.h"
#include "ui/toast/toast.h"
#include "window/window_session_controller.h"

#include <QtCore/QBuffer>
#include <QtCore/QCoreApplication>
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
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <algorithm>
#include <unordered_set>

namespace MediaTok {
namespace {

QNetworkAccessManager &NetworkManager() {
	static QNetworkAccessManager nam;
	return nam;
}

QString FindToolExecutable(const QString &name) {
	const auto appDirExe = QCoreApplication::applicationDirPath() + u"/"_q + name;
	if (QFileInfo::exists(appDirExe)) {
		return QFileInfo(appDirExe).absoluteFilePath();
	}
	const auto tdataExe = cWorkingDir() + u"tdata/"_q + name;
	if (QFileInfo::exists(tdataExe)) {
		return QFileInfo(tdataExe).absoluteFilePath();
	}
	return QString();
}

void CleanupOldVideoTempDirs() {
	const auto baseDirPath = cWorkingDir() + u"tdata/MediaTok_Videos"_q;
	QDir baseDir(baseDirPath);
	if (!baseDir.exists()) {
		return;
	}
	const auto now = QDateTime::currentDateTimeUtc();
	const auto entries = baseDir.entryInfoList(
		QDir::Dirs | QDir::NoDotAndDotDot);
	for (const auto &entry : entries) {
		if (entry.lastModified().toUTC().secsTo(now) > 12 * 3600) {
			QDir(entry.absoluteFilePath()).removeRecursively();
		}
	}
}

void DownloadBinaryTool(
		const QString &url,
		const QString &fileName,
		qint64 minBytes,
		int timeoutMs,
		Fn<void(QString exePath, QString error)> done) {
	const auto targetPath = QFileInfo(
		cWorkingDir() + u"tdata/"_q + fileName).absoluteFilePath();
	const auto tmpPath = targetPath + u".tmp"_q;
	QDir().mkpath(QFileInfo(targetPath).absolutePath());

	QNetworkRequest req((QUrl(url)));
	req.setAttribute(
		QNetworkRequest::RedirectPolicyAttribute,
		QNetworkRequest::NoLessSafeRedirectPolicy);
	req.setHeader(
		QNetworkRequest::UserAgentHeader,
		u"MediaTokDesktop/1.0"_q);

	QPointer<QNetworkReply> reply = NetworkManager().get(req);
	auto timer = new QTimer(reply);
	timer->setSingleShot(true);
	timer->setInterval(timeoutMs);
	QObject::connect(timer, &QTimer::timeout, reply, [=] {
		if (reply) {
			reply->abort();
		}
	});
	timer->start();

	QObject::connect(reply, &QNetworkReply::finished, [=] {
		if (!reply) {
			done(QString(), u"Загрузка %1 отменена"_q.arg(fileName));
			return;
		}
		reply->deleteLater();
		if (reply->error() != QNetworkReply::NoError) {
			done(QString(), reply->errorString());
			return;
		}
		const auto bytes = reply->readAll();
		if (bytes.size() < minBytes) {
			done(QString(), u"Некорректный файл %1"_q.arg(fileName));
			return;
		}
		QFile::remove(tmpPath);
		QFile f(tmpPath);
		if (!f.open(QIODevice::WriteOnly)) {
			done(QString(), u"Не удалось сохранить %1"_q.arg(fileName));
			return;
		}
		f.write(bytes);
		f.close();
		QFile::remove(targetPath);
		if (!QFile::rename(tmpPath, targetPath)) {
			QFile::remove(tmpPath);
			done(QString(), u"Не удалось переименовать %1"_q.arg(fileName));
			return;
		}
		done(targetPath, QString());
	});
}

void EnsureVideoTools(
		not_null<Window::SessionController*> controller,
		bool needYtDlp,
		bool needFfmpeg,
		Fn<void(QString ytDlpExe, QString ffmpegExe, QString error)> done) {
	struct State {
		QString ytDlp;
		QString ffmpeg;
		QString error;
		int pending = 0;
	};
	auto state = std::make_shared<State>();
	state->ytDlp = FindToolExecutable(u"yt-dlp.exe"_q);
	state->ffmpeg = FindToolExecutable(u"ffmpeg.exe"_q);

	const bool dlYt = needYtDlp && state->ytDlp.isEmpty();
	const bool dlFf = needFfmpeg && state->ffmpeg.isEmpty();
	if (!dlYt && !dlFf) {
		done(state->ytDlp, state->ffmpeg, QString());
		return;
	}

	if (dlYt && dlFf) {
		controller->showToast(u"MediaTok: скачиваю yt-dlp.exe и ffmpeg.exe (один раз)..."_q);
	} else if (dlYt) {
		controller->showToast(u"MediaTok: скачиваю yt-dlp.exe (один раз)..."_q);
	} else {
		controller->showToast(u"MediaTok: скачиваю ffmpeg.exe (~79 МБ, один раз)..."_q);
	}

	state->pending = (dlYt ? 1 : 0) + (dlFf ? 1 : 0);
	const auto finishOne = [=] {
		if (--state->pending == 0) {
			done(state->ytDlp, state->ffmpeg, state->error);
		}
	};

	if (dlYt) {
		DownloadBinaryTool(
			u"https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe"_q,
			u"yt-dlp.exe"_q,
			1024 * 1024,
			120000,
			[=](QString path, QString err) {
				if (path.isEmpty() && state->error.isEmpty()) {
					state->error = err;
				}
				state->ytDlp = path;
				finishOne();
			});
	}
	if (dlFf) {
		DownloadBinaryTool(
			u"https://github.com/eugeneware/ffmpeg-static/releases/latest/download/ffmpeg-win32-x64"_q,
			u"ffmpeg.exe"_q,
			10 * 1024 * 1024,
			240000,
			[=](QString path, QString err) {
				if (path.isEmpty() && state->error.isEmpty()) {
					state->error = err;
				}
				state->ffmpeg = path;
				finishOne();
			});
	}
}

void ConvertVideoFileToCircleAndSend(
		not_null<Window::SessionController*> controller,
		not_null<History*> history,
		const Api::SendAction &baseAction,
		const QString &sourcePath,
		const QString &cleanupDir) {
	const auto weak = base::make_weak(controller);
	const auto session = &history->session();

	EnsureVideoTools(controller, false, true, [=](
			QString,
			QString ffmpegExe,
			QString error) {
		const auto strong = weak.get();
		if (ffmpegExe.isEmpty()) {
			if (!cleanupDir.isEmpty()) {
				QDir(cleanupDir).removeRecursively();
			}
			if (strong) {
				strong->showToast(u"MediaTok ffmpeg: %1"_q.arg(error));
			}
			return;
		}
		if (strong) {
			strong->showToast(u"MediaTok: делаю кружок..."_q);
		}

		CleanupOldVideoTempDirs();
		const auto circleDir = QFileInfo(
			cWorkingDir()
			+ u"tdata/MediaTok_Videos/circle_%1_%2"_q
				.arg(QDateTime::currentMSecsSinceEpoch())
				.arg(base::RandomValue<uint32>())
		).absoluteFilePath();
		QDir().mkpath(circleDir);
		const auto outPath = circleDir + u"/circle.mp4"_q;

		crl::async([=] {
			QStringList args;
			args << u"-y"_q
				<< u"-i"_q << sourcePath
				<< u"-t"_q << u"60"_q
				<< u"-vf"_q << u"crop='min(iw,ih)':'min(iw,ih)',scale=384:384:flags=lanczos,setsar=1"_q
				<< u"-c:v"_q << u"libx264"_q
				<< u"-profile:v"_q << u"baseline"_q
				<< u"-level"_q << u"3.1"_q
				<< u"-preset"_q << u"veryfast"_q
				<< u"-crf"_q << u"23"_q
				<< u"-pix_fmt"_q << u"yuv420p"_q
				<< u"-c:a"_q << u"aac"_q
				<< u"-b:a"_q << u"96k"_q
				<< u"-ac"_q << u"1"_q
				<< u"-ar"_q << u"48000"_q
				<< u"-movflags"_q << u"+faststart"_q
				<< outPath;

			int code = -1;
			const bool ok = RunHiddenProcess(ffmpegExe, args, circleDir, 180000, &code);
			QByteArray mp4Bytes;
			if (ok) {
				QFile f(outPath);
				if (f.open(QIODevice::ReadOnly)) {
					if (f.size() > 0 && f.size() <= 50 * 1024 * 1024) {
						mp4Bytes = f.readAll();
					}
					f.close();
				}
			}
			QDir(circleDir).removeRecursively();
			if (!cleanupDir.isEmpty()) {
				QDir(cleanupDir).removeRecursively();
			}

			crl::on_main(session, [=] {
				if (mp4Bytes.isEmpty()) {
					if (const auto s = weak.get()) {
						s->showToast(u"MediaTok: не удалось сконвертировать видео в кружок"_q);
					}
					return;
				}
				auto sendAction = baseAction;
				sendAction.clearDraft = false;
				sendAction.generateLocal = true;
				session->api().sendVoiceMessage(
					mp4Bytes,
					{},
					0,
					true,
					sendAction);
			});
		});
	});
}

void RunYtDlpDownload(
		not_null<Window::SessionController*> controller,
		not_null<History*> history,
		const Api::SendAction &baseAction,
		const QString &ytDlpExe,
		const QString &ffmpegExe,
		const QString &url,
		bool asCircle,
		std::optional<Api::MessageToSend> fallbackMessage) {
	const auto weak = base::make_weak(controller);
	const auto session = &history->session();

	CleanupOldVideoTempDirs();
	const auto jobDir = QFileInfo(
		cWorkingDir()
		+ u"tdata/MediaTok_Videos/dl_%1_%2"_q
			.arg(QDateTime::currentMSecsSinceEpoch())
			.arg(base::RandomValue<uint32>())
	).absoluteFilePath();
	QDir().mkpath(jobDir);

	const auto fallbackHolder = std::make_shared<std::optional<Api::MessageToSend>>(
		std::move(fallbackMessage));

	const auto showErrorOrFallback = [=](const QString &err) {
		if (fallbackHolder->has_value()) {
			auto msg = std::move(**fallbackHolder);
			fallbackHolder->reset();
			msg.action.generateLocal = true;
			session->api().sendMessage(std::move(msg));
			return;
		}
		if (const auto strong = weak.get()) {
			strong->showToast(u"MediaTok: %1"_q.arg(err));
		}
	};

	crl::async([=] {
		QStringList args;
		args << u"--no-playlist"_q
			<< u"--no-warnings"_q
			<< u"--socket-timeout"_q << u"15"_q
			<< u"--max-filesize"_q << u"1950M"_q;

		if (!ffmpegExe.isEmpty()) {
			args << u"--ffmpeg-location"_q << ffmpegExe
				<< u"-f"_q << u"bv*[vcodec^=avc1][filesize<1900M]+ba[ext=m4a]/b[ext=mp4]/b"_q
				<< u"--merge-output-format"_q << u"mp4"_q;
		} else {
			args << u"-f"_q << u"best[ext=mp4][vcodec!=none][acodec!=none]/best[ext=mp4]/best"_q;
		}
		args << u"-o"_q << u"video.%(ext)s"_q << url;

		int code = -1;
		const bool ok = RunHiddenProcess(ytDlpExe, args, jobDir, 180000, &code);

		QString downloadedPath;
		QDir outDir(jobDir);
		const auto files = outDir.entryInfoList(
			QDir::Files | QDir::NoDotAndDotDot,
			QDir::Size | QDir::Reversed);
		for (const auto &fi : files) {
			const auto name = fi.fileName().toLower();
			if (!name.endsWith(u".part"_q)
				&& !name.endsWith(u".ytdl"_q)
				&& fi.size() > 0) {
				downloadedPath = fi.absoluteFilePath();
				break;
			}
		}

		crl::on_main(session, [=] {
			if (!ok || downloadedPath.isEmpty()) {
				QDir(jobDir).removeRecursively();
				showErrorOrFallback(
					u"не удалось скачать видео по ссылке"_q);
				return;
			}
			if (asCircle) {
				if (const auto s = weak.get()) {
					ConvertVideoFileToCircleAndSend(
						s,
						history,
						baseAction,
						downloadedPath,
						jobDir);
				} else {
					QDir(jobDir).removeRecursively();
				}
				return;
			}
			const auto premium = session->user()->isPremium();
			auto list = Storage::PrepareMediaList(
				QStringList{ downloadedPath },
				st::sendMediaPreviewSize,
				premium);
			if (list.error != Ui::PreparedList::Error::None
				|| list.files.empty()) {
				QDir(jobDir).removeRecursively();
				showErrorOrFallback(
					u"не удалось подготовить скачанный файл"_q);
				return;
			}
			auto sendAction = baseAction;
			sendAction.clearDraft = false;
			sendAction.generateLocal = true;
			session->api().sendFiles(
				std::move(list),
				SendMediaType::Photo,
				nullptr,
				sendAction);
		});
	});
}

void DownloadAndSendVideoFromUrl(
		not_null<Window::SessionController*> controller,
		not_null<History*> history,
		const Api::SendAction &baseAction,
		const QString &url,
		bool asCircle,
		std::optional<Api::MessageToSend> fallbackMessage) {
	const auto weak = base::make_weak(controller);
	const auto session = &history->session();
	const auto fallbackHolder = std::make_shared<std::optional<Api::MessageToSend>>(
		std::move(fallbackMessage));

	EnsureVideoTools(controller, true, asCircle, [=](
			QString ytDlpExe,
			QString ffmpegExe,
			QString error) {
		const auto strong = weak.get();
		if (ytDlpExe.isEmpty() || (asCircle && ffmpegExe.isEmpty())) {
			if (fallbackHolder->has_value()) {
				auto msg = std::move(**fallbackHolder);
				fallbackHolder->reset();
				msg.action.generateLocal = true;
				session->api().sendMessage(std::move(msg));
			} else if (strong) {
				strong->showToast(u"MediaTok: %1"_q.arg(error));
			}
			return;
		}
		if (strong) {
			strong->showToast(asCircle
				? u"MediaTok: скачиваю видео для кружка..."_q
				: (fallbackHolder->has_value()
					? u"MediaTok: загружаю видео по ссылке..."_q
					: u"MediaTok: скачиваю видео..."_q));
			auto nextFallback = std::move(*fallbackHolder);
			fallbackHolder->reset();
			RunYtDlpDownload(
				strong,
				history,
				baseAction,
				ytDlpExe,
				ffmpegExe,
				url,
				asCircle,
				std::move(nextFallback));
		}
	});
}

QByteArray ExtractPhotoJpeg(HistoryItem *item) {
	if (!item) {
		return {};
	}
	const auto media = item->media();
	if (!media) {
		return {};
	}
	const auto photo = media->photo();
	if (!photo || photo->isNull()) {
		return {};
	}
	const auto view = photo->createMediaView();
	view->wanted(Data::PhotoSize::Large, item->fullId());
	auto bytes = view->imageBytes(Data::PhotoSize::Large);
	if (!bytes.isEmpty()) {
		return bytes;
	}
	const auto img = view->image(Data::PhotoSize::Large)
		? view->image(Data::PhotoSize::Large)
		: view->image(Data::PhotoSize::Thumbnail);
	if (img && !img->original().isNull()) {
		QBuffer buf(&bytes);
		buf.open(QIODevice::WriteOnly);
		img->original().save(&buf, "JPEG", 85);
	}
	return bytes;
}

std::vector<QString> SplitMessageChunks(const QString &text, int maxLen = 4000) {
	std::vector<QString> chunks;
	int pos = 0;
	const int len = text.size();
	while (pos < len) {
		if (len - pos <= maxLen) {
			chunks.push_back(text.mid(pos));
			break;
		}
		int splitAt = text.lastIndexOf('\n', pos + maxLen);
		if (splitAt <= pos + maxLen / 2) {
			splitAt = text.lastIndexOf(' ', pos + maxLen);
		}
		if (splitAt <= pos) {
			splitAt = pos + maxLen;
		}
		const auto chunk = text.mid(pos, splitAt - pos).trimmed();
		if (!chunk.isEmpty()) {
			chunks.push_back(chunk);
		}
		pos = splitAt;
		while (pos < len && (text[pos] == '\n' || text[pos] == ' ')) {
			++pos;
		}
	}
	return chunks;
}

bool IsStandaloneHttpUrl(const QString &text) {
	if (text.isEmpty() || text.contains(QRegularExpression(u"\\s"_q))) {
		return false;
	}
	if (!text.startsWith(u"http://"_q, Qt::CaseInsensitive)
		&& !text.startsWith(u"https://"_q, Qt::CaseInsensitive)) {
		return false;
	}
	const QUrl url(text);
	if (!url.isValid() || url.host().isEmpty()) {
		return false;
	}
	const auto host = url.host().toLower();
	if (host == u"t.me"_q
		|| host.endsWith(u".t.me"_q)
		|| host == u"telegram.me"_q
		|| host == u"telegram.dog"_q) {
		return false;
	}
	return true;
}

std::optional<int> ParseVoicemodArg(const QString &arg) {
	const auto k = arg.trimmed().toLower();
	if (k == u"0"_q || k == u"off"_q || k == u"выкл"_q || k == u"normal"_q) {
		return 0;
	} else if (k == u"1"_q || k == u"deep"_q || k == u"demon"_q || k == u"бас"_q || k == u"демон"_q) {
		return 1;
	} else if (k == u"2"_q || k == u"helium"_q || k == u"high"_q || k == u"гелий"_q) {
		return 2;
	} else if (k == u"3"_q || k == u"robot"_q || k == u"робот"_q) {
		return 3;
	} else if (k == u"4"_q || k == u"radio"_q || k == u"megaphone"_q || k == u"рация"_q || k == u"мегафон"_q) {
		return 4;
	} else if (k == u"5"_q || k == u"echo"_q || k == u"эхо"_q) {
		return 5;
	} else if (k == u"6"_q || k == u"anon"_q || k == u"whisper"_q || k == u"аноним"_q || k == u"шёпот"_q || k == u"шепот"_q) {
		return 6;
	} else if (k == u"7"_q || k == u"bass"_q || k == u"studio"_q || k == u"студия"_q) {
		return 7;
	}
	return std::nullopt;
}

} // namespace

QString VoicemodPresetName(int preset) {
	switch (preset) {
	case 1: return u"Демон (Глубокий бас)"_q;
	case 2: return u"Гелий (Высокий голос)"_q;
	case 3: return u"Робот"_q;
	case 4: return u"Рация / Мегафон"_q;
	case 5: return u"Эхо"_q;
	case 6: return u"Аноним (Шёпот)"_q;
	case 7: return u"Студийный бас"_q;
	default: return u"Выкл (Обычный)"_q;
	}
}

void AskGemini(
		not_null<Main::Session*> session,
		const QString &prompt,
		Fn<void(QString answer, QString error)> callback,
		const QByteArray &imageJpegBytes) {
	const auto &settings = AyuSettings::getInstance();
	const auto apiKey = settings.geminiApiKey().trimmed();
	if (apiKey.isEmpty()) {
		callback(
			QString(),
			u"Не указан Gemini API Key! Задайте его в Настройки -> MediaTok."_q);
		return;
	}
	auto model = settings.geminiModel().trimmed();
	if (model.startsWith(u"models/"_q, Qt::CaseInsensitive)) {
		model = model.mid(7);
	}
	if (model.isEmpty()) {
		model = u"gemini-2.5-flash"_q;
	}

	const auto endpoint = u"https://generativelanguage.googleapis.com/v1beta/models/%1:generateContent?key=%2"_q
		.arg(QString::fromLatin1(QUrl::toPercentEncoding(model, "-._")),
			QString::fromLatin1(QUrl::toPercentEncoding(apiKey)));

	QJsonArray parts;
	if (!imageJpegBytes.isEmpty()) {
		QJsonObject inlineData;
		inlineData.insert(u"mime_type"_q, u"image/jpeg"_q);
		inlineData.insert(u"data"_q, QString::fromLatin1(imageJpegBytes.toBase64()));
		QJsonObject imgPart;
		imgPart.insert(u"inline_data"_q, inlineData);
		parts.append(imgPart);
	}
	QJsonObject textPart;
	textPart.insert(u"text"_q, prompt);
	parts.append(textPart);

	QJsonObject content;
	content.insert(u"parts"_q, parts);
	QJsonArray contents;
	contents.append(content);
	QJsonObject root;
	root.insert(u"contents"_q, contents);

	const auto systemPrompt = settings.geminiSystemPrompt().trimmed();
	if (!systemPrompt.isEmpty()) {
		QJsonObject sysPart;
		sysPart.insert(u"text"_q, systemPrompt);
		QJsonArray sysParts;
		sysParts.append(sysPart);
		QJsonObject sysObj;
		sysObj.insert(u"parts"_q, sysParts);
		root.insert(u"system_instruction"_q, sysObj);
	}

	const auto body = QJsonDocument(root).toJson(QJsonDocument::Compact);

	QNetworkRequest req((QUrl(endpoint)));
	req.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_q);

	QPointer<QNetworkReply> reply = NetworkManager().post(req, body);
	auto timer = new QTimer(reply);
	timer->setSingleShot(true);
	timer->setInterval(60000);
	QObject::connect(timer, &QTimer::timeout, reply, [=] {
		if (reply) {
			reply->abort();
		}
	});
	timer->start();

	QObject::connect(reply, &QNetworkReply::finished, [=] {
		if (!reply) {
			callback(QString(), u"Запрос к Gemini прерван"_q);
			return;
		}
		reply->deleteLater();
		const auto raw = reply->readAll();
		const auto doc = QJsonDocument::fromJson(raw);
		if (doc.isObject()) {
			const auto obj = doc.object();
			if (obj.contains(u"error"_q)) {
				const auto errMsg = obj.value(u"error"_q).toObject().value(u"message"_q).toString();
				callback(QString(), errMsg.isEmpty() ? u"Ошибка Gemini API"_q : errMsg);
				return;
			}
			const auto candidates = obj.value(u"candidates"_q).toArray();
			if (!candidates.isEmpty()) {
				const auto cParts = candidates.at(0).toObject()
					.value(u"content"_q).toObject()
					.value(u"parts"_q).toArray();
				QStringList outParts;
				for (const auto &p : cParts) {
					const auto txt = p.toObject().value(u"text"_q).toString();
					if (!txt.isEmpty()) {
						outParts.push_back(txt);
					}
				}
				const auto finalStr = outParts.join(u"\n"_q).trimmed();
				if (!finalStr.isEmpty()) {
					callback(finalStr, QString());
					return;
				}
			}
		}
		if (reply->error() != QNetworkReply::NoError) {
			callback(QString(), reply->errorString());
			return;
		}
		callback(QString(), u"Пустой ответ от Gemini"_q);
	});
}

void TestGeminiApi(not_null<Window::SessionController*> controller) {
	const auto weak = base::make_weak(controller);
	const auto session = &controller->session();
	auto model = AyuSettings::getInstance().geminiModel().trimmed();
	if (model.isEmpty()) {
		model = u"gemini-2.5-flash"_q;
	}
	controller->showToast(u"Gemini (%1): проверяю ключ..."_q.arg(model));
	AskGemini(session, u"Ответь ровно одним словом: OK"_q, [=](QString answer, QString error) {
		const auto strong = weak.get();
		if (!strong) {
			return;
		}
		if (!error.isEmpty()) {
			strong->showToast(u"Gemini ошибка: %1"_q.arg(error));
			return;
		}
		strong->showToast(u"Gemini (%1) работает: %2"_q.arg(model, answer.left(80)));
	});
}

void AskGeminiAboutItem(
		not_null<Window::SessionController*> controller,
		not_null<HistoryItem*> item) {
	const auto weak = base::make_weak(controller);
	const auto session = &controller->session();
	const auto text = item->originalText().text.trimmed();
	const auto photoBytes = ExtractPhotoJpeg(item);
	if (text.isEmpty() && photoBytes.isEmpty()) {
		controller->showToast(u"MediaTok: в этом сообщении нет текста или загруженного фото"_q);
		return;
	}
	QString prompt;
	if (!photoBytes.isEmpty() && !text.isEmpty()) {
		prompt = u"Проанализируй изображение и текст:\n\"%1\"\n\nДай понятный ответ или перевод."_q.arg(text);
	} else if (!photoBytes.isEmpty()) {
		prompt = u"Что на этом изображении? Опиши кратко и по сути (и переведи текст на нём, если он есть)."_q;
	} else {
		prompt = u"Объясни или переведи это сообщение понятно и по сути:\n\n%1"_q.arg(text);
	}
	controller->showToast(
		u"Gemini (%1) анализирует..."_q.arg(AyuSettings::getInstance().geminiModel()));
	AskGemini(session, prompt, [=](QString answer, QString error) {
		const auto strong = weak.get();
		if (!strong) {
			return;
		}
		if (!error.isEmpty()) {
			strong->showToast(u"Gemini: %1"_q.arg(error));
			return;
		}
		strong->show(Ui::MakeInformBox(answer));
	}, photoBytes);
}

bool HasYtDlpTool() {
	return !FindToolExecutable(u"yt-dlp.exe"_q).isEmpty();
}

bool HasFfmpegTool() {
	return !FindToolExecutable(u"ffmpeg.exe"_q).isEmpty();
}

void DownloadVideoTools(
		not_null<Window::SessionController*> controller,
		Fn<void()> onUpdated) {
	const auto weak = base::make_weak(controller);
	EnsureVideoTools(controller, true, true, [=](
			QString yt,
			QString ff,
			QString error) {
		if (const auto strong = weak.get()) {
			if (yt.isEmpty() || ff.isEmpty()) {
				strong->showToast(u"MediaTok инструменты: %1"_q.arg(error));
			} else {
				strong->showToast(u"MediaTok: yt-dlp.exe и ffmpeg.exe готовы!"_q);
			}
		}
		if (onUpdated) {
			onUpdated();
		}
	});
}

bool CanResendAsCircle(HistoryItem *item) {
	if (!item || !item->isRegular() || item->isLocal()) {
		return false;
	}
	const auto media = item->media();
	if (!media) {
		return false;
	}
	const auto doc = media->document();
	if (!doc || doc->isVideoMessage()) {
		return false;
	}
	return doc->isVideoFile() || doc->isAnimation() || doc->isGifv();
}

void ResendItemAsCircle(
		not_null<Window::SessionController*> controller,
		not_null<HistoryItem*> item) {
	if (!CanResendAsCircle(item)) {
		return;
	}
	const auto doc = item->media()->document();
	if (!doc) {
		return;
	}
	const auto history = item->history();
	const auto action = Api::SendAction(history);

	const auto path = doc->filepath(true);
	if (!path.isEmpty() && QFileInfo::exists(path)) {
		ConvertVideoFileToCircleAndSend(controller, history, action, path, QString());
		return;
	}
	if (const auto view = doc->createMediaView()) {
		const auto bytes = view->bytes();
		if (!bytes.isEmpty()) {
			const auto tempDir = QFileInfo(
				cWorkingDir()
				+ u"tdata/MediaTok_Videos/src_%1_%2"_q
					.arg(QDateTime::currentMSecsSinceEpoch())
					.arg(base::RandomValue<uint32>())
			).absoluteFilePath();
			QDir().mkpath(tempDir);
			const auto tmpPath = tempDir + u"/source.mp4"_q;
			QFile f(tmpPath);
			if (f.open(QIODevice::WriteOnly)) {
				f.write(bytes);
				f.close();
				ConvertVideoFileToCircleAndSend(controller, history, action, tmpPath, tempDir);
				return;
			}
			QDir(tempDir).removeRecursively();
		}
	}
	controller->showToast(u"MediaTok: сначала открой/скачай видео в чате, чтобы сделать кружок"_q);
}

void DeleteMyMessagesInPeer(
		not_null<PeerData*> peer,
		Window::SessionController *controller,
		Fn<void(int deletedCount)> onDone) {
	const auto session = &peer->session();
	const auto weakController = controller ? base::make_weak(controller) : nullptr;

	if (const auto channel = peer->asChannel()) {
		if (channel->isMegagroup() && channel->canDeleteMessages()) {
			session->api().deleteAllFromParticipant(channel, session->user());
			if (const auto strong = weakController.get()) {
				if (!onDone) {
					strong->showToast(u"MediaTok: все ваши сообщения удалены у всех"_q);
				}
			}
			if (onDone) {
				onDone(0);
			}
			return;
		}
	}

	auto collected = std::make_shared<std::vector<MsgId>>();
	auto seenIds = std::make_shared<std::unordered_set<int64>>();
	const auto removeNext = std::make_shared<Fn<void(int)>>();
	const auto requestNext = std::make_shared<Fn<void(MsgId)>>();

	*removeNext = [=](int index) {
		if (index >= int(collected->size())) {
			if (const auto strong = weakController.get()) {
				if (!onDone) {
					strong->showToast(collected->empty()
						? u"MediaTok: ваши сообщения в этом чате не найдены"_q
						: u"MediaTok: удалено %1 ваших сообщений у всех"_q
							.arg(collected->size()));
				}
			}
			if (onDone) {
				onDone(int(collected->size()));
			}
			*removeNext = nullptr;
			*requestNext = nullptr;
			return;
		}

		QVector<MTPint> ids;
		ids.reserve(std::min<int>(100, collected->size() - index));
		for (auto i = 0; i < 100 && (index + i) < int(collected->size()); ++i) {
			ids.push_back(MTP_int((*collected)[index + i].bare));
		}

		const auto done = [=](const MTPmessages_AffectedMessages &result) {
			session->api().applyAffectedMessages(peer, result);
			if (peer->isChannel()) {
				session->data().processMessagesDeleted(peer->id, ids);
			} else {
				session->data().processNonChannelMessagesDeleted(ids);
			}
			const auto deleted = index + ids.size();
			const auto delay = crl::time(450 + base::RandomValue<int>() % 350);
			base::call_delayed(delay, session, [=] { (*removeNext)(deleted); });
		};
		const auto fail = [=](const MTP::Error &error) {
			const auto type = error.type();
			if (type.startsWith(u"FLOOD_WAIT_"_q)
				|| type.startsWith(u"FLOOD_PREMIUM_WAIT_"_q)) {
				const auto underscore = type.lastIndexOf('_');
				const auto seconds = (underscore >= 0)
					? type.mid(underscore + 1).toInt()
					: 1;
				base::call_delayed(
					crl::time(std::max(seconds, 1) * 1000),
					session,
					[=] { (*removeNext)(index); });
				return;
			}
			if (type == u"MESSAGE_DELETE_FORBIDDEN"_q
				|| type == u"MSG_ID_INVALID"_q
				|| type == u"MESSAGE_ID_INVALID"_q) {
				base::call_delayed(
					crl::time(450),
					session,
					[=] { (*removeNext)(index + ids.size()); });
				return;
			}
			if (onDone) {
				onDone(index);
			}
			*removeNext = nullptr;
			*requestNext = nullptr;
		};

		if (const auto channel = peer->asChannel()) {
			session->api().request(MTPchannels_DeleteMessages(
				channel->inputChannel(),
				MTP_vector<MTPint>(ids)
			)).done(done).fail(fail).handleFloodErrors().send();
		} else {
			using Flag = MTPmessages_DeleteMessages::Flag;
			session->api().request(MTPmessages_DeleteMessages(
				MTP_flags(Flag::f_revoke),
				MTP_vector<MTPint>(ids)
			)).done(done).fail(fail).handleFloodErrors().send();
		}
	};

	*requestNext = [=](MsgId from) {
		const auto mtpOffsetId = int(std::clamp(
			from.bare,
			int64(0),
			int64(0x3FFFFFFF)));
		const auto onPage = [=](const Api::HistoryRequestResult &result) {
			auto parsed = Api::ParseHistoryResult(
				peer,
				from,
				Data::LoadDirection::Before,
				result);
			const auto rawCount = [&] {
				switch (result.type()) {
				case mtpc_messages_messages:
					return int(result.c_messages_messages().vmessages().v.size());
				case mtpc_messages_messagesSlice:
					return int(result.c_messages_messagesSlice().vmessages().v.size());
				case mtpc_messages_channelMessages:
					return int(result.c_messages_channelMessages().vmessages().v.size());
				default:
					return 0;
				}
			}();
			MsgId minId;
			for (const auto &id : parsed.messageIds) {
				if (!minId || id < minId) {
					minId = id;
				}
				if (const auto item = peer->owner().message(peer->id, id)) {
					if (item->out() && !item->isService()) {
						if (seenIds->insert(id.bare).second) {
							collected->push_back(id);
						}
					}
				} else if (!peer->isUser()) {
					if (seenIds->insert(id.bare).second) {
						collected->push_back(id);
					}
				}
			}
			if (rawCount >= 100 && minId > MsgId(1) && (!from || minId < from)) {
				base::call_delayed(crl::time(250), session, [=] {
					(*requestNext)(minId);
				});
			} else {
				(*removeNext)(0);
			}
		};
		const auto onFail = [=](const MTP::Error &) {
			if (!collected->empty()) {
				(*removeNext)(0);
				return;
			}
			if (onDone) {
				onDone(0);
			}
			*removeNext = nullptr;
			*requestNext = nullptr;
		};

		if (peer->isUser()) {
			session->api().request(MTPmessages_GetHistory(
				peer->input(),
				MTP_int(mtpOffsetId),
				MTP_int(0),
				MTP_int(0),
				MTP_int(100),
				MTP_int(0),
				MTP_int(0),
				MTP_long(0)
			)).done(onPage).fail(onFail).handleFloodErrors().send();
		} else {
			using Flag = MTPmessages_Search::Flag;
			session->api().request(MTPmessages_Search(
				MTP_flags(Flag::f_from_id),
				peer->input(),
				MTP_string(),
				MTP_inputPeerSelf(),
				MTPInputPeer(),
				MTPVector<MTPReaction>(),
				MTP_int(0),
				MTP_inputMessagesFilterEmpty(),
				MTP_int(0),
				MTP_int(0),
				MTP_int(mtpOffsetId),
				MTP_int(0),
				MTP_int(100),
				MTP_int(0),
				MTP_int(0),
				MTP_long(0)
			)).done(onPage).fail(onFail).handleFloodErrors().send();
		}
	};

	(*requestNext)(MsgId(0));
}

void DeleteMyMessagesEverywhere(not_null<Window::SessionController*> controller) {
	if (controller->showFrozenError()) {
		return;
	}
	const auto weak = base::make_weak(controller);
	controller->show(Ui::MakeConfirmBox({
		.text = u"Вы уверены, что хотите удалить ВСЕ ваши отправленные сообщения во ВСЕХ чатах и ЛС? (Избранное не трогается). Это действие необратимо!"_q,
		.confirmed = [=](Fn<void()> &&close) {
			close();
			const auto strong = weak.get();
			if (!strong) {
				return;
			}
			const auto session = &strong->session();
			auto peers = std::make_shared<std::vector<not_null<PeerData*>>>();
			auto seen = std::unordered_set<uint64>();

			const auto collectFrom = [&](not_null<Dialogs::IndexedList*> list) {
				for (const auto &row : list->all()) {
					if (const auto history = row->history()) {
						const auto peer = history->peer;
						if (peer->isSelf() || peer->isNotificationsUser()) {
							continue;
						}
						if (peer->isChannel() && !peer->isMegagroup()) {
							continue;
						}
						if (seen.insert(peer->id.value).second) {
							peers->push_back(peer);
						}
					}
				}
			};
			collectFrom(session->data().chatsList()->indexed());
			if (const auto archive = session->data().folderLoaded(Data::Folder::kId)) {
				collectFrom(archive->chatsList()->indexed());
			}

			if (peers->empty()) {
				strong->showToast(u"MediaTok: нет доступных чатов для очистки"_q);
				return;
			}
			strong->showToast(
				u"MediaTok: запускаю удаление ваших сообщений в %1 чатах..."_q.arg(peers->size()));

			auto totalDeleted = std::make_shared<int>(0);
			auto stepFn = std::make_shared<Fn<void(int)>>();
			*stepFn = [=](int idx) {
				if (idx >= int(peers->size())) {
					if (const auto s = weak.get()) {
						s->showToast(
							u"MediaTok: готово! Удалено %1 сообщений по всем чатам"_q.arg(*totalDeleted));
					}
					*stepFn = nullptr;
					return;
				}
				const auto peer = (*peers)[idx];
				DeleteMyMessagesInPeer(peer, nullptr, [=](int deleted) {
					*totalDeleted += deleted;
					if ((idx + 1) % 5 == 0) {
						if (const auto s = weak.get()) {
							s->showToast(
								u"MediaTok: очищено чатов %1/%2 (удалено %3 сообщ.)"_q
									.arg(idx + 1)
									.arg(peers->size())
									.arg(*totalDeleted));
						}
					}
					base::call_delayed(crl::time(700), session, [=] {
						(*stepFn)(idx + 1);
					});
				});
			};
			(*stepFn)(0);
		},
		.confirmText = u"Удалить везде"_q,
		.cancelText = tr::lng_cancel(),
		.confirmStyle = &st::attentionBoxButton,
	}));
}

void WipeIncomingPeerHistory(not_null<PeerData*> peer) {
	if (!peer->isUser() || peer->isSelf()) {
		return;
	}
	const auto history = peer->owner().history(peer);
	peer->owner().histories().deleteAllMessages(history, 0, true, true);
}

void CheckAutoDeleteIncoming(not_null<HistoryItem*> item) {
	if (item->out() || !item->isRegular() || item->id <= 0) {
		return;
	}
	const auto peer = item->history()->peer;
	if (!peer->isUser() || peer->isSelf()) {
		return;
	}
	const auto dialogId = getDialogIdFromPeer(peer);
	if (!AyuSettings::getInstance().isAutoDeleteIncomingPeer(dialogId)) {
		return;
	}

	const auto session = &peer->session();
	const auto fullId = item->fullId();
	const auto rawId = item->id.bare;

	crl::on_main(session, [=] {
		using Flag = MTPmessages_DeleteMessages::Flag;
		session->api().request(MTPmessages_DeleteMessages(
			MTP_flags(Flag::f_revoke),
			MTP_vector<MTPint>(1, MTP_int(rawId))
		)).done([=](const MTPmessages_AffectedMessages &result) {
			session->api().applyAffectedMessages(peer, result);
		}).handleFloodErrors().send();

		if (const auto msg = session->data().message(fullId)) {
			msg->destroy();
		}
	});
}

bool HandleOutgoingText(
		not_null<Window::SessionController*> controller,
		const Api::MessageToSend &message) {
	const auto rawText = message.textWithTags.text;
	const auto trimmed = rawText.trimmed();
	if (trimmed.isEmpty()) {
		return false;
	}

	const auto history = message.action.history;
	const auto peer = history->peer;
	const auto session = &history->session();
	const auto weak = base::make_weak(controller);

	if (trimmed.compare(u".mt"_q, Qt::CaseInsensitive) == 0
		|| trimmed.compare(u".help"_q, Qt::CaseInsensitive) == 0) {
		controller->show(Ui::MakeInformBox(
			u"Команды MediaTok:\n\n"
			"• .ai <вопрос> — спросить Gemini и отправить ответ в чат (понимает текст и фото в реплае!)\n"
			"• .ai! <вопрос> — спросить Gemini скрытно (показать ответ только вам)\n"
			"• .circle <ссылка> — скачать видео и отправить как КРУЖОК (или реплаем .circle на видео/GIF в чате)\n"
			"• .dl <ссылка> — скачать видео и отправить вместо ссылки\n"
			"• .raw <текст> — отправить текст/ссылку без авто-скачивания видео\n"
			"• .delme — удалить все ваши сообщения в этом чате\n"
			"• .delmeall — удалить все ваши сообщения во всех чатах\n"
			"• .wipe — включить мгновенное удаление входящих от собеседника в ЛС (у обоих)\n"
			"• .wipe all — снести всю историю ЛС у обоих + включить .wipe\n"
			"• .unwipe — выключить авто-удаление входящих в ЛС\n"
			"• .vm <off|deep|helium|robot|radio|echo|anon|bass> — переключить Voicemod"_q));
		return true;
	}

	if (trimmed.startsWith(u".raw "_q, Qt::CaseInsensitive)) {
		auto plain = message;
		plain.textWithTags.text = trimmed.mid(5);
		plain.textWithTags.tags.clear();
		plain.action.clearDraft = false;
		plain.action.generateLocal = true;
		session->api().sendMessage(std::move(plain));
		return true;
	}

	if (trimmed.compare(u".delme"_q, Qt::CaseInsensitive) == 0) {
		controller->showToast(u"MediaTok: удаляю все ваши сообщения в этом чате..."_q);
		DeleteMyMessagesInPeer(peer, controller);
		return true;
	}

	if (trimmed.compare(u".delmeall"_q, Qt::CaseInsensitive) == 0) {
		DeleteMyMessagesEverywhere(controller);
		return true;
	}

	if (trimmed.compare(u".wipe"_q, Qt::CaseInsensitive) == 0
		|| trimmed.compare(u".wipe all"_q, Qt::CaseInsensitive) == 0) {
		if (!peer->isUser() || peer->isSelf()) {
			controller->showToast(u"MediaTok: .wipe работает только в личных чатах (ЛС)"_q);
			return true;
		}
		AyuSettings::getInstance().addAutoDeleteIncomingPeer(getDialogIdFromPeer(peer));
		if (trimmed.endsWith(u"all"_q, Qt::CaseInsensitive)) {
			WipeIncomingPeerHistory(peer);
			controller->showToast(u"MediaTok: ЛС очищено для обоих + авто-удаление входящих ВКЛ"_q);
		} else {
			controller->showToast(u"MediaTok: авто-удаление входящих от собеседника ВКЛ"_q);
		}
		return true;
	}

	if (trimmed.compare(u".unwipe"_q, Qt::CaseInsensitive) == 0) {
		AyuSettings::getInstance().removeAutoDeleteIncomingPeer(getDialogIdFromPeer(peer));
		controller->showToast(u"MediaTok: авто-удаление входящих ВЫКЛ"_q);
		return true;
	}

	if (trimmed.compare(u".vm"_q, Qt::CaseInsensitive) == 0
		|| trimmed.startsWith(u".vm "_q, Qt::CaseInsensitive)) {
		const auto arg = trimmed.mid(3).trimmed();
		if (arg.isEmpty()) {
			const int nextPreset = (AyuSettings::getInstance().voicemodPreset() + 1) % kVoicemodPresetCount;
			AyuSettings::getInstance().setVoicemodPreset(nextPreset);
			controller->showToast(u"Voicemod: %1"_q.arg(VoicemodPresetName(nextPreset)));
			return true;
		}
		if (const auto parsed = ParseVoicemodArg(arg)) {
			AyuSettings::getInstance().setVoicemodPreset(*parsed);
			controller->showToast(u"Voicemod: %1"_q.arg(VoicemodPresetName(*parsed)));
		} else {
			controller->showToast(
				u"Режимы Voicemod: off, deep, helium, robot, radio, echo, anon, bass (0..7)"_q);
		}
		return true;
	}

	const bool isAiPrivate = (trimmed.compare(u".ai!"_q, Qt::CaseInsensitive) == 0
		|| trimmed.startsWith(u".ai! "_q, Qt::CaseInsensitive));
	const bool isAiPublic = (!isAiPrivate
		&& (trimmed.compare(u".ai"_q, Qt::CaseInsensitive) == 0
			|| trimmed.startsWith(u".ai "_q, Qt::CaseInsensitive)));
	if (isAiPrivate || isAiPublic) {
		auto prompt = isAiPrivate
			? trimmed.mid(4).trimmed()
			: trimmed.mid(3).trimmed();
		QByteArray replyPhotoJpeg;
		if (const auto replyId = message.action.replyTo.messageId) {
			if (const auto replyItem = history->owner().message(replyId)) {
				replyPhotoJpeg = ExtractPhotoJpeg(replyItem);
				const auto replyText = replyItem->originalText().text.trimmed();
				if (!replyText.isEmpty()) {
					prompt = prompt.isEmpty()
						? replyText
						: u"Контекст сообщения:\n\"%1\"\n\nЗапрос:\n%2"_q.arg(replyText, prompt);
				}
			}
		}
		if (prompt.isEmpty() && !replyPhotoJpeg.isEmpty()) {
			prompt = u"Что изображено на этом фото? Опиши понятно и по сути."_q;
		}
		if (prompt.isEmpty()) {
			controller->showToast(u"Напиши запрос после .ai или ответь на сообщение/фото"_q);
			return true;
		}
		controller->showToast(
			u"Gemini (%1) думает..."_q.arg(AyuSettings::getInstance().geminiModel()));
		const auto savedAction = message.action;
		AskGemini(session, prompt, [=](QString answer, QString error) {
			const auto strong = weak.get();
			if (!error.isEmpty()) {
				if (strong) {
					strong->showToast(u"Gemini: %1"_q.arg(error));
				}
				return;
			}
			if (isAiPrivate) {
				if (strong) {
					strong->show(Ui::MakeInformBox(answer));
				}
				return;
			}
			const auto chunks = SplitMessageChunks(answer, 4000);
			for (int i = 0; i < int(chunks.size()); ++i) {
				auto outMsg = Api::MessageToSend(savedAction);
				if (i > 0) {
					outMsg.action.replyTo = {};
				}
				outMsg.textWithTags = { chunks[i], {} };
				outMsg.action.clearDraft = false;
				outMsg.action.generateLocal = true;
				if (i == 0) {
					session->api().sendMessage(std::move(outMsg));
				} else {
					const auto delayedMsg = std::make_shared<Api::MessageToSend>(
						std::move(outMsg));
					base::call_delayed(
						crl::time(i * 350),
						session,
						[=] {
							session->api().sendMessage(std::move(*delayedMsg));
						});
				}
			}
		}, replyPhotoJpeg);
		return true;
	}

	if (trimmed.compare(u".circle"_q, Qt::CaseInsensitive) == 0
		|| trimmed.startsWith(u".circle "_q, Qt::CaseInsensitive)) {
		const auto arg = trimmed.mid(7).trimmed();
		if (!arg.isEmpty()) {
			DownloadAndSendVideoFromUrl(
				controller,
				history,
				message.action,
				arg,
				true,
				std::nullopt);
			return true;
		}
		if (const auto replyId = message.action.replyTo.messageId) {
			if (const auto replyItem = history->owner().message(replyId)) {
				if (CanResendAsCircle(replyItem)) {
					ResendItemAsCircle(controller, replyItem);
					return true;
				}
			}
		}
		controller->showToast(u"Укажи ссылку (.circle https://...) или ответь .circle на видео/GIF"_q);
		return true;
	}

	if (trimmed.startsWith(u".dl "_q, Qt::CaseInsensitive)) {
		const auto url = trimmed.mid(4).trimmed();
		if (!url.isEmpty()) {
			DownloadAndSendVideoFromUrl(
				controller,
				history,
				message.action,
				url,
				false,
				std::nullopt);
			return true;
		}
	}

	if (AyuSettings::getInstance().autoVideoFromLinks()
		&& IsStandaloneHttpUrl(trimmed)) {
		DownloadAndSendVideoFromUrl(
			controller,
			history,
			message.action,
			trimmed,
			false,
			message);
		return true;
	}

	return false;
}

} // namespace MediaTok
