/*
 *   Copyright 2016-2026 melonDS team
 *
 *   This file is part of melonDS.
 *
 *   melonDS is free software: you can redistribute it and/or modify it under
 *   the terms of the GNU General Public License as published by the Free
 *   Software Foundation, either version 3 of the License, or (at your option)
 *   any later version.
 *
 *   melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
 *   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *   FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with melonDS. If not, see http://www.gnu.org/licenses/.
 */

#include "OverlayWidget.h"
#include "Window.h"
#include "EmuInstance.h"
#include "EmuThread.h"
#include "Config.h"

#include <QApplication>
#include <QPalette>
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QScrollArea>
#include <QComboBox>
#include <QSpinBox>
#include <QRadioButton>
#include <QGroupBox>
#include <QStandardItemModel>
#include <QSignalBlocker>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QListWidgetItem>
#include <QSet>
#include <algorithm>
#include <QFrame>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static QWidget* makeSeparator()
{
    bool isDark = QApplication::palette().color(QPalette::Window).lightness() < 128;
    QString col = isDark ? "#888888" : "#666666";
    QWidget* line = new QWidget();
    line->setFixedHeight(1);
    line->setStyleSheet(QString("background-color: %1;").arg(col));
    QWidget* wrap = new QWidget();
    QVBoxLayout* lay = new QVBoxLayout(wrap);
    lay->setContentsMargins(0, 4, 0, 4);
    lay->setSpacing(0);
    lay->addWidget(line);
    return wrap;
}

static QLabel* makeSectionLabel(const QString& text)
{
    QLabel* lbl = new QLabel(text.toUpper());
    QFont f = lbl->font();
    f.setPointSize(qMax(f.pointSize() - 1, 7));
    f.setBold(true);
    lbl->setFont(f);
    bool isDark = QApplication::palette().color(QPalette::Window).lightness() < 128;
    lbl->setStyleSheet(isDark ? "color: #aaaaaa; background: transparent;"
                              : "color: #555555; background: transparent;");
    lbl->setContentsMargins(0, 8, 0, 2);
    return lbl;
}

static QPushButton* makeMenuBtn(const QString& text)
{
    QPushButton* btn = new QPushButton(text);
    btn->setFlat(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFocusPolicy(Qt::StrongFocus);
    btn->setCheckable(false);
    btn->setIcon(QIcon()); // prevent Qt 6.7+ from rendering a checkbox indicator
    btn->setStyleSheet(
        "QPushButton { text-align: left; padding: 6px 10px; border-radius: 5px; }"
        "QPushButton:hover { background: palette(highlight); color: palette(highlighted-text); }"
        "QPushButton:focus { background: palette(highlight); color: palette(highlighted-text); outline: none; }"
        "QPushButton:pressed { background: palette(dark); }"
        "QPushButton::menu-indicator { width: 0; }"
    );
    return btn;
}

static QCheckBox* makeMenuToggle(const QString& text)
{
    QCheckBox* cb = new QCheckBox(text);
    cb->setCursor(Qt::PointingHandCursor);
    cb->setFocusPolicy(Qt::StrongFocus);
    cb->setStyleSheet(
        "QCheckBox { padding: 5px 4px; border-radius: 5px; spacing: 8px; }"
        "QCheckBox:hover { background: palette(highlight); color: palette(highlighted-text); }"
        "QCheckBox:focus { background: palette(highlight); color: palette(highlighted-text); outline: none; }"
    );
    return cb;
}

// ---------------------------------------------------------------------------
// OverlayWidget
// ---------------------------------------------------------------------------

OverlayWidget::OverlayWidget(MainWindow* mainWin, QWidget* container, EmuInstance* inst)
: QWidget(container)  // child of m_panelContainer, same level as the GL panel
, m_mainWindow(mainWin)
, m_emuInstance(inst)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setFocusPolicy(Qt::StrongFocus);
    // Fill the container
    setGeometry(container->rect());
    qApp->installEventFilter(this);
    buildUI();
    hide();
}

void OverlayWidget::open()
{
    if (m_open) return;
    m_open = true;

    reposition();
    updateGamePageState();
    updateConfigPageState();
    if (m_sidebar) m_sidebar->setCurrentRow(2);
    show();
    raise();
    // Force repaint of the current page including its scroll area viewport
    if (m_pages)
    {
        m_pages->repaint();
        if (QWidget* page = m_pages->currentWidget())
        {
            page->repaint();
            // Repaint all children (including QScrollArea viewport)
            for (QObject* obj : page->findChildren<QWidget*>())
                static_cast<QWidget*>(obj)->repaint();
        }
    }
    m_sidebar->setFocus();
    animateIn();
}

void OverlayWidget::close()
{
    if (!m_open) return;
    m_open = false;  // set immediately so isOpen() returns false right away
    animateOut();
}


