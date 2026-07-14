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

/* Audio-only output to Icecast using the OBS-bundled FFmpeg libraries.
 * Modeled on obs-ffmpeg-output.c (connect thread, packet queue + write
 * thread), but without any video path. */

#include "hearthis-output.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
}

#include <obs-module.h>
#include <plugin-support.h>
#include <media-io/audio-io.h>
#include <util/threading.h>

#include <deque>

namespace {

struct hearthis_output {
	obs_output_t *output = nullptr;

	/* FFmpeg resources; valid while `initialized` is set */
	AVFormatContext *fmt = nullptr;
	AVCodecContext *enc = nullptr;
	AVStream *stream = nullptr;
	AVAudioFifo *fifo = nullptr;
	AVFrame *frame = nullptr;
	int64_t next_pts = 0;

	volatile bool initialized = false;
	volatile bool active = false;
	volatile bool stopping = false;

	pthread_t connect_thread{};
	bool connect_thread_active = false;

	pthread_t write_thread{};
	bool write_thread_active = false;
	pthread_mutex_t write_mutex{};
	os_sem_t *write_sem = nullptr;
	os_event_t *stop_event = nullptr;
	std::deque<AVPacket *> packets;

	volatile uint64_t total_bytes = 0;
};

const char *av_err_str(int errnum, char (&buf)[AV_ERROR_MAX_STRING_SIZE])
{
	if (av_strerror(errnum, buf, sizeof(buf)) < 0)
		snprintf(buf, sizeof(buf), "error %d", errnum);
	return buf;
}

void set_av_error(hearthis_output *ctx, const char *what, int errnum)
{
	char buf[AV_ERROR_MAX_STRING_SIZE];
	av_err_str(errnum, buf);
	obs_log(LOG_WARNING, "hearthis output: %s: %s", what, buf);

	char message[512];
	snprintf(message, sizeof(message), "%s: %s", what, buf);
	obs_output_set_last_error(ctx->output, message);
}

void free_av_resources(hearthis_output *ctx)
{
	if (ctx->frame)
		av_frame_free(&ctx->frame);
	if (ctx->fifo) {
		av_audio_fifo_free(ctx->fifo);
		ctx->fifo = nullptr;
	}
	if (ctx->enc)
		avcodec_free_context(&ctx->enc);
	if (ctx->fmt) {
		if (ctx->fmt->pb)
			avio_closep(&ctx->fmt->pb);
		avformat_free_context(ctx->fmt);
		ctx->fmt = nullptr;
	}
	ctx->stream = nullptr;

	for (AVPacket *pkt : ctx->packets)
		av_packet_free(&pkt);
	ctx->packets.clear();
}

/* Sets up encoder + Icecast connection (runs on the connect thread) */
bool open_stream(hearthis_output *ctx)
{
	obs_data_t *settings = obs_output_get_settings(ctx->output);
	const char *url = obs_data_get_string(settings, "url");
	int bitrate = (int)obs_data_get_int(settings, "bitrate");
	const char *ice_name = obs_data_get_string(settings, "ice_name");
	const char *ice_description = obs_data_get_string(settings, "ice_description");

	audio_t *audio = obs_output_audio(ctx->output);
	const int sample_rate = (int)audio_output_get_sample_rate(audio);

	int ret = avformat_alloc_output_context2(&ctx->fmt, nullptr, "mp3", url);
	if (ret < 0) {
		set_av_error(ctx, "avformat_alloc_output_context2", ret);
		goto fail;
	}

	{
		const AVCodec *codec = avcodec_find_encoder_by_name("libmp3lame");
		if (!codec) {
			obs_log(LOG_ERROR, "hearthis output: libmp3lame not found in OBS FFmpeg");
			obs_output_set_last_error(ctx->output, obs_module_text("Error.NoLame"));
			goto fail;
		}

		ctx->enc = avcodec_alloc_context3(codec);
		if (!ctx->enc)
			goto fail;

		ctx->enc->bit_rate = (int64_t)bitrate * 1000;
		ctx->enc->sample_rate = sample_rate;
		ctx->enc->sample_fmt = AV_SAMPLE_FMT_FLTP;
		av_channel_layout_default(&ctx->enc->ch_layout, 2);
		ctx->enc->time_base = AVRational{1, sample_rate};
		if (ctx->fmt->oformat->flags & AVFMT_GLOBALHEADER)
			ctx->enc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

		ret = avcodec_open2(ctx->enc, codec, nullptr);
		if (ret < 0) {
			set_av_error(ctx, "avcodec_open2 (libmp3lame)", ret);
			goto fail;
		}

		ctx->stream = avformat_new_stream(ctx->fmt, nullptr);
		if (!ctx->stream)
			goto fail;
		ctx->stream->time_base = ctx->enc->time_base;
		avcodec_parameters_from_context(ctx->stream->codecpar, ctx->enc);
	}

	{
		AVDictionary *opts = nullptr;
		av_dict_set(&opts, "content_type", "audio/mpeg", 0);
		/* bound connect/write time, otherwise a dead socket hangs for minutes */
		av_dict_set(&opts, "rw_timeout", "8000000", 0);
		if (ice_name && *ice_name)
			av_dict_set(&opts, "ice_name", ice_name, 0);
		if (ice_description && *ice_description)
			av_dict_set(&opts, "ice_description", ice_description, 0);

		ret = avio_open2(&ctx->fmt->pb, url, AVIO_FLAG_WRITE, nullptr, &opts);
		if (ret < 0) {
			av_dict_free(&opts);
			set_av_error(ctx, "avio_open2 (icecast)", ret);
			goto fail;
		}

		ret = avformat_write_header(ctx->fmt, &opts);
		av_dict_free(&opts);
		if (ret < 0) {
			set_av_error(ctx, "avformat_write_header", ret);
			goto fail;
		}
	}

	ctx->fifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_FLTP, 2, ctx->enc->frame_size * 4);
	if (!ctx->fifo)
		goto fail;

