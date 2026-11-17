// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/data/messages_storage.h"

#include "ayu/ayu_settings.h"
#include "ayu/data/ayu_database.h"
#include "ayu/utils/ayu_mapper.h"
#include "ayu/utils/telegram_helpers.h"
#include "base/unixtime.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_forum_topic.h"
#include "data/data_media_types.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "main/main_session.h"
#include "ui/image/image.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

namespace AyuMessages {

namespace {

QString resolveMediaExtension(DocumentData *document) {
	if (!document) {
		return u".jpg"_q;
	}
	const auto filename = document->filename();
	if (!filename.isEmpty()) {
		const auto suffix = QFileInfo(filename).suffix();
		if (!suffix.isEmpty()) {
			return u"."_q + suffix;
		}
	}
	if (document->isVoiceMessage()) {
		return u".ogg"_q;
	} else if (document->isVideoMessage() || document->isVideoFile() || document->isAnimation()) {
		return u".mp4"_q;
	} else if (document->isAudioFile() || document->isSong()) {
		return u".mp3"_q;
	}
	return u".bin"_q;
}

void ensureSavedMediaDirMigrated() {
	static bool checked = false;
	if (checked) {
		return;
	}
	checked = true;
	const auto legacyPath = u"./tdata/AyuGramPlus_SavedMedia"_q;
	const auto modernPath = u"./tdata/MediaTok_SavedMedia"_q;
	if (!QDir(modernPath).exists() && QDir(legacyPath).exists()) {
		QDir().rename(legacyPath, modernPath);
	}
}

QString buildSavedMediaPath(not_null<HistoryItem*> item, bool isTtl) {
	ensureSavedMediaDirMigrated();
	const auto dialogId = getDialogIdFromPeer(item->history()->peer);
	const auto folder = isTtl
		? u"./tdata/MediaTok_SavedMedia/ttl/chat_%1"_q.arg(dialogId)
		: u"./tdata/MediaTok_SavedMedia/deleted/chat_%1"_q.arg(dialogId);
	QDir().mkpath(folder);

	const auto media = item->media();
	const auto ext = (media && media->document())
		? resolveMediaExtension(media->document())
		: u".jpg"_q;

	const auto fileName = u"msg_%1_%2%3"_q
		.arg(item->id.bare)
		.arg(item->date())
		.arg(ext);
	return QDir(folder).filePath(fileName);
}

} // namespace

QString saveItemMediaToDisk(not_null<HistoryItem*> item, bool isTtl) {
	const auto media = item->media();
	if (!media) {
		return QString();
	}

	const auto hasTtl = (media->ttlSeconds() > 0);
	const auto targetPath = buildSavedMediaPath(item, isTtl || hasTtl);
	const auto absPath = QFileInfo(targetPath).absoluteFilePath();
	if (QFileInfo::exists(absPath) && QFileInfo(absPath).size() > 0) {
		return absPath;
	}

	const auto origin = Data::FileOrigin(item->fullId());

	if (const auto photo = media->photo()) {
		auto photoMedia = photo->activeMediaView()
			? photo->activeMediaView()
			: photo->createMediaView();
		if (photoMedia->saveToFile(absPath)) {
			return absPath;
		}
		if (const auto smallImg = photoMedia->image(Data::PhotoSize::Small)) {
			if (!smallImg->original().isNull() && !isTtl) {
				if (smallImg->original().save(absPath, "JPG")) {
					return absPath;
				}
			}
		}
		if (const auto thumbImg = photoMedia->image(Data::PhotoSize::Thumbnail)) {
			if (!thumbImg->original().isNull() && !isTtl) {
				if (thumbImg->original().save(absPath, "JPG")) {
					return absPath;
				}
			}
		}
		photo->load(Data::PhotoSize::Large, origin, LoadFromCloudOrLocal, true);
		if (photo->hasVideo()) {
			photo->loadVideo(Data::PhotoSize::Large, origin);
		}
		auto lifetime = std::make_shared<rpl::lifetime>();
		photo->session().downloaderTaskFinished(
		) | rpl::on_next([=]() mutable {
			if (photoMedia->loaded()) {
				photoMedia->saveToFile(absPath);
				if (lifetime) {
					base::take(lifetime)->destroy();
				}
			} else if (photo->failed(Data::PhotoSize::Large) || photo->cancelled()) {
				if (lifetime) {
					base::take(lifetime)->destroy();
				}
			}
		}, *lifetime);
		return absPath;
	}

	if (const auto document = media->document()) {
		const auto existingPath = document->filepath(true);
		if (!existingPath.isEmpty() && QFileInfo::exists(existingPath)) {
			QFile::remove(absPath);
			if (QFile::copy(existingPath, absPath)) {
				return absPath;
			}
		}
		auto docMedia = document->activeMediaView()
			? document->activeMediaView()
			: document->createMediaView();
		if (!docMedia->bytes().isEmpty()) {
			QFile f(absPath);
			if (f.open(QIODevice::WriteOnly)) {
				f.write(docMedia->bytes());
				f.close();
				return absPath;
			}
		}
		document->save(origin, absPath, LoadFromCloudOrLocal, true);
		auto lifetime = std::make_shared<rpl::lifetime>();
		document->session().downloaderTaskFinished(
		) | rpl::on_next([=]() mutable {
			if (QFileInfo::exists(absPath) && QFileInfo(absPath).size() > 0) {
				if (lifetime) {
					base::take(lifetime)->destroy();
				}
				return;
			}
			if (docMedia->loaded(true) && !docMedia->bytes().isEmpty()) {
				QFile f(absPath);
				if (f.open(QIODevice::WriteOnly)) {
					f.write(docMedia->bytes());
					f.close();
				}
				if (lifetime) {
					base::take(lifetime)->destroy();
				}
			} else if (document->cancelled()) {
				if (lifetime) {
					base::take(lifetime)->destroy();
				}
			}
		}, *lifetime);
		return absPath;
	}

	return QString();
}

void autoSaveTtlMedia(not_null<HistoryItem*> item) {
	if (!AyuSettings::getInstance().saveDeletedMessages()) {
		return;
	}
	const auto media = item->media();
	if (!media || media->ttlSeconds() <= 0) {
		return;
	}
	saveItemMediaToDisk(item, true);
}

template<typename DerivedMessage>
std::vector<AyuMessageBase> convertToBase(const std::vector<DerivedMessage> &messages) {
	std::vector<AyuMessageBase> based;
	based.reserve(messages.size());
	for (const auto &msg : messages) {
		based.push_back(static_cast<AyuMessageBase>(msg));
	}
	return based;
}

void map(not_null<HistoryItem*> item, AyuMessageBase &message) {
	const ID userId = item->history()->owner().session().userId().bare & PeerId::kChatTypeMask;

	message.userId = userId;
	message.dialogId = getDialogIdFromPeer(item->history()->peer);
	message.groupedId = item->groupId().raw();
	message.peerId = item->history()->peer->id.value & PeerId::kChatTypeMask;
	message.fromId = item->from()->id.value & PeerId::kChatTypeMask;
	if (item->topic()) {
		message.topicId = item->topicRootId().bare;
	} else {
		message.topicId = 0;
	}
	message.messageId = item->id.bare;
	message.date = item->date();
	message.flags = AyuMapper::mapItemFlagsToMTPFlags(item);

	if (const auto edited = item->Get<HistoryMessageEdited>()) {
		message.editDate = edited->date;
	} else {
		message.editDate = base::unixtime::now();
	}

	message.views = item->viewsCount();
	message.fwdFlags = 0;
	message.fwdFromId = 0;
	// message.fwdName
	message.fwdDate = 0;
	// message.fwdPostAuthor
	if (const auto msgsigned = item->Get<HistoryMessageSigned>()) {
		message.postAuthor = msgsigned->author.toStdString();
	}
	message.replyFlags = 0;
	message.replyMessageId = 0;
	message.replyPeerId = 0;
	message.replyTopId = 0;
	message.replyForumTopic = false;
	// message.replySerialized
	// message.replyMarkupSerialized
	message.entityCreateDate = base::unixtime::now();

	auto serializedText = AyuMapper::serializeTextWithEntities(item);
	message.text = serializedText.first;
	message.textEntities = serializedText.second;

	const auto savedPath = saveItemMediaToDisk(item, false);
	if (!savedPath.isEmpty()) {
		message.mediaPath = savedPath.toStdString();
		if (const auto media = item->media()) {
			if (const auto doc = media->document()) {
				message.documentType = static_cast<int>(doc->type);
				message.mimeType = doc->mimeString().toStdString();
			} else if (media->photo()) {
				message.documentType = 1;
				message.mimeType = "image/jpeg";
			}
		}
	} else {
		message.mediaPath = "/";
		message.documentType = 0;
	}
}

void addEditedMessage(not_null<HistoryItem *> item) {
	EditedMessage message;
	map(item, message);

	if (message.text.empty()) {
		return;
	}

	AyuDatabase::addEditedMessage(message);
}

std::vector<AyuMessageBase> getEditedMessages(not_null<HistoryItem*> item, ID minId, ID maxId, int totalLimit) {
	const ID userId = item->history()->owner().session().userId().bare & PeerId::kChatTypeMask;
	const auto dialogId = getDialogIdFromPeer(item->history()->peer);
	const auto msgId = item->id.bare;

	return convertToBase(AyuDatabase::getEditedMessages(userId, dialogId, msgId, minId, maxId, totalLimit));
}

bool hasRevisions(not_null<HistoryItem*> item) {
	const ID userId = item->history()->owner().session().userId().bare & PeerId::kChatTypeMask;
	const auto dialogId = getDialogIdFromPeer(item->history()->peer);
	const auto msgId = item->id.bare;

	return AyuDatabase::hasRevisions(userId, dialogId, msgId);
}

void addDeletedMessage(not_null<HistoryItem*> item) {
	DeletedMessage message;
	map(item, message);

	if (message.text.empty() && (message.mediaPath.empty() || message.mediaPath == "/")) {
		return;
	}

	AyuDatabase::addDeletedMessage(message);
}

std::vector<AyuMessageBase>
getDeletedMessages(not_null<PeerData*> peer, ID topicId, ID minId, ID maxId, int totalLimit, const QString &searchQuery) {
	const ID userId = peer->session().userId().bare & PeerId::kChatTypeMask;
	return convertToBase(
		AyuDatabase::getDeletedMessages(userId, getDialogIdFromPeer(peer), topicId, minId, maxId, totalLimit, searchQuery.toStdString()));
}

bool hasDeletedMessages(not_null<PeerData*> peer, ID topicId) {
	const ID userId = peer->session().userId().bare & PeerId::kChatTypeMask;
	return AyuDatabase::hasDeletedMessages(userId, getDialogIdFromPeer(peer), topicId);
}

void removeDeletedMessage(not_null<HistoryItem*> item) {
	const auto peer = item->history()->peer;
	const ID userId = peer->session().userId().bare & PeerId::kChatTypeMask;
	AyuDatabase::removeDeletedMessage(userId, getDialogIdFromPeer(peer), item->id.bare);
}

void clearDeletedMessages(not_null<PeerData*> peer, ID topicId) {
	const ID userId = peer->session().userId().bare & PeerId::kChatTypeMask;
	AyuDatabase::clearDeletedMessages(userId, getDialogIdFromPeer(peer), topicId);
}

}
