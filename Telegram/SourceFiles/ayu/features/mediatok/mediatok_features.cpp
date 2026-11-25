// This is part of MediaTok (AyuGramPlus fork).
#include "ayu/features/mediatok/mediatok_features.h"

#include "apiwrap.h"
#include "api/api_sending.h"
#include "ayu/ayu_settings.h"
#include "ayu/features/mediatok/mediatok_fonts.h"
#include "ayu/features/mediatok/mediatok_langs.h"
#include "ayu/features/mediatok/mediatok_process.h"
#include "ayu/features/mediatok/mediatok_voicemod.h"
#include "ayu/features/mediatok/mediatok_waves.h"
#include "ayu/utils/telegram_helpers.h"
#include "base/call_delayed.h"
#include "base/random.h"
#include "base/unixtime.h"
#include "core/application.h"
#include "core/file_utilities.h"
#include "data/data_changes.h"
#include "data/data_channel.h"
#include "data/data_chat.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_folder.h"
#include "data/data_histories.h"
#include "data/data_media_types.h"
#include "data/data_peer.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_search_controller.h"
#include "data/data_session.h"
#include "data/data_types.h"
#include "data/data_user.h"
#include "data/stickers/data_custom_emoji.h"
#include "dialogs/dialogs_indexed_list.h"
#include "dialogs/dialogs_main_list.h"
#include "dialogs/dialogs_row.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/view/history_view_element.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "media/media_video_frames.h"
#include "storage/localimageloader.h"
#include "storage/storage_media_prepare.h"
#include "styles/style_boxes.h"
#include "styles/style_info.h"
#include "styles/style_layers.h"
#include "styles/style_passcode_box.h"
#include "ui/boxes/confirm_box.h"
#include "ui/chat/attach/attach_prepare.h"
#include "ui/image/image.h"
#include "ui/image/image_location_factory.h"
#include "ui/layers/generic_box.h"
#include "ui/text/text_utilities.h"
#include "ui/toast/toast.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_adaptive.h"
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
#include <QtCore/QUrlQuery>
#include <QtGui/QClipboard>
#include <QtGui/QDesktopServices>
#include <QtGui/QGuiApplication>
#include <QtGui/QImageReader>
#include <QtNetwork/QHttpMultiPart>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
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

enum class MediaOutputMode {
	Video,
	Circle,
	AudioMp3,
	VoiceOgg,
	GifAnimation,
};

struct MediaDownloadOptions {
	QString url;
	QString ytdlpSection;
	std::optional<double> startSec;
	std::optional<double> endSec;
	double speed = 1.0;
};

[[nodiscard]] std::optional<double> ParseTimestampSeconds(const QString &raw) {
	const auto parts = raw.trimmed().split(':');
	if (parts.isEmpty() || parts.size() > 3) {
		return std::nullopt;
	}
	double total = 0.0;
	for (const auto &p : parts) {
		bool ok = false;
		const double val = p.toDouble(&ok);
		if (!ok || val < 0.0) {
			return std::nullopt;
		}
		total = total * 60.0 + val;
	}
	return total;
}

[[nodiscard]] MediaDownloadOptions ParseMediaDownloadOptions(const QString &raw) {
	MediaDownloadOptions opts;
	static const QRegularExpression kSpeedRe(
		uR"(^(\d+(?:[.,]\d+)?)[xх]$)"_q,
		QRegularExpression::CaseInsensitiveOption);
	static const QRegularExpression kRangeRe(
		uR"(^((?:\d{1,2}:)?\d{1,2}(?::\d{2})?)-((?:\d{1,2}:)?\d{1,2}(?::\d{2})?)$)"_q);

	const auto tokens = raw.trimmed().split(
		QRegularExpression(u"\\s+"_q),
		Qt::SkipEmptyParts);
	for (const auto &tok : tokens) {
		if (tok.startsWith(u"http://"_q, Qt::CaseInsensitive)
			|| tok.startsWith(u"https://"_q, Qt::CaseInsensitive)) {
			opts.url = tok;
			continue;
		}
		if (const auto sm = kSpeedRe.match(tok); sm.hasMatch()) {
			auto valStr = sm.captured(1);
			valStr.replace(',', '.');
			bool ok = false;
			const double s = valStr.toDouble(&ok);
			if (ok && s >= 0.5 && s <= 3.0) {
				opts.speed = s;
			}
			continue;
		}
		if (const auto rm = kRangeRe.match(tok); rm.hasMatch()) {
			const auto s1 = ParseTimestampSeconds(rm.captured(1));
			const auto s2 = ParseTimestampSeconds(rm.captured(2));
			if (s1 && s2 && *s2 > *s1) {
				opts.startSec = *s1;
				opts.endSec = *s2;
				opts.ytdlpSection = u"*%1-%2"_q.arg(rm.captured(1), rm.captured(2));
			}
		}
	}
	return opts;
}

[[nodiscard]] QString BuildAtempoFilterChain(double speed) {
	QStringList parts;
	double rem = std::clamp(speed, 0.5, 3.0);
	while (rem > 2.0 + 1e-4) {
		parts << u"atempo=2.0"_q;
		rem /= 2.0;
	}
	while (rem < 0.5 - 1e-4) {
		parts << u"atempo=0.5"_q;
		rem /= 0.5;
	}
	parts << u"atempo=%1"_q.arg(rem, 0, 'f', 3);
	return parts.join(',');
}

[[nodiscard]] VoiceWaveform ComputeVoiceWaveformAndMs(
		const QByteArray &pcmBytes,
		int &outDurationMs) {
	const int totalSamples = int(pcmBytes.size() / 2);
	outDurationMs = std::clamp(
		int((int64(totalSamples) * 1000) / 48000),
		1000,
		3600000);
	VoiceWaveform wf;
	if (totalSamples <= 0) {
		return wf;
	}
	const auto *samples = reinterpret_cast<const int16_t*>(pcmBytes.constData());
	const int bars = std::min(100, std::max(16, totalSamples / 480));
	wf.reserve(bars);
	for (int i = 0; i < bars; ++i) {
		const int start = int((int64(i) * totalSamples) / bars);
		const int end = std::max(
			start + 1,
			int((int64(i + 1) * totalSamples) / bars));
		int peak = 0;
		for (int s = start; s < end && s < totalSamples; ++s) {
			const int v = std::abs(int(samples[s]));
			if (v > peak) {
				peak = v;
			}
		}
		wf.push_back(static_cast<signed char>(
			std::clamp((peak * 31) / 24000, 1, 31)));
	}
	return wf;
}

