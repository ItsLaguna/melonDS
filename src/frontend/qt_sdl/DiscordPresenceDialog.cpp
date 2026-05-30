// src/frontend/qt_sdl/DiscordPresenceDialog.cpp

#include "DiscordPresenceDialog.h"
#include "DiscordPresence.h"
#include "Config.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QRunnable>
#include <QThreadPool>
#include <QVBoxLayout>

DiscordPresenceDialog::DiscordPresenceDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Discord Rich Presence"));
    setModal(true);
    setMinimumWidth(400);

    Config::Table cfg = Config::GetGlobalTable();

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    // ── Enable toggle ────────────────────────────────────────────────────────
    m_enabledCheck = new QCheckBox(tr("Enable Discord Rich Presence"), this);
    m_enabledCheck->setChecked(cfg.GetBool("DiscordPresence.Enabled"));
    layout->addWidget(m_enabledCheck);

    // ── App ID ───────────────────────────────────────────────────────────────
    auto* appIDGroup  = new QGroupBox(tr("Application ID"), this);
    auto* appIDLayout = new QFormLayout(appIDGroup);

    m_appIDEdit = new QLineEdit(this);
    m_appIDEdit->setPlaceholderText(tr("e.g. 1234567890123456789"));
    m_appIDEdit->setText(cfg.GetQString("DiscordPresence.AppID"));
    m_appIDEdit->setToolTip(tr(
        "Your Discord Developer Application Client ID.\n"
        "Create one at https://discord.com/developers/applications\n"
        "Leave blank to use the default melonDS application."));

    auto* appIDHint = new QLabel(
        tr("<a href=\"https://discord.com/developers/applications\">Create an application</a> "
           "to use your own presence."), this);
    appIDHint->setOpenExternalLinks(true);
    appIDHint->setWordWrap(true);

    appIDLayout->addRow(tr("Client ID:"), m_appIDEdit);
    appIDLayout->addRow(appIDHint);
    layout->addWidget(appIDGroup);

    // ── Second detail line ───────────────────────────────────────────────────
    auto* detailGroup  = new QGroupBox(tr("Game info line"), this);
    auto* detailLayout = new QVBoxLayout(detailGroup);

    m_showCodeRadio   = new QRadioButton(tr("Show game code (e.g. NTRE)"), this);
    m_showRegionRadio = new QRadioButton(tr("Show region name (e.g. USA)"), this);

    const bool showCode = cfg.GetBool("DiscordPresence.ShowGameCode");
    m_showCodeRadio->setChecked(showCode);
    m_showRegionRadio->setChecked(!showCode);

    detailLayout->addWidget(m_showCodeRadio);
    detailLayout->addWidget(m_showRegionRadio);
    layout->addWidget(detailGroup);

    // ── Cover art type ───────────────────────────────────────────────────────
    auto* artGroup  = new QGroupBox(tr("Cover art"), this);
    auto* artLayout = new QFormLayout(artGroup);

    m_artTypeCombo = new QComboBox(this);
    m_artTypeCombo->addItem(tr("No cover art (melonDS logo only)"), QString("none|"));
    m_artTypeCombo->addItem(tr("Front cover (cover)"),              QString("cover|jpg"));
    m_artTypeCombo->addItem(tr("Cartridge (cart)"),                 QString("cart|png"));

    const QString saved = cfg.GetQString("DiscordPresence.ArtType");
    for (int i = 0; i < m_artTypeCombo->count(); ++i)
    {
        if (m_artTypeCombo->itemData(i).toString() == saved)
        {
            m_artTypeCombo->setCurrentIndex(i);
            break;
        }
    }

    auto* artHint = new QLabel(
        tr("Cover images are fetched from <a href=\"https://www.gametdb.com\">GameTDB</a>."),
        this);
    artHint->setOpenExternalLinks(true);

    artLayout->addRow(tr("Image type:"), m_artTypeCombo);
    artLayout->addRow(artHint);
    layout->addWidget(artGroup);

    // ── Buttons ──────────────────────────────────────────────────────────────
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &DiscordPresenceDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void DiscordPresenceDialog::onAccepted()
{
    Config::Table cfg = Config::GetGlobalTable();

    cfg.SetBool("DiscordPresence.Enabled",      m_enabledCheck->isChecked());
    cfg.SetString("DiscordPresence.AppID",      m_appIDEdit->text().trimmed().toStdString());
    cfg.SetBool("DiscordPresence.ShowGameCode",  m_showCodeRadio->isChecked());
    cfg.SetString("DiscordPresence.ArtType",
                  m_artTypeCombo->currentData().toString().toStdString());

    Config::Save();

    // reinit() connects a socket and may do a URL check — run off the UI thread
    // so the dialog closes immediately without freezing.
    QThreadPool::globalInstance()->start(
        []() { DiscordPresence::get().reinit(); });

    accept();
}
