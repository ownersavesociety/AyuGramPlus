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

[[nodiscard]] QJsonObject LoadMergedSyncProfilePayload(int64 dialogId);

void SaveRemoteSyncProfileCache(int64 dialogId, const QJsonObject &obj);

void SignSyncPayloadV2(int64 dialogId, QJsonObject &payload);

[[nodiscard]] bool VerifySyncPayloadV2(int64 dialogId, const QJsonObject &payload);

void PublishMergedProfileToCloudAsync(
	int64 dialogId,
	const QJsonObject &localDelta,
	const QByteArray &userAgent);

void ShowWavesHubBox(not_null<Window::SessionController*> controller, PeerData *peer = nullptr);

void ShowOnlineUpscalerBox(not_null<Window::SessionController*> controller);

void ShowAfkConfigBox(not_null<Window::SessionController*> controller);

void ShowCommandPaletteBox(not_null<Window::SessionController*> controller, PeerData *peer = nullptr);

void ShowIntegrationsManagerBox(not_null<Window::SessionController*> controller, PeerData *peer = nullptr);

void ShowVisualGiftSenderBox(not_null<Window::SessionController*> controller, PeerData *peer = nullptr);

} // namespace MediaTok