void ConvertLocalFileAndSend(
		not_null<Window::SessionController*> controller,
		not_null<History*> history,
		const Api::SendAction &baseAction,
		const QString &sourcePath,
		const QString &cleanupDir,
		MediaOutputMode mode,
		const MediaDownloadOptions &opts,
		bool applyTimeRange = false) {
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
			const auto statusMsg = [&] {
				switch (mode) {
				case MediaOutputMode::Circle: return u"MediaTok: делаю кружок..."_q;
				case MediaOutputMode::AudioMp3: return u"MediaTok: извлекаю MP3..."_q;
				case MediaOutputMode::VoiceOgg: return u"MediaTok: готовлю голосовое..."_q;
				case MediaOutputMode::GifAnimation: return u"MediaTok: конвертирую в GIF..."_q;
				default: return u"MediaTok: обрабатываю видео..."_q;
				}
			}();
			strong->showToast(statusMsg);
		}

		CleanupOldVideoTempDirs();
		const auto workDir = QFileInfo(
			cWorkingDir()
			+ u"tdata/MediaTok_Videos/conv_%1_%2"_q
				.arg(QDateTime::currentMSecsSinceEpoch())
				.arg(base::RandomValue<uint32>())
		).absoluteFilePath();
		QDir().mkpath(workDir);

		const bool hasSpeed = (std::abs(opts.speed - 1.0) > 0.02);
		const int voicemodPreset = AyuSettings::getInstance().voicemodPreset();

		crl::async([=] {
			auto appendTrimArgs = [&](QStringList &args) {
				if (applyTimeRange && opts.startSec && opts.endSec) {
					args << u"-ss"_q << QString::number(*opts.startSec, 'f', 2)
						<< u"-to"_q << QString::number(*opts.endSec, 'f', 2);
				}
			};

			if (mode == MediaOutputMode::AudioMp3) {
				const auto outMp3 = workDir + u"/audio.mp3"_q;
				QStringList args;
				args << u"-y"_q;
				appendTrimArgs(args);
				args << u"-i"_q << sourcePath << u"-vn"_q;
				if (hasSpeed) {
					args << u"-af"_q << BuildAtempoFilterChain(opts.speed);
				}
				args << u"-c:a"_q << u"libmp3lame"_q << u"-q:a"_q << u"2"_q << outMp3;

				int code = -1;
				const bool ok = RunHiddenProcess(ffmpegExe, args, workDir, 180000, &code);
				const bool valid = ok && QFileInfo(outMp3).size() > 0;
				if (!cleanupDir.isEmpty()) {
					QDir(cleanupDir).removeRecursively();
				}
				crl::on_main(session, [=] {
					if (!valid) {
						QDir(workDir).removeRecursively();
						if (const auto s = weak.get()) {
							s->showToast(u"MediaTok: не удалось извлечь MP3"_q);
						}
						return;
					}
					const auto premium = session->user()->isPremium();
					auto list = Storage::PrepareMediaList(
						QStringList{ outMp3 },
						st::sendMediaPreviewSize,
						premium);
					if (list.error != Ui::PreparedList::Error::None || list.files.empty()) {
						QDir(workDir).removeRecursively();
						if (const auto s = weak.get()) {
							s->showToast(u"MediaTok: не удалось подготовить MP3"_q);
						}
						return;
					}
					auto sendAction = baseAction;
					sendAction.clearDraft = false;
					sendAction.generateLocal = true;
					session->api().sendFiles(
						std::move(list),
						SendMediaType::File,
						nullptr,
						sendAction);
				});
				return;
			}

			if (mode == MediaOutputMode::VoiceOgg) {
				const auto pcmPath = workDir + u"/clip.pcm"_q;
				const auto oggPath = workDir + u"/voice.ogg"_q;
				QStringList pcmArgs;
				pcmArgs << u"-y"_q;
				appendTrimArgs(pcmArgs);
				pcmArgs << u"-i"_q << sourcePath << u"-t"_q << u"300"_q << u"-vn"_q;
				if (hasSpeed) {
					pcmArgs << u"-af"_q << BuildAtempoFilterChain(opts.speed);
				}
				pcmArgs << u"-ac"_q << u"1"_q
					<< u"-ar"_q << u"48000"_q
					<< u"-f"_q << u"s16le"_q
					<< pcmPath;

				int code = -1;
				const bool pcmOk = RunHiddenProcess(ffmpegExe, pcmArgs, workDir, 180000, &code);
				QByteArray pcmBytes;
				if (pcmOk) {
					QFile f(pcmPath);
					if (f.open(QIODevice::ReadOnly)) {
						pcmBytes = f.readAll();
						f.close();
					}
				}
				int durationMs = 0;
				VoiceWaveform wf;
				QByteArray oggBytes;
				if (pcmBytes.size() >= 2) {
					const int totalSamples = int(pcmBytes.size() / 2);
					if (voicemodPreset != 0) {
						Voicemod::State vmState;
						Voicemod::ProcessSamples(
							reinterpret_cast<short*>(pcmBytes.data()),
							totalSamples,
							48000,
							vmState);
						QFile fw(pcmPath);
						if (fw.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
							fw.write(pcmBytes);
							fw.close();
						}
					}
					wf = ComputeVoiceWaveformAndMs(pcmBytes, durationMs);
					QStringList oggArgs;
					oggArgs << u"-y"_q
						<< u"-f"_q << u"s16le"_q
						<< u"-ar"_q << u"48000"_q
						<< u"-ac"_q << u"1"_q
						<< u"-i"_q << pcmPath
						<< u"-c:a"_q << u"libopus"_q
						<< u"-b:a"_q << u"64k"_q
						<< u"-vbr"_q << u"on"_q
						<< u"-application"_q << u"voip"_q
						<< oggPath;
					if (RunHiddenProcess(ffmpegExe, oggArgs, workDir, 120000, &code)) {
						QFile fo(oggPath);
						if (fo.open(QIODevice::ReadOnly) && fo.size() > 0 && fo.size() <= 50 * 1024 * 1024) {
							oggBytes = fo.readAll();
						}
					}
				}
				QDir(workDir).removeRecursively();
				if (!cleanupDir.isEmpty()) {
					QDir(cleanupDir).removeRecursively();
				}
				crl::on_main(session, [=] {
					if (oggBytes.isEmpty()) {
						if (const auto s = weak.get()) {
							s->showToast(u"MediaTok: не удалось сконвертировать в голосовое"_q);
						}
						return;
					}
					auto sendAction = baseAction;
					sendAction.clearDraft = false;
					sendAction.generateLocal = true;
					session->api().sendVoiceMessage(
						oggBytes,
						wf,
						durationMs,
						false,
						sendAction);
				});
				return;
			}

			if (mode == MediaOutputMode::GifAnimation) {
				const auto outGif = workDir + u"/anim.mp4"_q;
				const auto vf = hasSpeed
					? u"setpts=PTS/%1,scale='min(640,iw)':-2:flags=lanczos"_q.arg(opts.speed, 0, 'f', 3)
					: u"scale='min(640,iw)':-2:flags=lanczos"_q;
				QStringList args;
				args << u"-y"_q;
				appendTrimArgs(args);
				args << u"-i"_q << sourcePath
					<< u"-t"_q << u"30"_q
					<< u"-an"_q
					<< u"-vf"_q << vf
					<< u"-c:v"_q << u"libx264"_q
					<< u"-preset"_q << u"veryfast"_q
					<< u"-crf"_q << u"24"_q
					<< u"-pix_fmt"_q << u"yuv420p"_q
					<< u"-movflags"_q << u"+faststart"_q
					<< outGif;
				int code = -1;
				const bool ok = RunHiddenProcess(ffmpegExe, args, workDir, 180000, &code);
				const bool valid = ok && QFileInfo(outGif).size() > 0 && QFileInfo(outGif).size() <= 50 * 1024 * 1024;
				if (!cleanupDir.isEmpty()) {
					QDir(cleanupDir).removeRecursively();
				}
				crl::on_main(session, [=] {
					if (!valid) {
						QDir(workDir).removeRecursively();
						if (const auto s = weak.get()) {
							s->showToast(u"MediaTok: не удалось сделать GIF"_q);
						}
						return;
					}
					const auto premium = session->user()->isPremium();
					auto list = Storage::PrepareMediaList(
						QStringList{ outGif },
						st::sendMediaPreviewSize,
						premium);
					if (list.error != Ui::PreparedList::Error::None || list.files.empty()) {
						QDir(workDir).removeRecursively();
						if (const auto s = weak.get()) {
							s->showToast(u"MediaTok: не удалось подготовить GIF"_q);
						}
						return;
					}
					if (list.files.front().information) {
						if (auto video = std::get_if<Ui::PreparedFileInformation::Video>(
								&list.files.front().information->media)) {
							video->modifications.gif = true;
						}
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
				return;
			}

			if (mode == MediaOutputMode::Video) {
				const auto outVideo = workDir + u"/video_proc.mp4"_q;
				const auto vf = u"setpts=PTS/%1"_q.arg(opts.speed, 0, 'f', 3);
				QStringList args;
				args << u"-y"_q;
				appendTrimArgs(args);
				args << u"-i"_q << sourcePath
					<< u"-vf"_q << vf
					<< u"-af"_q << BuildAtempoFilterChain(opts.speed)
					<< u"-c:v"_q << u"libx264"_q
					<< u"-preset"_q << u"veryfast"_q
					<< u"-crf"_q << u"23"_q
					<< u"-pix_fmt"_q << u"yuv420p"_q
					<< u"-c:a"_q << u"aac"_q
					<< u"-b:a"_q << u"128k"_q
					<< u"-movflags"_q << u"+faststart"_q
					<< outVideo;
				int code = -1;
				const bool ok = RunHiddenProcess(ffmpegExe, args, workDir, 240000, &code);
				const bool valid = ok && QFileInfo(outVideo).size() > 0;
				if (!cleanupDir.isEmpty()) {
					QDir(cleanupDir).removeRecursively();
				}
				crl::on_main(session, [=] {
					if (!valid) {
						QDir(workDir).removeRecursively();
						if (const auto s = weak.get()) {
							s->showToast(u"MediaTok: ошибка обработки видео"_q);
						}
						return;
					}
					const auto premium = session->user()->isPremium();
					auto list = Storage::PrepareMediaList(
						QStringList{ outVideo },
						st::sendMediaPreviewSize,
						premium);
					if (list.error != Ui::PreparedList::Error::None || list.files.empty()) {
						QDir(workDir).removeRecursively();
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
				return;
			}

			// MediaOutputMode::Circle
			const auto outPath = workDir + u"/circle.mp4"_q;
			const auto baseVf = u"crop='min(iw,ih)':'min(iw,ih)',scale=384:384:flags=lanczos,setsar=1"_q;
			const auto vf = hasSpeed
				? u"setpts=PTS/%1,%2"_q.arg(opts.speed, 0, 'f', 3).arg(baseVf)
				: baseVf;
			QStringList args;
			args << u"-y"_q;
			appendTrimArgs(args);
			args << u"-i"_q << sourcePath
				<< u"-t"_q << u"60"_q
				<< u"-vf"_q << vf;
			if (hasSpeed) {
				args << u"-af"_q << BuildAtempoFilterChain(opts.speed);
			}
			args << u"-c:v"_q << u"libx264"_q
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
			const bool ok = RunHiddenProcess(ffmpegExe, args, workDir, 180000, &code);
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
			QDir(workDir).removeRecursively();
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
		const MediaDownloadOptions &opts,
		MediaOutputMode mode,
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

	const bool hasSection = !opts.ytdlpSection.isEmpty();
	const bool hasSpeed = (std::abs(opts.speed - 1.0) > 0.02);

	crl::async([=] {
		QStringList args;
		args << u"--no-playlist"_q
			<< u"--no-warnings"_q
			<< u"--socket-timeout"_q << u"15"_q
			<< u"--max-filesize"_q << u"1950M"_q;

		if (!ffmpegExe.isEmpty()) {
			args << u"--ffmpeg-location"_q << ffmpegExe;
			if (hasSection) {
				args << u"--download-sections"_q << opts.ytdlpSection
					<< u"--force-keyframes-at-cuts"_q;
			}
		}

		if (mode == MediaOutputMode::AudioMp3 && !hasSpeed) {
			args << u"-x"_q << u"--audio-format"_q << u"mp3"_q << u"--audio-quality"_q << u"0"_q
				<< u"-o"_q << u"audio.%(ext)s"_q << opts.url;
		} else if (mode == MediaOutputMode::AudioMp3 || mode == MediaOutputMode::VoiceOgg) {
			args << u"-f"_q << u"ba/b"_q << u"-o"_q << u"media.%(ext)s"_q << opts.url;
		} else {
			if (!ffmpegExe.isEmpty()) {
				args << u"-f"_q << u"bv*[vcodec^=avc1][filesize<1900M]+ba[ext=m4a]/b[ext=mp4]/b"_q
					<< u"--merge-output-format"_q << u"mp4"_q;
			} else {
				args << u"-f"_q << u"best[ext=mp4][vcodec!=none][acodec!=none]/best[ext=mp4]/best"_q;
			}
			args << u"-o"_q << u"video.%(ext)s"_q << opts.url;
		}

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
				showErrorOrFallback(u"не удалось скачать медиа по ссылке"_q);
				return;
			}
			const bool needFfmpegStep = (mode == MediaOutputMode::Circle)
				|| (mode == MediaOutputMode::VoiceOgg)
				|| (mode == MediaOutputMode::GifAnimation)
				|| (mode == MediaOutputMode::AudioMp3 && hasSpeed)
				|| (mode == MediaOutputMode::Video && hasSpeed);

			if (needFfmpegStep) {
				if (const auto s = weak.get()) {
					ConvertLocalFileAndSend(
						s,
						history,
						baseAction,
						downloadedPath,
						jobDir,
						mode,
						opts,
						false);
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
				(mode == MediaOutputMode::AudioMp3)
					? SendMediaType::File
					: SendMediaType::Photo,
				nullptr,
				sendAction);
		});
	});
}

void DownloadMediaFromUrl(
		not_null<Window::SessionController*> controller,
		not_null<History*> history,
		const Api::SendAction &baseAction,
		const MediaDownloadOptions &opts,
		MediaOutputMode mode,
		std::optional<Api::MessageToSend> fallbackMessage) {
	const auto weak = base::make_weak(controller);
	const auto session = &history->session();
	const auto fallbackHolder = std::make_shared<std::optional<Api::MessageToSend>>(
		std::move(fallbackMessage));
	const bool needFfmpeg = (mode != MediaOutputMode::Video)
		|| !opts.ytdlpSection.isEmpty()
		|| (std::abs(opts.speed - 1.0) > 0.02);

	EnsureVideoTools(controller, true, needFfmpeg, [=](
			QString ytDlpExe,
			QString ffmpegExe,
			QString error) {
		const auto strong = weak.get();
		if (ytDlpExe.isEmpty() || (needFfmpeg && ffmpegExe.isEmpty())) {
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
			const auto toast = [&] {
				switch (mode) {
				case MediaOutputMode::Circle: return u"MediaTok: скачиваю видео для кружка..."_q;
				case MediaOutputMode::AudioMp3: return u"MediaTok: скачиваю аудио (MP3)..."_q;
				case MediaOutputMode::VoiceOgg: return u"MediaTok: скачиваю для голосового..."_q;
				case MediaOutputMode::GifAnimation: return u"MediaTok: скачиваю видео для GIF..."_q;
				default:
					return fallbackHolder->has_value()
						? u"MediaTok: загружаю видео по ссылке..."_q
						: u"MediaTok: скачиваю видео..."_q;
				}
			}();
			strong->showToast(toast);
			auto nextFallback = std::move(*fallbackHolder);
			fallbackHolder->reset();
			RunYtDlpDownload(
				strong,
				history,
				baseAction,
				ytDlpExe,
				ffmpegExe,
				opts,
				mode,
				std::move(nextFallback));
		}
	});
}

void WithLocalDocumentMediaPath(
		not_null<Window::SessionController*> controller,
		not_null<HistoryItem*> item,
		Fn<void(QString path, QString cleanupDir)> ready) {
	const auto media = item->media();
	const auto doc = media ? media->document() : nullptr;
	if (!doc) {
		controller->showToast(u"MediaTok: у сообщения нет медиафайла"_q);
		return;
	}
	const auto path = doc->filepath(true);
	if (!path.isEmpty() && QFileInfo::exists(path)) {
		ready(path, QString());
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
			const auto ext = doc->isVoiceMessage()
				? u".ogg"_q
				: (doc->isAudioFile() || doc->isSong())
					? u".mp3"_q
					: u".mp4"_q;
			const auto tmpPath = tempDir + u"/source"_q + ext;
			QFile f(tmpPath);
			if (f.open(QIODevice::WriteOnly)) {
				f.write(bytes);
				f.close();
				ready(tmpPath, tempDir);
				return;
			}
			QDir(tempDir).removeRecursively();
		}
	}
	controller->showToast(u"MediaTok: сначала загрузи/прослушай медиа в чате"_q);
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

struct GeminiMediaBlob {
	QByteArray bytes;
	QString mimeType;
};

[[nodiscard]] GeminiMediaBlob ExtractItemMediaForGemini(
		HistoryItem *item,
		qint64 maxBytes = 16 * 1024 * 1024) {
	if (!item) {
		return {};
	}
	if (auto photoBytes = ExtractPhotoJpeg(item); !photoBytes.isEmpty()) {
		if (photoBytes.size() <= maxBytes) {
			return { std::move(photoBytes), u"image/jpeg"_q };
		}
	}
	const auto media = item->media();
	const auto doc = media ? media->document() : nullptr;
	if (!doc || doc->size <= 0 || doc->size > maxBytes) {
		return {};
	}
	QString mime;
	if (doc->isVoiceMessage()) {
		mime = u"audio/ogg"_q;
	} else if (doc->isVideoMessage() || doc->isVideoFile()) {
		mime = u"video/mp4"_q;
	} else if (doc->isAudioFile() || doc->isSong()) {
		const auto docMime = doc->mimeString().trimmed();
		mime = docMime.isEmpty() ? u"audio/mpeg"_q : docMime;
	} else {
		return {};
	}
	QByteArray bytes;
	const auto path = doc->filepath(true);
	if (!path.isEmpty() && QFileInfo::exists(path)) {
		QFile f(path);
		if (f.open(QIODevice::ReadOnly)) {
			bytes = f.readAll();
		}
	} else if (const auto view = doc->createMediaView()) {
		bytes = view->bytes();
	}
	if (bytes.isEmpty() || bytes.size() > maxBytes) {
		return {};
	}
	return { std::move(bytes), mime };
}

[[nodiscard]] QString CollectReplyChainText(
		not_null<History*> history,
		FullMsgId replyId) {
	QStringList chain;
	auto curId = replyId;
	for (int depth = 0; depth < 5 && curId; ++depth) {
		const auto cur = history->owner().message(curId);
		if (!cur) {
			break;
		}
		auto txt = cur->originalText().text.trimmed();
		if (txt.isEmpty()) {
			if (const auto m = cur->media()) {
				if (m->photo()) txt = u"[Фото]"_q;
				else if (const auto d = m->document()) {
					if (d->isVoiceMessage()) txt = u"[Голосовое]"_q;
					else if (d->isVideoMessage()) txt = u"[Кружок]"_q;
					else if (d->isVideoFile()) txt = u"[Видео]"_q;
				}
			}
		}
		if (!txt.isEmpty()) {
			if (txt.size() > 800) {
				txt = txt.left(800) + u"..."_q;
			}
			const auto sender = cur->from()->name().trimmed();
			chain.push_back(u"[%1]: %2"_q.arg(
				sender.isEmpty() ? u"User"_q : sender,
				txt));
		}
		curId = cur->replyToFullId();
	}
	std::reverse(chain.begin(), chain.end());
	return chain.join(u"\n"_q);
}

[[nodiscard]] QString CollectRecentChatText(
		not_null<History*> history,
		int limit) {
	QStringList lines;
	lines.reserve(limit);
	for (const auto &blockPtr : ranges::views::reverse(history->blocks)) {
		for (const auto &viewPtr : ranges::views::reverse(blockPtr->messages)) {
			const auto item = viewPtr->data();
			if (!item->isRegular() || item->isService() || item->isLocal()) {
				continue;
			}
			auto body = item->originalText().text.trimmed();
			if (body.isEmpty()) {
				if (const auto m = item->media()) {
					if (m->photo()) body = u"[Фото]"_q;
					else if (const auto d = m->document()) {
						if (d->isVoiceMessage()) body = u"[Голосовое]"_q;
						else if (d->isVideoMessage()) body = u"[Кружок]"_q;
					}
				}
			}
			if (body.isEmpty()) {
				continue;
			}
			if (body.size() > 450) {
				body = body.left(450) + u"..."_q;
			}
			const auto sender = item->from()->name().trimmed();
			lines.push_back(u"%1: %2"_q.arg(
				sender.isEmpty() ? u"User"_q : sender,
				body));
			if (int(lines.size()) >= limit) {
				break;
			}
		}
		if (int(lines.size()) >= limit) {
			break;
		}
	}
	std::reverse(lines.begin(), lines.end());
	return lines.join(u"\n"_q);
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

void SendGeminiTextReply(
		not_null<Window::SessionController*> controller,
		not_null<History*> history,
		const Api::SendAction &savedAction,
		const QString &prompt,
		bool isPrivate,
		const QByteArray &mediaBytes = {},
		const QString &mimeType = u"image/jpeg"_q) {
	const auto weak = base::make_weak(controller);
	const auto session = &history->session();
	controller->showToast(
		u"Gemini (%1) думает..."_q.arg(AyuSettings::getInstance().geminiModel()));
	AskGemini(session, prompt, [=](QString answer, QString error) {
		const auto strong = weak.get();
		if (!error.isEmpty()) {
			if (strong) {
				strong->showToast(u"Gemini: %1"_q.arg(error));
			}
			return;
		}
		if (isPrivate) {
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
	}, mediaBytes, mimeType);
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
		const QByteArray &mediaBytes,
		const QString &mimeType) {
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
		model = u"gemini-4-hydrogen"_q;
	}

	const auto endpoint = u"https://generativelanguage.googleapis.com/v1beta/models/%1:generateContent?key=%2"_q
		.arg(QString::fromLatin1(QUrl::toPercentEncoding(model, "-._")),
			QString::fromLatin1(QUrl::toPercentEncoding(apiKey)));

	QJsonArray parts;
	if (!mediaBytes.isEmpty()) {
		QJsonObject inlineData;
		inlineData.insert(
			u"mime_type"_q,
			mimeType.trimmed().isEmpty() ? u"image/jpeg"_q : mimeType);
		inlineData.insert(u"data"_q, QString::fromLatin1(mediaBytes.toBase64()));
		QJsonObject mediaPart;
		mediaPart.insert(u"inline_data"_q, inlineData);
		parts.append(mediaPart);
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
		model = u"gemini-4-hydrogen"_q;
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

void FetchGeminiModels(
		not_null<Window::SessionController*> controller,
		Fn<void(std::vector<QString> models, QString error)> callback) {
	const auto apiKey = AyuSettings::getInstance().geminiApiKey().trimmed();
	if (apiKey.isEmpty()) {
		callback({}, u"Сначала введите Gemini API Key!"_q);
		return;
	}
	const auto weak = base::make_weak(controller);
	const auto endpoint = u"https://generativelanguage.googleapis.com/v1beta/models?pageSize=200&key=%1"_q
		.arg(QString::fromLatin1(QUrl::toPercentEncoding(apiKey)));

	QNetworkRequest req((QUrl(endpoint)));
	QPointer<QNetworkReply> reply = NetworkManager().get(req);
	auto timer = new QTimer(reply);
	timer->setSingleShot(true);
	timer->setInterval(20000);
	QObject::connect(timer, &QTimer::timeout, reply, [=] {
		if (reply) {
			reply->abort();
		}
	});
	timer->start();

	QObject::connect(reply, &QNetworkReply::finished, [=] {
		if (!weak.get()) {
			if (reply) {
				reply->deleteLater();
			}
			return;
		}
		if (!reply) {
			callback({}, u"Запрос списка моделей прерван"_q);
			return;
		}
		reply->deleteLater();
		const auto raw = reply->readAll();
		const auto doc = QJsonDocument::fromJson(raw);
		if (doc.isObject()) {
			const auto obj = doc.object();
			if (obj.contains(u"error"_q)) {
				const auto errMsg = obj.value(u"error"_q).toObject().value(u"message"_q).toString();
				callback({}, errMsg.isEmpty() ? u"Ошибка Gemini API"_q : errMsg);
				return;
			}
			std::vector<QString> result;
			const auto arr = obj.value(u"models"_q).toArray();
			for (const auto &entry : arr) {
				const auto m = entry.toObject();
				const auto methods = m.value(u"supportedGenerationMethods"_q).toArray();
				bool supportsGenerate = false;
				for (const auto &method : methods) {
					if (method.toString() == u"generateContent"_q) {
						supportsGenerate = true;
						break;
					}
				}
				if (!supportsGenerate) {
					continue;
				}
				auto name = m.value(u"name"_q).toString().trimmed();
				if (name.startsWith(u"models/"_q, Qt::CaseInsensitive)) {
					name = name.mid(7);
				}
				if (name.isEmpty() || !name.startsWith(u"gemini"_q, Qt::CaseInsensitive)) {
					continue;
				}
				result.push_back(name);
			}
			if (result.empty()) {
				callback({}, u"API не вернул доступных Gemini-моделей"_q);
				return;
			}
			std::sort(result.begin(), result.end(), [](const QString &a, const QString &b) {
				return a > b;
			});
			callback(std::move(result), QString());
			return;
		}
		if (reply->error() != QNetworkReply::NoError) {
			callback({}, reply->errorString());
			return;
		}
		callback({}, u"Пустой ответ от Gemini API"_q);
	});
}

void AskGeminiAboutItem(
		not_null<Window::SessionController*> controller,
		not_null<HistoryItem*> item) {
	const auto weak = base::make_weak(controller);
	const auto session = &controller->session();
	const auto text = item->originalText().text.trimmed();
	const auto media = ExtractItemMediaForGemini(item);
	if (text.isEmpty() && media.bytes.isEmpty()) {
		controller->showToast(u"MediaTok: в этом сообщении нет текста или загруженного медиа"_q);
		return;
	}
	QString prompt;
	if (!media.bytes.isEmpty() && !text.isEmpty()) {
		if (media.mimeType.startsWith(u"audio/"_q) || media.mimeType.startsWith(u"video/"_q)) {
			prompt = u"Проанализируй аудио/видео и подпись к нему:\n\"%1\"\n\nДай понятный ответ или тезисы."_q.arg(text);
		} else {
			prompt = u"Проанализируй изображение и текст:\n\"%1\"\n\nДай понятный ответ или перевод."_q.arg(text);
		}
	} else if (!media.bytes.isEmpty()) {
		if (media.mimeType.startsWith(u"audio/"_q) || media.mimeType.startsWith(u"video/"_q)) {
			prompt = u"Расшифруй речь и кратко перескажи суть этого аудио/видеосообщения."_q;
		} else {
			prompt = u"Что на этом изображении? Опиши кратко и по сути (и переведи текст на нём, если он есть)."_q;
		}
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
	}, media.bytes, media.mimeType);
}

bool CanTranscribeMedia(HistoryItem *item) {
	if (!item || !item->isRegular() || item->isLocal()) {
		return false;
	}
	const auto media = item->media();
	const auto doc = media ? media->document() : nullptr;
	return doc && (doc->isVoiceMessage() || doc->isVideoMessage());
}

void TranscribeMediaWithGemini(
		not_null<Window::SessionController*> controller,
		not_null<HistoryItem*> item) {
	if (!CanTranscribeMedia(item)) {
		return;
	}
	const auto media = ExtractItemMediaForGemini(item, 18 * 1024 * 1024);
	if (media.bytes.isEmpty()) {
		controller->showToast(
			u"MediaTok: сначала нажми play / загрузи голосовое или кружок (до 18 МБ)"_q);
		return;
	}
	const auto weak = base::make_weak(controller);
	const auto session = &controller->session();
	controller->showToast(
		u"Gemini (%1) расшифровывает..."_q.arg(AyuSettings::getInstance().geminiModel()));
	const auto prompt = u"Сделай точную дословную расшифровку речи из этого аудио/видеосообщения (на языке оригинала). Если сообщение длиннее пары фраз, в конце добавь короткое резюме в 1-2 пунктах."_q;
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
	}, media.bytes, media.mimeType);
}

void ShowHelpBox(not_null<Window::SessionController*> controller) {
	controller->show(Ui::MakeInformBox(
		u"Команды MediaTok:\n\n"
		"ИИ (Gemini):\n"
		"• .ai <запрос> / .ai! <запрос> — спросить Gemini (в чат / скрытно), видит цепочку реплаев + фото/ГС/кружки\n"
		"• .tldr [N] / .tldr! [N] — выжимка последних N (по умолч. 60) сообщений чата\n"
		"• .tr <язык> <текст> / .tr! — AI-перевод (можно реплаем без текста)\n"
		"• .fix <текст> / .fix! — исправить ошибки и опечатки\n"
		"• .tone <стиль> <текст> / .tone! — переписать в нужном стиле (деловой, краткий, дерзкий...)\n"
		"• ПКМ на ГС или кружке -> «Расшифровать (AI)» — бесплатная расшифровка речи\n\n"
		"Медиа (поддерживают таймкод 0:45-1:15 и скорость 1.5x):\n"
		"• .circle <ссылка> — скачать видео и отправить КРУЖКОМ (или реплаем на видео/GIF)\n"
		"• .gif <ссылка> — сделать зацикленную GIF (или реплаем / ПКМ на видео)\n"
		"• .voice <ссылка> — отправить как голосовое (с активным Voicemod, или реплаем)\n"
		"• .mp3 <ссылка> — скачать аудио-треком MP3 (или реплаем из видео/кружка)\n"
		"• .dl <ссылка> / .raw <текст> — скачать видео / отправить текст без скачивания\n\n"
		"Приватность и очистка:\n"
		"• .delme [N | 30m | 2h | 7d] — удалить свои сообщения в чате (все, N последних или за период)\n"
		"• .delmeall — удалить все свои сообщения во всех чатах\n"
		"• .autodel <30s | 5m | 2h | off | status> — авто-удаление ВАШИХ новых сообщений у всех\n"
		"• .wipe / .wipe all / .unwipe — тихий снос входящих в ЛС у обоих\n"
		"• .vm <off|deep|helium|robot|radio|echo|anon|bass> — пресет Voicemod\n"
		"• .vis / .visuser / .visname / .visbio / .visphone / .visbadge / .visclear — визуал профиля + 4K ава (себе и любому!)\n"
		"• .font [стиль|off|list] — студия шрифтов (80+ клиента, редактор .ttf и 32 стиля чата)\n"
		"• .f <стиль> <текст> — отправить сообщение в выбранном стиле (видят все)\n"
		"• .color <neon|gold|fire|#HEX> <текст> — цветной прозрачный текст-стикер твоим шрифтом\n"
		"• .lang [edit] — каталог 66 языков ТГ + фулл-редактор ВСЕХ 8669 фраз клиента\n"
		"• В профиле (кнопка ⋮) -> «MediaTok: Визуал и 4K ава» / «Личная заметка MediaTok»"_q));
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
	const auto history = item->history();
	const auto action = Api::SendAction(history);
	const auto weak = base::make_weak(controller);
	WithLocalDocumentMediaPath(controller, item, [=](QString path, QString cleanupDir) {
		if (const auto s = weak.get()) {
			ConvertLocalFileAndSend(
				s,
				history,
				action,
				path,
				cleanupDir,
				MediaOutputMode::Circle,
				MediaDownloadOptions{});
		} else if (!cleanupDir.isEmpty()) {
			QDir(cleanupDir).removeRecursively();
		}
	});
}

void ResendItemAsGif(
		not_null<Window::SessionController*> controller,
		not_null<HistoryItem*> item) {
	if (!item || !item->isRegular() || item->isLocal()) {
		return;
	}
	const auto media = item->media();
	const auto doc = media ? media->document() : nullptr;
	if (!doc || (!doc->isVideoFile() && !doc->isVideoMessage() && !doc->isAnimation() && !doc->isGifv())) {
		return;
	}
	const auto history = item->history();
	const auto action = Api::SendAction(history);
	const auto weak = base::make_weak(controller);
	WithLocalDocumentMediaPath(controller, item, [=](QString path, QString cleanupDir) {
		if (const auto s = weak.get()) {
			ConvertLocalFileAndSend(
				s,
				history,
				action,
				path,
				cleanupDir,
				MediaOutputMode::GifAnimation,
				MediaDownloadOptions{});
		} else if (!cleanupDir.isEmpty()) {
			QDir(cleanupDir).removeRecursively();
		}
	});
}

void DeleteMyMessagesInPeer(
		not_null<PeerData*> peer,
		Window::SessionController *controller,
		Fn<void(int deletedCount)> onDone,
		int maxCount,
		TimeId sinceDate) {
	const auto session = &peer->session();
	const auto weakController = controller ? base::make_weak(controller) : nullptr;

	if (maxCount <= 0 && sinceDate <= 0) {
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
	}

	auto collected = std::make_shared<std::vector<MsgId>>();
	auto seenIds = std::make_shared<std::unordered_set<int64>>();
	const auto removeNext = std::make_shared<Fn<void(int)>>();
	const auto requestNext = std::make_shared<Fn<void(MsgId)>>();
	const auto releaseLoop = [=] {
		// Defer breaking the shared_ptr->Fn self-cycle until after the active lambda returns.
		crl::on_main(session, [removeNext, requestNext] {
			*removeNext = nullptr;
			*requestNext = nullptr;
		});
	};

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
			releaseLoop();
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
			releaseLoop();
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
			bool stopPagination = false;
			for (const auto &id : parsed.messageIds) {
				if (!minId || id < minId) {
					minId = id;
				}
				if (const auto item = peer->owner().message(peer->id, id)) {
					if (sinceDate > 0 && item->date() < sinceDate) {
						stopPagination = true;
						continue;
					}
					if (item->out() && !item->isService()) {
						if (seenIds->insert(id.bare).second) {
							collected->push_back(id);
							if (maxCount > 0 && int(collected->size()) >= maxCount) {
								stopPagination = true;
								break;
							}
						}
					}
				} else if (!peer->isUser() && sinceDate <= 0) {
					if (seenIds->insert(id.bare).second) {
						collected->push_back(id);
						if (maxCount > 0 && int(collected->size()) >= maxCount) {
							stopPagination = true;
							break;
						}
					}
				}
			}
			if (!stopPagination && rawCount >= 100 && minId > MsgId(1) && (!from || minId < from)) {
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
			releaseLoop();
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
					crl::on_main(session, [stepFn] {
						*stepFn = nullptr;
					});
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
	OnWavesIncomingMessage(item);
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

std::optional<int> ParseDurationSeconds(const QString &raw) {
	const auto k = raw.trimmed().toLower();
	if (k.isEmpty()) {
		return std::nullopt;
	}
	if (k == u"0"_q || k == u"off"_q || k == u"выкл"_q || k == u"stop"_q || k == u"disable"_q) {
		return 0;
	}
	static const QRegularExpression kRe(
		uR"(^(\d+)\s*(s|sec|сек|с|m|min|мин|м|h|hr|ч|d|д)?$)"_q,
		QRegularExpression::CaseInsensitiveOption);
	const auto m = kRe.match(k);
	if (!m.hasMatch()) {
		return std::nullopt;
	}
	bool ok = false;
	const int64 num = m.captured(1).toLongLong(&ok);
	if (!ok || num <= 0) {
		return std::nullopt;
	}
	const auto unit = m.captured(2).toLower();
	int64 mult = 1;
	if (unit == u"m"_q || unit == u"min"_q || unit == u"мин"_q || unit == u"м"_q) {
		mult = 60;
	} else if (unit == u"h"_q || unit == u"hr"_q || unit == u"ч"_q) {
		mult = 3600;
	} else if (unit == u"d"_q || unit == u"д"_q) {
		mult = 86400;
	}
	const int64 secs = num * mult;
	return int(std::clamp<int64>(secs, 2, 604800));
}

QString FormatDurationShort(int seconds) {
	if (seconds <= 0) {
		return u"выкл"_q;
	}
	if (seconds % 86400 == 0) {
		return u"%1 д"_q.arg(seconds / 86400);
	}
	if (seconds % 3600 == 0) {
		return u"%1 ч"_q.arg(seconds / 3600);
	}
	if (seconds % 60 == 0) {
		return u"%1 мин"_q.arg(seconds / 60);
	}
	return u"%1 сек"_q.arg(seconds);
}

void CheckAutoDeleteOutgoing(not_null<HistoryItem*> item) {
	OnWavesOutgoingMessageSent(item);
	if (!item->out() || !item->isRegular() || item->id <= 0) {
		return;
	}
	const auto peer = item->history()->peer;
	const auto dialogId = getDialogIdFromPeer(peer);
	const int seconds = AyuSettings::getInstance().autoDeleteOutgoingSeconds(dialogId);
	if (seconds <= 0) {
		return;
	}

	const auto session = &peer->session();
	const auto peerId = peer->id;
	const auto rawId = item->id.bare;

	struct DedupeKey {
		uint64 sessionUser;
		uint64 peerRaw;
		int64 msgId;
		bool operator==(const DedupeKey &other) const = default;
	};
	struct DedupeHash {
		size_t operator()(const DedupeKey &k) const noexcept {
			return size_t(k.sessionUser) ^ (size_t(k.peerRaw) * 1315423911ULL) ^ size_t(k.msgId);
		}
	};
	static std::unordered_set<DedupeKey, DedupeHash> scheduledSet;
	static std::deque<DedupeKey> scheduledQueue;
	const DedupeKey key{ session->userId().bare, peerId.value, rawId };
	if (!scheduledSet.insert(key).second) {
		return;
	}
	scheduledQueue.push_back(key);
	while (scheduledQueue.size() > 512) {
		scheduledSet.erase(scheduledQueue.front());
		scheduledQueue.pop_front();
	}

	base::call_delayed(crl::time(seconds) * 1000, session, [=] {
		const auto currentPeer = session->data().peerLoaded(peerId);
		if (!currentPeer) {
			return;
		}
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

bool HandleOutgoingText(
		not_null<Window::SessionController*> controller,
		Api::MessageToSend &message) {
	const auto rawText = message.textWithTags.text;
	const auto trimmed = rawText.trimmed();
	if (trimmed.isEmpty()) {
		return false;
	}

	const auto history = message.action.history;
	const auto peer = history->peer;
	const auto session = &history->session();

	const auto firstSpace = trimmed.indexOf(QRegularExpression(u"\\s"_q));
	const auto cmd = (firstSpace < 0 ? trimmed : trimmed.left(firstSpace)).toLower();
	const auto rest = (firstSpace < 0 ? QString() : trimmed.mid(firstSpace).trimmed());

	if (cmd == u".mt"_q || cmd == u".help"_q) {
		ShowHelpBox(controller);
		return true;
	}

	if (HandleFontCommand(controller, message, cmd, rest)) {
		return true;
	}

	if (cmd == u".lang"_q || cmd == u".langs"_q) {
		ShowLanguageStudioBox(controller, rest.trimmed().compare(u"edit"_q, Qt::CaseInsensitive) == 0 ? 1 : 0);
		return true;
	}

	if (cmd == u".raw"_q) {
		if (rest.isEmpty()) {
			controller->showToast(u"Использование: .raw <текст или ссылка>"_q);
			return true;
		}
		auto plain = message;
		plain.textWithTags.text = rest;
		plain.textWithTags.tags.clear();
		plain.action.clearDraft = false;
		plain.action.generateLocal = true;
		session->api().sendMessage(std::move(plain));
		return true;
	}

	if (cmd == u".delme"_q) {
		if (rest.isEmpty()) {
			controller->showToast(u"MediaTok: удаляю все ваши сообщения в этом чате..."_q);
			DeleteMyMessagesInPeer(peer, controller);
			return true;
		}
		bool isCount = false;
		const int count = rest.toInt(&isCount);
		if (isCount && count > 0) {
			controller->showToast(
				u"MediaTok: удаляю последние %1 ваших сообщений..."_q.arg(count));
			DeleteMyMessagesInPeer(peer, controller, nullptr, count, 0);
			return true;
		}
		if (const auto secs = ParseDurationSeconds(rest); secs && *secs > 0) {
			controller->showToast(
				u"MediaTok: удаляю ваши сообщения за последние %1..."_q.arg(
					FormatDurationShort(*secs)));
			DeleteMyMessagesInPeer(
				peer,
				controller,
				nullptr,
				0,
				base::unixtime::now() - *secs);
			return true;
		}
		controller->showToast(u"Использование: .delme [число | 30m | 2h | 7d]"_q);
		return true;
	}

	if (cmd == u".delmeall"_q) {
		DeleteMyMessagesEverywhere(controller);
		return true;
	}

	if (cmd == u".wipe"_q) {
		const bool wipeAll = (rest.compare(u"all"_q, Qt::CaseInsensitive) == 0);
		if (!rest.isEmpty() && !wipeAll) {
			return false;
		}
		if (!peer->isUser() || peer->isSelf()) {
			controller->showToast(u"MediaTok: .wipe работает только в личных чатах (ЛС)"_q);
			return true;
		}
		AyuSettings::getInstance().addAutoDeleteIncomingPeer(getDialogIdFromPeer(peer));
		if (wipeAll) {
			WipeIncomingPeerHistory(peer);
			controller->showToast(u"MediaTok: ЛС очищено для обоих + авто-удаление входящих ВКЛ"_q);
		} else {
			controller->showToast(u"MediaTok: авто-удаление входящих от собеседника ВКЛ"_q);
		}
		return true;
	}

	if (cmd == u".unwipe"_q && rest.isEmpty()) {
		AyuSettings::getInstance().removeAutoDeleteIncomingPeer(getDialogIdFromPeer(peer));
		controller->showToast(u"MediaTok: авто-удаление входящих ВЫКЛ"_q);
		return true;
	}

	if (cmd == u".autodel"_q) {
		const auto dialogId = getDialogIdFromPeer(peer);
		if (rest.isEmpty() || rest.compare(u"status"_q, Qt::CaseInsensitive) == 0) {
			const int current = AyuSettings::getInstance().autoDeleteOutgoingSeconds(dialogId);
			controller->showToast(
				current > 0
					? u"MediaTok .autodel в этом чате: %1"_q.arg(FormatDurationShort(current))
					: u"MediaTok .autodel в этом чате выкл (.autodel 30s / 5m / 2h)"_q);
			return true;
		}
		if (const auto secs = ParseDurationSeconds(rest)) {
			AyuSettings::getInstance().setAutoDeleteOutgoingSeconds(dialogId, *secs);
			controller->showToast(
				*secs > 0
					? u"MediaTok: ваши новые сообщения в чате будут сноситься через %1"_q.arg(
						FormatDurationShort(*secs))
					: u"MediaTok: авто-удаление исходящих ВЫКЛ"_q);
		} else {
			controller->showToast(u"Формат: .autodel <10s|5m|2h|1d|off>"_q);
		}
		return true;
	}

	if (cmd == u".vm"_q) {
		if (rest.isEmpty()) {
			const int nextPreset = (AyuSettings::getInstance().voicemodPreset() + 1) % kVoicemodPresetCount;
			AyuSettings::getInstance().setVoicemodPreset(nextPreset);
			controller->showToast(u"Voicemod: %1"_q.arg(VoicemodPresetName(nextPreset)));
			return true;
		}
		if (const auto parsed = ParseVoicemodArg(rest)) {
			AyuSettings::getInstance().setVoicemodPreset(*parsed);
			controller->showToast(u"Voicemod: %1"_q.arg(VoicemodPresetName(*parsed)));
		} else {
			controller->showToast(
				u"Режимы Voicemod: off, deep, helium, robot, radio, echo, anon, bass (0..7)"_q);
		}
		return true;
	}

	if (cmd == u".vis"_q) {
		ShowVisualProfileBox(controller, peer);
		return true;
	}

	if (cmd == u".integrations"_q || cmd == u".steam"_q || cmd == u".spotify"_q || cmd == u".connections"_q) {
		const auto id = VisualDialogId(peer);
		CheckGlobal4kAvatarForPeer(peer);
		const auto path = cWorkingDir() + u"tdata/mediatok_integrations.json"_q;
		QJsonObject rootObj;
		if (QFile f(path); f.open(QIODevice::ReadOnly)) {
			rootObj = QJsonDocument::fromJson(f.readAll()).object();
		}
		const auto peerObj = rootObj.value(QString::number(id)).toObject();
		controller->show(Box([=](not_null<Ui::GenericBox*> box) {
			box->setTitle(rpl::single(u"Интеграции MediaTok"_q));
			box->addButton(rpl::single(u"Закрыть"_q), [=] { box->closeBox(); });
			auto addedAny = false;
			const auto addLinkRow = [&](const QString &label, const QString &rawVal, const QString &url) {
				if (rawVal.trimmed().isEmpty()) return;
				addedAny = true;
				box->verticalLayout()->add(object_ptr<Ui::SettingsButton>(
					box,
					rpl::single(u"%1: %2"_q.arg(label, rawVal.trimmed())),
					st::infoProfileButton))->setClickedCallback([=] {
					if (!url.isEmpty()) {
						QDesktopServices::openUrl(QUrl(url));
					} else if (auto *cb = QGuiApplication::clipboard()) {
						cb->setText(rawVal.trimmed());
						controller->showToast(u"Скопировано: %1"_q.arg(rawVal.trimmed()));
					}
				});
			};
			const auto steam = peerObj.value(u"steamInput"_q).toString().trimmed();
			if (!steam.isEmpty()) {
				const auto url = steam.startsWith(u"http"_q) ? steam : u"https://steamcommunity.com/id/%1"_q.arg(steam);
				addLinkRow(u"Steam"_q, steam.section('/', -1), url);
			}
			const auto spotify = peerObj.value(u"spotifyInput"_q).toString().trimmed();
			if (!spotify.isEmpty()) {
				const auto parts = spotify.split('|');
				const auto name = parts.value(0).trimmed();
				const auto url = parts.size() > 1 ? parts.value(1).trimmed() : u"https://open.spotify.com/"_q;
				addLinkRow(u"Spotify"_q, name.isEmpty() ? u"Spotify"_q : name, url);
			}
			const auto discord = peerObj.value(u"discordTag"_q).toString().trimmed();
			addLinkRow(u"Discord"_q, discord, QString());
			const auto yt = peerObj.value(u"youtubeInput"_q).toString().trimmed();
			if (!yt.isEmpty()) {
				const auto url = yt.startsWith(u"http"_q) ? yt : u"https://www.youtube.com/%1"_q.arg(yt.startsWith('@') ? yt : (u"@"_q + yt));
				addLinkRow(u"YouTube"_q, yt, url);
			}
			const auto rbx = peerObj.value(u"robloxInput"_q).toString().trimmed();
			if (!rbx.isEmpty()) {
				const auto url = rbx.startsWith(u"http"_q) ? rbx : u"https://www.roblox.com/users/%1/profile"_q.arg(rbx);
				addLinkRow(u"Roblox"_q, rbx, url);
			}
			const auto tw = peerObj.value(u"twitchInput"_q).toString().trimmed();
			if (!tw.isEmpty()) {
				const auto url = tw.startsWith(u"http"_q) ? tw : u"https://www.twitch.tv/%1"_q.arg(tw);
				addLinkRow(u"Twitch"_q, tw, url);
			}
			const auto gh = peerObj.value(u"githubInput"_q).toString().trimmed();
			if (!gh.isEmpty()) {
				const auto url = gh.startsWith(u"http"_q) ? gh : u"https://github.com/%1"_q.arg(gh);
				addLinkRow(u"GitHub"_q, gh, url);
			}
			if (!addedAny) {
				box->addRow(object_ptr<Ui::FlatLabel>(
					box,
					rpl::single(u"У этого профиля пока нет подключённых интеграций. Подключите Steam / Spotify / Discord / YouTube в MediaTok на iOS или Android — они синхронизируются автоматически."_q),
					st::boxLabel));
			}
		}));
		return true;
	}

	if (cmd == u".tonpay"_q || cmd == u".tonkeeper"_q || cmd == u".xrocket"_q) {
		const auto prefill = rest.trimmed();
		controller->show(Box([=](not_null<Ui::GenericBox*> box) {
			box->setTitle(rpl::single(u"Перевод TON (Tonkeeper / xRocket)"_q));
			const auto addrField = box->addRow(object_ptr<Ui::InputField>(
				box, st::defaultInputField, rpl::single(u"Адрес TON (EQ... / UQ... / name.ton)"_q), prefill));
			const auto amountField = box->addRow(object_ptr<Ui::InputField>(
				box, st::defaultInputField, rpl::single(u"Сумма в TON (напр. 1.5)"_q)));
			const auto memoField = box->addRow(object_ptr<Ui::InputField>(
				box, st::defaultInputField, rpl::single(u"Комментарий / Memo"_q)));
			box->addButton(rpl::single(u"Tonkeeper"_q), [=] {
				const auto addr = addrField->getLastText().trimmed();
				if (addr.isEmpty()) {
					controller->showToast(u"Укажите TON-адрес получателя"_q);
					return;
				}
				const auto amtStr = amountField->getLastText().trimmed().replace(',', '.');
				const auto memo = memoField->getLastText().trimmed();
				bool ok = false;
				const double tonVal = amtStr.toDouble(&ok);
				const auto nano = (ok && tonVal > 0.0) ? qint64(std::llround(tonVal * 1000000000.0)) : qint64(0);
				QUrl url(u"https://app.tonkeeper.com/transfer/%1"_q.arg(addr));
				QUrlQuery q;
				if (nano > 0) q.addQueryItem(u"amount"_q, QString::number(nano));
				if (!memo.isEmpty()) q.addQueryItem(u"text"_q, memo);
				url.setQuery(q);
				QDesktopServices::openUrl(url);
				box->closeBox();
			});
			box->addButton(rpl::single(u"xRocket"_q), [=] {
				const auto addr = addrField->getLastText().trimmed();
				const auto amt = amountField->getLastText().trimmed();
				const auto memo = memoField->getLastText().trimmed();
				QStringList parts;
				if (!addr.isEmpty()) parts << addr;
				if (!amt.isEmpty()) parts << u"%1 TON"_q.arg(amt);
				if (!memo.isEmpty()) parts << u"Комментарий: %1"_q.arg(memo);
				if (auto *cb = QGuiApplication::clipboard(); cb && !parts.isEmpty()) {
					cb->setText(parts.join(u" | "_q));
				}
				controller->showToast(u"Реквизиты скопированы — открываю @xrocket"_q);
				QDesktopServices::openUrl(QUrl(u"https://t.me/xrocket"_q));
				box->closeBox();
			});
			box->addLeftButton(rpl::single(u"Отмена"_q), [=] { box->closeBox(); });
		}));
		return true;
	}

	if (cmd == u".bincheck"_q) {
		auto digits = QString();
		const auto src = rest.isEmpty() ? (QGuiApplication::clipboard() ? QGuiApplication::clipboard()->text() : QString()) : rest;
		for (const auto ch : src) {
			if (ch.isDigit()) digits.append(ch);
		}
		if (digits.size() < 6) {
			controller->showToast(u"Укажите минимум 6 цифр карты (.bincheck <номер>)"_q);
			return true;
		}
		int sum = 0;
		bool alt = false;
		for (int i = digits.size() - 1; i >= 0; --i) {
			int n = digits[i].digitValue();
			if (alt) {
				n *= 2;
				if (n > 9) n -= 9;
			}
			sum += n;
			alt = !alt;
		}
		const bool luhnOk = (digits.size() >= 13 && digits.size() <= 19 && (sum % 10 == 0));
		QString net = u"Неизвестная сеть"_q;
		if (digits.startsWith(u"220"_q)) net = u"МИР"_q;
		else if (digits.startsWith('4')) net = u"Visa"_q;
		else if (digits.startsWith('5') || digits.startsWith('2')) net = u"Mastercard"_q;
		else if (digits.startsWith(u"34"_q) || digits.startsWith(u"37"_q)) net = u"AmEx"_q;
		else if (digits.startsWith(u"62"_q)) net = u"UnionPay"_q;
		controller->showToast(u"BIN %1 · %2 · Luhn: %3"_q.arg(digits.left(6), net, luhnOk ? u"OK"_q : u"FAIL"_q));
		return true;
	}

	if (cmd == u".songlink"_q) {
		const auto target = rest.trimmed().isEmpty() ? (QGuiApplication::clipboard() ? QGuiApplication::clipboard()->text().trimmed() : QString()) : rest.trimmed();
		if (!target.startsWith(u"http"_q)) {
			controller->showToast(u"Укажите ссылку на трек (.songlink <url>)"_q);
			return true;
		}
		controller->showToast(u"Ищу трек на всех площадках..."_q);
		QUrl apiUrl(u"https://api.song.link/v1-alpha.1/links"_q);
		QUrlQuery q;
		q.addQueryItem(u"url"_q, target);
		apiUrl.setQuery(q);
		QNetworkRequest req(apiUrl);
		req.setHeader(QNetworkRequest::UserAgentHeader, u"MediaTok/Songlink"_q);
		const auto weakCtrl = base::make_weak(controller);
		QPointer<QNetworkReply> reply = NetworkManager().get(req);
		QObject::connect(reply, &QNetworkReply::finished, [=] {
			if (!reply) return;
			reply->deleteLater();
			const auto ctrl = weakCtrl.get();
			if (!ctrl || reply->error() != QNetworkReply::NoError) return;
			const auto obj = QJsonDocument::fromJson(reply->readAll()).object();
			const auto pageUrl = obj.value(u"pageUrl"_q).toString();
			const auto links = obj.value(u"linksByPlatform"_q).toObject();
			QStringList lines;
			if (!pageUrl.isEmpty()) lines << u"Songlink: %1"_q.arg(pageUrl);
			for (const auto &[k, label] : std::vector<std::pair<QString, QString>>{
				{ u"spotify"_q, u"Spotify"_q },
				{ u"appleMusic"_q, u"Apple Music"_q },
				{ u"youtubeMusic"_q, u"YouTube Music"_q },
				{ u"yandex"_q, u"Яндекс Музыка"_q }
			}) {
				const auto u = links.value(k).toObject().value(u"url"_q).toString();
				if (!u.isEmpty()) lines << u"%1: %2"_q.arg(label, u);
			}
			const auto out = lines.join('\n');
			if (out.isEmpty()) {
				ctrl->showToast(u"Трек не найден в Songlink"_q);
				return;
			}
			if (auto *cb = QGuiApplication::clipboard()) cb->setText(out);
			ctrl->showToast(u"Ссылки скопированы в буфер (%1)"_q.arg(lines.size()));
		});
		return true;
	}

	if (cmd == u".spin"_q) {
		if (rest.isEmpty()) {
			controller->showToast(u"Формат: .spin {Привет|Здарова} {друг|бро}!"_q);
			return true;
		}
		auto out = rest;
		const QRegularExpression re(u"\\{([^{}]+)\\}"_q);
		for (int guard = 0; guard < 12; ++guard) {
			const auto m = re.match(out);
			if (!m.hasMatch()) break;
			const auto opts = m.captured(1).split('|');
			const auto pick = opts.isEmpty() ? QString() : opts.at(int(base::RandomValue<uint32>() % uint32(opts.size())));
			out.replace(m.capturedStart(), m.capturedLength(), pick);
		}
		message.textWithTags = { out, {} };
		return false;
	}

	if (cmd == u".prostore"_q || cmd == u".tribute"_q) {
		QDesktopServices::openUrl(QUrl(u"https://t.me/tribute"_q));
		return true;
	}

	if (HandleWavesOutgoingText(controller, message)) {
		return true;
	}

	if (cmd == u".visclear"_q) {
		const auto id = VisualDialogId(peer);
		if (const auto existing = AyuSettings::getInstance().visualProfile(id)) {
			if (!existing->avatarPath.isEmpty()) {
				QFile::remove(existing->avatarPath);
			}
		}
		AyuSettings::getInstance().clearVisualProfile(id);
		peer->session().changes().peerUpdated(
			peer,
			Data::PeerUpdate::Flag::Name
				| Data::PeerUpdate::Flag::Username
				| Data::PeerUpdate::Flag::Usernames
				| Data::PeerUpdate::Flag::Photo
				| Data::PeerUpdate::Flag::About
				| Data::PeerUpdate::Flag::PhoneNumber
				| Data::PeerUpdate::Flag::EmojiStatus
				| Data::PeerUpdate::Flag::VerifyInfo
				| Data::PeerUpdate::Flag::FullInfo);
		controller->showToast(u"MediaTok: визуал и 4K ава для чата сброшены"_q);
		return true;
	}

	if (cmd == u".visname"_q || cmd == u".visbio"_q || cmd == u".visphone"_q || cmd == u".visuser"_q || cmd == u".visbadge"_q) {
		const auto id = VisualDialogId(peer);
		auto ov = AyuSettings::getInstance().visualProfile(id).value_or(VisualPeerOverride{});
		const bool turnOff = rest.isEmpty() || rest.compare(u"off"_q, Qt::CaseInsensitive) == 0;
		if (cmd == u".visname"_q) {
			ov.name = turnOff ? QString() : rest.trimmed();
		} else if (cmd == u".visbio"_q) {
			ov.bio = turnOff ? QString() : rest.trimmed();
		} else if (cmd == u".visphone"_q) {
			ov.phone = turnOff ? QString() : rest.trimmed();
		} else if (cmd == u".visbadge"_q) {
			const auto low = rest.trimmed().toLower();
			if (turnOff || low == u"default"_q) {
				ov.badgeMode = 0;
			} else if (low == u"v"_q || low == u"verified"_q) {
				ov.badgeMode = 1;
			} else if (low == u"p"_q || low == u"premium"_q) {
				ov.badgeMode = 2;
			} else if (low == u"scam"_q) {
				ov.badgeMode = 3;
			} else if (low == u"fake"_q) {
				ov.badgeMode = 4;
			} else if (low == u"hide"_q || low == u"none"_q) {
				ov.badgeMode = 5;
			} else {
				controller->showToast(u"Формат: .visbadge <verified|premium|scam|fake|hide|off>"_q);
				return true;
			}
		} else if (cmd == u".visuser"_q) {
			if (turnOff) {
				ov.username.clear();
				ov.alsoUsernames.clear();
			} else if (rest.startsWith('+')) {
				ov.alsoUsernames = rest.mid(1).trimmed();
			} else if (rest.contains('|')) {
				const auto idx = rest.indexOf('|');
				ov.username = rest.left(idx).trimmed();
				ov.alsoUsernames = rest.mid(idx + 1).trimmed();
			} else {
				const auto parts = rest.split(QRegularExpression(u"[,;\\s]+"_q), Qt::SkipEmptyParts);
				if (parts.size() >= 2) {
					ov.username = parts.front();
					ov.alsoUsernames = parts.mid(1).join(u", "_q);
				} else if (rest.startsWith('=')) {
					ov.username = rest.mid(1).trimmed();
				} else {
					ov.alsoUsernames = rest.trimmed();
				}
			}
		}
		AyuSettings::getInstance().setVisualProfile(id, ov);
		peer->session().changes().peerUpdated(
			peer,
			Data::PeerUpdate::Flag::Name
				| Data::PeerUpdate::Flag::Username
				| Data::PeerUpdate::Flag::Usernames
				| Data::PeerUpdate::Flag::Photo
				| Data::PeerUpdate::Flag::About
				| Data::PeerUpdate::Flag::PhoneNumber
				| Data::PeerUpdate::Flag::EmojiStatus
				| Data::PeerUpdate::Flag::VerifyInfo
				| Data::PeerUpdate::Flag::FullInfo);
		controller->showToast(u"MediaTok: визуал обновлён (.vis для полного редактора)"_q);
		return true;
	}

	if (cmd == u".ai"_q || cmd == u".ai!"_q) {
		const bool isPrivate = (cmd == u".ai!"_q);
		auto prompt = rest;
		GeminiMediaBlob replyMedia;
		if (const auto replyId = message.action.replyTo.messageId) {
			if (const auto replyItem = history->owner().message(replyId)) {
				replyMedia = ExtractItemMediaForGemini(replyItem, 12 * 1024 * 1024);
			}
			const auto chainText = CollectReplyChainText(history, replyId);
			if (!chainText.isEmpty()) {
				prompt = prompt.isEmpty()
					? chainText
					: u"Контекст переписки:\n%1\n\nЗапрос:\n%2"_q.arg(chainText, prompt);
			}
		}
		if (prompt.isEmpty() && !replyMedia.bytes.isEmpty()) {
			if (replyMedia.mimeType.startsWith(u"audio/"_q) || replyMedia.mimeType.startsWith(u"video/"_q)) {
				prompt = u"Расшифруй речь и кратко опиши суть этого сообщения."_q;
			} else {
				prompt = u"Что изображено на этом фото? Опиши понятно и по сути."_q;
			}
		}
		if (prompt.isEmpty()) {
			controller->showToast(u"Напиши запрос после .ai или ответь на сообщение/медиа"_q);
			return true;
		}
		SendGeminiTextReply(
			controller,
			history,
			message.action,
			prompt,
			isPrivate,
			replyMedia.bytes,
			replyMedia.mimeType);
		return true;
	}

	if (cmd == u".tldr"_q || cmd == u".tldr!"_q) {
		const bool isPrivate = (cmd == u".tldr!"_q);
		int count = 60;
		if (!rest.isEmpty()) {
			bool ok = false;
			const int parsed = rest.toInt(&ok);
			if (!ok || parsed <= 0) {
				controller->showToast(u"Использование: .tldr [число_сообщений, напр. 50]"_q);
				return true;
			}
			count = std::clamp(parsed, 10, 120);
		}
		const auto transcript = CollectRecentChatText(history, count);
		if (transcript.isEmpty()) {
			controller->showToast(u"MediaTok: нет загруженных текстовых сообщений для анализа"_q);
			return true;
		}
		const auto prompt = u"Сделай сжатую, структурированную выжимку переписки на русском (ключевые темы, решения, вопросы). Кратко буллетами:\n\n%1"_q
			.arg(transcript);
		SendGeminiTextReply(controller, history, message.action, prompt, isPrivate);
		return true;
	}

	if (cmd == u".tr"_q || cmd == u".tr!"_q) {
		const bool isPrivate = (cmd == u".tr!"_q);
		const auto space2 = rest.indexOf(QRegularExpression(u"\\s"_q));
		const auto targetLang = (space2 < 0 ? rest : rest.left(space2)).trimmed();
		auto textToTranslate = (space2 < 0 ? QString() : rest.mid(space2).trimmed());
		if (textToTranslate.isEmpty() && message.action.replyTo.messageId) {
			if (const auto replyItem = history->owner().message(message.action.replyTo.messageId)) {
				textToTranslate = replyItem->originalText().text.trimmed();
			}
		}
		if (targetLang.isEmpty() || textToTranslate.isEmpty()) {
			controller->showToast(u"Использование: .tr <язык> <текст> (или ответом на сообщение)"_q);
			return true;
		}
		const auto prompt = u"Переведи следующий текст на язык '%1'. Выведи ТОЛЬКО готовый естественный перевод без пояснений и кавычек:\n\n%2"_q
			.arg(targetLang, textToTranslate);
		SendGeminiTextReply(controller, history, message.action, prompt, isPrivate);
		return true;
	}

	if (cmd == u".fix"_q || cmd == u".fix!"_q) {
		const bool isPrivate = (cmd == u".fix!"_q);
		auto sourceText = rest;
		if (sourceText.isEmpty() && message.action.replyTo.messageId) {
			if (const auto replyItem = history->owner().message(message.action.replyTo.messageId)) {
				sourceText = replyItem->originalText().text.trimmed();
			}
		}
		if (sourceText.isEmpty()) {
			controller->showToast(u"Использование: .fix <текст> (или ответом на сообщение)"_q);
			return true;
		}
		const auto prompt = u"Исправь орфографию, пунктуацию и опечатки в тексте, сохранив исходный язык, смысл и стиль. Выведи ТОЛЬКО исправленный текст:\n\n%1"_q
			.arg(sourceText);
		SendGeminiTextReply(controller, history, message.action, prompt, isPrivate);
		return true;
	}

	if (cmd == u".tone"_q || cmd == u".tone!"_q) {
		const bool isPrivate = (cmd == u".tone!"_q);
		const auto space2 = rest.indexOf(QRegularExpression(u"\\s"_q));
		const auto style = (space2 < 0 ? rest : rest.left(space2)).trimmed();
		auto sourceText = (space2 < 0 ? QString() : rest.mid(space2).trimmed());
		if (sourceText.isEmpty() && message.action.replyTo.messageId) {
			if (const auto replyItem = history->owner().message(message.action.replyTo.messageId)) {
				sourceText = replyItem->originalText().text.trimmed();
			}
		}
		if (style.isEmpty() || sourceText.isEmpty()) {
			controller->showToast(u"Использование: .tone <стиль> <текст> (или ответом на сообщение)"_q);
			return true;
		}
		const auto prompt = u"Перепиши следующий текст в стиле '%1', сохраняя исходный язык и суть. Выведи ТОЛЬКО готовый вариант:\n\n%2"_q
			.arg(style, sourceText);
		SendGeminiTextReply(controller, history, message.action, prompt, isPrivate);
		return true;
	}

	const auto resolveMediaMode = [&]() -> std::optional<MediaOutputMode> {
		if (cmd == u".circle"_q) return MediaOutputMode::Circle;
		if (cmd == u".dl"_q) return MediaOutputMode::Video;
		if (cmd == u".mp3"_q) return MediaOutputMode::AudioMp3;
		if (cmd == u".voice"_q) return MediaOutputMode::VoiceOgg;
		if (cmd == u".gif"_q) return MediaOutputMode::GifAnimation;
		return std::nullopt;
	};
	if (const auto mode = resolveMediaMode()) {
		const auto opts = ParseMediaDownloadOptions(rest);
		if (!opts.url.isEmpty()) {
			DownloadMediaFromUrl(
				controller,
				history,
				message.action,
				opts,
				*mode,
				std::nullopt);
			return true;
		}
		if (const auto replyId = message.action.replyTo.messageId) {
			if (const auto replyItem = history->owner().message(replyId)) {
				const auto replyMedia = replyItem->media();
				const auto doc = replyMedia ? replyMedia->document() : nullptr;
				const bool canUseReply = doc && (
					(*mode == MediaOutputMode::Circle || *mode == MediaOutputMode::GifAnimation)
						? (doc->isVideoFile() || doc->isVideoMessage() || doc->isAnimation() || doc->isGifv())
						: (doc->isVideoFile() || doc->isVideoMessage() || doc->isVoiceMessage() || doc->isAudioFile() || doc->isSong()));
				if (canUseReply) {
					const auto weak = base::make_weak(controller);
					const auto action = Api::SendAction(history);
					WithLocalDocumentMediaPath(controller, replyItem, [=](QString path, QString cleanupDir) {
						if (const auto s = weak.get()) {
							ConvertLocalFileAndSend(
								s,
								history,
								action,
								path,
								cleanupDir,
								*mode,
								opts,
								opts.startSec && opts.endSec);
						} else if (!cleanupDir.isEmpty()) {
							QDir(cleanupDir).removeRecursively();
						}
					});
					return true;
				}
			}
		}
		controller->showToast(
			u"Укажи ссылку (%1 https://... [0:10-0:30] [1.5x]) или ответь на медиа в чате"_q.arg(cmd));
		return true;
	}

	if (AyuSettings::getInstance().autoVideoFromLinks()
		&& IsStandaloneHttpUrl(trimmed)) {
		MediaDownloadOptions opts;
		opts.url = trimmed;
		DownloadMediaFromUrl(
			controller,
			history,
			message.action,
			opts,
			MediaOutputMode::Video,
			message);
		return true;
	}

	TransformOutgoingMessage(message);
	if (!history->session().premium() && !message.textWithTags.tags.empty()) {
		constexpr auto kTagLen = 35;
		const QChar digits[4] = { QChar(0x200B), QChar(0x200C), QChar(0x200D), QChar(0x2060) };
		struct PendingCustomEmoji {
			int insertPos = 0;
			uint64 docId = 0;
		};
		std::vector<PendingCustomEmoji> pending;
		TextWithTags::Tags kept;
		kept.reserve(message.textWithTags.tags.size());
		for (const auto &tag : message.textWithTags.tags) {
			if (Ui::InputField::IsCustomEmojiLink(tag.id)) {
				const auto data = Ui::InputField::CustomEmojiEntityData(tag.id);
				const auto docId = Data::ParseCustomEmojiData(data);
				if (docId != 0
					&& tag.offset >= 0
					&& tag.length > 0
					&& tag.offset + tag.length <= message.textWithTags.text.size()) {
					pending.push_back({ tag.offset + tag.length, uint64(docId) });
					continue;
				}
			}
			kept.push_back(tag);
		}
		if (!pending.empty()) {
			std::sort(pending.begin(), pending.end(), [](const PendingCustomEmoji &a, const PendingCustomEmoji &b) {
				return a.insertPos > b.insertPos;
			});
			for (const auto &item : pending) {
				QString encoded;
				encoded.reserve(kTagLen);
				encoded.append(QChar(0x2063));
				encoded.append(QChar(0x2062));
				for (int shift = 62; shift >= 0; shift -= 2) {
					encoded.append(digits[(item.docId >> shift) & 0x3ULL]);
				}
				encoded.append(QChar(0x2063));
				message.textWithTags.text.insert(item.insertPos, encoded);
				for (auto &t : kept) {
					if (t.offset >= item.insertPos) {
						t.offset += kTagLen;
					} else if (t.offset + t.length > item.insertPos) {
						t.length += kTagLen;
					}
				}
			}
			message.textWithTags.tags = std::move(kept);
		}
	}
	return false;
}

bool CanSearchUserMessagesInChat(
		const Dialogs::Key &chat,
		not_null<PeerData*> targetUser) {
	const auto chatPeer = chat.peer();
	if (!chatPeer || chatPeer == targetUser || chatPeer->isSelf()) {
		return false;
	}
	if (chatPeer->isUser() || chatPeer->isChat()) {
		return true;
	}
	if (chatPeer->isMegagroup()) {
		return !chatPeer->isForum() || (chat.topic() != nullptr);
	}
	return false;
}

void SearchUserMessagesInChat(
		not_null<Window::SessionController*> controller,
		const Dialogs::Key &chat,
		not_null<PeerData*> targetUser) {
	if (!CanSearchUserMessagesInChat(chat, targetUser)) {
		return;
	}
	if (controller->adaptive().isOneColumn()) {
		if (const auto thread = chat.thread()) {
			controller->showThread(thread);
		}
	}
	controller->hideLayer();
	controller->hideSpecialLayer();
	controller->searchInChat(chat, targetUser);
}

namespace {

constexpr auto kVisualAvatarPhotoIdMax = PhotoId(-900000000000000LL);
constexpr auto kVisualAvatarPhotoIdMin = PhotoId(-900999999999999LL);
constexpr auto kVisualAvatarMaxSide = 3840;
constexpr auto kVisualThumbSide = 512;

struct CachedVisualThumb {
	QString path;
	qint64 mtimeMs = -1;
	std::shared_ptr<QImage> thumb;
	uint64 cacheKey = 0;
};

struct CachedVisualNameState {
	int revision = -1;
	std::map<int64, QString> names;
};

[[nodiscard]] QString NormalizeUsernameToken(QString token) {
	token = token.trimmed();
	if (token.startsWith(u"https://t.me/"_q, Qt::CaseInsensitive)) {
		token = token.mid(13);
	} else if (token.startsWith(u"http://t.me/"_q, Qt::CaseInsensitive)) {
		token = token.mid(12);
	} else if (token.startsWith(u"t.me/"_q, Qt::CaseInsensitive)) {
		token = token.mid(5);
	}
	while (token.startsWith('@')) {
		token = token.mid(1);
	}
	const auto slash = token.indexOf('/');
	if (slash >= 0) {
		token = token.left(slash);
	}
	return token.trimmed();
}

[[nodiscard]] std::vector<QString> ParseUsernameTokens(const QString &raw) {
	std::vector<QString> result;
	const auto parts = raw.split(
		QRegularExpression(u"[,;\\s|\\r\\n]+"_q),
		Qt::SkipEmptyParts);
	for (const auto &part : parts) {
		const auto clean = NormalizeUsernameToken(part);
		if (clean.isEmpty()) {
			continue;
		}
		const auto exists = std::any_of(
			result.begin(),
			result.end(),
			[&](const QString &u) {
				return u.compare(clean, Qt::CaseInsensitive) == 0;
			});
		if (!exists) {
			result.push_back(clean);
		}
	}
	return result;
}

[[nodiscard]] QImage SquareCrop512(const QImage &src) {
	if (src.isNull()) {
		return {};
	}
	const auto side = std::min(src.width(), src.height());
	const auto x = (src.width() - side) / 2;
	const auto y = (src.height() - side) / 2;
	auto cropped = (src.width() == src.height())
		? src
		: src.copy(x, y, side, side);
	if (cropped.width() > kVisualThumbSide || cropped.height() > kVisualThumbSide) {
		cropped = cropped.scaled(
			kVisualThumbSide,
			kVisualThumbSide,
			Qt::KeepAspectRatio,
			Qt::SmoothTransformation);
	}
	return cropped.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

[[nodiscard]] std::map<int64, CachedVisualThumb> &VisualThumbCache() {
	static std::map<int64, CachedVisualThumb> cache;
	return cache;
}

[[nodiscard]] const CachedVisualThumb *EnsureVisualThumb(not_null<const PeerData*> peer) {
	const auto id = VisualDialogId(peer);
	const auto ov = AyuSettings::getInstance().visualProfilePtr(id);
	auto &cache = VisualThumbCache();
	if (!ov || ov->avatarPath.isEmpty()) {
		cache.erase(id);
		return nullptr;
	}
	const QFileInfo info(ov->avatarPath);
	if (!info.exists() || !info.isFile()) {
		cache.erase(id);
		return nullptr;
	}
	const auto mtime = info.lastModified().toMSecsSinceEpoch();
	auto &entry = cache[id];
	if (entry.path == ov->avatarPath
		&& entry.mtimeMs == mtime
		&& entry.thumb
		&& !entry.thumb->isNull()) {
		return &entry;
	}
	QImageReader reader(ov->avatarPath);
	reader.setAutoTransform(true);
	const auto loaded = reader.read();
	if (loaded.isNull()) {
		cache.erase(id);
		return nullptr;
	}
	auto squared = SquareCrop512(loaded);
	if (squared.isNull()) {
		cache.erase(id);
		return nullptr;
	}
	static uint64 nextThumbKey = 1;
	entry.path = ov->avatarPath;
	entry.mtimeMs = mtime;
	entry.thumb = std::make_shared<QImage>(std::move(squared));
	entry.cacheKey = (uint64(uint32(id)) << 32) | (++nextThumbKey & 0xFFFFFFFFULL);
	return &entry;
}

[[nodiscard]] std::vector<QString> RealPeerUsernames(not_null<const PeerData*> peer) {
	if (const auto user = peer->asUser()) {
		return user->usernames();
	} else if (const auto channel = peer->asChannel()) {
		return channel->usernames();
	}
	return {};
}

[[nodiscard]] QString RealMainUsername(not_null<const PeerData*> peer) {
	if (const auto user = peer->asUser()) {
		return user->username();
	} else if (const auto channel = peer->asChannel()) {
		return channel->username();
	}
	return QString();
}

struct CachedVisualVideoPhoto {
	Main::Session *session = nullptr;
	QString path;
	qint64 mtimeMs = -1;
	PhotoData *photo = nullptr;
	std::shared_ptr<Data::PhotoMedia> media;
};

[[nodiscard]] std::map<int64, CachedVisualVideoPhoto> &VisualVideoCache() {
	static std::map<int64, CachedVisualVideoPhoto> cache;
	return cache;
}

[[nodiscard]] QString VisualAvatarsDirPath() {
	return cWorkingDir() + u"tdata/MediaTok_Avatars"_q;
}

[[nodiscard]] QString VisualAvatarVideoFilePath(int64 dialogId) {
	return VisualAvatarsDirPath() + u"/avatar_video_%1.mp4"_q.arg(dialogId);
}

[[nodiscard]] QString SyncStateFilePath() {
	return VisualAvatarsDirPath() + u"/sync_state.json"_q;
}

[[nodiscard]] int64 LoadSyncTimestamp(int64 dialogId) {
	QFile f(SyncStateFilePath());
	if (!f.open(QIODevice::ReadOnly)) {
		return 0;
	}
	const auto doc = QJsonDocument::fromJson(f.readAll());
	if (!doc.isObject()) {
		return 0;
	}
	return doc.object().value(QString::number(dialogId)).toInteger(0);
}

void SaveSyncTimestamp(int64 dialogId, int64 ts) {
	QDir().mkpath(VisualAvatarsDirPath());
	QJsonObject obj;
	QFile readF(SyncStateFilePath());
	if (readF.open(QIODevice::ReadOnly)) {
		const auto doc = QJsonDocument::fromJson(readF.readAll());
		if (doc.isObject()) {
			obj = doc.object();
		}
		readF.close();
	}
	obj.insert(QString::number(dialogId), qint64(ts));
	QFile writeF(SyncStateFilePath());
	if (writeF.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		writeF.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
	}
}

void UploadFileToCatboxAsync(const QString &filePath, Fn<void(QString)> done) {
	if (filePath.isEmpty() || !QFileInfo::exists(filePath)) {
		done(QString());
		return;
	}
	auto file = new QFile(filePath);
	if (!file->open(QIODevice::ReadOnly)) {
		delete file;
		done(QString());
		return;
	}
	auto multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
	file->setParent(multiPart);

	QHttpPart reqTypePart;
	reqTypePart.setHeader(
		QNetworkRequest::ContentDispositionHeader,
		QVariant(u"form-data; name=\"reqtype\""_q));
	reqTypePart.setBody("fileupload");
	multiPart->append(reqTypePart);

	QHttpPart filePart;
	filePart.setHeader(
		QNetworkRequest::ContentDispositionHeader,
		QVariant(u"form-data; name=\"fileToUpload\"; filename=\"%1\""_q
			.arg(QFileInfo(filePath).fileName())));
	filePart.setHeader(
		QNetworkRequest::ContentTypeHeader,
		QVariant(u"application/octet-stream"_q));
	filePart.setBodyDevice(file);
	multiPart->append(filePart);

	QNetworkRequest req(QUrl(u"https://catbox.moe/user/api.php"_q));
	req.setHeader(QNetworkRequest::UserAgentHeader, u"MediaTok/4K"_q);
	QPointer<QNetworkReply> reply = NetworkManager().post(req, multiPart);
	multiPart->setParent(reply);

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
			done(QString());
			return;
		}
		reply->deleteLater();
		if (reply->error() != QNetworkReply::NoError) {
			done(QString());
			return;
		}
		const auto res = QString::fromUtf8(reply->readAll()).trimmed();
		done(res.startsWith(u"https://"_q) ? res : QString());
	});
}

void DownloadUrlToFileAsync(
		const QString &url,
		const QString &dstPath,
		Fn<void(bool)> done) {
	if (!url.startsWith(u"https://"_q)) {
		done(false);
		return;
	}
	QNetworkRequest req((QUrl(url)));
	req.setHeader(QNetworkRequest::UserAgentHeader, u"MediaTok/4K"_q);
	QPointer<QNetworkReply> reply = NetworkManager().get(req);
	auto timer = new QTimer(reply);
	timer->setSingleShot(true);
	timer->setInterval(45000);
	QObject::connect(timer, &QTimer::timeout, reply, [=] {
		if (reply) {
			reply->abort();
		}
	});
	timer->start();

	QObject::connect(reply, &QNetworkReply::finished, [=] {
		if (!reply) {
			done(false);
			return;
		}
		reply->deleteLater();
		if (reply->error() != QNetworkReply::NoError) {
			done(false);
			return;
		}
		const auto bytes = reply->readAll();
		if (bytes.isEmpty()) {
			done(false);
			return;
		}
		QDir().mkpath(QFileInfo(dstPath).absolutePath());
		QFile out(dstPath);
		if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)
			|| out.write(bytes) != bytes.size()) {
			out.close();
			QFile::remove(dstPath);
			done(false);
			return;
		}
		done(true);
	});
}

void PublishGlobal4kUrls(
		int64 dialogId,
		int64 ts,
		const QString &photoUrl,
		const QString &videoUrl) {
	if (photoUrl.isEmpty() && videoUrl.isEmpty()) {
		return;
	}
	QJsonObject obj;
	if (!photoUrl.isEmpty()) {
		obj.insert(u"photoUrl"_q, photoUrl);
	}
	if (!videoUrl.isEmpty()) {
		obj.insert(u"videoUrl"_q, videoUrl);
	}
	obj.insert(u"ts"_q, qint64(ts));

	QNetworkRequest req(QUrl(u"https://ntfy.sh/mt4k_v1_%1"_q.arg(dialogId)));
	req.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_q);
	req.setHeader(QNetworkRequest::UserAgentHeader, u"MediaTok/4K"_q);
	QPointer<QNetworkReply> reply = NetworkManager().post(
		req,
		QJsonDocument(obj).toJson(QJsonDocument::Compact));
	SaveSyncTimestamp(dialogId, ts);
	QObject::connect(reply, &QNetworkReply::finished, [=] {
		if (reply) {
			reply->deleteLater();
		}
	});
}

void NotifyVisualPeerUpdated(not_null<PeerData*> peer);

void PublishGlobalVisualProfile(
		int64 dialogId,
		const VisualPeerOverride &ov) {
	if (!dialogId) {
		return;
	}
	const auto ts = QDateTime::currentSecsSinceEpoch();
	QJsonObject obj;
	obj.insert(u"ts"_q, qint64(ts));
	obj.insert(u"name"_q, ov.name);
	obj.insert(u"username"_q, ov.username);
	obj.insert(u"alsoUsernames"_q, ov.alsoUsernames);
	obj.insert(u"phone"_q, ov.phone);
	obj.insert(u"bio"_q, ov.bio);
	obj.insert(u"badgeMode"_q, ov.badgeMode);

	QNetworkRequest req(QUrl(u"https://ntfy.sh/mtprof_v1_%1"_q.arg(dialogId)));
	req.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_q);
	req.setHeader(QNetworkRequest::UserAgentHeader, u"MediaTok/ProfileSync"_q);
	QPointer<QNetworkReply> reply = NetworkManager().post(
		req,
		QJsonDocument(obj).toJson(QJsonDocument::Compact));
	QObject::connect(reply, &QNetworkReply::finished, [=] {
		if (reply) {
			reply->deleteLater();
		}
	});
}

void CheckGlobalVisualProfileForPeer(not_null<PeerData*> peer) {
	const auto dialogId = VisualDialogId(peer);
	if (!dialogId) {
		return;
	}
	static std::map<int64, int64> lastAppliedTs;
	const auto peerId = peer->id;
	const auto weakSession = base::make_weak(&peer->session());

	QNetworkRequest req(QUrl(u"https://ntfy.sh/mtprof_v1_%1/json?poll=1&since=all"_q.arg(dialogId)));
	req.setHeader(QNetworkRequest::UserAgentHeader, u"MediaTok/ProfileSync"_q);
	QPointer<QNetworkReply> reply = NetworkManager().get(req);
	auto timer = new QTimer(reply);
	timer->setSingleShot(true);
	timer->setInterval(12000);
	QObject::connect(timer, &QTimer::timeout, reply, [=] {
		if (reply) {
			reply->abort();
		}
	});
	timer->start();

	QObject::connect(reply, &QNetworkReply::finished, [=] {
		if (!reply) {
			return;
		}
		reply->deleteLater();
		const auto session = weakSession.get();
		if (reply->error() != QNetworkReply::NoError || !session) {
			return;
		}
		const auto lines = QString::fromUtf8(reply->readAll()).split('\n', Qt::SkipEmptyParts);
		QJsonObject latest;
		int64 latestTs = 0;
		for (const auto &line : lines) {
			const auto doc = QJsonDocument::fromJson(line.trimmed().toUtf8());
			if (!doc.isObject()) {
				continue;
			}
			auto obj = doc.object();
			if (obj.contains(u"message"_q)) {
				const auto innerDoc = QJsonDocument::fromJson(obj.value(u"message"_q).toString().toUtf8());
				if (innerDoc.isObject()) {
					obj = innerDoc.object();
				}
			}
			const auto ts = int64(obj.value(u"ts"_q).toInteger(0));
			if (ts > latestTs && (obj.contains(u"badgeMode"_q) || obj.contains(u"name"_q) || obj.contains(u"bio"_q) || obj.contains(u"phone"_q) || obj.contains(u"username"_q) || obj.contains(u"alsoUsernames"_q) || obj.contains(u"steamInput"_q) || obj.contains(u"spotifyInput"_q) || obj.contains(u"discordTag"_q) || obj.contains(u"githubInput"_q) || obj.contains(u"robloxInput"_q))) {
				latestTs = ts;
				latest = obj;
			}
		}
		if (latestTs <= 0 || latestTs <= lastAppliedTs[dialogId]) {
			return;
		}
		lastAppliedTs[dialogId] = latestTs;
		auto cur = AyuSettings::getInstance().visualProfile(dialogId).value_or(VisualPeerOverride{});
		if (latest.contains(u"name"_q)) cur.name = latest.value(u"name"_q).toString().trimmed();
		if (latest.contains(u"username"_q)) cur.username = NormalizeUsernameToken(latest.value(u"username"_q).toString());
		if (latest.contains(u"alsoUsernames"_q)) cur.alsoUsernames = latest.value(u"alsoUsernames"_q).toString().trimmed();
		const auto mainPhone = latest.value(u"phone"_q).toString().trimmed();
		const auto alsoPhones = latest.value(u"alsoPhones"_q).toString().trimmed();
		if (!mainPhone.isEmpty() || !alsoPhones.isEmpty()) {
			cur.phone = (!mainPhone.isEmpty() && !alsoPhones.isEmpty())
				? (mainPhone + u" • "_q + alsoPhones)
				: (!mainPhone.isEmpty() ? mainPhone : alsoPhones);
		}
		if (latest.contains(u"bio"_q)) cur.bio = latest.value(u"bio"_q).toString().trimmed();
		if (latest.contains(u"badgeMode"_q)) cur.badgeMode = std::clamp(latest.value(u"badgeMode"_q).toInt(0), 0, 5);
		AyuSettings::getInstance().setVisualProfile(dialogId, cur);
		if (latest.contains(u"steamInput"_q) || latest.contains(u"spotifyInput"_q) || latest.contains(u"discordTag"_q) || latest.contains(u"youtubeInput"_q) || latest.contains(u"robloxInput"_q) || latest.contains(u"twitchInput"_q) || latest.contains(u"githubInput"_q)) {
			const auto intPath = cWorkingDir() + u"tdata/mediatok_integrations.json"_q;
			QJsonObject allInt;
			if (QFile rf(intPath); rf.open(QIODevice::ReadOnly)) {
				allInt = QJsonDocument::fromJson(rf.readAll()).object();
			}
			QJsonObject pInt = allInt.value(QString::number(dialogId)).toObject();
			for (const auto &k : { u"steamInput"_q, u"spotifyInput"_q, u"discordTag"_q, u"youtubeInput"_q, u"robloxInput"_q, u"twitchInput"_q, u"githubInput"_q, u"tiktokInput"_q, u"kickInput"_q }) {
				if (latest.contains(k)) {
					pInt.insert(k, latest.value(k).toString());
				}
			}
			allInt.insert(QString::number(dialogId), pInt);
			if (QFile wf(intPath); wf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
				wf.write(QJsonDocument(allInt).toJson(QJsonDocument::Compact));
			}
		}
		if (const auto loadedPeer = session->data().peerLoaded(peerId)) {
			NotifyVisualPeerUpdated(loadedPeer);
		}
	});
}

void UploadVisualFilesGlobal(
		int64 dialogId,
		const QString &photoPath,
		const QString &videoPath) {
	if (photoPath.isEmpty() && videoPath.isEmpty()) {
		return;
	}
	const auto ts = QDateTime::currentSecsSinceEpoch();
	SaveSyncTimestamp(dialogId, ts);
	UploadFileToCatboxAsync(photoPath, [=](QString photoUrl) {
		if (videoPath.isEmpty() || !QFileInfo::exists(videoPath)) {
			PublishGlobal4kUrls(dialogId, ts, photoUrl, QString());
			return;
		}
		UploadFileToCatboxAsync(videoPath, [=](QString videoUrl) {
			PublishGlobal4kUrls(dialogId, ts, photoUrl, videoUrl);
		});
	});
}

void NotifyVisualPeerUpdated(not_null<PeerData*> peer) {
	VisualThumbCache().erase(VisualDialogId(peer));
	VisualVideoCache().erase(VisualDialogId(peer));
	peer->session().changes().peerUpdated(
		peer,
		Data::PeerUpdate::Flag::Name
			| Data::PeerUpdate::Flag::Username
			| Data::PeerUpdate::Flag::Usernames
			| Data::PeerUpdate::Flag::Photo
			| Data::PeerUpdate::Flag::About
			| Data::PeerUpdate::Flag::PhoneNumber
			| Data::PeerUpdate::Flag::EmojiStatus
			| Data::PeerUpdate::Flag::VerifyInfo
			| Data::PeerUpdate::Flag::FullInfo);
}

[[nodiscard]] QString SaveVisualAvatar4kImage(
		QImage image,
		int64 dialogId,
		QSize *outDimensions,
		QString *outError) {
	if (image.isNull()) {
		if (outError) {
			*outError = u"Не удалось прочитать изображение"_q;
		}
		return QString();
	}
	if (image.width() > kVisualAvatarMaxSide || image.height() > kVisualAvatarMaxSide) {
		image = image.scaled(
			kVisualAvatarMaxSide,
			kVisualAvatarMaxSide,
			Qt::KeepAspectRatio,
			Qt::SmoothTransformation);
	}
	if (outDimensions) {
		*outDimensions = image.size();
	}
	const auto dirPath = VisualAvatarsDirPath();
	QDir().mkpath(dirPath);
	const bool keepAlpha = image.hasAlphaChannel();
	const auto ext = keepAlpha ? u"png"_q : u"jpg"_q;
	const auto targetPath = dirPath + u"/avatar_%1.%2"_q.arg(dialogId).arg(ext);
	QFile::remove(dirPath + u"/avatar_%1.%2"_q.arg(dialogId).arg(keepAlpha ? u"jpg"_q : u"png"_q));

	const bool ok = keepAlpha
		? image.save(targetPath, "PNG")
		: image.save(targetPath, "JPG", 96);
	if (!ok) {
		if (outError) {
			*outError = u"Ошибка записи файла аватарки"_q;
		}
		return QString();
	}
	return targetPath;
}

[[nodiscard]] QString SaveVisualAvatar4kFile(
		const QString &sourcePath,
		int64 dialogId,
		QSize *outDimensions,
		QString *outError,
		bool *outHasVideo = nullptr) {
	if (outHasVideo) {
		*outHasVideo = false;
	}
	const auto lower = sourcePath.toLower();
	const bool extVideo = lower.endsWith(u".mp4"_q)
		|| lower.endsWith(u".mov"_q)
		|| lower.endsWith(u".webm"_q)
		|| lower.endsWith(u".mkv"_q)
		|| lower.endsWith(u".m4v"_q);

	QImageReader reader(sourcePath);
	reader.setAutoTransform(true);
	auto image = extVideo ? QImage() : reader.read();
	if (extVideo || (image.isNull() && Media::Video::ReadFileInfo(sourcePath).valid())) {
		auto cover = Media::Video::ExtractFrame(
			sourcePath,
			QByteArray(),
			crl::time(0),
			QSize(kVisualAvatarMaxSide, kVisualAvatarMaxSide));
		if (cover.isNull()) {
			if (outError) {
				*outError = u"Не удалось извлечь кадр из видео"_q;
			}
			return QString();
		}
		const auto savedCover = SaveVisualAvatar4kImage(
			std::move(cover),
			dialogId,
			outDimensions,
			outError);
		if (savedCover.isEmpty()) {
			return QString();
		}
		const auto videoDst = VisualAvatarVideoFilePath(dialogId);
		QFile::remove(videoDst);
		if (!QFile::copy(sourcePath, videoDst)) {
			if (outError) {
				*outError = u"Ошибка сохранения 4K видео-авы"_q;
			}
			return QString();
		}
		if (outHasVideo) {
			*outHasVideo = true;
		}
		return savedCover;
	}
	QFile::remove(VisualAvatarVideoFilePath(dialogId));
	return SaveVisualAvatar4kImage(
		std::move(image),
		dialogId,
		outDimensions,
		outError);
}

} // namespace

int64 VisualDialogId(not_null<const PeerData*> peer) {
	auto rawId = int64(peer->id.value & PeerId::kChatTypeMask);
	if (peer->isChannel() || peer->isChat()) {
		rawId = -rawId;
	}
	return rawId;
}

const QString *VisualNamePtr(not_null<const PeerData*> peer) {
	static CachedVisualNameState state;
	const auto &settings = AyuSettings::getInstance();
	const int rev = settings.visualProfilesRevision();
	if (state.revision != rev) {
		state.names.clear();
		for (const auto &[id, ov] : settings.visualProfiles()) {
			if (!ov.name.trimmed().isEmpty()) {
				state.names.emplace(id, ov.name.trimmed());
			}
		}
		state.revision = rev;
	}
	const auto it = state.names.find(VisualDialogId(peer));
	return (it != state.names.end()) ? &it->second : nullptr;
}

int VisualBadgeMode(not_null<const PeerData*> peer) {
	const auto ov = AyuSettings::getInstance().visualProfilePtr(VisualDialogId(peer));
	return ov ? ov->badgeMode : 0;
}

std::vector<QString> ResolveVisualUsernames(
		not_null<const PeerData*> peer,
		const QString &legacySelfRaw) {
	const auto ov = AyuSettings::getInstance().visualProfilePtr(VisualDialogId(peer));
	const auto realMain = NormalizeUsernameToken(RealMainUsername(peer));
	const auto realAll = RealPeerUsernames(peer);

	QString customMain = ov ? NormalizeUsernameToken(ov->username) : QString();
	std::vector<QString> customAlso = ov
		? ParseUsernameTokens(ov->alsoUsernames)
		: std::vector<QString>{};

	if (peer->isSelf() && customMain.isEmpty() && customAlso.empty()) {
		const auto trimmedLegacy = legacySelfRaw.trimmed();
		if (trimmedLegacy.startsWith('+')) {
			customAlso = ParseUsernameTokens(trimmedLegacy.mid(1));
		} else {
			const auto legacyTokens = ParseUsernameTokens(trimmedLegacy);
			if (!legacyTokens.empty()) {
				customMain = legacyTokens.front();
				if (legacyTokens.size() > 1) {
					customAlso.assign(legacyTokens.begin() + 1, legacyTokens.end());
				}
			}
		}
	}

	std::vector<QString> result;
	const auto appendUnique = [&](const QString &candidate) {
		if (candidate.isEmpty()) {
			return;
		}
		const bool exists = std::any_of(
			result.begin(),
			result.end(),
			[&](const QString &u) {
				return u.compare(candidate, Qt::CaseInsensitive) == 0;
			});
		if (!exists) {
			result.push_back(candidate);
		}
	};

	const auto primary = !customMain.isEmpty() ? customMain : realMain;
	appendUnique(primary);
	for (const auto &u : realAll) {
		appendUnique(NormalizeUsernameToken(u));
	}
	for (const auto &u : customAlso) {
		appendUnique(u);
	}
	return result;
}

bool IsVisualOnlyUsername(
		not_null<const PeerData*> peer,
		const QString &username,
		const QString &legacySelfRaw) {
	const auto clean = NormalizeUsernameToken(username);
	if (clean.isEmpty()) {
		return false;
	}
	const auto realMain = NormalizeUsernameToken(RealMainUsername(peer));
	if (realMain.compare(clean, Qt::CaseInsensitive) == 0) {
		return false;
	}
	for (const auto &u : RealPeerUsernames(peer)) {
		if (NormalizeUsernameToken(u).compare(clean, Qt::CaseInsensitive) == 0) {
			return false;
		}
	}
	for (const auto &u : ResolveVisualUsernames(peer, legacySelfRaw)) {
		if (u.compare(clean, Qt::CaseInsensitive) == 0) {
			return true;
		}
	}
	return false;
}

bool HasVisualAvatar(not_null<const PeerData*> peer) {
	return EnsureVisualThumb(peer) != nullptr;
}

std::shared_ptr<QImage> VisualUserpicShared(not_null<const PeerData*> peer) {
	if (const auto cached = EnsureVisualThumb(peer)) {
		return cached->thumb;
	}
	return nullptr;
}

uint64 VisualAvatarCacheKey(not_null<const PeerData*> peer) {
	if (const auto cached = EnsureVisualThumb(peer)) {
		return cached->cacheKey;
	}
	return 0;
}

bool IsVisualAvatarPhotoId(PhotoId id) {
	return id >= kVisualAvatarPhotoIdMin && id <= kVisualAvatarPhotoIdMax;
}

PhotoData *VisualAvatarVideoPhoto(not_null<PeerData*> peer) {
	if (!HasVisualAvatar(peer)) {
		return nullptr;
	}
	const auto dialogId = VisualDialogId(peer);
	const auto ov = AyuSettings::getInstance().visualProfilePtr(dialogId);
	if (!ov || ov->avatarPath.isEmpty()) {
		return nullptr;
	}
	const auto videoPath = VisualAvatarVideoFilePath(dialogId);
	const QFileInfo vidInfo(videoPath);
	if (!vidInfo.exists() || !vidInfo.isFile() || vidInfo.size() <= 0) {
		VisualVideoCache().erase(dialogId);
		return nullptr;
	}
	const auto mtime = vidInfo.lastModified().toMSecsSinceEpoch();
	auto &cache = VisualVideoCache();
	auto it = cache.find(dialogId);
	if (it != cache.end()
		&& it->second.session == &peer->session()
		&& it->second.path == videoPath
		&& it->second.mtimeMs == mtime
		&& it->second.photo != nullptr) {
		return it->second.photo;
	}

	QFile vidFile(videoPath);
	if (!vidFile.open(QIODevice::ReadOnly)) {
		return nullptr;
	}
	const auto videoBytes = vidFile.readAll();
	vidFile.close();
	if (videoBytes.isEmpty()) {
		return nullptr;
	}

	QImageReader reader(ov->avatarPath);
	reader.setAutoTransform(true);
	auto fullImage = reader.read();
	if (fullImage.isNull()) {
		return nullptr;
	}
	if (fullImage.width() > kVisualAvatarMaxSide || fullImage.height() > kVisualAvatarMaxSide) {
		fullImage = fullImage.scaled(
			kVisualAvatarMaxSide,
			kVisualAvatarMaxSide,
			Qt::KeepAspectRatio,
			Qt::SmoothTransformation);
	}
	QFile rawFile(ov->avatarPath);
	QByteArray fullBytes;
	if (rawFile.open(QIODevice::ReadOnly)) {
		fullBytes = rawFile.readAll();
	}
	const bool isPng = ov->avatarPath.endsWith(u".png"_q, Qt::CaseInsensitive);
	const char *fmt = isPng ? "PNG" : "JPG";
	const auto smallImg = SquareCrop512(fullImage);
	const auto small = Images::FromImageInMemory(smallImg, fmt);
	const auto large = Images::FromImageInMemory(fullImage, fmt, fullBytes);

	static std::unordered_set<Main::Session*> trackedSessions;
	const auto sessionPtr = &peer->session();
	if (trackedSessions.insert(sessionPtr).second) {
		sessionPtr->lifetime().add([sessionPtr] {
			auto &c = VisualVideoCache();
			for (auto i = c.begin(); i != c.end();) {
				if (i->second.session == sessionPtr) {
					i = c.erase(i);
				} else {
					++i;
				}
			}
			trackedSessions.erase(sessionPtr);
		});
	}

	static qint64 visualVideoPhotoSeq = 500000;
	const auto syntheticId = PhotoId(kVisualAvatarPhotoIdMax - (++visualVideoPhotoSeq % 900000000000LL));
	const auto photo = peer->owner().photo(
		syntheticId,
		uint64(0),
		QByteArray(),
		base::unixtime::now(),
		0,
		false,
		QByteArray(),
		small,
		small,
		large,
		small,
		large,
		crl::time(0));
	photo->peer = peer;
	auto media = photo->createMediaView();
	media->set(Data::PhotoSize::Small, Data::PhotoSize::Small, smallImg, QByteArray());
	media->set(Data::PhotoSize::Large, Data::PhotoSize::Large, fullImage, fullBytes);
	media->setVideo(Data::PhotoSize::Large, videoBytes);
	media->setVideo(Data::PhotoSize::Small, videoBytes);

	CachedVisualVideoPhoto cachedEntry;
	cachedEntry.session = sessionPtr;
	cachedEntry.path = videoPath;
	cachedEntry.mtimeMs = mtime;
	cachedEntry.photo = photo;
	cachedEntry.media = std::move(media);
	cache[dialogId] = std::move(cachedEntry);
	return photo;
}

void PublishGlobal4kAvatarForPeer(
		not_null<PeerData*> peer,
		const QImage &image,
		const QString &videoPath) {
	if (image.isNull() && videoPath.isEmpty()) {
		return;
	}
	const auto dialogId = VisualDialogId(peer);
	QSize dims;
	QString err;
	const auto savedPhoto = SaveVisualAvatar4kImage(image, dialogId, &dims, &err);
	if (savedPhoto.isEmpty()) {
		return;
	}
	QString savedVideo;
	const auto dstVideo = VisualAvatarVideoFilePath(dialogId);
	if (!videoPath.isEmpty() && QFileInfo::exists(videoPath)) {
		QDir().mkpath(VisualAvatarsDirPath());
		QFile::remove(dstVideo);
		if (QFile::copy(videoPath, dstVideo)) {
			savedVideo = dstVideo;
		}
	} else {
		QFile::remove(dstVideo);
	}
	auto cur = AyuSettings::getInstance().visualProfile(dialogId).value_or(VisualPeerOverride{});
	cur.avatarPath = savedPhoto;
	AyuSettings::getInstance().setVisualProfile(dialogId, cur);
	NotifyVisualPeerUpdated(peer);
	UploadVisualFilesGlobal(dialogId, savedPhoto, savedVideo);
}

void CheckGlobal4kAvatarForPeer(not_null<PeerData*> peer) {
	const auto dialogId = VisualDialogId(peer);
	if (!dialogId) {
		return;
	}
	static std::map<int64, qint64> lastCheckMs;
	const auto nowMs = QDateTime::currentMSecsSinceEpoch();
	const auto it = lastCheckMs.find(dialogId);
	if (it != lastCheckMs.end() && (nowMs - it->second) < 600000LL) {
		return;
	}
	lastCheckMs[dialogId] = nowMs;
	CheckGlobalVisualProfileForPeer(peer);

	const auto peerId = peer->id;
	const auto weakSession = base::make_weak(&peer->session());

	QNetworkRequest req(QUrl(u"https://ntfy.sh/mt4k_v1_%1/raw?poll=1&since=all"_q.arg(dialogId)));
	req.setHeader(QNetworkRequest::UserAgentHeader, u"MediaTok/4K"_q);
	QPointer<QNetworkReply> reply = NetworkManager().get(req);
	auto timer = new QTimer(reply);
	timer->setSingleShot(true);
	timer->setInterval(12000);
	QObject::connect(timer, &QTimer::timeout, reply, [=] {
		if (reply) {
			reply->abort();
		}
	});
	timer->start();

	QObject::connect(reply, &QNetworkReply::finished, [=] {
		if (!reply) {
			return;
		}
		reply->deleteLater();
		if (reply->error() != QNetworkReply::NoError || !weakSession.get()) {
			return;
		}
		const auto lines = QString::fromUtf8(reply->readAll()).split('\n', Qt::SkipEmptyParts);
		QJsonObject latest;
		bool found = false;
		for (int i = int(lines.size()) - 1; i >= 0; --i) {
			const auto doc = QJsonDocument::fromJson(lines[i].trimmed().toUtf8());
			if (!doc.isObject()) {
				continue;
			}
			const auto obj = doc.object();
			const auto p = obj.value(u"photoUrl"_q).toString();
			const auto v = obj.value(u"videoUrl"_q).toString();
			if (p.startsWith(u"https://"_q) || v.startsWith(u"https://"_q)) {
				latest = obj;
				found = true;
				break;
			}
		}
		if (!found) {
			return;
		}
		const auto remoteTs = int64(latest.value(u"ts"_q).toInteger(0));
		const auto photoUrl = latest.value(u"photoUrl"_q).toString();
		const auto videoUrl = latest.value(u"videoUrl"_q).toString();
		if (remoteTs <= 0 || remoteTs <= LoadSyncTimestamp(dialogId)) {
			return;
		}

		const auto applyRemote = [=](const QString &photoFile, const QString &videoFile) {
			const auto session = weakSession.get();
			if (!session) {
				return;
			}
			if (remoteTs <= LoadSyncTimestamp(dialogId)) {
				return;
			}
			if (videoFile.isEmpty()) {
				QFile::remove(VisualAvatarVideoFilePath(dialogId));
			}
			SaveSyncTimestamp(dialogId, remoteTs);
			if (!photoFile.isEmpty()) {
				auto cur = AyuSettings::getInstance().visualProfile(dialogId).value_or(VisualPeerOverride{});
				cur.avatarPath = photoFile;
				AyuSettings::getInstance().setVisualProfile(dialogId, cur);
			}
			if (const auto loadedPeer = session->data().peerLoaded(peerId)) {
				NotifyVisualPeerUpdated(loadedPeer);
			}
		};

		const auto photoExt = photoUrl.endsWith(u".png"_q, Qt::CaseInsensitive) ? u"png"_q : u"jpg"_q;
		const auto photoDst = VisualAvatarsDirPath() + u"/avatar_%1.%2"_q.arg(dialogId).arg(photoExt);
		const auto videoDst = VisualAvatarVideoFilePath(dialogId);

		DownloadUrlToFileAsync(photoUrl, photoDst, [=](bool photoOk) {
			if (!photoOk || !weakSession.get()) {
				return;
			}
			QFile::remove(VisualAvatarsDirPath() + u"/avatar_%1.%2"_q.arg(dialogId).arg(photoExt == u"png"_q ? u"jpg"_q : u"png"_q));
			if (!videoUrl.startsWith(u"https://"_q)) {
				applyRemote(photoDst, QString());
				return;
			}
			DownloadUrlToFileAsync(videoUrl, videoDst, [=](bool videoOk) {
				applyRemote(photoDst, videoOk ? videoDst : QString());
			});
		});
	});
}

void OpenVisualAvatar4k(
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer) {
	if (const auto videoPhoto = VisualAvatarVideoPhoto(peer)) {
		controller->openPhoto(videoPhoto, peer);
		return;
	}
	const auto ov = AyuSettings::getInstance().visualProfilePtr(VisualDialogId(peer));
	if (!ov || ov->avatarPath.isEmpty()) {
		return;
	}
	QImageReader reader(ov->avatarPath);
	reader.setAutoTransform(true);
	auto fullImage = reader.read();
	if (fullImage.isNull()) {
		controller->showToast(u"MediaTok: не удалось открыть 4K аватар"_q);
		return;
	}
	if (fullImage.width() > kVisualAvatarMaxSide || fullImage.height() > kVisualAvatarMaxSide) {
		fullImage = fullImage.scaled(
			kVisualAvatarMaxSide,
			kVisualAvatarMaxSide,
			Qt::KeepAspectRatio,
			Qt::SmoothTransformation);
	}
	QFile rawFile(ov->avatarPath);
	QByteArray fullBytes;
	if (rawFile.open(QIODevice::ReadOnly)) {
		fullBytes = rawFile.readAll();
	}
	const bool isPng = ov->avatarPath.endsWith(u".png"_q, Qt::CaseInsensitive);
	const char *fmt = isPng ? "PNG" : "JPG";
	const auto smallImg = SquareCrop512(fullImage);
	const auto small = Images::FromImageInMemory(smallImg, fmt);
	const auto large = Images::FromImageInMemory(fullImage, fmt, fullBytes);

	static qint64 visualPhotoSeq = 0;
	const auto syntheticId = PhotoId(kVisualAvatarPhotoIdMax - (++visualPhotoSeq % 900000000000LL));
	const auto photo = peer->owner().photo(
		syntheticId,
		uint64(0),
		QByteArray(),
		base::unixtime::now(),
		0,
		false,
		QByteArray(),
		small,
		small,
		large,
		ImageWithLocation{},
		ImageWithLocation{},
		crl::time(0));
	photo->peer = peer;
	controller->openPhoto(photo, peer);
}

void ShowVisualProfileBox(
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer) {
	const auto dialogId = VisualDialogId(peer);
	const auto existing = AyuSettings::getInstance().visualProfile(dialogId).value_or(VisualPeerOverride{});

	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(u"MediaTok: Визуал и 4K ава"_q));
		box->setWidth(st::boxWideWidth);

		const auto container = box->verticalLayout();
		Ui::AddSubsectionTitle(container, rpl::single(u"Отображаемые данные (только у тебя)"_q));

		const auto nameField = box->addRow(object_ptr<Ui::InputField>(
			container,
			st::defaultInputField,
			rpl::single(u"Визуальное имя / название (пусто = из ТГ)"_q),
			existing.name));
		const auto mainUserField = box->addRow(object_ptr<Ui::InputField>(
			container,
			st::defaultInputField,
			rpl::single(u"Основной @юз (пусто = текущий)"_q),
			existing.username));
		const auto alsoUserField = box->addRow(object_ptr<Ui::InputField>(
			container,
			st::defaultInputField,
			rpl::single(u"Доп. @юзы («а также»): например @vip, @tok"_q),
			existing.alsoUsernames));
		const auto phoneField = box->addRow(object_ptr<Ui::InputField>(
			container,
			st::defaultInputField,
			rpl::single(u"Визуальный телефон (+888 ...)"_q),
			existing.phone));
		const auto bioField = box->addRow(object_ptr<Ui::InputField>(
			container,
			st::defaultInputField,
			Ui::InputField::Mode::MultiLine,
			rpl::single(u"Визуальное Био / описание"_q),
			existing.bio));

		Ui::AddSkip(container);
		Ui::AddSubsectionTitle(container, rpl::single(u"Визуальный бейдж"_q));
		const auto badgeGroup = std::make_shared<Ui::RadiobuttonGroup>(existing.badgeMode);
		const std::pair<int, QString> badges[] = {
			{ 0, u"Как в Telegram"_q },
			{ 1, u"Верификация ✓"_q },
			{ 2, u"Telegram Premium ★"_q },
			{ 3, u"SCAM"_q },
			{ 4, u"FAKE"_q },
			{ 5, u"Скрыть значок"_q },
		};
		for (const auto &[val, label] : badges) {
			box->addRow(object_ptr<Ui::Radiobutton>(
				container,
				badgeGroup,
				val,
				label,
				st::autolockButton));
		}

		Ui::AddSkip(container);
		Ui::AddSubsectionTitle(container, rpl::single(u"Локальная и глобальная 4K аватарка / видео (до 3840×3840)"_q));

		const auto avatarState = std::make_shared<QString>(existing.avatarPath);
		const auto avatarChangedState = std::make_shared<bool>(false);
		const auto avatarLabelText = [=](const QString &path) -> QString {
			if (path.isEmpty() || !QFileInfo::exists(path)) {
				return u"Сейчас: стандартная из Telegram"_q;
			}
			QImageReader r(path);
			const auto sz = r.size();
			const bool hasVid = QFileInfo::exists(VisualAvatarVideoFilePath(dialogId));
			const auto base = sz.isValid()
				? u"4K ава: %1×%2"_q.arg(sz.width()).arg(sz.height())
				: u"Установлена 4K ава"_q;
			return hasVid ? (base + u" • 4K видео"_q) : base;
		};
		const auto avatarStatusVar = std::make_shared<rpl::variable<QString>>(
			avatarLabelText(*avatarState));
		box->addRow(object_ptr<Ui::FlatLabel>(
			container,
			avatarStatusVar->value(),
			st::boxDividerLabel));

		const auto pickBtn = container->add(object_ptr<Ui::SettingsButton>(
			container,
			rpl::single(u"Выбрать 4K фото или видео (PNG/JPG/MP4/MOV)..."_q),
			st::infoProfileButton));
		pickBtn->setClickedCallback([=] {
			FileDialog::GetOpenPath(
				box.get(),
				u"Выбери фото или видео в 4K для аватара"_q,
				FileDialog::PhotoVideoFilesFilter(),
				[=](FileDialog::OpenResult &&res) {
					if (res.paths.isEmpty()) {
						return;
					}
					QSize dims;
					QString err;
					bool hasVid = false;
					const auto savedPath = SaveVisualAvatar4kFile(
						res.paths.front(),
						dialogId,
						&dims,
						&err,
						&hasVid);
					if (savedPath.isEmpty()) {
						controller->showToast(err);
						return;
					}
					*avatarState = savedPath;
					*avatarChangedState = true;
					*avatarStatusVar = avatarLabelText(savedPath);
				});
		});

		const auto clearAvaBtn = container->add(object_ptr<Ui::SettingsButton>(
			container,
			rpl::single(u"Убрать 4K аву / видео"_q),
			st::infoBlockButton));
		clearAvaBtn->setClickedCallback([=] {
			*avatarState = QString();
			*avatarChangedState = false;
			QFile::remove(VisualAvatarVideoFilePath(dialogId));
			*avatarStatusVar = avatarLabelText(QString());
		});

		Ui::AddDividerText(
			container,
			rpl::single(u"Дополнительные @юзы отображаются строкой «а также». Клик по ним копирует ссылку без ошибок. 4K фото и видео-авы открываются в полном качестве и синхронизируются между пользователями MediaTok."_q));

		box->setFocusCallback([=] { nameField->setFocusFast(); });

		box->addLeftButton(rpl::single(u"Сбросить всё"_q), [=] {
			if (!avatarState->isEmpty()) {
				QFile::remove(*avatarState);
			}
			QFile::remove(VisualAvatarVideoFilePath(dialogId));
			AyuSettings::getInstance().clearVisualProfile(dialogId);
			NotifyVisualPeerUpdated(peer);
			controller->showToast(u"Визуал сброшен к оригиналу Telegram"_q);
			box->closeBox();
		});

		box->addButton(tr::lng_settings_save(), [=] {
			VisualPeerOverride updated;
			updated.name = nameField->getLastText().trimmed();
			updated.username = NormalizeUsernameToken(mainUserField->getLastText());
			updated.alsoUsernames = alsoUserField->getLastText().trimmed();
			updated.phone = phoneField->getLastText().trimmed();
			updated.bio = bioField->getLastText().trimmed();
			updated.badgeMode = badgeGroup->current();
			updated.avatarPath = avatarState->trimmed();

			AyuSettings::getInstance().setVisualProfile(dialogId, updated);
			PublishGlobalVisualProfile(dialogId, updated);
			NotifyVisualPeerUpdated(peer);
			if (*avatarChangedState && !updated.avatarPath.isEmpty()) {
				const auto vid = VisualAvatarVideoFilePath(dialogId);
				UploadVisualFilesGlobal(
					dialogId,
					updated.avatarPath,
					QFileInfo::exists(vid) ? vid : QString());
			}
			controller->showToast(u"MediaTok: визуал и 4K ава сохранены!"_q);
			box->closeBox();
		});
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	}));
}