void OverlayWidget::reposition()
{
    if (QWidget* p = parentWidget())
        setGeometry(p->rect());
}

void OverlayWidget::setFrozenFrame(const QPixmap& px)
{
    m_frozenFrame = px;
    update();
}

void OverlayWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    bool isDark = (QApplication::palette().color(QPalette::Window).lightness() < 128);
    // Fill solid first to prevent GL/bugged graphics bleeding through
    QColor solidBg = isDark ? QColor(18, 18, 18) : QColor(210, 210, 210);
    p.fillRect(rect(), solidBg);
    if (!m_frozenFrame.isNull())
    {
        p.drawPixmap(rect(), m_frozenFrame.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
        p.fillRect(rect(), isDark ? QColor(0, 0, 0, 150) : QColor(0, 0, 0, 110));
    }
}

void OverlayWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) { onResumeClicked(); return; }
    QWidget::keyPressEvent(event);
}

void OverlayWidget::mousePressEvent(QMouseEvent* event)
{
    QWidget::mousePressEvent(event);
}

void OverlayWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    reposition();
}

bool OverlayWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (!m_open) return false;

    // Only intercept keyboard events going to the main window or its panels.
    // Let events through to any dialog or settings window (they are top-level
    // QWidgets with no parent relationship to MainWindow).
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
    {
        // If the target is a top-level window that isn't MainWindow, let it through
        QWidget* w = qobject_cast<QWidget*>(obj);
        if (w && w->isWindow() && w != m_mainWindow)
            return false;

        if (event->type() == QEvent::KeyPress)
        {
            QKeyEvent* ke = static_cast<QKeyEvent*>(event);
            if (!ke->isAutoRepeat())
                keyPressEvent(ke);
            return true;
        }
        return true; // consume KeyRelease too
    }
    return false;
}

// ---------------------------------------------------------------------------
// UI Construction
// ---------------------------------------------------------------------------

