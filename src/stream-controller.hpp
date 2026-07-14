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

#include <QObject>
#include <QString>

#include <obs.h>

enum class StreamState { Idle, Connecting, Live, Reconnecting, Stopping, Error };

class StreamController : public QObject {
	Q_OBJECT

public:
	explicit StreamController(QObject *parent = nullptr);
	~StreamController() override;

	StreamState state() const { return currentState; }
	bool canStart() const { return currentState == StreamState::Idle || currentState == StreamState::Error; }
	uint64_t totalBytes() const;

	void start();
	void stop();

signals:
	void stateChanged(StreamState state, const QString &message);

private:
	static void onOutputStart(void *param, calldata_t *data);
	static void onOutputStop(void *param, calldata_t *data);
	static void onOutputReconnect(void *param, calldata_t *data);
	static void onOutputReconnectSuccess(void *param, calldata_t *data);

	/* Sets the state directly (UI thread only) */
	void applyState(StreamState state, const QString &message = QString());
	/* Posts a state change from any thread (OBS signal callbacks) */
	void postState(StreamState state, const QString &message = QString());

	void connectOutputSignals();
	void disconnectOutputSignals();
	void releaseOutput();

	obs_output_t *output = nullptr;
	StreamState currentState = StreamState::Idle;
};
