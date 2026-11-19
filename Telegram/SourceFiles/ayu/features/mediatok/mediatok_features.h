// This is part of MediaTok (AyuGramPlus fork).
#pragma once

#include "api/api_common.h"
#include "base/basic_types.h"
#include "data/data_types.h"
#include "dialogs/dialogs_key.h"

#include <QtCore/QByteArray>
#include <QtCore/QSize>
#include <QtCore/QString>

#include <memory>
#include <optional>
#include <vector>

class HistoryItem;
class PeerData;
class QImage;

namespace Main {
class Session;
} // namespace Main

namespace Window {
class SessionController;
} // namespace Window

namespace MediaTok {

void AskGemini(
	not_null<Main::Session*> session,
	const QString &prompt,
	Fn<void(QString answer, QString error)> callback,
	const QByteArray &mediaBytes = {},
	const QString &mimeType = u"image/jpeg"_q);

void TestGeminiApi(not_null<Window::SessionController*> controller);

void FetchGeminiModels(
	not_null<Window::SessionController*> controller,
	Fn<void(std::vector<QString> models, QString error)> callback);

void AskGeminiAboutItem(
	not_null<Window::SessionController*> controller,
	not_null<HistoryItem*> item);

[[nodiscard]] bool CanTranscribeMedia(HistoryItem *item);
void TranscribeMediaWithGemini(
	not_null<Window::SessionController*> controller,
	not_null<HistoryItem*> item);

void ShowHelpBox(not_null<Window::SessionController*> controller);

void DeleteMyMessagesInPeer(
	not_null<PeerData*> peer,
	Window::SessionController *controller = nullptr,
	Fn<void(int deletedCount)> onDone = nullptr,
	int maxCount = 0,
	TimeId sinceDate = 0);

void DeleteMyMessagesEverywhere(
	not_null<Window::SessionController*> controller);

void WipeIncomingPeerHistory(not_null<PeerData*> peer);

void CheckAutoDeleteIncoming(not_null<HistoryItem*> item);
void CheckAutoDeleteOutgoing(not_null<HistoryItem*> item);

[[nodiscard]] std::optional<int> ParseDurationSeconds(const QString &raw);
[[nodiscard]] QString FormatDurationShort(int seconds);

[[nodiscard]] bool HandleOutgoingText(
	not_null<Window::SessionController*> controller,
	Api::MessageToSend &message);

[[nodiscard]] bool CanResendAsCircle(HistoryItem *item);

void ResendItemAsCircle(
	not_null<Window::SessionController*> controller,
	not_null<HistoryItem*> item);

void ResendItemAsGif(
	not_null<Window::SessionController*> controller,
	not_null<HistoryItem*> item);

[[nodiscard]] bool HasYtDlpTool();
[[nodiscard]] bool HasFfmpegTool();
void DownloadVideoTools(
	not_null<Window::SessionController*> controller,
	Fn<void()> onUpdated = nullptr);

[[nodiscard]] QString VoicemodPresetName(int preset);
constexpr int kVoicemodPresetCount = 8;

[[nodiscard]] bool CanSearchUserMessagesInChat(
	const Dialogs::Key &chat,
	not_null<PeerData*> targetUser);

void SearchUserMessagesInChat(
	not_null<Window::SessionController*> controller,
	const Dialogs::Key &chat,
	not_null<PeerData*> targetUser);

[[nodiscard]] int64 VisualDialogId(not_null<const PeerData*> peer);
[[nodiscard]] const QString *VisualNamePtr(not_null<const PeerData*> peer);
[[nodiscard]] int VisualBadgeMode(not_null<const PeerData*> peer);
[[nodiscard]] std::vector<QString> ResolveVisualUsernames(
	not_null<const PeerData*> peer,
	const QString &legacySelfRaw);
[[nodiscard]] bool IsVisualOnlyUsername(
	not_null<const PeerData*> peer,
	const QString &username,
	const QString &legacySelfRaw);
[[nodiscard]] bool HasVisualAvatar(not_null<const PeerData*> peer);
[[nodiscard]] std::shared_ptr<QImage> VisualUserpicShared(
	not_null<const PeerData*> peer);
[[nodiscard]] uint64 VisualAvatarCacheKey(not_null<const PeerData*> peer);
[[nodiscard]] bool IsVisualAvatarPhotoId(PhotoId id);
[[nodiscard]] PhotoData *VisualAvatarVideoPhoto(not_null<PeerData*> peer);
void CheckGlobal4kAvatarForPeer(not_null<PeerData*> peer);
void PublishGlobal4kAvatarForPeer(
	not_null<PeerData*> peer,
	const QImage &image,
	const QString &videoPath = QString());
void OpenVisualAvatar4k(
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer);
void ShowVisualProfileBox(
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer);

} // namespace MediaTok
