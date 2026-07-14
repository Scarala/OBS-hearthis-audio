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

#pragma once

#include <QElapsedTimer>
#include <QFrame>

#include "../stream-controller.hpp"

class QLabel;
class QPushButton;
class QTimer;
class VolumeMeter;

class HearthisDock : public QFrame {
	Q_OBJECT

public:
	explicit HearthisDock(StreamController *controller, QWidget *parent = nullptr);

private slots:
	void onStartStopClicked();
	void onSettingsClicked();
	void onStateChanged(StreamState state, const QString &message);
	void onTick();

private:
	StreamController *controller;

	QLabel *statusDot;
	QLabel *statusText;
	QLabel *timerLabel;
	QLabel *errorLabel;
	QPushButton *startStopButton;
	QPushButton *settingsButton;
	VolumeMeter *levelMeter;

	QTimer *tickTimer;
	QElapsedTimer liveTime;
	uint64_t lastBytes = 0;
	int lastKbps = 0;
};
