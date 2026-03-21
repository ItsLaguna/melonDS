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

#include <QStyleFactory>
#include <QPalette>
#include "InterfaceSettingsDialog.h"
#include "ui_InterfaceSettingsDialog.h"

#include "types.h"
#include "Platform.h"
#include "Config.h"
#include "main.h"

// Shared helper — same palette used in main.cpp on startup
static void applyDarkPalette()
{
    qApp->setStyle("fusion");
    QPalette dark;
    dark.setColor(QPalette::Window,          QColor(0x31, 0x36, 0x3b));
    dark.setColor(QPalette::WindowText,      QColor(0xfc, 0xfc, 0xfc));
    dark.setColor(QPalette::Base,            QColor(0x1b, 0x1e, 0x20));
    dark.setColor(QPalette::AlternateBase,   QColor(0x31, 0x36, 0x3b));
    dark.setColor(QPalette::ToolTipBase,     QColor(0x31, 0x36, 0x3b));
    dark.setColor(QPalette::ToolTipText,     QColor(0xfc, 0xfc, 0xfc));
    dark.setColor(QPalette::Text,            QColor(0xfc, 0xfc, 0xfc));
    dark.setColor(QPalette::BrightText,      QColor(0xff, 0xff, 0xff));
    dark.setColor(QPalette::Button,          QColor(0x3b, 0x41, 0x47));
    dark.setColor(QPalette::ButtonText,      QColor(0xfc, 0xfc, 0xfc));
    dark.setColor(QPalette::Light,           QColor(0x48, 0x4e, 0x55));
    dark.setColor(QPalette::Midlight,        QColor(0x3f, 0x45, 0x4b));
    dark.setColor(QPalette::Mid,             QColor(0x2d, 0x32, 0x36));
    dark.setColor(QPalette::Dark,            QColor(0x1b, 0x1e, 0x20));
    dark.setColor(QPalette::Shadow,          QColor(0x0e, 0x10, 0x11));
    dark.setColor(QPalette::Highlight,       QColor(0x29, 0x80, 0xb9));
    dark.setColor(QPalette::HighlightedText, QColor(0xfc, 0xfc, 0xfc));
    dark.setColor(QPalette::Link,            QColor(0x29, 0x80, 0xb9));
    dark.setColor(QPalette::LinkVisited,     QColor(0x7f, 0x8c, 0x8d));
    dark.setColor(QPalette::Disabled, QPalette::WindowText,      QColor(0x6e, 0x74, 0x7a));
    dark.setColor(QPalette::Disabled, QPalette::Text,            QColor(0x6e, 0x74, 0x7a));
    dark.setColor(QPalette::Disabled, QPalette::ButtonText,      QColor(0x6e, 0x74, 0x7a));
    dark.setColor(QPalette::Disabled, QPalette::Highlight,       QColor(0x3b, 0x41, 0x47));
    dark.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(0x6e, 0x74, 0x7a));
    qApp->setPalette(dark);
}

InterfaceSettingsDialog* InterfaceSettingsDialog::currentDlg = nullptr;
InterfaceSettingsDialog::InterfaceSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::InterfaceSettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    emuInstance = ((MainWindow*)parent)->getEmuInstance();

    auto& cfg = emuInstance->getGlobalConfig();

    ui->cbMouseHide->setChecked(cfg.GetBool("Mouse.Hide"));
    ui->spinMouseHideSeconds->setEnabled(ui->cbMouseHide->isChecked());
    ui->spinMouseHideSeconds->setValue(cfg.GetInt("Mouse.HideSeconds"));
    ui->cbPauseLostFocus->setChecked(cfg.GetBool("PauseLostFocus"));
    ui->cbMuteFastForward->setChecked(cfg.GetBool("MuteFastForward"));
    ui->spinTargetFPS->setValue(cfg.GetDouble("TargetFPS"));
    ui->spinFFW->setValue(cfg.GetDouble("FastForwardFPS"));
    ui->spinSlow->setValue(cfg.GetDouble("SlowmoFPS"));

    const QList<QString> themeKeys = QStyleFactory::keys();
    const QString currentTheme = qApp->style()->objectName();
    QString cfgTheme = cfg.GetQString("UITheme");

    ui->cbxUITheme->addItem("System default", "");
    ui->cbxUITheme->addItem("Dark", "dark");
    if (cfgTheme == "dark")
        ui->cbxUITheme->setCurrentIndex(1);

    for (int i = 0; i < themeKeys.length(); i++)
    {
        ui->cbxUITheme->addItem(themeKeys[i], themeKeys[i]);
        if (!cfgTheme.isEmpty() && cfgTheme != "dark"
                && themeKeys[i].compare(currentTheme, Qt::CaseInsensitive) == 0)
            ui->cbxUITheme->setCurrentIndex(i + 2); // +2 to account for the extra Breeze Dark entry
    }
}