void OverlayWidget::buildUI()
{
    bool isDark = (QApplication::palette().color(QPalette::Window).lightness() < 128);
    QColor winColor = QApplication::palette().color(QPalette::Window);
    QColor panelColor = isDark ? winColor.darker(130) : winColor.lighter(103);
    QColor accentColor = isDark ? panelColor.darker(115) : panelColor.darker(105);

    auto rgba = [](QColor c) {
        return QString("rgba(%1,%2,%3,255)").arg(c.red()).arg(c.green()).arg(c.blue());
    };

    // Panel fills the entire overlay
    QVBoxLayout* outerV = new QVBoxLayout(this);
    outerV->setContentsMargins(0, 0, 0, 0);
    outerV->setSpacing(0);

    m_panel = new QWidget(this);
    m_panel->setObjectName("overlayPanel");
    m_panel->setAttribute(Qt::WA_StyledBackground, true);
    m_panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_panel->setStyleSheet(QString(
        "QWidget#overlayPanel { background-color: rgba(%1,%2,%3,235); }")
    .arg(panelColor.red()).arg(panelColor.green()).arg(panelColor.blue()));

    outerV->addWidget(m_panel);

    QVBoxLayout* panelLayout = new QVBoxLayout(m_panel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(0);

    // Header
    QWidget* header = new QWidget();
    header->setFixedHeight(54);
    header->setStyleSheet(
        "background-color: " + rgba(accentColor) + ";");

    QHBoxLayout* hdrLayout = new QHBoxLayout(header);
    hdrLayout->setContentsMargins(18, 0, 18, 0);
    hdrLayout->setSpacing(10);

    // melonDS icon
    QLabel* iconLbl = new QLabel();
    QIcon appIcon = QIcon(":/melon-icon");
    if (!appIcon.isNull())
        iconLbl->setPixmap(appIcon.pixmap(28, 28));
    hdrLayout->addWidget(iconLbl);

    QLabel* titleLbl = new QLabel("melonDS");
    QFont tf = titleLbl->font();
    tf.setPointSize(tf.pointSize() + 3);
    tf.setBold(true);
    titleLbl->setFont(tf);
    hdrLayout->addWidget(titleLbl);
    hdrLayout->addStretch();
    panelLayout->addWidget(header);

    // Body
    QWidget* body = new QWidget();
    QHBoxLayout* bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    m_sidebar = new QListWidget();
    m_sidebar->setFixedWidth(160);
    m_sidebar->setFrameShape(QFrame::NoFrame);
    m_sidebar->setFocusPolicy(Qt::StrongFocus);
    {
        QString borderCol = isDark ? "#888888" : "#666666";
        m_sidebar->setStyleSheet(
            QString("QListWidget { border-right: 1px solid %1; background: transparent; outline: none; color: palette(window-text); }"
                    "QListWidget::item { padding: 10px 14px; color: palette(window-text); }"
                    "QListWidget::item:selected { background: palette(highlight); color: palette(highlighted-text); }"
                    "QListWidget::item:hover:!selected { background: palette(light); }").arg(borderCol)
        );
    }
    // Row 0: Resume
    { QListWidgetItem* it = new QListWidgetItem("▶  Resume");
      it->setTextAlignment(Qt::AlignVCenter|Qt::AlignLeft);
      QFont f=it->font(); f.setBold(true); it->setFont(f);
      m_sidebar->addItem(it); }
    // Row 1: separator
    { QListWidgetItem* sep = new QListWidgetItem();
      sep->setFlags(Qt::NoItemFlags); sep->setSizeHint(QSize(0,1));
      { bool isDark_ = QApplication::palette().color(QPalette::Window).lightness() < 128;
      sep->setBackground(QColor(isDark_ ? "#888888" : "#666666")); }
      m_sidebar->addItem(sep); }
    // Rows 2-5: categories
    for (const QString& cat : { QString("Game"), QString("System"), QString("View"), QString("Config") })
    { QListWidgetItem* it = new QListWidgetItem(cat);
      it->setTextAlignment(Qt::AlignVCenter|Qt::AlignLeft);
      m_sidebar->addItem(it); }
    // Row 6: separator
    { QListWidgetItem* sep = new QListWidgetItem();
      sep->setFlags(Qt::NoItemFlags); sep->setSizeHint(QSize(0,1));
      { bool isDark_ = QApplication::palette().color(QPalette::Window).lightness() < 128;
      sep->setBackground(QColor(isDark_ ? "#888888" : "#666666")); }
      m_sidebar->addItem(sep); }
    // Row 7: Quit to Library
    { QListWidgetItem* it = new QListWidgetItem("⏏  Quit to Library");
      it->setTextAlignment(Qt::AlignVCenter|Qt::AlignLeft);
      m_sidebar->addItem(it); }
    // Row 8: Quit melonDS
    { QListWidgetItem* it = new QListWidgetItem("✕  Quit melonDS");
      it->setTextAlignment(Qt::AlignVCenter|Qt::AlignLeft);
      it->setForeground(QApplication::palette().color(QPalette::Link));
      m_sidebar->addItem(it); }
    m_sidebar->setCurrentRow(2);
    connect(m_sidebar, &QListWidget::currentRowChanged, this, &OverlayWidget::onCategoryChanged);
    connect(m_sidebar, &QListWidget::itemActivated, this, [this](QListWidgetItem*) {
        activateSidebarRow(m_sidebar->currentRow());
    });

    m_pages = new QStackedWidget();
    m_gamePage   = buildGamePage();
    m_systemPage = buildSystemPage();
    m_viewPage   = buildViewPage();
    m_configPage = buildConfigPage();
    m_pages->addWidget(m_gamePage);
    m_pages->addWidget(m_systemPage);
    m_pages->addWidget(m_viewPage);
    m_pages->addWidget(m_configPage);
    m_pages->addWidget(new QWidget()); // index 4 — blank for action rows

    bodyLayout->addWidget(m_sidebar);
    bodyLayout->addWidget(m_pages, 1);
    panelLayout->addWidget(body, 1);


    // Animation
    m_panelAnim = new QPropertyAnimation(this);
    m_panelAnim->setDuration(180);
}

// ---------------------------------------------------------------------------
// Page builders
// ---------------------------------------------------------------------------

QWidget* OverlayWidget::buildGamePage()
{
    QScrollArea* scroll = new QScrollArea();
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QWidget* w = new QWidget();
    QVBoxLayout* lay = new QVBoxLayout(w);
    lay->setContentsMargins(16, 10, 16, 16);
    lay->setSpacing(2);

    // GBA
    lay->addWidget(makeSectionLabel("GBA"));

    // GBA buttons — checkmarks are refreshed in updateGamePageState() each open
    QPushButton* insertGBA = makeMenuBtn("Insert GBA ROM Cart...");
    insertGBA->setObjectName("btnInsertGBA");
    m_insertGBABtn = insertGBA;
    connect(insertGBA, &QPushButton::clicked, this, [this]() {
        m_mainWindow->actInsertGBACart->trigger();
        close();
    });
    lay->addWidget(insertGBA);

    for (QAction* act : m_mainWindow->actInsertGBAAddon)
    {
        QPushButton* btn = makeMenuBtn(act->text());
        btn->setObjectName("btnGBAAddon_" + act->text());
        m_gbaAddonBtns.append(btn);
        connect(btn, &QPushButton::clicked, this, [this, act]() {
            act->trigger();
            close();
        });
        lay->addWidget(btn);
    }

    QPushButton* ejectGBA = makeMenuBtn("Eject GBA Cart");
    connect(ejectGBA, &QPushButton::clicked, this, [this]() {
        m_mainWindow->actEjectGBACart->trigger();
        close();
    });
    lay->addWidget(ejectGBA);

    lay->addWidget(makeSeparator());

    // Save
    lay->addWidget(makeSectionLabel("Save"));

    QPushButton* importSave = makeMenuBtn("Import Savefile...");
    connect(importSave, &QPushButton::clicked, this, [this]() {
        // Don't close the overlay — the dialog handles its own flow and the
        // game may reset internally. Just trigger it with the overlay still open.
        m_mainWindow->actImportSavefile->trigger();
    });
    lay->addWidget(importSave);

    lay->addWidget(makeSectionLabel("Save State  (slot)"));
    {
        QHBoxLayout* row = new QHBoxLayout();
        row->setSpacing(4);
        for (int i = 1; i <= 8; i++)
        {
            QPushButton* btn = new QPushButton(QString::number(i));
            btn->setFixedSize(38, 38);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setFocusPolicy(Qt::StrongFocus);
            btn->setStyleSheet(
                "QPushButton { border-radius: 5px; font-weight: bold; }"
                "QPushButton:hover { background: palette(highlight); color: palette(highlighted-text); }"
                "QPushButton:focus { background: palette(highlight); color: palette(highlighted-text); outline: none; }"
            );
            connect(btn, &QPushButton::clicked, this, [this, i]() {
                m_mainWindow->actSaveState[i]->trigger();
                close();
            });
            row->addWidget(btn);
        }
        row->addStretch();
        lay->addLayout(row);
    }

    lay->addWidget(makeSectionLabel("Load State  (slot)"));
    {
        QHBoxLayout* row = new QHBoxLayout();
        row->setSpacing(4);
        for (int i = 1; i <= 8; i++)
        {
            QPushButton* btn = new QPushButton(QString::number(i));
            btn->setFixedSize(38, 38);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setFocusPolicy(Qt::StrongFocus);
            btn->setStyleSheet(
                "QPushButton { border-radius: 5px; font-weight: bold; }"
                "QPushButton:hover { background: palette(highlight); color: palette(highlighted-text); }"
                "QPushButton:focus { background: palette(highlight); color: palette(highlighted-text); outline: none; }"
            );
            connect(btn, &QPushButton::clicked, this, [this, i]() {
                m_mainWindow->actLoadState[i]->trigger();
                close();
            });
            row->addWidget(btn);
        }
        row->addStretch();
        lay->addLayout(row);
    }

    lay->addWidget(makeSeparator());

    // Misc
    lay->addWidget(makeSectionLabel("Misc"));

    QCheckBox* cheatsCB = makeMenuToggle("Enable Cheats");
    cheatsCB->setObjectName("cheatsCB");
    connect(cheatsCB, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_mainWindow->actEnableCheats->isChecked() != checked)
            m_mainWindow->actEnableCheats->trigger();
    });
    connect(m_mainWindow->actEnableCheats, &QAction::toggled, cheatsCB, &QCheckBox::setChecked);
    lay->addWidget(cheatsCB);

    QPushButton* setupCheats = makeMenuBtn("Setup Cheat Codes...");
    connect(setupCheats, &QPushButton::clicked, this, [this]() {
        m_mainWindow->actSetupCheats->trigger();
    });
    lay->addWidget(setupCheats);

    QCheckBox* limitFPS = makeMenuToggle("Limit Framerate");
    limitFPS->setObjectName("limitFpsCB");
    connect(limitFPS, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_mainWindow->actLimitFramerate->isChecked() != checked)
            m_mainWindow->actLimitFramerate->trigger();
    });
    connect(m_mainWindow->actLimitFramerate, &QAction::toggled, limitFPS, &QCheckBox::setChecked);
    lay->addWidget(limitFPS);

    QCheckBox* audioSync = makeMenuToggle("Audio Sync");
    audioSync->setObjectName("audioSyncCB");
    connect(audioSync, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_mainWindow->actAudioSync->isChecked() != checked)
            m_mainWindow->actAudioSync->trigger();
    });
    connect(m_mainWindow->actAudioSync, &QAction::toggled, audioSync, &QCheckBox::setChecked);
    lay->addWidget(audioSync);

    lay->addStretch();
    scroll->setWidget(w);
    return scroll;
}

