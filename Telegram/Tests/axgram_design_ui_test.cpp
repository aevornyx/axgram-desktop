// AxGram isolated offscreen UI regressions. GPLv3.
#include "base/basic_types.h"
#include "../SourceFiles/ax/ui/appearance/glass_accent.h"
#include <QtCore/QRegularExpression>
#include "../SourceFiles/intro/intro_brand.h"
#include "../SourceFiles/intro/intro_glass_button.h"
#include "ax/ax_ui_settings.h"
#include "../SourceFiles/ax/ui/appearance/glass_defaults.h"
#include "../SourceFiles/ax/ui/appearance/glass_icon_animation.h"
#include "../SourceFiles/ax/ui/appearance/glass_icon_palette.h"
#include <QtCore/QTemporaryDir>
#include "base/integration.h"
#include <rpl/event_stream.h>
#include "ui/integration.h"
#include "ui/style/style_core.h"
#include "ui/style/style_core_font.h"
#include "ui/effects/animations.h"
#include "ui/widgets/menu/menu.h"
#include "ui/widgets/menu/menu_item_base.h"
#include "ui/widgets/fields/input_field.h"
#include "lottie/lottie_icon.h"
#include "styles/style_widgets.h"
#include "styles/palette.h"
#include "crl/crl_on_main.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QTextEdit>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTimer>
#include <QtCore/QPointer>
#include <QtCore/QThread>
#include <QtCore/QPluginLoader>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtPlugin>
#include <QtCore/QElapsedTimer>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtGui/QTextCursor>
#include <cstdio>
#include <cstdlib>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif
#ifdef AXGRAM_TEST_STATIC_OFFSCREEN
Q_IMPORT_PLUGIN(QOffscreenIntegrationPlugin)
#endif