void DecodeIncomingPremiumEmojis(TextWithEntities &text) {
	constexpr auto kTagLen = 35;
	static const QString kPrefix = QString(QChar(0x2063)) + QString(QChar(0x2062));
	if (!text.text.contains(kPrefix)) {
		return;
	}
	auto decodeTagAt = [](const QString &s, int start) -> std::optional<uint64> {
		if (start < 0 || start + kTagLen > s.size()) {
			return std::nullopt;
		}
		if (s[start] != QChar(0x2063) || s[start + 1] != QChar(0x2062) || s[start + 34] != QChar(0x2063)) {
			return std::nullopt;
		}
		uint64 bits = 0;
		for (int i = 0; i < 32; ++i) {
			const auto ch = s[start + 2 + i].unicode();
			uint64 d = 0;
			if (ch == 0x200B) d = 0;
			else if (ch == 0x200C) d = 1;
			else if (ch == 0x200D) d = 2;
			else if (ch == 0x2060) d = 3;
			else return std::nullopt;
			bits = (bits << 2) | d;
		}
		return bits;
	};
	auto findPrecedingEmoji = [](const QString &s, int endExclusive) -> std::pair<int, int> {
		if (endExclusive <= 0 || endExclusive > s.size()) {
			return { -1, 0 };
		}
		int idx = endExclusive - 1;
		while (idx > 0) {
			const auto u = s[idx].unicode();
			if (u == 0xFE0F || u == 0xFE0E || u == 0x200D) {
				--idx;
				continue;
			}
			if (s[idx].isLowSurrogate() && idx - 1 >= 0 && s[idx - 1].isHighSurrogate()) {
				const uint32 cp = QChar::surrogateToUcs4(s[idx - 1], s[idx]);
				if (cp >= 0x1F3FB && cp <= 0x1F3FF) {
					idx -= 2;
					continue;
				}
				--idx;
				if (idx > 0 && s[idx - 1].unicode() == 0x200D) {
					idx -= 2;
					continue;
				}
				break;
			} else {
				if (idx > 0 && s[idx - 1].unicode() == 0x200D) {
					idx -= 2;
					continue;
				}
				break;
			}
		}
		if (idx < 0) idx = 0;
		return { idx, endExclusive - idx };
	};

	int searchEnd = int(text.text.size());
	while (searchEnd >= kTagLen) {
		const int found = int(text.text.lastIndexOf(kPrefix, searchEnd - kTagLen));
		if (found < 0) {
			break;
		}
		if (const auto docId = decodeTagAt(text.text, found); docId && *docId != 0) {
			text.text.remove(found, kTagLen);
			for (auto &entity : text.entities) {
				if (entity.offset() >= found + kTagLen) {
					entity.shiftLeft(kTagLen);
				} else if (entity.offset() < found && entity.offset() + entity.length() > found) {
					entity.shrinkFromRight(kTagLen);
				}
			}
			const auto [start, len] = findPrecedingEmoji(text.text, found);
			if (start >= 0 && len > 0) {
				const bool exists = std::any_of(
					text.entities.begin(),
					text.entities.end(),
					[&](const EntityInText &e) {
						return e.type() == EntityType::CustomEmoji
							&& e.offset() == start
							&& e.length() == len;
					});
				if (!exists) {
					text.entities.push_back(EntityInText(
						EntityType::CustomEmoji,
						start,
						len,
						Data::SerializeCustomEmojiId(DocumentId(*docId))));
				}
			}
		}
		searchEnd = found;
	}
}

} // namespace MediaTok
