/*
    Copyright 2016-2026 melonDS team

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

#ifndef WINDOW_H
#define WINDOW_H

#include "glad/glad.h"
#include "ScreenLayout.h"
#include "duckstation/gl/context.h"

#include <QWidget>
#include <QWindow>
#include <QMainWindow>
#include <QImage>
#include <QActionGroup>
#include <QTimer>
#include <QMutex>
#include <QPointer>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QScreen>
#include <QCloseEvent>

#include <atomic>

#include "Screen.h"
#include "Config.h"
#include "MPInterface.h"
#include "LibraryController.h"
#include "OverlayWidget.h"


class EmuInstance;
class EmuThread;

const int kMaxRecentROMs = 10;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(int id, EmuInstance* inst, QWidget* parent = nullptr);
    ~MainWindow();

    EmuInstance* getEmuInstance() { return emuInstance; }
    Config::Table& getWindowConfig() { return windowCfg; }
    int getWindowID() { return windowID; }
    OverlayWidget* getOverlay() { return m_overlay; }
    QString getGBACartSlotText() const { return actCurrentGBACart ? actCurrentGBACart->text() : QString(); }

    bool winHasMenu() { return hasMenu; }

    void saveEnabled(bool enabled);

    void toggleFullscreen();

    bool hasOpenGL() { return hasOGL; }
    GL::Context* getOGLContext();
    void initOpenGL();
    void deinitOpenGL();
    void setGLSwapInterval(int intv);
    void makeCurrentGL();
    void releaseGL();

    void drawScreen();

    // Called after initContext succeeds to insert the panel into the stack and show it.
    void showScreenPanel();
    // Called on emu stop to remove and destroy the panel, returning to the library page.
    void destroyScreenPanel();
    // Called before every bootROM — ensures panel + GL context are fresh and ready.
    void prepareScreenForBoot();

    bool preloadROMs(QStringList file, QStringList gbafile, bool boot);
    QStringList splitArchivePath(const QString& filename, bool useMemberSyntax);

    void onAppStateChanged(Qt::ApplicationState state);

    void onFocusIn();
    void onFocusOut();
    bool isFocused() { return focused; }

    void osdAddMessage(unsigned int color, const char* msg);

    // called when the MP interface is changed
    void updateMPInterface(melonDS::MPInterfaceType type);

    void loadRecentFilesMenu(bool loadcfg);
    //void updateVideoSettings(bool glchange);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

signals:
    void screenLayoutChange();

public slots:
    void onLibNav(int hk);
    void onOverlayToggle();
    void onOverlayResume();

private slots:
    void onOpenFile();
    void onAddLibraryFolder();
    void onClickRecentFile();
    void onClearRecentFiles();
    void onBootFirmware();
    void onInsertCart();
    void onEjectCart();
    void onInsertGBACart();
    void onInsertGBAAddon();
    void onEjectGBACart();
    void onSaveState();
    void onLoadState();
    void onUndoStateLoad();
    void onImportSavefile();
    void onQuit();

    void onPause(bool checked);
    void onReset();
    void onStop();
    void onFrameStep();
    void onOpenPowerManagement();
    void onOpenDateTime();
    void onEnableCheats(bool checked);
    void onSetupCheats();
    void onCheatsDialogFinished(int res);
    void onROMInfo();
    void onRAMInfo();
    void onOpenTitleManager();
    void onMPNewInstance();
    void onLANStartHost();
    void onLANStartClient();
    void onNPStartHost();
    void onNPStartClient();
    void onNPTest();

    void onOpenEmuSettings();
    void onEmuSettingsDialogFinished(int res);
    void onOpenInputConfig();
    void onInputConfigFinished(int res);
    void onOpenVideoSettings();
    void onOpenCameraSettings();
    void onCameraSettingsFinished(int res);
    void onOpenAudioSettings();
    void onUpdateAudioVolume(int vol, int dsisync);
    void onUpdateAudioSettings();
    void onAudioSettingsFinished(int res);
    void onOpenMPSettings();
    void onMPSettingsFinished(int res);
    void onOpenWifiSettings();
    void onWifiSettingsFinished(int res);
    void onOpenFirmwareSettings();
    void onFirmwareSettingsFinished(int res);
    void onOpenPathSettings();
    void onPathSettingsFinished(int res);
    void onOpenInterfaceSettings();
    void onInterfaceSettingsFinished(int res);
    void onUpdateInterfaceSettings();
    void onChangeScreenSize();
    void onChangeScreenRotation(QAction* act);
    void onChangeScreenGap(QAction* act);
    void onChangeScreenLayout(QAction* act);
    void onChangeScreenSwap(bool checked);
    void onChangeScreenSizing(QAction* act);
    void onChangeScreenAspect(QAction* act);
    void onChangeIntegerScaling(bool checked);
    void onOpenNewWindow();
    void onChangeScreenFiltering(bool checked);
    void onChangeShowOSD(bool checked);
    void onChangeLimitFramerate(bool checked);
    void onChangeAudioSync(bool checked);

    void onTitleUpdate(QString title);

    void onEmuStart();
    void onEmuStop();
    void onEmuPause(bool pause);
    void onEmuReset();

    void onUpdateVideoSettings(bool glchange);

    void onFullscreenToggled();
    void onScreenEmphasisToggled();

private:
    virtual void closeEvent(QCloseEvent* event) override;

    QStringList currentROM;
    QStringList currentGBAROM;
    QList<QString> recentFileList;
    QMenu *recentMenu;
    void updateRecentFilesMenu();

    bool verifySetup();
    QString pickFileFromArchive(QString archiveFileName);
    QStringList pickROM(bool gba);
    void updateCartInserted(bool gba);

    void createScreenPanel();

    bool lanWarning(bool host);

    bool showOSD;

    bool hasOGL;

    bool pauseOnLostFocus;
    bool pausedManually;

    int windowID;
    bool enabledSaved;

    bool focused;

    EmuInstance* emuInstance;
    EmuThread* emuThread;

    Config::Table& globalCfg;
    Config::Table& localCfg;
    Config::Table windowCfg;

    // Debounce for GL context changes in video settings — accumulates glchange=true
    // signals and fires onUpdateVideoSettings(true) once when dialog closes.
    QTimer* m_videoSettingsTimer = nullptr;
    bool m_pendingGLChange = false;

public:
    ScreenPanel*       panel = nullptr;

    bool hasMenu;
    bool m_pendingLibrarySwitch = false;
    // Written on UI thread, read on emu thread — must be atomic.
    std::atomic<bool> m_libraryVisible {true};

    // Accessed directly by EmuThread and EmuInstance
    QAction* actQuit;
    QAction* actStop;
    QAction* actLimitFramerate;
    QAction* actScreenSwap;
    QAction* actNewWindow;
    QAction* actPause;
    QAction* actReset;
    QAction* actFrameStep;

    // Actions accessed by OverlayWidget
    QAction* actInsertGBACart;
    QList<QAction*> actInsertGBAAddon;
    QAction* actEjectGBACart;
    QAction* actImportSavefile;
    QAction* actSaveState[9];
    QAction* actLoadState[9];
    QAction* actEnableCheats;
    QAction* actSetupCheats;
    QAction* actPowerManagement;
    QAction* actDateTime;
    QAction* actTitleManager;
    QAction* actLANStartHost;
    QAction* actLANStartClient;
    QAction* actEmuSettings;
    QAction* actInputConfig;
    QAction* actVideoSettings;
    QAction* actAudioSettings;
    QAction* actIntegerScaling;
    QAction* actScreenFiltering;
    QAction* actShowOSD;
    QAction* actAudioSync;
    QActionGroup* grpScreenLayout;
    QActionGroup* grpScreenRotation;
    QActionGroup* grpScreenSizing;
    QActionGroup* grpScreenGap;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void moveEvent(QMoveEvent* event) override;

    QAction* actOpenROM;
    QAction* actAddLibraryFolder;
    QAction* actBootFirmware;
    QAction* actCurrentCart;
    QAction* actInsertCart;
    QAction* actEjectCart;
    QAction* actCurrentGBACart;
    QAction* actUndoStateLoad;
    QAction* actOpenConfig;

    QAction* actROMInfo;
    QAction* actRAMInfo;
    QAction* actMPNewInstance;
    QAction* actNPStartHost;
    QAction* actNPStartClient;
    QAction* actNPTest;

#ifdef __APPLE__
    QAction* actPreferences;
#endif
    QAction* actCameraSettings;
    QAction* actMPSettings;
    QAction* actWifiSettings;
    QAction* actFirmwareSettings;
    QAction* actPathSettings;
    QAction* actInterfaceSettings;
    QAction* actScreenSize[4];
    QAction* actScreenRotation[screenRot_MAX];
    QAction* actScreenGap[6];
    QAction* actScreenLayout[screenLayout_MAX];
    QAction* actScreenSizing[screenSizing_MAX];
    QActionGroup* grpScreenAspectTop;
    QAction** actScreenAspectTop;
    QActionGroup* grpScreenAspectBot;
    QAction** actScreenAspectBot;

    QAction* actAbout;

    // Game library — shown as background when no game is running
    QStackedWidget*    m_panelContainer = nullptr;
    LibraryController* m_library        = nullptr;
    OverlayWidget*     m_overlay        = nullptr;
};

#endif // WINDOW_H