namespace {

rpl::event_stream<> UpdateRequests;

class TestApplication final : public QApplication {
public:
	using QApplication::QApplication;
	bool notify(QObject *receiver, QEvent *event) override {
		if (QThread::currentThread() == thread() && event->type() == QEvent::UpdateRequest) {
			const auto weak = QPointer<QObject>(receiver);
			UpdateRequests.fire({});
			if (!weak) {
				return true;
			}
		}
		return QApplication::notify(receiver, event);
	}

};

class BaseIntegration final : public base::Integration {
public:
	using base::Integration::Integration;
	void enterFromEventLoop(FnMut<void()> &&method) override { method(); }
	bool logSkipDebug() override { return true; }
	void logMessageDebug(const QString&) override {}
	void logMessage(const QString &message) override {
		fprintf(stderr, "%s\n", message.toUtf8().constData());
	}
	void logAssertionViolation(const QString &message) override {
		fprintf(stderr, "ASSERT: %s\n", message.toUtf8().constData());
		std::exit(1);
	}

};

QString Root;
int Checks = 0;
void Check(bool value, const char *message) {
	++Checks;
	if (!value) {
		fprintf(stderr, "FAILED: %s\n", message);
		fflush(stderr);
		std::exit(1);
	}
}

class Integration final : public Ui::Integration {
public:
	void postponeCall(FnMut<void()> &&callback) override {
		const auto owned = std::make_shared<FnMut<void()>>(std::move(callback));
		QTimer::singleShot(0, QCoreApplication::instance(), [=] { (*owned)(); });
	}
	void registerLeaveSubscription(not_null<QWidget*>) override {}
	void unregisterLeaveSubscription(not_null<QWidget*>) override {}
	QString emojiCacheFolder() override { return Root + "/ui-cache"; }
	QString openglCheckFilePath() override { return Root + "/ui-opengl"; }
	QString angleBackendFilePath() override { return Root + "/ui-angle"; }
	void touchCounterIncrement() override { ++_touches; }
	int touchCounterNow() override { return _touches; }

private:
	int _touches = 0;

};

void Pump(int ms = 20) {
	QElapsedTimer time;
	time.start();
	do {
		QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
	} while (time.elapsed() < ms);
}

void Key(Ui::Menu::Menu &menu, int key) {
	auto e = QKeyEvent(QEvent::KeyPress, key, Qt::NoModifier);
	menu.handleKeyPress(&e);
	Pump();
}

void MenuRegression(bool rounded, bool animated) {
	auto parent = QWidget();
	auto menu = Ui::Menu::Menu(&parent);
	menu.setItemSelectionGeometry(
		rounded ? 9 : 0, rounded ? QMargins(6, 2, 6, 2) : QMargins(), animated);
	menu.setForceWidth(260);
	auto triggered = 0;
	auto first = menu.addAction("First", [&] { triggered += 1; });
	auto disabled = menu.addAction("Disabled", [&] { triggered += 100; });
	disabled->setEnabled(false);
	menu.addSeparator();
	auto last = menu.addAction("Last", [&] { triggered += 10; });
	menu.setTriggeredCallback([&](const Ui::Menu::CallbackData &data) {
		data.action->trigger();
	});
	parent.resize(300, 250);
	parent.show();
	menu.show();
	Pump();
	menu.clearSelection();
	Key(menu, Qt::Key_Down);
	Check(menu.findSelectedAction() && menu.findSelectedAction()->action().get() == first.get(),
		"Down selects first item");
	Key(menu, Qt::Key_Down);
	Check(menu.findSelectedAction()->action().get() == last.get(),
		"Down skips disabled item and separator");
	Key(menu, Qt::Key_Up);
	Check(menu.findSelectedAction()->action().get() == first.get(),
		"Up skips disabled item and separator");
	Key(menu, Qt::Key_Return);
	Check(triggered == 1, "Return invokes selected callback exactly once");
	Key(menu, Qt::Key_Escape);
	Check(triggered == 1, "Escape never invokes a callback");
	menu.itemForAction(disabled)->setClicked(Ui::Menu::TriggeredSource::Keyboard);
	Check(triggered == 1, "Disabled QAction cannot trigger");
	for (auto i = 0; i != 12; ++i) {
		Key(menu, (i % 2) ? Qt::Key_Up : Qt::Key_Down);
	}
	menu.finishAnimating();
	Check(menu.findSelectedAction()->action().get() == first.get(),
		"Rapid interrupted selection retains correct item");
	auto image = QImage(menu.size(), QImage::Format_ARGB32_Premultiplied);
	image.fill(Qt::transparent);
	menu.render(&image);
	auto textPixels = 0;
	for (auto y = 0; y != image.height(); ++y) {
		for (auto x = 25; x < image.width() - 25; ++x) {
			textPixels += (image.pixelColor(x, y).lightness() < 100);
		}
	}
	Check(textPixels > 20, "Native menu text is visible");
	Check(image.save(Root + QString("/menu-%1-%2.png").arg(rounded).arg(animated)),
		"Menu render saved");
	menu.setItemSelectionGeometry(0, {}, false);
	Check(menu.actions().size() == 4, "Restoring standard geometry retains actions");
	Key(menu, Qt::Key_Down);
	Key(menu, Qt::Key_Return);
	Check(triggered == 11, "Restoring geometry retains callback identity");
}

void InputRegression() {
	auto parent = QWidget();
	parent.resize(400, 200);
	auto style = st::defaultInputField;
	auto field = Ui::InputField(&parent, style, rpl::single(QString("Search")));
	field.resize(340, 44);
	parent.show();
	field.show();
	QApplication::setActiveWindow(&parent);
	field.setFocus();
	field.setText("AxGram search 123");
	auto cursor = field.textCursor();
	cursor.setPosition(3);
	cursor.setPosition(9, QTextCursor::KeepAnchor);
	field.setTextCursor(cursor);
	Pump();
	const auto text = field.getLastText();
	const auto position = field.textCursor().position();
	const auto anchor = field.textCursor().anchor();
	const auto focus = field.hasFocus();
	Check(focus, "Offscreen field takes focus");
	for (auto i = 0; i != 40; ++i) {
		style.borderRadius = (i % 2) ? 16 : st::defaultInputField.borderRadius;
		style.border = (i % 2) ? 2 : st::defaultInputField.border;
		style.borderFgActive = (i % 2) ? st::activeLineFg : st::defaultInputField.borderFgActive;
		field.update();
		Pump(2);
		Check(field.getLastText() == text, "Style refresh preserves text");
		Check(field.textCursor().position() == position, "Style refresh preserves cursor");
		Check(field.textCursor().anchor() == anchor, "Style refresh preserves selection");
		Check(field.hasFocus() == focus, "Style refresh preserves focus");
	}
}

class TestIconButton final : public Ui::IconButton {
public:
	using Ui::IconButton::IconButton;
	using Ui::IconButton::iconOverOpacity;

};

void ToolbarIconRegression() {
	auto parent = QWidget();
	parent.resize(200, 100);
	auto style = st::backButton;
	style.iconOver = style.icon;
	style.duration = 150;
	auto button = TestIconButton(&parent, style);
	parent.show();
	button.show();
	button.setSynteticOver(true);
	Pump(200);
	const auto render = [&] {
		auto result = QImage(button.size(), QImage::Format_ARGB32_Premultiplied);
		result.fill(Qt::transparent);
		button.render(&result);
		return result;
	};
	AxUiSettings::setAnimatedIconsEnabled(false);
	const auto original = render();
	AxUiSettings::setAnimatedIconsEnabled(true);
	Check(AxUiSettings::animatedIconsEnabled(), "Common icon animation policy enabled");
	Check(button.isOver(), "Toolbar fixture holds hover state");
	const auto animated = render();
	original.save(Root + "/toolbar-stock.png");
	animated.save(Root + "/toolbar-animated.png");
	fprintf(stderr, "Toolbar opacity=%f size=%dx%d icon=%dx%d\\n",
		button.iconOverOpacity(), button.width(), button.height(),
		style.icon.width(), style.icon.height());
	Check(original != animated, "Common toolbar icon animates without changing its glyph");
	Check(button.size() == QSize(style.width, style.height), "Icon animation preserves hit target");
	AxUiSettings::setAnimatedIconsEnabled(false);
	Check(render() == original, "Telegram mode restores original toolbar icon exactly");
	AxUiSettings::setAnimatedIconsEnabled(true);
	anim::SetDisabled(true);
	Check(!AxUiSettings::animatedIconsEnabled(), "Reduced motion disables common glyph animation");
	Check(render() == original, "Reduced motion restores stable toolbar icon");
	anim::SetDisabled(false);
	AxUiSettings::setAnimatedIconsPolicy([] { return false; });
	Check(!AxUiSettings::animatedIconsEnabled(), "Host power-saving policy disables common glyph animation");
	Check(render() == original, "Power saving preserves stable toolbar glyph");
	AxUiSettings::setAnimatedIconsPolicy(nullptr);
	for (auto i = 0; i != 12; ++i) {
		button.setSynteticOver(i % 2);
		Pump(5);
	}
	button.setSynteticOver(false);
	Pump(200);
	AxUiSettings::setAnimatedIconsEnabled(false);
}

void FreshProfileRegression() {
	auto directory = QTemporaryDir();
	Check(directory.isValid(), "Temporary profile directory exists");
	const auto root = QDir(directory.path());
	Check(AxGlass::IsFreshProfile(directory.path()), "Missing tdata is new");
	root.mkpath("tdata/dumps");
	for (const auto &name : { "working", "usertag" }) {
		auto file = QFile(root.filePath(QString("tdata/") + name));
		Check(file.open(QIODevice::WriteOnly), "Bootstrap file created");
	}
	Check(AxGlass::IsFreshProfile(directory.path()), "Crash markers do not prevent first-run Glass");
	for (const auto &name : { "settingss", "key_datas", "config", "ax_settings.json", ".unknown" }) {
		auto file = QFile(root.filePath(QString("tdata/") + name));
		Check(file.open(QIODevice::WriteOnly), "Existing-profile marker created");
		file.close();
		Check(!AxGlass::IsFreshProfile(directory.path()), "Existing preferences and profiles are preserved");
		Check(file.remove(), "Temporary fixture removed");
	}
	for (const auto &name : { "data", "data_config" }) {
		auto file = QFile(root.filePath(name));
		Check(file.open(QIODevice::WriteOnly), "Legacy marker created");
		file.close();
		Check(!AxGlass::IsFreshProfile(directory.path()), "Legacy profile preserved");
		Check(file.remove(), "Temporary legacy fixture removed");
	}
	root.mkpath("tdata/D877F783D5D3EF8C");
	Check(!AxGlass::IsFreshProfile(directory.path()), "Account subdirectory preserved");
}

void CompositeIconRegression() {
	const uchar sizeData[] = "GENERATE:SIZE:\x00\x00\x00\x18\x00\x00\x00\x18";
	const uchar shapeData[] =
		"SVG:<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"24\" height=\"24\">"
		"<path d=\"M5 19L12 5L19 19M8 14H16\" fill=\"none\" stroke=\"white\" stroke-width=\"2\"/>"
		"</svg>";
	const auto sizeMask = style::internal::IconMask(sizeData);
	const auto shapeMask = style::internal::IconMask(shapeData);
	auto glyph = style::icon(std::in_place,
		style::internal::MonoIcon(&sizeMask, st::transparent, QMargins()),
		style::internal::MonoIcon(&shapeMask, st::menuIconFg, QMargins()));
	for (const auto accent : { QColor("#85ccd9"), QColor("#2d74ab") }) {
		const auto render = [&](bool preservePalette) {
			auto image = QImage(24, 24, QImage::Format_ARGB32_Premultiplied);
			image.fill(Qt::transparent);
			auto painter = QPainter(&image);
			if (preservePalette) {
				auto cached = glyph.withPalette(AxGlass::IconPalette(accent));
				cached.paint(painter, QPoint(), 24);
			} else {
				glyph.paint(painter, QPoint(), 24, accent);
			}
			return image;
		};
		const auto broken = render(false);
		Check(broken.pixelColor(0, 0).alpha() == 255,
			"Negative control reproduces the solid spacer square");
		const auto fixed = render(true);
		Check(fixed.pixelColor(0, 0).alpha() == 0,
			"Palette-aware painting preserves transparent top corner");
		Check(fixed.pixelColor(23, 23).alpha() == 0,
			"Palette-aware painting preserves transparent bottom corner");
		auto coverage = 0;
		for (auto y = 0; y != 24; ++y) {
			for (auto x = 0; x != 24; ++x) {
				const auto pixel = fixed.pixelColor(x, y);
				if (pixel.alpha() > 200) {
					++coverage;
					Check(std::abs(pixel.red() - accent.red()) < 3,
						"Visible composite glyph retains accent tint");
				}
			}
		}
		Check(coverage > 20 && coverage < 200, "Glyph is visible and not a rectangle");
		Check(fixed.save(Root + (accent.lightness() > 150
			? "/composite-dark.png" : "/composite-light.png")), "Composite render saved");
	}
}

void DialogFieldRegression() {
	const auto saved = style::main_palette::save();
	for (const auto dark : { false, true }) {
		auto parent = QWidget();
		parent.resize(330, 180);
		const auto background = dark ? QColor("#1c2732") : QColor("#f7fafd");
		style::main_palette::setColor(QLatin1String("boxBg"),
			background.red(), background.green(), background.blue(), 255);
		auto palette = parent.palette();
		palette.setColor(QPalette::Window, background);
		parent.setPalette(palette);
		parent.setAutoFillBackground(true);
		auto fieldStyle = st::defaultInputField;
		fieldStyle.textBg = st::boxBg;
		fieldStyle.textBgActive = st::boxBg;
		auto field = Ui::InputField(&parent, fieldStyle, rpl::single(QString("Name")));
		field.resize(300, 55);
		field.move(15, 10);
		field.setText("Channel name");
		parent.show();
		field.show();
		Pump();
		auto image = QImage(field.size(), QImage::Format_ARGB32_Premultiplied);
		image.fill(Qt::transparent);
		field.render(&image);
		Check(image.pixelColor(2, 2) == background, "Dialog field has no contrasting background rectangle");
		Check(field.getLastText() == "Channel name", "Dialog styling preserves entered text");
	}
	Check(style::main_palette::load(saved), "Palette restored after dialog fixture");
	style::NotifyPaletteChanged();
	const auto label = QString::fromUtf8("\xd0\x92\xd0\xb5\xd1\x80\xd1\x81\xd0\xb8\xd1\x8f 7.0.12 x64 \xe2\x80\x93 \xd0\x9e \xd0\xbf\xd1\x80\xd0\xbe\xd0\xb3\xd1\x80\xd0\xb0\xd0\xbc\xd0\xbc\xd0\xb5");
	Check(st::defaultTextStyle.font->width(label) + 33 < 268,
		"Russian Release About footer fits compact main menu");
}

void GlyphRegression() {
	for (const auto &name : { "settings", "user", "notification", "lock", "message-circle", "folder", "activity", "phone", "globe", "help", "star", "download", "shield", "edit", "mail", "archive", "search-to-x", "menu", "plus-to-x", "video", "monitor", "book", "filter", "power", "users", "credit-card", "bookmark", "moon", "speaker" }) {
		auto icon = Lottie::MakeIcon({
			.path = Root + "/assets/" + name + ".json",
			.sizeOverride = QSize(24, 24),
			.limitFps = true,
			.colorizeUsingAlpha = true,
		});
		Check(icon->valid() && icon->framesCount() > 1, "Native Icon wrapper loads resource");
		for (const auto color : { QColor("#2d74ab"), QColor("#85ccd9") }) {
			auto image = QImage(24, 24, QImage::Format_ARGB32_Premultiplied);
			image.fill(Qt::transparent);
			{
				auto p = QPainter(&image);
				icon->paintInCenter(p, image.rect(), color);
			}
			auto visible = 0;
			for (auto y = 0; y != image.height(); ++y) {
				for (auto x = 0; x != image.width(); ++x) {
					const auto pixel = image.pixelColor(x, y);
					if (pixel.alpha() > 220) {
						Check(std::abs(pixel.red() - color.red()) < 4
							&& std::abs(pixel.blue() - color.blue()) < 4,
							"Native Icon uses alpha tint, including black source glyphs");
						++visible;
					}
				}
			}
			Check(visible > 2, "Native tinted glyph visible");
		}
		icon->jumpTo(icon->framesCount() / 2, [] {});
		icon->animate([] {}, icon->frameIndex(), 0, crl::time(80));
		Pump(250);
		Check(!icon->animating(), "Reverse animation finishes");
		Check(icon->frameIndex() == 0, "Reverse returns to poster");
		for (auto replay = 0; replay != 3; ++replay) {
			icon->jumpTo(icon->framesCount() - 1, [] {});
			Pump(25);
			Check(icon->frameIndex() == icon->framesCount() - 1,
				"Replay fixture starts at previous final frame");
			AxGlass::ReplayIcon(icon.get(), [] {}, crl::time(120));
			Pump(45);
			Check(icon->frameIndex() < icon->framesCount() - 1,
				"Repeated hover actually leaves the previous final frame");
			Pump(140);
			Check(!icon->animating(), "Repeated hover completes");
			Check(icon->frameIndex() == icon->framesCount() - 1,
				"Repeated hover reaches its intended last frame");
		}
	}
}

} // namespace


