// This is part of MediaTok (AyuGramPlus fork).
#pragma once

#include "api/api_common.h"

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
	Fn<void(QString answer, QString error)> callback);

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

[[nodiscard]] QString VoicemodPresetName(int preset);

} // namespace MediaTok