	ctx->frame = av_frame_alloc();
	if (!ctx->frame)
		goto fail;
	ctx->frame->format = AV_SAMPLE_FMT_FLTP;
	av_channel_layout_default(&ctx->frame->ch_layout, 2);
	ctx->frame->sample_rate = sample_rate;
	ctx->frame->nb_samples = ctx->enc->frame_size;
	ret = av_frame_get_buffer(ctx->frame, 0);
	if (ret < 0) {
		set_av_error(ctx, "av_frame_get_buffer", ret);
		goto fail;
	}

	ctx->next_pts = 0;
	obs_data_release(settings);
	os_atomic_set_bool(&ctx->initialized, true);
	return true;

fail:
	free_av_resources(ctx);
	obs_data_release(settings);
	return false;
}

/* Writes one packet; returns < 0 on network/muxer errors */
int write_one_packet(hearthis_output *ctx, AVPacket *pkt)
{
	ctx->total_bytes += (uint64_t)pkt->size;
	int ret = av_interleaved_write_frame(ctx->fmt, pkt);
	if (ret < 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE];
		obs_log(LOG_WARNING, "hearthis output: error writing packet: %s", av_err_str(ret, buf));
	}
	return ret;
}

void *write_thread_fn(void *param)
{
	auto *ctx = static_cast<hearthis_output *>(param);

	while (os_sem_wait(ctx->write_sem) == 0) {
		if (os_event_try(ctx->stop_event) == 0)
			break;

		AVPacket *pkt = nullptr;
		pthread_mutex_lock(&ctx->write_mutex);
		if (!ctx->packets.empty()) {
			pkt = ctx->packets.front();
			ctx->packets.pop_front();
		}
		pthread_mutex_unlock(&ctx->write_mutex);

		if (!pkt)
			continue;

		int ret = write_one_packet(ctx, pkt);
		av_packet_free(&pkt);

		if (ret < 0 && !os_atomic_load_bool(&ctx->stopping)) {
			/* connection lost: clean up ourselves and let libobs
			 * drive the auto-reconnect */
			pthread_detach(ctx->write_thread);
			ctx->write_thread_active = false;

			obs_output_signal_stop(ctx->output, OBS_OUTPUT_DISCONNECTED);
			os_atomic_set_bool(&ctx->active, false);

			pthread_mutex_lock(&ctx->write_mutex);
			bool was_initialized = os_atomic_set_bool(&ctx->initialized, false);
			pthread_mutex_unlock(&ctx->write_mutex);
			if (was_initialized)
				free_av_resources(ctx);
			break;
		}
	}

	return nullptr;
}

/* Encodes the (filled) ctx->frame; caller holds write_mutex.
 * Returns the number of newly queued packets. */
