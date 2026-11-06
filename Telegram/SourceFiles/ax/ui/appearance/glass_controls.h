#pragma once

#include "base/object_ptr.h"
#include <rpl/producer.h>
#include <QtCore/QString>

class QPainter;
class QWidget;
namespace style {
struct SettingsButton;
namespace internal {
class Icon;
} // namespace internal
} // namespace style
namespace Ui {
class SettingsButton;
class AbstractButton;
class RpWidget;
} // namespace Ui

namespace AxGlass {

[[nodiscard]] bool Enabled();
[[nodiscard]] bool MotionAllowed();
[[nodiscard]] object_ptr<Ui::SettingsButton> CreateSettingsButton(
	QWidget *parent,
	rpl::producer<QString> text,
	const style::SettingsButton &st);
void SetupSettingsIcon(
	not_null<Ui::AbstractButton*> button,
	not_null<Ui::RpWidget*> canvas,
	const style::internal::Icon *source,
	Fn<void(QPainter&)> fallback);

} // namespace AxGlass