QWidget* OverlayWidget::buildSystemPage()
{
    QScrollArea* scroll = new QScrollArea();
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QWidget* w = new QWidget();
    QVBoxLayout* lay = new QVBoxLayout(w);
    lay->setContentsMargins(16, 10, 16, 16);
    lay->setSpacing(2);

    lay->addWidget(makeSectionLabel("Multiplayer"));

    QPushButton* hostLAN = makeMenuBtn("Host LAN Game...");
    connect(hostLAN, &QPushButton::clicked, this, [this]() {
        m_mainWindow->actLANStartHost->trigger();
    });
    lay->addWidget(hostLAN);

    QPushButton* joinLAN = makeMenuBtn("Join LAN Game...");
    connect(joinLAN, &QPushButton::clicked, this, [this]() {
        m_mainWindow->actLANStartClient->trigger();
    });
    lay->addWidget(joinLAN);

    lay->addWidget(makeSeparator());

    QPushButton* dsiTitles = makeMenuBtn("Manage DSi Titles...");
    connect(dsiTitles, &QPushButton::clicked, this, [this]() {
        m_mainWindow->actTitleManager->trigger();
    });
    lay->addWidget(dsiTitles);

    QPushButton* powerMgmt = makeMenuBtn("Power Management...");
    connect(powerMgmt, &QPushButton::clicked, this, [this]() {
        m_mainWindow->actPowerManagement->trigger();
    });
    lay->addWidget(powerMgmt);

    lay->addStretch();
    scroll->setWidget(w);
    return scroll;
}