int encode_frame_locked(hearthis_output *ctx, AVFrame *frame)
{
	int queued = 0;
	int ret = avcodec_send_frame(ctx->enc, frame);
	if (ret < 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE];
		obs_log(LOG_WARNING, "hearthis output: avcodec_send_frame: %s", av_err_str(ret, buf));
		return 0;
	}

	for (;;) {
		AVPacket *pkt = av_packet_alloc();
		ret = avcodec_receive_packet(ctx->enc, pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			av_packet_free(&pkt);
			break;
		}
		if (ret < 0) {
			char buf[AV_ERROR_MAX_STRING_SIZE];
			obs_log(LOG_WARNING, "hearthis output: avcodec_receive_packet: %s", av_err_str(ret, buf));
			av_packet_free(&pkt);
			break;
		}

		av_packet_rescale_ts(pkt, ctx->enc->time_base, ctx->stream->time_base);
		pkt->stream_index = ctx->stream->index;
		ctx->packets.push_back(pkt);
		queued++;
	}

	return queued;
}

void hearthis_raw_audio(void *data, struct audio_data *frames)
{
	auto *ctx = static_cast<hearthis_output *>(data);
	int queued = 0;

	pthread_mutex_lock(&ctx->write_mutex);
	if (!os_atomic_load_bool(&ctx->initialized)) {
		pthread_mutex_unlock(&ctx->write_mutex);
		return;
	}

	if (av_audio_fifo_write(ctx->fifo, (void **)frames->data, (int)frames->frames) < (int)frames->frames) {
		obs_log(LOG_WARNING, "hearthis output: audio fifo write failed");
		pthread_mutex_unlock(&ctx->write_mutex);
		return;
	}

	while (av_audio_fifo_size(ctx->fifo) >= ctx->enc->frame_size) {
		if (av_audio_fifo_read(ctx->fifo, (void **)ctx->frame->data, ctx->enc->frame_size) <
		    ctx->enc->frame_size)
			break;
		ctx->frame->pts = ctx->next_pts;
		ctx->next_pts += ctx->enc->frame_size;
		queued += encode_frame_locked(ctx, ctx->frame);
	}
	pthread_mutex_unlock(&ctx->write_mutex);

	for (int i = 0; i < queued; i++)
		os_sem_post(ctx->write_sem);
}

/* Stops the write thread and closes the connection.
 * graceful: write remaining packets, flush the encoder, write the trailer. */
void hearthis_deactivate(hearthis_output *ctx, bool graceful)
{
	if (ctx->write_thread_active) {
		os_atomic_set_bool(&ctx->stopping, true);
		os_event_signal(ctx->stop_event);
		os_sem_post(ctx->write_sem);
		pthread_join(ctx->write_thread, nullptr);
		ctx->write_thread_active = false;
	}

	pthread_mutex_lock(&ctx->write_mutex);
	bool was_initialized = os_atomic_set_bool(&ctx->initialized, false);
	pthread_mutex_unlock(&ctx->write_mutex);
	if (!was_initialized)
		return;

	if (graceful) {
		/* drain the queue */
		for (AVPacket *pkt : ctx->packets) {
			write_one_packet(ctx, pkt);
			av_packet_free(&pkt);
		}
		ctx->packets.clear();

		/* flush the encoder */
		if (avcodec_send_frame(ctx->enc, nullptr) == 0) {
			for (;;) {
				AVPacket *pkt = av_packet_alloc();
				int ret = avcodec_receive_packet(ctx->enc, pkt);
				if (ret < 0) {
					av_packet_free(&pkt);
					break;
				}
				av_packet_rescale_ts(pkt, ctx->enc->time_base, ctx->stream->time_base);
				pkt->stream_index = ctx->stream->index;
				write_one_packet(ctx, pkt);
				av_packet_free(&pkt);
			}
		}

		av_write_trailer(ctx->fmt);
	}

	free_av_resources(ctx);
	obs_log(LOG_INFO, "hearthis output: stream closed (%s)", graceful ? "graceful" : "disconnected");
}

