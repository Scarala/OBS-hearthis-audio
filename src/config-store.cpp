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

#include "config-store.hpp"

#include <obs-module.h>
#include <util/platform.h>

static const char *CONFIG_FILE = "config.json";

QString HearthisConfig::effectiveMount() const
{
	QString m = mount.trimmed();
	if (m.isEmpty())
		m = username.trimmed() + QStringLiteral(".ogg");
	while (m.startsWith(QLatin1Char('/')))
		m.remove(0, 1);
	return m;
}

QString HearthisConfig::effectiveDescription() const
{
	/* hashtag order matches the original Python script */
	QString desc = description;
	if (tagOnAir)
		desc.prepend(QStringLiteral("#OnAir "));
	if (tagRecording)
		desc.prepend(QStringLiteral("#Recording "));
	return desc.trimmed();
}

HearthisConfig ConfigStore::load()
{
	HearthisConfig cfg;

	char *path = obs_module_config_path(CONFIG_FILE);
	obs_data_t *data = obs_data_create_from_json_file_safe(path, "bak");
	bfree(path);
	if (!data)
		return cfg;

	cfg.username = QString::fromUtf8(obs_data_get_string(data, "username"));
	cfg.password = QString::fromUtf8(obs_data_get_string(data, "password"));
	cfg.streamName = QString::fromUtf8(obs_data_get_string(data, "stream_name"));
	cfg.description = QString::fromUtf8(obs_data_get_string(data, "description"));
	cfg.tagOnAir = obs_data_get_bool(data, "tag_onair");
	cfg.tagRecording = obs_data_get_bool(data, "tag_recording");
	cfg.track = (int)obs_data_get_int(data, "track");
	cfg.bitrate = (int)obs_data_get_int(data, "bitrate");
	/* read as int for compatibility with configs saved by older plugin versions
	 * (0 = off, 1 = with OBS stream, 2 = with OBS recording; any nonzero value now means "on") */
	cfg.autoStart = obs_data_get_int(data, "auto_start") != 0;

	const char *server = obs_data_get_string(data, "server");
	if (server && *server)
		cfg.server = QString::fromUtf8(server);
	int port = (int)obs_data_get_int(data, "port");
	if (port > 0 && port <= 65535)
		cfg.port = port;
	cfg.mount = QString::fromUtf8(obs_data_get_string(data, "mount"));

	obs_data_release(data);

	if (cfg.track < 1 || cfg.track > MAX_AUDIO_MIXES)
		cfg.track = 1;
	if (cfg.bitrate < 32 || cfg.bitrate > 320)
		cfg.bitrate = 192;

	return cfg;
}

void ConfigStore::save(const HearthisConfig &cfg)
{
	obs_data_t *data = obs_data_create();
	obs_data_set_string(data, "username", cfg.username.toUtf8().constData());
	obs_data_set_string(data, "password", cfg.password.toUtf8().constData());
	obs_data_set_string(data, "stream_name", cfg.streamName.toUtf8().constData());
	obs_data_set_string(data, "description", cfg.description.toUtf8().constData());
	obs_data_set_bool(data, "tag_onair", cfg.tagOnAir);
	obs_data_set_bool(data, "tag_recording", cfg.tagRecording);
	obs_data_set_int(data, "track", cfg.track);
	obs_data_set_int(data, "bitrate", cfg.bitrate);
	obs_data_set_int(data, "auto_start", cfg.autoStart ? 1 : 0);
	obs_data_set_string(data, "server", cfg.server.toUtf8().constData());
	obs_data_set_int(data, "port", cfg.port);
	obs_data_set_string(data, "mount", cfg.mount.toUtf8().constData());

	char *dir = obs_module_config_path("");
	if (dir) {
		os_mkdirs(dir);
		bfree(dir);
	}

	char *path = obs_module_config_path(CONFIG_FILE);
	if (path) {
		obs_data_save_json_safe(data, path, "tmp", "bak");
		bfree(path);
	}

	obs_data_release(data);
}
