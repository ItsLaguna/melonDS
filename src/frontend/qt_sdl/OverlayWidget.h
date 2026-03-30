/*
 C opyright 2016-2026 m*elonDS team

 This file is part of melonDS.

 melonDS is free software: you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free
 Software Foundation, either version 3 of the License, or (at your option)
 any later version.

 melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along
 with melonDS. If not, see http://www.gnu.org/licenses/.
 */

#ifndef OVERLAYWIDGET_H
#define OVERLAYWIDGET_H

#include <QWidget>
#include <QStackedWidget>
#include <QListWidget>
#include <QCheckBox>
#include <QPushButton>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QPixmap>

class EmuInstance;
class MainWindow;

class OverlayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit OverlayWidget(MainWindow* mainWin, QWidget* container, EmuInstance* inst);
    ~OverlayWidget() = default;

    void open();
    void close();
    void reposition();
    void setFrozenFrame(const QPixmap& px);
    void navKey(int hk);
    bool isOpen() const { return m_open; }
    bool didPauseGame() const { return m_didPauseGame; }
    void setDidPauseGame(bool v) { m_didPauseGame = v; }

signals:
    void closed();

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onCategoryChanged(int index);
    void onResumeClicked();
    void onQuitToLibrary();
    void onQuitEmulator();

private:
    void buildUI();
    QWidget* buildGamePage();
    QWidget* buildSystemPage();
    QWidget* buildViewPage();
    QWidget* buildConfigPage();

    void animateIn();
    void animateOut();
    void activateSidebarRow(int row);
    void updateGamePageState();
    void updateViewPageState();
    void updateConfigPageState();

    MainWindow*  m_mainWindow;
    EmuInstance* m_emuInstance;
    bool         m_open         = false;
    bool         m_didPauseGame = false;
    QPixmap      m_frozenFrame;

    QWidget*        m_panel   = nullptr;
    QListWidget*    m_sidebar = nullptr;
    QStackedWidget* m_pages   = nullptr;

    QWidget* m_gamePage   = nullptr;
    QWidget* m_systemPage = nullptr;
    QWidget* m_viewPage   = nullptr;
    QWidget* m_configPage = nullptr;

    // Direct pointers for updateGamePageState
    QPushButton*         m_insertGBABtn  = nullptr;
    QList<QPushButton*>  m_gbaAddonBtns;

    QPropertyAnimation*     m_panelAnim     = nullptr;
    QGraphicsOpacityEffect* m_opacityEffect = nullptr;
};

#endif // OVERLAYWIDGET_H
