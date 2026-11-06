// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#pragma once

#include "ax/data/entities.h"

class History;

namespace HistoryView {
class ElementDelegate;
class Element;
} // namespace HistoryView

namespace MessageHistory {

class OwnedItem;

void GenerateItems(
	not_null<HistoryView::ElementDelegate*> delegate,
	not_null<History*> history,
	AxMessageBase message,
	Fn<void(OwnedItem item, TimeId sentDate, MsgId)> callback);

// Smart pointer wrapper for HistoryItem* that destroys the owned item.
class OwnedItem
{
public:
	OwnedItem(std::nullptr_t = nullptr);
	OwnedItem(
		not_null<HistoryView::ElementDelegate*> delegate,
		not_null<HistoryItem*> data,
		AxMessageBase archive);
	OwnedItem(const OwnedItem &other) = delete;
	OwnedItem &operator=(const OwnedItem &other) = delete;
	OwnedItem(OwnedItem &&other);
	OwnedItem &operator=(OwnedItem &&other);
	~OwnedItem();

	[[nodiscard]] HistoryView::Element *get() const {
		return _view.get();
	}

	[[nodiscard]] HistoryView::Element *operator->() const {
		return get();
	}

	[[nodiscard]] operator HistoryView::Element*() const {
		return get();
	}

	void refreshView(not_null<HistoryView::ElementDelegate*> delegate);
	void clearView();
	[[nodiscard]] const AxMessageBase &archive() const { return _archive; }

private:
	HistoryItem *_data = nullptr;
	std::unique_ptr<HistoryView::Element> _view;
	AxMessageBase _archive;

};

} // namespace MessageHistory
