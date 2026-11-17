// This is part of MediaTok (AyuGramPlus fork).
#pragma once

#include "api/api_common.h"
#include "dialogs/dialogs_key.h"

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <vector>

class HistoryItem;
class PeerData;

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
	const QByteArray &imageJpegBytes = {});

void TestGeminiApi(not_null<Window::SessionController*> controller);

void FetchGeminiModels(
	not_null<Window::SessionController*> controller,
	Fn<void(std::vector<QString> models, QString error)> callback);

void AskGeminiAboutItem(
	not_null<Window::SessionController*> controller,
	not_null<HistoryItem*> item);

void DeleteMyMessagesInPeer(
	not_null<PeerData*> peer,
	Window::SessionController *controller = nullptr,
	Fn<void(int deletedCount)> onDone = nullptr);

void DeleteMyMessagesEverywhere(
	not_null<Window::SessionController*> controller);

void WipeIncomingPeerHistory(not_null<PeerData*> peer);

void CheckAutoDeleteIncoming(not_null<HistoryItem*> item);

[[nodiscard]] bool HandleOutgoingText(
	not_null<Window::SessionController*> controller,
	const Api::MessageToSend &message);

[[nodiscard]] bool CanResendAsCircle(HistoryItem *item);

void ResendItemAsCircle(
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

} // namespace MediaTok
