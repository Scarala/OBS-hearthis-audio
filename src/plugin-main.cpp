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

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>

#include "config-store.hpp"
#include "hearthis-output.hpp"
#include "stream-controller.hpp"
#include "ui/hearthis-dock.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static StreamController *controller = nullptr;

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Audio-only streaming to Hearthis.at using the OBS-internal FFmpeg";
}

/* Auto start/stop coupling: follow the main OBS stream when enabled */
static void onFrontendEvent(enum obs_frontend_event event, void *)
{
	if (!controller)
		return;

	switch (event) {
	case OBS_FRONTEND_EVENT_EXIT:
		controller->stop();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		if (ConfigStore::load().autoStart && controller->canStart())
			controller->start();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		if (ConfigStore::load().autoStart)
			controller->stop();
		break;
	default:
		break;
	}
}

bool obs_module_load(void)
{
	register_hearthis_output();

	controller = new StreamController();

	HearthisDock *dock = new HearthisDock(controller);
	if (!obs_frontend_add_dock_by_id("hearthis_dock", obs_module_text("Dock.Title"), dock)) {
		obs_log(LOG_ERROR, "failed to register the Hearthis dock");
		delete dock;
		delete controller;
		controller = nullptr;
		return false;
	}

	obs_frontend_add_event_callback(onFrontendEvent, nullptr);

	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(onFrontendEvent, nullptr);

	delete controller;
	controller = nullptr;

	obs_log(LOG_INFO, "plugin unloaded");
}