InterfaceSettingsDialog::~InterfaceSettingsDialog()
{
    delete ui;
}

void InterfaceSettingsDialog::on_cbMouseHide_clicked()
{
    ui->spinMouseHideSeconds->setEnabled(ui->cbMouseHide->isChecked());
}

void InterfaceSettingsDialog::on_pbClean_clicked()
{
    ui->spinTargetFPS->setValue(60.0000);
}

void InterfaceSettingsDialog::on_pbAccurate_clicked()
{
    ui->spinTargetFPS->setValue(59.8261);
}

void InterfaceSettingsDialog::on_pb2x_clicked()
{
    ui->spinFFW->setValue(ui->spinTargetFPS->value() * 2.0);
}

void InterfaceSettingsDialog::on_pb3x_clicked()
{
    ui->spinFFW->setValue(ui->spinTargetFPS->value() * 3.0);
}

void InterfaceSettingsDialog::on_pbMAX_clicked()
{
    ui->spinFFW->setValue(1000.0);
}

void InterfaceSettingsDialog::on_pbHalf_clicked()
{
    ui->spinSlow->setValue(ui->spinTargetFPS->value() / 2.0);
}

void InterfaceSettingsDialog::on_pbQuarter_clicked()
{
    ui->spinSlow->setValue(ui->spinTargetFPS->value() / 4.0);
}

void InterfaceSettingsDialog::done(int r)
{
    if (!((MainWindow*)parent())->getEmuInstance())
    {
        QDialog::done(r);
        closeDlg();
        return;
    }

    if (r == QDialog::Accepted)
    {
        auto& cfg = emuInstance->getGlobalConfig();

        cfg.SetBool("Mouse.Hide", ui->cbMouseHide->isChecked());
        cfg.SetInt("Mouse.HideSeconds", ui->spinMouseHideSeconds->value());
        cfg.SetBool("PauseLostFocus", ui->cbPauseLostFocus->isChecked());
        cfg.SetBool("MuteFastForward", ui->cbMuteFastForward->isChecked());

        double val = ui->spinTargetFPS->value();
        if (val == 0.0) cfg.SetDouble("TargetFPS", 0.0001);
        else cfg.SetDouble("TargetFPS", val);
        
        val = ui->spinFFW->value();
        if (val == 0.0) cfg.SetDouble("FastForwardFPS", 0.0001);
        else cfg.SetDouble("FastForwardFPS", val);
        
        val = ui->spinSlow->value();
        if (val == 0.0) cfg.SetDouble("SlowmoFPS", 0.0001);
        else cfg.SetDouble("SlowmoFPS", val);

        QString themeName = ui->cbxUITheme->currentData().toString();
        cfg.SetQString("UITheme", themeName);

        Config::Save();

        if (themeName == "dark")
        {
            applyDarkPalette();
        }
        else if (!themeName.isEmpty())
        {
            qApp->setPalette(QApplication::style()->standardPalette());
            qApp->setStyle(themeName);
        }
        else
        {
            qApp->setPalette(QApplication::style()->standardPalette());
            qApp->setStyle(*systemThemeName);
        }

        emit updateInterfaceSettings();
    }

    QDialog::done(r);

    closeDlg();
}