void WelcomeBrandRegression() {
	for (const auto dark : { false, true }) {
		for (const auto side : { 48, 78, 156 }) {
			const auto render = [&](qreal elapsed) {
				auto image = QImage(side * 4, side * 4, QImage::Format_ARGB32_Premultiplied);
				image.fill(Qt::transparent);
				auto painter = QPainter(&image);
				Intro::Brand::PaintMark(painter, QRectF(side, side, side, side), dark, elapsed);
				return image;
			};
			const auto empty = render(0.);
			const auto initial = render(300.);
			const auto complete = render(1400.);
			Check(empty != initial && initial != complete, "brand entrance has distinct assembly frames");
			Check(complete == render(4000.), "brand remains stable after entrance completes");
			Check(qAlpha(empty.pixel(side * 2, side * 2)) == 0, "brand starts transparent");
			Check(qAlpha(complete.pixel(0, 0)) == 0, "brand paint preserves exterior transparency");
			Check(complete.save(Root + QString("/brand-%1-%2.png").arg(dark).arg(side)), "brand final frame saved");
		}
		const auto wallpaper = QImage(Root + (dark ? "/welcome-dark.png" : "/welcome-light.png"));
		Check(!wallpaper.isNull(), "approved welcome wallpaper available");
		for (const auto size : { QSize(380, 500), QSize(800, 600), QSize(1280, 800) }) {
			auto image = QImage(size, QImage::Format_ARGB32_Premultiplied);
			image.fill(Qt::transparent);
			auto painter = QPainter(&image);
			const auto paint = [&] {
				Intro::Brand::PaintWallpaper(painter, image.rect(), wallpaper,
					QColor(dark ? 0x10161d : 0xf1f5fa), .65, 1400.);
			};
			paint();
			const auto first = image.copy();
			paint();
			Check(image == first, "wallpaper repaint does not accumulate translucent layers");
			Check(qAlpha(image.pixel(0, 0)) == 255 && qAlpha(image.pixel(size.width()-1, size.height()-1)) == 255,
				"wallpaper fills narrow and wide layouts without transparent edges");
			painter.end();
			Check(image.save(Root + QString("/welcome-background-%1-%2.png").arg(dark).arg(size.width())), "wallpaper preview saved");
		}
	}
	Check(Intro::Brand::Channel(0., 920., 380.) == 0., "button starts hidden");
	Check(Intro::Brand::Channel(1400., 920., 380.) == 1., "button completes entrance");
	for (auto time = 0; time < 1400; ++time) {
		Check(Intro::Brand::Channel(time, 920., 380.) <= Intro::Brand::Channel(time + 1, 920., 380.),
			"button entrance has no backward jump");
	}
}


