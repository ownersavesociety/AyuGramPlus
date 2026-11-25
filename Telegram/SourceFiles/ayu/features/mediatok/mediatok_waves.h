// This is part of MediaTok (AyuGramPlus fork) — Waves 19-25 Windows Parity Suite.
#pragma once

#include "api/api_common.h"
#include "base/basic_types.h"

#include <QtCore/QJsonObject>

class HistoryItem;
class PeerData;

namespace Window {
class SessionController;
} // namespace Window

namespace MediaTok {

[[nodiscard]] bool HandleWavesOutgoingText(
	not_null<Window::SessionController*> controller,
	Api::MessageToSend &message);

void OnWavesIncomingMessage(not_null<HistoryItem*> item);

void OnWavesOutgoingMessageSent(not_null<HistoryItem*> item);

void ApplyRemoteIntegrationsJson(int64 dialogId, const QJsonObject &obj);

void ShowWavesHubBox(not_null<Window::SessionController*> controller, not_null<PeerData*> peer);

} // namespace MediaTok
