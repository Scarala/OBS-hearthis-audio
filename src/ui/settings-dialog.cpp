/*
Hearthis.at Audio Stream
Copyright (C) 2026 Scarala <scarala@googlemail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "settings-dialog.hpp"
#include "../config-store.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <cstdio>

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

static QString text(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle(text("Settings.Title"));
	setMinimumWidth(420);

	/* Credentials */
	auto *credentialsGroup = new QGroupBox(text("Settings.Credentials"), this);
	auto *credentialsForm = new QFormLayout(credentialsGroup);

	usernameEdit = new QLineEdit(credentialsGroup);
	credentialsForm->addRow(text("Settings.Username"), usernameEdit);

	passwordEdit = new QLineEdit(credentialsGroup);
	passwordEdit->setEchoMode(QLineEdit::Password);
	auto *showPassword = new QToolButton(credentialsGroup);
	showPassword->setText(QStringLiteral("👁"));
	showPassword->setCheckable(true);
	showPassword->setToolTip(text("Settings.ShowPassword"));
	connect(showPassword, &QToolButton::toggled, this, [this](bool visible) {
		passwordEdit->setEchoMode(visible ? QLineEdit::Normal : QLineEdit::Password);
	});
	auto *passwordRow = new QHBoxLayout();
	passwordRow->setContentsMargins(0, 0, 0, 0);
	passwordRow->addWidget(passwordEdit, 1);
	passwordRow->addWidget(showPassword);
	credentialsForm->addRow(text("Settings.Password"), passwordRow);

	auto *livePageButton = new QPushButton(text("Settings.OpenLivePage"), credentialsGroup);
	connect(livePageButton, &QPushButton::clicked, this,
		[]() { QDesktopServices::openUrl(QUrl(QStringLiteral("https://hearthis.at/live/#audio-only"))); });
	credentialsForm->addRow(QString(), livePageButton);

	/* Stream metadata */
	auto *metadataGroup = new QGroupBox(text("Settings.Metadata"), this);
	auto *metadataForm = new QFormLayout(metadataGroup);

	nameEdit = new QLineEdit(metadataGroup);
	metadataForm->addRow(text("Settings.StreamName"), nameEdit);
	descriptionEdit = new QLineEdit(metadataGroup);
	metadataForm->addRow(text("Settings.Description"), descriptionEdit);

	onAirCheck = new QCheckBox(text("Settings.TagOnAir"), metadataGroup);
	metadataForm->addRow(QString(), onAirCheck);
	recordingCheck = new QCheckBox(text("Settings.TagRecording"), metadataGroup);
	metadataForm->addRow(QString(), recordingCheck);

	/* Audio */
	auto *audioGroup = new QGroupBox(text("Settings.Audio"), this);
	auto *audioForm = new QFormLayout(audioGroup);

	trackCombo = new QComboBox(audioGroup);
	config_t *profile = obs_frontend_get_profile_config();
	for (int i = 1; i <= MAX_AUDIO_MIXES; i++) {
		QString label = text("Settings.Track").arg(i);
		char key[16];
		snprintf(key, sizeof(key), "Track%dName", i);
		const char *trackName = profile ? config_get_string(profile, "AdvOut", key) : nullptr;
		if (trackName && *trackName)
			label += QStringLiteral(" – ") + QString::fromUtf8(trackName);
		trackCombo->addItem(label, i);
	}
	audioForm->addRow(text("Settings.TrackLabel"), trackCombo);

	bitrateCombo = new QComboBox(audioGroup);
	for (int rate : {32, 64, 96, 128, 160, 192, 256, 320})
		bitrateCombo->addItem(text("Settings.BitrateValue").arg(rate), rate);
	audioForm->addRow(text("Settings.Bitrate"), bitrateCombo);

	/* Advanced */
	auto *advancedGroup = new QGroupBox(text("Settings.Advanced"), this);
	auto *advancedForm = new QFormLayout(advancedGroup);

	serverEdit = new QLineEdit(advancedGroup);
	advancedForm->addRow(text("Settings.Server"), serverEdit);
	portSpin = new QSpinBox(advancedGroup);
	portSpin->setRange(1, 65535);
	advancedForm->addRow(text("Settings.Port"), portSpin);

	autoStartCheck = new QCheckBox(text("Settings.AutoStart"), advancedGroup);
	advancedForm->addRow(QString(), autoStartCheck);

	/* Buttons */
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
	buttons->button(QDialogButtonBox::Save)->setText(text("Settings.Save"));
	buttons->button(QDialogButtonBox::Cancel)->setText(text("Settings.Cancel"));
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	auto *layout = new QVBoxLayout(this);
	layout->addWidget(credentialsGroup);
	layout->addWidget(metadataGroup);
	layout->addWidget(audioGroup);
	layout->addWidget(advancedGroup);
	layout->addWidget(buttons);

	loadConfig();
}

void SettingsDialog::loadConfig()
{
	const HearthisConfig cfg = ConfigStore::load();

	usernameEdit->setText(cfg.username);
	passwordEdit->setText(cfg.password);
	nameEdit->setText(cfg.streamName);
	descriptionEdit->setText(cfg.description);
	onAirCheck->setChecked(cfg.tagOnAir);
	recordingCheck->setChecked(cfg.tagRecording);

	int trackIndex = trackCombo->findData(cfg.track);
	trackCombo->setCurrentIndex(trackIndex >= 0 ? trackIndex : 0);
	int bitrateIndex = bitrateCombo->findData(cfg.bitrate);
	bitrateCombo->setCurrentIndex(bitrateIndex >= 0 ? bitrateIndex : bitrateCombo->findData(192));

	serverEdit->setText(cfg.server);
	portSpin->setValue(cfg.port);

	autoStartCheck->setChecked(cfg.autoStart);
}

void SettingsDialog::saveConfig()
{
	HearthisConfig cfg;
	cfg.username = usernameEdit->text().trimmed();
	cfg.password = passwordEdit->text();
	cfg.streamName = nameEdit->text();
	cfg.description = descriptionEdit->text();
	cfg.tagOnAir = onAirCheck->isChecked();
	cfg.tagRecording = recordingCheck->isChecked();
	cfg.track = trackCombo->currentData().toInt();
	cfg.bitrate = bitrateCombo->currentData().toInt();
	cfg.server = serverEdit->text().trimmed();
	cfg.port = portSpin->value();
	cfg.autoStart = autoStartCheck->isChecked();

	ConfigStore::save(cfg);
}

bool SettingsDialog::edit(QWidget *parent)
{
	SettingsDialog dialog(parent);
	if (dialog.exec() != QDialog::Accepted)
		return false;
	dialog.saveConfig();
	return true;
}