QWidget* OverlayWidget::buildViewPage()
{
    QScrollArea* scroll = new QScrollArea();
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QWidget* w = new QWidget();
    QVBoxLayout* lay = new QVBoxLayout(w);
    lay->setContentsMargins(16, 10, 16, 16);
    lay->setSpacing(2);

    auto addToggle = [&](QAction* act) {
        if (!act) return;
        QCheckBox* cb = makeMenuToggle(act->text());
        cb->setChecked(act->isChecked());
        connect(cb, &QCheckBox::toggled, this, [act](bool checked) {
            if (act->isChecked() != checked) act->trigger();
        });
            connect(act, &QAction::toggled, cb, &QCheckBox::setChecked);
            lay->addWidget(cb);
    };

    auto addRadioGroup = [&](QActionGroup* grp) {
        if (!grp) return;
        for (QAction* act : grp->actions())
        {
            QCheckBox* cb = makeMenuToggle(act->text());
            cb->setChecked(act->isChecked());
            connect(cb, &QCheckBox::clicked, this, [act]() { act->trigger(); });
            connect(act, &QAction::toggled, cb, &QCheckBox::setChecked);
            lay->addWidget(cb);
        }
    };

    lay->addWidget(makeSectionLabel("Display"));
    addToggle(m_mainWindow->actLimitFramerate);
    addToggle(m_mainWindow->actIntegerScaling);
    addToggle(m_mainWindow->actScreenFiltering);
    addToggle(m_mainWindow->actShowOSD);
    addToggle(m_mainWindow->actAudioSync);

    lay->addWidget(makeSeparator());
    lay->addWidget(makeSectionLabel("Screen"));
    addToggle(m_mainWindow->actScreenSwap);

    lay->addWidget(makeSectionLabel("Layout"));
    addRadioGroup(m_mainWindow->grpScreenLayout);

    lay->addWidget(makeSectionLabel("Rotation"));
    addRadioGroup(m_mainWindow->grpScreenRotation);

    lay->addWidget(makeSectionLabel("Sizing"));
    addRadioGroup(m_mainWindow->grpScreenSizing);

    lay->addWidget(makeSectionLabel("Gap"));
    addRadioGroup(m_mainWindow->grpScreenGap);

    lay->addStretch();
    scroll->setWidget(w);
    return scroll;
}

// A row widget that highlights its background when any child has focus or mouse hover.
// Used for RPCS3-style setting rows in the Config page.
class HighlightRow : public QWidget
{
public:
    explicit HighlightRow(const QString& labelText, QWidget* ctrl, QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFocusProxy(ctrl);
        setAttribute(Qt::WA_Hover, true);

        QHBoxLayout* lay = new QHBoxLayout(this);
        lay->setContentsMargins(6, 3, 6, 3);
        lay->setSpacing(12);

        m_label = new QLabel(labelText);
        m_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        lay->addWidget(m_label, 1);
        lay->addWidget(ctrl);

        // Install event filter on ctrl to repaint row on focus change
        ctrl->installEventFilter(this);
        m_ctrl = ctrl;
    }

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override
    {
        if (obj == m_ctrl &&
            (ev->type() == QEvent::FocusIn || ev->type() == QEvent::FocusOut))
            update();
        return false;
    }

    void paintEvent(QPaintEvent*) override
    {
        bool focused = m_ctrl && m_ctrl->hasFocus();
        bool hovered = underMouse();
        if (focused || hovered)
        {
            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);
            QColor bg = palette().color(QPalette::Highlight);
            if (!focused) bg.setAlpha(80); // subtle hover
            p.setBrush(bg);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(rect(), 5, 5);
            // Make label text readable over highlight
            if (focused)
                m_label->setStyleSheet("color: palette(highlighted-text);");
            else
                m_label->setStyleSheet("");
        }
        else
        {
            m_label->setStyleSheet("");
        }
    }

private:
    QLabel*  m_label = nullptr;
    QWidget* m_ctrl  = nullptr;
};

