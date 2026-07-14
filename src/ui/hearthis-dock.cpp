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

#include "hearthis-dock.hpp"
#include "settings-dialog.hpp"
#include "volume-meter.hpp"
#include "../config-store.hpp"

#include <obs-module.h>
#include <plugin-support.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

static QString text(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

static void setDotColor(QLabel *dot, const char *color)
{
	dot->setStyleSheet(QStringLiteral("color: %1; font-size: 16px;").arg(QLatin1String(color)));
}

HearthisDock::HearthisDock(StreamController *controller_, QWidget *parent) : QFrame(parent), controller(controller_)
{
	setFrameShape(QFrame::NoFrame);

	statusDot = new QLabel(QStringLiteral("●"), this);
	statusText = new QLabel(this);
	timerLabel = new QLabel(QStringLiteral("00:00:00"), this);

	auto *statusRow = new QHBoxLayout();
	statusRow->addWidget(statusDot);
	statusRow->addWidget(statusText, 1);
	statusRow->addWidget(timerLabel);

	startStopButton = new QPushButton(this);
	connect(startStopButton, &QPushButton::clicked, this, &HearthisDock::onStartStopClicked);

	levelMeter = new VolumeMeter(this);
	levelMeter->setTrack(ConfigStore::load().track);

	errorLabel = new QLabel(this);
	errorLabel->setWordWrap(true);
	errorLabel->setStyleSheet(QStringLiteral("color: #d9534f;"));
	errorLabel->hide();

	settingsButton = new QPushButton(text("Dock.Settings"), this);
	connect(settingsButton, &QPushButton::clicked, this, &HearthisDock::onSettingsClicked);

	auto *versionLabel = new QLabel(QStringLiteral("v") + QString::fromUtf8(PLUGIN_VERSION), this);
	versionLabel->setStyleSheet(QStringLiteral("color: #808080; font-size: 10px;"));

	auto *bottomRow = new QHBoxLayout();
	bottomRow->addWidget(versionLabel);
	bottomRow->addStretch(1);
	bottomRow->addWidget(settingsButton);

	auto *layout = new QVBoxLayout(this);
	layout->addLayout(statusRow);
	layout->addWidget(startStopButton);
	layout->addWidget(levelMeter);
	layout->addWidget(errorLabel);
	layout->addLayout(bottomRow);
	layout->addStretch(1);

	tickTimer = new QTimer(this);
	tickTimer->setInterval(1000);
	connect(tickTimer, &QTimer::timeout, this, &HearthisDock::onTick);

	connect(controller, &StreamController::stateChanged, this, &HearthisDock::onStateChanged);

	onStateChanged(StreamState::Idle, QString());
}

void HearthisDock::onStartStopClicked()
{
	const StreamState state = controller->state();
	if (state == StreamState::Live || state == StreamState::Reconnecting) {
		controller->stop();
		return;
	}

	if (!controller->canStart())
		return;

	if (!ConfigStore::load().hasCredentials()) {
		if (!SettingsDialog::edit(this))
			return;
		levelMeter->setTrack(ConfigStore::load().track);
	}

	controller->start();
}

void HearthisDock::onSettingsClicked()
{
	if (SettingsDialog::edit(this))
		levelMeter->setTrack(ConfigStore::load().track);
}

void HearthisDock::onStateChanged(StreamState state, const QString &message)
{
	errorLabel->hide();

	switch (state) {
	case StreamState::Idle:
		setDotColor(statusDot, "#808080");
		statusText->setText(text("Dock.Status.Ready"));
		startStopButton->setText(text("Dock.Start"));
		startStopButton->setEnabled(true);
		tickTimer->stop();
		timerLabel->setText(QStringLiteral("00:00:00"));
		break;
	case StreamState::Connecting:
		setDotColor(statusDot, "#e0a800");
		statusText->setText(text("Dock.Status.Connecting"));
		startStopButton->setText(text("Dock.Start"));
		startStopButton->setEnabled(false);
		break;
	case StreamState::Live:
		setDotColor(statusDot, "#d32f2f");
		statusText->setText(text("Dock.Status.Live"));
		startStopButton->setText(text("Dock.Stop"));
		startStopButton->setEnabled(true);
		if (!tickTimer->isActive()) {
			/* fresh start (not a reconnect): restart the timer */
			liveTime.start();
			lastBytes = controller->totalBytes();
			lastKbps = 0;
			timerLabel->setText(QStringLiteral("00:00:00"));
			tickTimer->start();
		}
		break;
	case StreamState::Reconnecting:
		setDotColor(statusDot, "#ff8c00");
		statusText->setText(text("Dock.Status.Reconnecting"));
		startStopButton->setText(text("Dock.Stop"));
		startStopButton->setEnabled(true);
		break;
	case StreamState::Stopping:
		setDotColor(statusDot, "#808080");
		statusText->setText(text("Dock.Status.Stopping"));
		startStopButton->setEnabled(false);
		tickTimer->stop();
		break;
	case StreamState::Error:
		setDotColor(statusDot, "#d9534f");
		statusText->setText(text("Dock.Status.Error"));
		startStopButton->setText(text("Dock.Start"));
		startStopButton->setEnabled(true);
		tickTimer->stop();
		if (!message.isEmpty()) {
			errorLabel->setText(message);
			errorLabel->show();
		}
		break;
	}
}

void HearthisDock::onTick()
{
	const qint64 secs = liveTime.elapsed() / 1000;
	timerLabel->setText(QStringLiteral("%1:%2:%3")
				    .arg(secs / 3600, 2, 10, QLatin1Char('0'))
				    .arg((secs / 60) % 60, 2, 10, QLatin1Char('0'))
				    .arg(secs % 60, 2, 10, QLatin1Char('0')));

	const uint64_t bytes = controller->totalBytes();
	if (bytes >= lastBytes)
		lastKbps = int((bytes - lastBytes) * 8 / 1000);
	lastBytes = bytes;
	if (controller->state() == StreamState::Live)
		statusText->setText(text("Dock.LiveStats").arg(lastKbps));
}