void *connect_thread_fn(void *param)
{
	auto *ctx = static_cast<hearthis_output *>(param);

	if (!open_stream(ctx)) {
		obs_output_signal_stop(ctx->output, OBS_OUTPUT_CONNECT_FAILED);
		return nullptr;
	}

	struct audio_convert_info aci = {};
	aci.samples_per_sec = (uint32_t)ctx->enc->sample_rate;
	aci.format = AUDIO_FORMAT_FLOAT_PLANAR;
	aci.speakers = SPEAKERS_STEREO;
	obs_output_set_audio_conversion(ctx->output, &aci);

	os_atomic_set_bool(&ctx->stopping, false);
	if (pthread_create(&ctx->write_thread, nullptr, write_thread_fn, ctx) != 0) {
		obs_log(LOG_ERROR, "hearthis output: failed to create write thread");
		hearthis_deactivate(ctx, false);
		obs_output_signal_stop(ctx->output, OBS_OUTPUT_ERROR);
		return nullptr;
	}
	ctx->write_thread_active = true;

	os_atomic_set_bool(&ctx->active, true);
	if (!obs_output_begin_data_capture(ctx->output, 0)) {
		os_atomic_set_bool(&ctx->active, false);
		hearthis_deactivate(ctx, false);
		obs_output_signal_stop(ctx->output, OBS_OUTPUT_ERROR);
		return nullptr;
	}

	obs_log(LOG_INFO, "hearthis output: connected (%d Hz, %d kbit/s)", ctx->enc->sample_rate,
		(int)(ctx->enc->bit_rate / 1000));
	return nullptr;
}

const char *hearthis_get_name(void *)
{
	return "Hearthis.at Icecast Output";
}

void *hearthis_create(obs_data_t *, obs_output_t *output)
{
	auto *ctx = new hearthis_output();
	ctx->output = output;

	if (pthread_mutex_init(&ctx->write_mutex, nullptr) != 0)
		goto fail;
	if (os_sem_init(&ctx->write_sem, 0) != 0)
		goto fail_mutex;
	if (os_event_init(&ctx->stop_event, OS_EVENT_TYPE_AUTO) != 0)
		goto fail_sem;

	return ctx;

fail_sem:
	os_sem_destroy(ctx->write_sem);
fail_mutex:
	pthread_mutex_destroy(&ctx->write_mutex);
fail:
	delete ctx;
	return nullptr;
}

void join_connect_thread(hearthis_output *ctx)
{
	if (ctx->connect_thread_active) {
		pthread_join(ctx->connect_thread, nullptr);
		ctx->connect_thread_active = false;
	}
}

void hearthis_stop(void *data, uint64_t ts)
{
	UNUSED_PARAMETER(ts);
	auto *ctx = static_cast<hearthis_output *>(data);

	join_connect_thread(ctx);

	if (os_atomic_set_bool(&ctx->active, false)) {
		obs_output_end_data_capture(ctx->output);
		hearthis_deactivate(ctx, true);
	}
}

void hearthis_destroy(void *data)
{
	auto *ctx = static_cast<hearthis_output *>(data);
	if (!ctx)
		return;

	join_connect_thread(ctx);
	if (os_atomic_set_bool(&ctx->active, false))
		obs_output_end_data_capture(ctx->output);
	hearthis_deactivate(ctx, false);

	os_event_destroy(ctx->stop_event);
	os_sem_destroy(ctx->write_sem);
	pthread_mutex_destroy(&ctx->write_mutex);
	delete ctx;
}

bool hearthis_start(void *data)
{
	auto *ctx = static_cast<hearthis_output *>(data);

	if (os_atomic_load_bool(&ctx->active))
		return false;
	if (!obs_output_can_begin_data_capture(ctx->output, 0))
		return false;

	join_connect_thread(ctx);

	ctx->total_bytes = 0;
	os_atomic_set_bool(&ctx->stopping, false);

	ctx->connect_thread_active = (pthread_create(&ctx->connect_thread, nullptr, connect_thread_fn, ctx) == 0);
	return ctx->connect_thread_active;
}

uint64_t hearthis_total_bytes(void *data)
{
	auto *ctx = static_cast<hearthis_output *>(data);
	return ctx->total_bytes;
}

} // namespace

void register_hearthis_output()
{
	static struct obs_output_info info = {};
	info.id = "hearthis_icecast_output";
	info.flags = OBS_OUTPUT_AUDIO;
	info.get_name = hearthis_get_name;
	info.create = hearthis_create;
	info.destroy = hearthis_destroy;
	info.start = hearthis_start;
	info.stop = hearthis_stop;
	info.raw_audio = hearthis_raw_audio;
	info.get_total_bytes = hearthis_total_bytes;
	obs_register_output(&info);
}