void WelcomeButtonRegression() {
	const auto saved = style::main_palette::save();
	auto parent = QWidget();
	parent.resize(340, 80);
	auto buttonStyle = st::defaultLightButton;
	buttonStyle.height = 36;
	buttonStyle.textTop = 10;
	auto theme = Ui::RoundButton(&parent, rpl::single(QString::fromUtf8("Р”РЅРµРІРЅР°СЏ")), buttonStyle);
	auto settings = Ui::RoundButton(&parent, rpl::single(QString::fromUtf8("РќР°СЃС‚СЂРѕР№РєРё")), buttonStyle);
	theme.move(15, 20);
	settings.move(theme.x() + theme.width() + 10, 20);
	Intro::ApplyGlassButton(&theme, 1.);
	Intro::ApplyGlassButton(&settings, 1.);
	parent.show();theme.show();settings.show();
	auto previous = QImage();
	for (const auto dark : { false, true, false, true }) {
		const auto bg = dark ? QColor("#10161d") : QColor("#f1f5fa");
		const auto fg = dark ? QColor("#eaf0f6") : QColor("#172234");
		auto parentPalette = parent.palette();
		parentPalette.setColor(QPalette::Window, bg);
		parent.setPalette(parentPalette); parent.setAutoFillBackground(true);
		style::main_palette::setColor(QLatin1String("windowBg"), bg.red(), bg.green(), bg.blue(), 255);
		style::main_palette::setColor(QLatin1String("windowFg"), fg.red(), fg.green(), fg.blue(), 255);
		style::NotifyPaletteChanged();
		Pump();
		auto image = QImage(parent.size(), QImage::Format_ARGB32_Premultiplied);
		image.fill(Qt::transparent);parent.render(&image);
		Check(theme.height() == settings.height(), "Welcome controls have matching heights");
		Check(theme.accessibilityName() == QString::fromUtf8("Р”РЅРµРІРЅР°СЏ"), "Theme label remains available");
		Check(settings.accessibilityName() == QString::fromUtf8("РќР°СЃС‚СЂРѕР№РєРё"), "Settings label preserves sentence case");
		if (!previous.isNull()) Check(image != previous, "Both glass controls repaint on repeated theme changes");
		Check(image.save(Root + QString("/welcome-buttons-%1.png").arg(dark)), "Native glass controls snapshot saved");
		previous = image;
	}
	Check(style::main_palette::load(saved), "Palette restored after welcome controls");
	style::NotifyPaletteChanged();
}


