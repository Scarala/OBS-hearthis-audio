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

#include "stream-controller.hpp"
#include "config-store.hpp"

#include <obs-module.h>
#include <plugin-support.h>

#include <QMetaObject>
#include <QUrl>

static QString percentEncode(const QString &value)
{
	return QString::fromLatin1(QUrl::toPercentEncoding(value));
}

StreamController::StreamController(QObject *parent) : QObject(parent) {}

StreamController::~StreamController()
{
	if (output && obs_output_active(output))
		obs_output_force_stop(output);
	releaseOutput();
}

uint64_t StreamController::totalBytes() const
{
	return output ? obs_output_get_total_bytes(output) : 0;
}

void StreamController::applyState(StreamState state, const QString &message)
{
	currentState = state;
	emit stateChanged(state, message);
}

void StreamController::postState(StreamState state, const QString &message)
{
	QMetaObject::invokeMethod(this, [this, state, message]() { applyState(state, message); }, Qt::QueuedConnection);
}

void StreamController::start()
{
	if (output && obs_output_active(output))
		return;
	if (currentState == StreamState::Connecting || currentState == StreamState::Stopping)
		return;

	HearthisConfig cfg = ConfigStore::load();
	if (!cfg.hasCredentials()) {
		applyState(StreamState::Error, QString::fromUtf8(obs_module_text("Error.NoCredentials")));
		return;
	}

	releaseOutput();

	const QString url = QStringLiteral("icecast://%1:%2@%3:%4/%5")
				    .arg(percentEncode(cfg.username), percentEncode(cfg.password), cfg.server.trimmed())
				    .arg(cfg.port)
				    .arg(cfg.effectiveMount());

	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "url", url.toUtf8().constData());
	obs_data_set_int(settings, "bitrate", cfg.bitrate);
	obs_data_set_string(settings, "ice_name", cfg.streamName.trimmed().toUtf8().constData());
	obs_data_set_string(settings, "ice_description", cfg.effectiveDescription().toUtf8().constData());

	output = obs_output_create("hearthis_icecast_output", "hearthis-audio-output", settings, nullptr);
	obs_data_release(settings);

	if (!output) {
		applyState(StreamState::Error, QString::fromUtf8(obs_module_text("Error.OutputCreateFailed")));
		return;
	}

	obs_output_set_media(output, nullptr, obs_get_audio());
	obs_output_set_mixer(output, (size_t)(cfg.track - 1));
	obs_output_set_reconnect_settings(output, 20, 2);
	connectOutputSignals();

	obs_log(LOG_INFO, "starting hearthis stream: track %d, %d kbit/s, mount '%s'", cfg.track, cfg.bitrate,
		cfg.effectiveMount().toUtf8().constData());

	applyState(StreamState::Connecting);

	if (!obs_output_start(output)) {
		const char *err = obs_output_get_last_error(output);
		applyState(StreamState::Error, err && *err ? QString::fromUtf8(err)
							   : QString::fromUtf8(obs_module_text("Error.StartFailed")));
	}
}

void StreamController::stop()
{
	if (!output)
		return;
	if (currentState == StreamState::Live || currentState == StreamState::Connecting ||
	    currentState == StreamState::Reconnecting) {
		applyState(StreamState::Stopping);
		obs_output_stop(output);
	}
}

void StreamController::onOutputStart(void *param, calldata_t *data)
{
	UNUSED_PARAMETER(data);
	auto *self = static_cast<StreamController *>(param);
	self->postState(StreamState::Live);
}

void StreamController::onOutputReconnect(void *param, calldata_t *data)
{
	UNUSED_PARAMETER(data);
	auto *self = static_cast<StreamController *>(param);
	self->postState(StreamState::Reconnecting);
}

void StreamController::onOutputReconnectSuccess(void *param, calldata_t *data)
{
	UNUSED_PARAMETER(data);
	auto *self = static_cast<StreamController *>(param);
	self->postState(StreamState::Live);
}

void StreamController::onOutputStop(void *param, calldata_t *data)
{
	auto *self = static_cast<StreamController *>(param);
	const long long code = calldata_int(data, "code");

	if (code == OBS_OUTPUT_SUCCESS) {
		self->postState(StreamState::Idle);
		return;
	}

	auto *out = static_cast<obs_output_t *>(calldata_ptr(data, "output"));
	const char *lastError = out ? obs_output_get_last_error(out) : nullptr;

	QString message;
	switch (code) {
	case OBS_OUTPUT_CONNECT_FAILED:
		message = QString::fromUtf8(obs_module_text("Error.ConnectFailed"));
		break;
	case OBS_OUTPUT_DISCONNECTED:
		message = QString::fromUtf8(obs_module_text("Error.Disconnected"));
		break;
	default:
		message = QString::fromUtf8(obs_module_text("Error.Generic"));
		break;
	}
	if (lastError && *lastError)
		message += QStringLiteral("\n") + QString::fromUtf8(lastError);

	self->postState(StreamState::Error, message);
}

void StreamController::connectOutputSignals()
{
	signal_handler_t *sh = obs_output_get_signal_handler(output);
	signal_handler_connect(sh, "start", onOutputStart, this);
	signal_handler_connect(sh, "stop", onOutputStop, this);
	signal_handler_connect(sh, "reconnect", onOutputReconnect, this);
	signal_handler_connect(sh, "reconnect_success", onOutputReconnectSuccess, this);
}

void StreamController::disconnectOutputSignals()
{
	if (!output)
		return;
	signal_handler_t *sh = obs_output_get_signal_handler(output);
	signal_handler_disconnect(sh, "start", onOutputStart, this);
	signal_handler_disconnect(sh, "stop", onOutputStop, this);
	signal_handler_disconnect(sh, "reconnect", onOutputReconnect, this);
	signal_handler_disconnect(sh, "reconnect_success", onOutputReconnectSuccess, this);
}

void StreamController::releaseOutput()
{
	disconnectOutputSignals();
	if (output) {
		obs_output_release(output);
		output = nullptr;
	}
}