QWidget* OverlayWidget::buildConfigPage()
{
    QScrollArea* scroll = new QScrollArea();
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QWidget* w = new QWidget();
    QVBoxLayout* lay = new QVBoxLayout(w);
    lay->setContentsMargins(16, 10, 16, 16);
    lay->setSpacing(2);

    QPushButton* videoBtn = makeMenuBtn("Video Settings...");
    connect(videoBtn, &QPushButton::clicked, this, [this]() {
        // The overlay stays open — video settings dialog is shown on top.
        // The GL context switch (if any) will happen when the dialog closes,
        // at which point the overlay will be gone naturally via onOverlayResume.
        m_mainWindow->actVideoSettings->trigger();
    });
    lay->addWidget(videoBtn);

    QPushButton* audioBtn = makeMenuBtn("Audio Settings...");
    connect(audioBtn, &QPushButton::clicked, this, [this]() {
        m_mainWindow->actAudioSettings->trigger();
    });
    lay->addWidget(audioBtn);

    QPushButton* inputBtn = makeMenuBtn("Input and Hotkeys...");
    connect(inputBtn, &QPushButton::clicked, this, [this]() {
        m_mainWindow->actInputConfig->trigger();
    });
    lay->addWidget(inputBtn);

    QPushButton* emuBtn = makeMenuBtn("Emulation Settings...");
    connect(emuBtn, &QPushButton::clicked, this, [this]() {
        m_mainWindow->actEmuSettings->trigger();
    });
    lay->addWidget(emuBtn);

    lay->addStretch();
    scroll->setWidget(w);
    return scroll;
}

void OverlayWidget::updateViewPageState() {}

void OverlayWidget::updateGamePageState()
{
    // Sync checkbox states that may have changed while overlay was closed
    if (QCheckBox* cb = findChild<QCheckBox*>("cheatsCB"))
        cb->setChecked(m_mainWindow->actEnableCheats->isChecked());
    if (QCheckBox* cb = findChild<QCheckBox*>("limitFpsCB"))
        cb->setChecked(m_mainWindow->actLimitFramerate->isChecked());
    if (QCheckBox* cb = findChild<QCheckBox*>("audioSyncCB"))
        cb->setChecked(m_mainWindow->actAudioSync->isChecked());

    // GBA cart checkmark — actCurrentGBACart->text() is "GBA slot: <label>"
    // It's "(empty)" when nothing is inserted.
    QString gbaSlotText = m_mainWindow->getGBACartSlotText();
    // Format: "GBA slot: <label>" — strip the prefix
    const QString prefix = "GBA slot: ";
    QString currentLabel = gbaSlotText.startsWith(prefix)
                           ? gbaSlotText.mid(prefix.length())
                           : gbaSlotText;
    bool cartInserted = !currentLabel.isEmpty()
                        && currentLabel != "(none)"
                        && currentLabel != "none (DSi)";

    // Determine if it's a ROM cart (not an addon)
    bool romInserted = cartInserted;
    if (romInserted)
        for (QAction* act : m_mainWindow->actInsertGBAAddon)
            if (act->text() == currentLabel) { romInserted = false; break; }

    if (m_insertGBABtn)
    {
        if (romInserted)
        {
            QString name = currentLabel;
            int dot = name.lastIndexOf('.');
            if (dot > 0) name = name.left(dot);
            m_insertGBABtn->setText("✓  " + name);
        }
        else
            m_insertGBABtn->setText("Insert GBA ROM Cart...");
    }

    for (int i = 0; i < m_gbaAddonBtns.size() && i < m_mainWindow->actInsertGBAAddon.size(); i++)
    {
        QAction* act = m_mainWindow->actInsertGBAAddon[i];
        bool active = cartInserted && (act->text() == currentLabel);
        m_gbaAddonBtns[i]->setText(active ? "✓  " + act->text() : act->text());
    }

}

void OverlayWidget::updateConfigPageState()
{
    // No inline settings currently
}

// ---------------------------------------------------------------------------
// Animation
// ---------------------------------------------------------------------------

void OverlayWidget::animateIn()
{
    if (!m_panelAnim || !m_panel) return;
    disconnect(m_panelAnim, &QPropertyAnimation::finished, nullptr, nullptr);

    if (!m_opacityEffect)
    {
        m_opacityEffect = new QGraphicsOpacityEffect(m_panel);
        m_panel->setGraphicsEffect(m_opacityEffect);
    }

    m_opacityEffect->setOpacity(0.0);
    m_panelAnim->stop();
    m_panelAnim->setTargetObject(m_opacityEffect);
    m_panelAnim->setPropertyName("opacity");
    m_panelAnim->setDuration(160);
    m_panelAnim->setEasingCurve(QEasingCurve::OutCubic);
    m_panelAnim->setStartValue(0.0);
    m_panelAnim->setEndValue(1.0);
    m_panelAnim->start();
}