void AddAccountContrastRegression() {
	const uchar plusData[] =
		"SVG:<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"24\" height=\"24\">"
		"<path d=\"M5 12H19M12 5V19\" fill=\"none\" stroke=\"white\" stroke-width=\"2\"/>"
		"</svg>";
	const auto mask = style::internal::IconMask(plusData);
	auto plus = style::icon(std::in_place,
		style::internal::MonoIcon(&mask, st::windowFgActive, QMargins()));
	for (const auto accent : { QColor("#85ccd9"), QColor("#2d74ab") }) {
		auto image = QImage(24, 24, QImage::Format_ARGB32_Premultiplied);
		image.fill(Qt::transparent);
		{
			auto painter = QPainter(&image);
			auto recolored = plus.withPalette(AxGlass::IconPalette(accent, true));
			recolored.paint(painter, QPoint(), 24);
		}
		Check(image.pixelColor(12,12) == accent, "Add-account cross has accent contrast in both themes");
		Check(image.pixelColor(0,0).alpha() == 0, "Add-account cross retains transparent background");
		Check(image.save(Root + (accent.lightness() > 150 ? "/add-account-dark.png" : "/add-account-light.png")), "Add-account glyph render saved");
		const auto &normal = AxGlass::IconPalette(accent, false);
		Check(normal.colorAtIndex(style::main_palette::indexOfColor(st::windowFgActive))->c == st::windowFgActive->c,
			"Other icons keep their original active foreground role");
	}
}


