#pragma once
// src/frontend/qt_sdl/DiscordPresenceDialog.h

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QRadioButton;

class DiscordPresenceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DiscordPresenceDialog(QWidget* parent = nullptr);

private slots:
    void onAccepted();

private:
    QCheckBox*    m_enabledCheck;
    QLineEdit*    m_appIDEdit;
    QRadioButton* m_showCodeRadio;
    QRadioButton* m_showRegionRadio;
    QComboBox*    m_artTypeCombo;
};