void OverlayWidget::animateOut()
{
    if (!m_panelAnim || !m_panel) { m_open = false; hide(); emit closed(); return; }

    if (!m_opacityEffect)
    {
        m_opacityEffect = new QGraphicsOpacityEffect(m_panel);
        m_panel->setGraphicsEffect(m_opacityEffect);
    }

    m_panelAnim->stop();
    disconnect(m_panelAnim, &QPropertyAnimation::finished, nullptr, nullptr);
    connect(m_panelAnim, &QPropertyAnimation::finished, this, [this]() {
        m_open = false;
        hide();
        emit closed();
    }, Qt::SingleShotConnection);

    m_panelAnim->setTargetObject(m_opacityEffect);
    m_panelAnim->setPropertyName("opacity");
    m_panelAnim->setDuration(120);
    m_panelAnim->setEasingCurve(QEasingCurve::InCubic);
    m_panelAnim->setStartValue(1.0);
    m_panelAnim->setEndValue(0.0);
    m_panelAnim->start();
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void OverlayWidget::onCategoryChanged(int row)
{
    if (row >= 2 && row <= 5)
        m_pages->setCurrentIndex(row - 2);
    else
        m_pages->setCurrentIndex(4); // blank for action rows
}

void OverlayWidget::onResumeClicked()
{
    close();
    QMetaObject::invokeMethod(m_mainWindow, "onOverlayResume", Qt::QueuedConnection);
}

void OverlayWidget::onQuitToLibrary()
{
    // Clear didPauseGame so onOverlayResume (fired by closed() signal) won't
    // switch to the panel or unpause — actStop handles the library transition.
    setDidPauseGame(false);
    close();
    QMetaObject::invokeMethod(m_mainWindow->actStop, "trigger", Qt::QueuedConnection);
}

void OverlayWidget::onQuitEmulator()
{
    m_mainWindow->actQuit->trigger();
}

static void ensureVisible(QWidget* w)
{
    if (!w) return;
    QWidget* p = w->parentWidget();
    while (p)
    {
        if (auto* sa = qobject_cast<QScrollArea*>(p))
        { sa->ensureWidgetVisible(w); return; }
        // QScrollArea wraps content in a QScrollAreaWidgetContainer;
        // the actual QScrollArea is one more level up.
        if (auto* sa = qobject_cast<QScrollArea*>(p->parentWidget()))
        { sa->ensureWidgetVisible(w); return; }
        p = p->parentWidget();
    }
}

static QList<QWidget*> pageFocusables(QWidget* page)
{
    QList<QWidget*> result;
    if (!page) return result;

    // Walk the full widget subtree of the page rather than the focus chain,
    // which can include sidebar and other overlay widgets incorrectly.
    QList<QWidget*> queue;
    queue.append(page);
    while (!queue.isEmpty())
    {
        QWidget* w = queue.takeFirst();
        for (QObject* obj : w->children())
        {
            QWidget* child = qobject_cast<QWidget*>(obj);
            if (!child || !child->isVisible() || !child->isEnabled()) continue;
            queue.append(child);
            if (child->focusPolicy() != Qt::NoFocus &&
                (qobject_cast<QPushButton*>(child) ||
                 qobject_cast<QCheckBox*>(child)   ||
                 qobject_cast<QComboBox*>(child)))
                result.append(child);
        }
    }

    // Sort top-to-bottom, left-to-right in global coordinates
    std::sort(result.begin(), result.end(), [](QWidget* a, QWidget* b) {
        QPoint pa = a->mapToGlobal(QPoint(0,0));
        QPoint pb = b->mapToGlobal(QPoint(0,0));
        if (qAbs(pa.y() - pb.y()) > 8) return pa.y() < pb.y();
        return pa.x() < pb.x();
    });
    return result;
}

static QList<QWidget*> horizontalSiblings(QWidget* w)
{
    if (!w || !w->parentWidget()) return {};
    int y = w->mapTo(w->parentWidget(), QPoint(0,0)).y();
    QList<QWidget*> siblings;
    for (QObject* obj : w->parentWidget()->children())
    {
        QWidget* s = qobject_cast<QWidget*>(obj);
        if (!s || !s->isVisible() || s->focusPolicy() == Qt::NoFocus) continue;
        if (!qobject_cast<QPushButton*>(s) && !qobject_cast<QCheckBox*>(s) && !qobject_cast<QComboBox*>(s)) continue;
        int sy = s->mapTo(s->parentWidget(), QPoint(0,0)).y();
        if (qAbs(sy - y) < 10) siblings.append(s);
    }
    std::sort(siblings.begin(), siblings.end(), [](QWidget* a, QWidget* b) {
        return a->mapTo(a->parentWidget(), QPoint(0,0)).x() <
               b->mapTo(b->parentWidget(), QPoint(0,0)).x();
    });
    return siblings.size() > 1 ? siblings : QList<QWidget*>{};
}

void OverlayWidget::activateSidebarRow(int row)
{
    if (row == 0)      onResumeClicked();
    else if (row == 7) onQuitToLibrary();
    else if (row == 8) onQuitEmulator();
    else if (row >= 2 && row <= 5)
    {
        // Focus first focusable widget in the page
        QWidget* page = m_pages->currentWidget();
        auto focs = pageFocusables(page);
        if (!focs.isEmpty()) { focs.first()->setFocus(); ensureVisible(focs.first()); }
        else if (page) page->setFocus();
    }
}

// Scroll a widget into view within its ancestor QScrollArea (if any).
void OverlayWidget::navKey(int hk)
{
    auto moveSidebar = [this](int delta) {
        int row = m_sidebar->currentRow(), target = row + delta;
        while (target >= 0 && target < m_sidebar->count())
        {
            if (m_sidebar->item(target)->flags() != Qt::NoItemFlags) break;
            target += delta;
        }
        if (target >= 0 && target < m_sidebar->count())
            m_sidebar->setCurrentRow(target);
    };

    QWidget* fw = QApplication::focusWidget();
    QWidget* page = m_pages->currentWidget();
    auto focs = pageFocusables(page);
    bool inPage = fw && page && (fw == page || page->isAncestorOf(fw) || focs.contains(fw));

    switch (hk)
    {
        case HK_NavLeft: // left
        {
            if (inPage)
            {
                auto horiz = horizontalSiblings(fw);
                int idx = horiz.indexOf(fw);
                if (!horiz.isEmpty() && idx > 0) { horiz[idx-1]->setFocus(); ensureVisible(horiz[idx-1]); break; }
            }
            m_sidebar->setFocus();
            break;
        }
        case HK_NavRight: // right
        {
            if (inPage)
            {
                auto horiz = horizontalSiblings(fw);
                int idx = horiz.indexOf(fw);
                if (!horiz.isEmpty() && idx >= 0 && idx < horiz.size()-1)
                { horiz[idx+1]->setFocus(); ensureVisible(horiz[idx+1]); }
            }
            else
            {
                if (!focs.isEmpty()) { focs.first()->setFocus(); ensureVisible(focs.first()); }
            }
            break;
        }
        case HK_NavUp: // up
        {
            if (!focs.isEmpty() && inPage)
            {
                int idx = focs.indexOf(fw);
                if (idx > 0)
                {
                    auto curGrp = horizontalSiblings(fw);
                    int target = idx - 1;
                    while (target > 0 && curGrp.contains(focs[target])) target--;
                    auto prevGrp = horizontalSiblings(focs[target]);
                    if (!prevGrp.isEmpty()) target = focs.indexOf(prevGrp.first());
                    focs[qMax(0,target)]->setFocus();
                    ensureVisible(focs[qMax(0,target)]);
                }
                else
                {
                    // At top — wrap to bottom
                    focs.last()->setFocus();
                    ensureVisible(focs.last());
                }
            }
            else
            {
                moveSidebar(-1);
                m_sidebar->setFocus();
            }
            break;
        }
        case HK_NavDown: // down
        {
            if (!focs.isEmpty() && inPage)
            {
                int idx = focs.indexOf(fw);
                if (idx >= 0 && idx < focs.size()-1)
                {
                    auto curGrp = horizontalSiblings(fw);
                    int target = idx + 1;
                    while (target < focs.size()-1 && curGrp.contains(focs[target])) target++;
                    auto nextGrp = horizontalSiblings(focs[target]);
                    if (!nextGrp.isEmpty()) target = focs.indexOf(nextGrp.first());
                    focs[target]->setFocus();
                    ensureVisible(focs[target]);
                }
                else
                {
                    // At bottom — wrap to top
                    focs.first()->setFocus();
                    ensureVisible(focs.first());
                }
            }
            else
            {
                moveSidebar(+1);
                m_sidebar->setFocus();
            }
            break;
        }
        case HK_NavConfirm:
        {
            if (auto* btn = qobject_cast<QPushButton*>(fw)) btn->click();
            else if (auto* cb = qobject_cast<QCheckBox*>(fw)) cb->toggle();
            else if (auto* combo = qobject_cast<QComboBox*>(fw))
                combo->showPopup();
            else activateSidebarRow(m_sidebar->currentRow());
            break;
        }

        case HK_NavBack:
        {
            // Back from page → sidebar; Back from sidebar → close overlay
            // (combo popup case is handled above before we get here)
            if (inPage)
                m_sidebar->setFocus();
            else if (!m_sidebar->hasFocus())
                m_sidebar->setFocus(); // safety: ensure sidebar has focus before closing
            else
                onResumeClicked();
            break;
        }

        default: break;
    }
}