void GlassAccentRegression() {
	for (const auto dark : { false, true }) {
		auto base = style::palette();
		auto file = QFile(Root + (dark ? "/accent-dark.palette" : "/accent-light.palette"));
		Check(file.open(QIODevice::ReadOnly), "Glass palette fixture opens");
		const auto lines = QString::fromUtf8(file.readAll()).split('\n');
		const auto pattern = QRegularExpression("^([A-Za-z0-9_]+):\\s*([^;]+);");
		for (const auto &line : lines) {
			const auto match = pattern.match(line);
			if (!match.hasMatch()) continue;
			const auto name = match.captured(1).toLatin1();
			const auto value = match.captured(2).trimmed();
			if (value.startsWith('#')) {
				auto color = QColor(value.left(7));
				if (value.size() == 9) color.setAlpha(value.right(2).toInt(nullptr,16));
				base.setColor(QLatin1String(name), color);
			} else {
				const auto alias = value.toLatin1();
				base.setColor(QLatin1String(name), QLatin1String(alias));
			}
		}
		base.finalize();
		for (const auto color : { QColor("#85ccd9"), QColor("#ff2020"), QColor("#31b45a"),
				QColor("#9155ff"), QColor("#ffdd00"), QColor("#000000"), QColor("#ffffff"), QColor("#888888") }) {
			auto tinted = style::palette();tinted = base;
			AxGlass::TintPalette(tinted,color,dark);
			const auto foreground = AxGlass::Accent(color,dark);
			const auto surface = dark ? QColor("#18212a") : QColor("#f7fafd");
			Check(AxGlass::Contrast(foreground,surface)>=4.49,"Arbitrary accent retains text/icon contrast");
			Check(tinted.windowBg()->c == base.windowBg()->c,"Accent leaves neutral page background unchanged");
			Check(tinted.boxBg()->c == base.boxBg()->c,"Accent leaves neutral dialog surface unchanged");
			Check(tinted.dialogsUnreadBgMuted()->c == base.dialogsUnreadBgMuted()->c,"Muted unread badges remain muted");
			Check(tinted.boxTextFgError()->c == base.boxTextFgError()->c,"Error red is preserved");
			Check(AxGlass::Contrast(tinted.activeButtonFg()->c,tinted.activeButtonBg()->c)>=4.49,"Button label retains contrast");
			Check(AxGlass::Contrast(tinted.dialogsUnreadFg()->c,tinted.dialogsUnreadBg()->c)>=4.49,"Unread count retains contrast");
			Check(std::abs(tinted.windowBgActive()->c.red()-foreground.red())<=2
				&&std::abs(tinted.windowBgActive()->c.green()-foreground.green())<=2
				&&std::abs(tinted.windowBgActive()->c.blue()-foreground.blue())<=2,"Main active role follows shared accent");
			AxGlass::ConfigureAccent(color);
			Check(AxGlass::CurrentAccent(dark)==foreground,"Native Glass components use the same accent");
			Check(AxGlass::IsGlassTheme(":/gui/axgram-glass-dark.tdesktop-theme"),"Glass resource recognized");
			Check(!AxGlass::IsGlassTheme("custom-theme.tdesktop-theme"),"Custom themes are not overwritten");
		}
		auto original = style::palette();original = base;
		AxGlass::TintPalette(original,QColor(),dark);
		Check(original.save()==base.save(),"Reset returns exact original theme palette");
	}
	AxGlass::ConfigureAccent(QColor());
	const uchar data[] = "SVG:<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"24\" height=\"24\"><path d=\"M5 19L12 5L19 19\" fill=\"none\" stroke=\"white\" stroke-width=\"2\"/></svg>";
	const auto mask = style::internal::IconMask(data);
	auto ghost = style::icon(std::in_place,style::internal::MonoIcon(&mask,st::windowBoldFg,QMargins()));
	const auto accent=QColor("#85ccd9");
	auto image=QImage(24,24,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);
	{ auto painter=QPainter(&image);auto icon=ghost.withPalette(AxGlass::IconPalette(accent));icon.paint(painter,QPoint(),24); }
	Check(image.pixelColor(0,0).alpha()==0,"Ghost-like fallback keeps its transparent background");
	auto found=false;
	for(auto y=0;y<24;++y)for(auto x=0;x<24;++x)if(image.pixelColor(x,y).alpha()>240) {
		Check(image.pixelColor(x,y).rgb()==accent.rgb(),"Legacy white foreground icon is tinted turquoise");found=true;
	}
	Check(found,"Fallback icon contains visible glyph pixels");
}


