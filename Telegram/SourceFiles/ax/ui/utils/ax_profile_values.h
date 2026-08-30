// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#pragma once

QString IDString(not_null<PeerData*> peer);
QString IDString(MsgId topicRootId);

rpl::producer<TextWithEntities> IDValue(not_null<PeerData*> peer);
rpl::producer<TextWithEntities> IDValue(MsgId topicRootId);