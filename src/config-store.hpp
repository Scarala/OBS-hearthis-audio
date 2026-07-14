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

#include <QString>

struct HearthisConfig {
	QString username;
	QString password;
	QString streamName;
	QString description;
	bool tagOnAir = false;
	bool tagRecording = false;
	int track = 1;     // 1..MAX_AUDIO_MIXES
	int bitrate = 192; // kbit/s
	bool autoStart = false; // start/stop the Hearthis stream together with the OBS stream
	QString server = QStringLiteral("streamlive2.hearthis.at");
	int port = 8080;
	QString mount; // empty => "<username>.ogg"

	bool hasCredentials() const { return !username.isEmpty() && !password.isEmpty(); }
	QString effectiveMount() const;
	QString effectiveDescription() const;
};

namespace ConfigStore {
HearthisConfig load();
void save(const HearthisConfig &config);
}