int main(int argc, char **argv) {
#ifdef _MSC_VER
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
	auto offscreen = false;
	for (const auto &plugin : QPluginLoader::staticPlugins()) {
		const auto keys = plugin.metaData().value("MetaData").toObject().value("Keys").toArray();
		for (const auto &key : keys) {
			offscreen = offscreen || (key.toString() == "offscreen");
		}
	}
	if (!offscreen) {
		fprintf(stderr, "Offscreen plugin missing; stopping before QApplication startup.\n");
		return 2;
	}
	qputenv("QT_QPA_PLATFORM", "offscreen");
	TestApplication app(argc, argv);
	BaseIntegration base(argc, argv);
	base::Integration::Set(&base);
	Root = (argc > 1) ? QString::fromLocal8Bit(argv[1]) : QDir::tempPath() + "/axgram-ui-test";
	QDir().mkpath(Root + "/ui-cache");
	Integration integration;
	Ui::Integration::Set(&integration);
	crl::init_main_queue([](void (*callback)(void*), void *argument) {
		QTimer::singleShot(0, QCoreApplication::instance(), [=] { callback(argument); });
	});
	Ui::Animations::Manager animations;
	style::internal::StartFonts();
	style::StartManager(100);
	MenuRegression(false, false);
	MenuRegression(true, false);
	MenuRegression(true, true);
	InputRegression();
	FreshProfileRegression();
	ToolbarIconRegression();
	CompositeIconRegression();
	DialogFieldRegression();
	GlassAccentRegression();
	GlyphRegression();
	WelcomeBrandRegression();
	WelcomeButtonRegression();
	AddAccountContrastRegression();
	anim::SetDisabled(true);
	MenuRegression(true, false);
	printf("%d offscreen native widget assertions passed.\n", Checks);
	fflush(stdout);
	return 0;
}

namespace crl {
rpl::producer<> on_main_update_requests() {
	return UpdateRequests.events();
}
} // namespace crl
