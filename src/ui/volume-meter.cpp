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

#include "volume-meter.hpp"

#include <obs-audio-controls.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <util/platform.h>

#include <QEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr int INDICATOR_THICKNESS = 3;
constexpr int CLIP_FLASH_DURATION_MS = 1000;
constexpr int TICK_SIZE = 2;
constexpr int TICK_DB_INTERVAL = 6;

constexpr const char *TICK_LABEL_TOKEN = "-88";
} // namespace

static constexpr float kMinusInfinity = -std::numeric_limits<float>::infinity();

/* True peak via 5x oversampling with Whittaker-Shannon interpolation, scalar
 * port of get_true_peak() in libobs/obs-audio-controls.c. Sample window at
 * t = -1.5, -0.5, +0.5, +1.5; oversample points at t = -0.3, -0.1, +0.1, +0.3. */
static float calcTruePeak(float prev[4], const float *samples, size_t count)
{
	static const float coeff[4][4] = {
		{-0.155915f, 0.935489f, 0.233872f, -0.103943f},
		{-0.216236f, 0.756827f, 0.504551f, -0.189207f},
		{-0.189207f, 0.504551f, 0.756827f, -0.216236f},
		{-0.103943f, 0.233872f, 0.935489f, -0.155915f},
	};

	float w0 = prev[0], w1 = prev[1], w2 = prev[2], w3 = prev[3];
	float peak = 0.0f;

	for (size_t i = 0; i < count; i++) {
		w0 = w1;
		w1 = w2;
		w2 = w3;
		w3 = samples[i];

		peak = fmaxf(peak, fabsf(w3));
		for (int j = 0; j < 4; j++) {
			float v = coeff[j][0] * w0 + coeff[j][1] * w1 + coeff[j][2] * w2 + coeff[j][3] * w3;
			peak = fmaxf(peak, fabsf(v));
		}
	}

	prev[0] = w0;
	prev[1] = w1;
	prev[2] = w2;
	prev[3] = w3;
	return peak;
}

static inline QColor color_from_int(long long val)
{
	QColor color(val & 0xff, (val >> 8) & 0xff, (val >> 16) & 0xff, (val >> 24) & 0xff);
	color.setAlpha(255);

	return color;
}

/* Honors the OBS accessibility color overrides (Settings -> Accessibility),
 * same behavior as the OBS 32 property setters. */
static bool accessibilityColor(const char *key, QColor &out)
{
	config_t *cfg = obs_frontend_get_user_config();
	if (!cfg || !config_get_bool(cfg, "Accessibility", "OverrideColors"))
		return false;

	out = color_from_int(config_get_int(cfg, "Accessibility", key));
	return true;
}

void VolumeMeter::setBackgroundNominalColor(QColor c)
{
	p_backgroundNominalColor = std::move(c);
	if (!accessibilityColor("MixerGreen", backgroundNominalColor))
		backgroundNominalColor = p_backgroundNominalColor;
}

void VolumeMeter::setBackgroundWarningColor(QColor c)
{
	p_backgroundWarningColor = std::move(c);
	if (!accessibilityColor("MixerYellow", backgroundWarningColor))
		backgroundWarningColor = p_backgroundWarningColor;
}

void VolumeMeter::setBackgroundErrorColor(QColor c)
{
	p_backgroundErrorColor = std::move(c);
	if (!accessibilityColor("MixerRed", backgroundErrorColor))
		backgroundErrorColor = p_backgroundErrorColor;
}

void VolumeMeter::setForegroundNominalColor(QColor c)
{
	p_foregroundNominalColor = std::move(c);
	if (!accessibilityColor("MixerGreenActive", foregroundNominalColor))
		foregroundNominalColor = p_foregroundNominalColor;
}

void VolumeMeter::setForegroundWarningColor(QColor c)
{
	p_foregroundWarningColor = std::move(c);
	if (!accessibilityColor("MixerYellowActive", foregroundWarningColor))
		foregroundWarningColor = p_foregroundWarningColor;
}

void VolumeMeter::setForegroundErrorColor(QColor c)
{
	p_foregroundErrorColor = std::move(c);
	if (!accessibilityColor("MixerRedActive", foregroundErrorColor))
		foregroundErrorColor = p_foregroundErrorColor;
}

void VolumeMeter::setMagnitudeColor(QColor c)
{
	magnitudeColor = std::move(c);
}

void VolumeMeter::setMajorTickColor(QColor c)
{
	majorTickColor = std::move(c);
}

void VolumeMeter::setMinorTickColor(QColor c)
{
	minorTickColor = std::move(c);
}

VolumeMeter::VolumeMeter(QWidget *parent) : QWidget(parent)
{
	setAttribute(Qt::WA_OpaquePaintEvent, true);
	setAttribute(Qt::WA_TransparentForMouseEvents);
	setFocusPolicy(Qt::NoFocus);

	/* OBS default meter settings; themes override them via stylesheet */
	backgroundNominalColor.setRgb(0x26, 0x7f, 0x26);
	backgroundWarningColor.setRgb(0x7f, 0x7f, 0x26);
	backgroundErrorColor.setRgb(0x7f, 0x26, 0x26);
	foregroundNominalColor.setRgb(0x4c, 0xff, 0x4c);
	foregroundWarningColor.setRgb(0xff, 0xff, 0x4c);
	foregroundErrorColor.setRgb(0xff, 0x4c, 0x4c);

	backgroundNominalColorDisabled.setRgb(90, 90, 90);
	backgroundWarningColorDisabled.setRgb(117, 117, 117);
	backgroundErrorColorDisabled.setRgb(65, 65, 65);
	foregroundNominalColorDisabled.setRgb(163, 163, 163);
	foregroundWarningColorDisabled.setRgb(217, 217, 217);
	foregroundErrorColorDisabled.setRgb(113, 113, 113);

	clipColor.setRgb(0xff, 0xff, 0xff);
	magnitudeColor.setRgb(0x00, 0x00, 0x00);
	majorTickColor.setRgb(0x00, 0x00, 0x00);
	minorTickColor.setRgb(0x32, 0x32, 0x32);
	minimumLevel = -60.0;
	/* True-peak levels per EBU, like OBS setPeakMeterType(TRUE_PEAK_METER):
	 * Permitted Maximum Level -2.0 dBTP, Alignment Level -13 dBTP */
	warningLevel = -13.0;
	errorLevel = -2.0;
	clipLevel = 0.0;
	minimumInputLevel = -50.0;
	peakDecayRate = 11.76; /* 20 dB / 1.7 s */
	magnitudeIntegrationTime = 0.3;
	peakHoldDuration = 20.0;
	inputPeakHoldDuration = 1.0;
	meterThickness = 3;

	resetLevels();
	updateTickLabelTokenSize();
	doLayout();

	redrawTimer = new QTimer(this);
	redrawTimer->setTimerType(Qt::PreciseTimer);
	connect(redrawTimer, &QTimer::timeout, this, [this]() { update(getBarRect()); });
	redrawTimer->start(16);
	lastRedrawTime = os_gettime_ns();
}

VolumeMeter::~VolumeMeter()
{
	setTrack(0);
}

void VolumeMeter::setTrack(int track)
{
	if (track == connectedTrack)
		return;

	audio_t *audio = obs_get_audio();
	if (connectedTrack > 0)
		audio_output_disconnect(audio, (size_t)(connectedTrack - 1), audioCallback, this);

	connectedTrack = 0;

	if (track >= 1 && track <= MAX_AUDIO_MIXES) {
		struct audio_convert_info conversion = {};
		conversion.samples_per_sec = audio_output_get_sample_rate(audio);
		conversion.format = AUDIO_FORMAT_FLOAT_PLANAR;
		conversion.speakers = SPEAKERS_STEREO;

		if (audio_output_connect(audio, (size_t)(track - 1), &conversion, audioCallback, this))
			connectedTrack = track;
	}

	resetLevels();
}

void VolumeMeter::audioCallback(void *param, size_t mixIdx, struct audio_data *data)
{
	UNUSED_PARAMETER(mixIdx);
	static_cast<VolumeMeter *>(param)->processAudio(data);
}

void VolumeMeter::processAudio(const struct audio_data *data)
{
	QMutexLocker locker(&dataMutex);

	for (int ch = 0; ch < kChannels; ch++) {
		const float *samples = (const float *)data->data[ch];
		if (!samples || !data->frames) {
			currentMagnitude[ch] = kMinusInfinity;
			currentPeak[ch] = kMinusInfinity;
			currentInputPeak[ch] = kMinusInfinity;
			continue;
		}

		float sum = 0.0f;
		for (size_t i = 0; i < data->frames; i++)
			sum += samples[i] * samples[i];

		currentMagnitude[ch] = obs_mul_to_db(sqrtf(sum / (float)data->frames));
		currentPeak[ch] = obs_mul_to_db(calcTruePeak(truePeakPrevSamples[ch], samples, data->frames));
		currentInputPeak[ch] = currentPeak[ch];
	}

	currentLastUpdateTime = os_gettime_ns();
}

void VolumeMeter::resetLevels()
{
	QMutexLocker locker(&dataMutex);

	currentLastUpdateTime = 0;
	for (int ch = 0; ch < kChannels; ch++) {
		currentMagnitude[ch] = kMinusInfinity;
		currentPeak[ch] = kMinusInfinity;
		currentInputPeak[ch] = kMinusInfinity;
		displayMagnitude[ch] = kMinusInfinity;
		displayPeak[ch] = kMinusInfinity;
		displayPeakHold[ch] = kMinusInfinity;
		displayPeakHoldLastUpdateTime[ch] = 0;
		displayInputPeakHold[ch] = kMinusInfinity;
		displayInputPeakHoldLastUpdateTime[ch] = 0;
		for (int i = 0; i < 4; i++)
			truePeakPrevSamples[ch][i] = 0.0f;
	}
}

bool VolumeMeter::detectIdle(uint64_t ts)
{
	dataMutex.lock();
	const double secondsSinceLastUpdate = (ts - currentLastUpdateTime) * 0.000000001;
	dataMutex.unlock();

	if (secondsSinceLastUpdate > 0.5) {
		resetLevels();
		return true;
	}
	return false;
}

/* Ballistics ported from OBS VolumeMeter::calculateBallisticsForChannel() */
void VolumeMeter::calculateBallistics(uint64_t ts, qreal timeSinceLastRedraw)
{
	QMutexLocker locker(&dataMutex);

	for (int ch = 0; ch < kChannels; ch++) {
		if (currentPeak[ch] >= displayPeak[ch] || std::isnan(displayPeak[ch])) {
			displayPeak[ch] = currentPeak[ch];
		} else {
			float decay = float(peakDecayRate * timeSinceLastRedraw);
			displayPeak[ch] =
				std::clamp(displayPeak[ch] - decay, std::min(currentPeak[ch], 0.f), 0.f);
		}

		if (currentPeak[ch] >= displayPeakHold[ch] || !std::isfinite(displayPeakHold[ch])) {
			displayPeakHold[ch] = currentPeak[ch];
			displayPeakHoldLastUpdateTime[ch] = ts;
		} else {
			qreal timeSinceLastPeak = (uint64_t)(ts - displayPeakHoldLastUpdateTime[ch]) * 0.000000001;
			if (timeSinceLastPeak > peakHoldDuration) {
				displayPeakHold[ch] = currentPeak[ch];
				displayPeakHoldLastUpdateTime[ch] = ts;
			}
		}

		if (currentInputPeak[ch] >= displayInputPeakHold[ch] || !std::isfinite(displayInputPeakHold[ch])) {
			displayInputPeakHold[ch] = currentInputPeak[ch];
			displayInputPeakHoldLastUpdateTime[ch] = ts;
		} else {
			qreal timeSinceLastPeak =
				(uint64_t)(ts - displayInputPeakHoldLastUpdateTime[ch]) * 0.000000001;
			if (timeSinceLastPeak > inputPeakHoldDuration) {
				displayInputPeakHold[ch] = currentInputPeak[ch];
				displayInputPeakHoldLastUpdateTime[ch] = ts;
			}
		}

		if (!std::isfinite(displayMagnitude[ch])) {
			displayMagnitude[ch] = currentMagnitude[ch];
		} else {
			float attack = float((currentMagnitude[ch] - displayMagnitude[ch]) *
					     (timeSinceLastRedraw / magnitudeIntegrationTime) * 0.99);
			displayMagnitude[ch] =
				std::clamp(displayMagnitude[ch] + attack, (float)minimumLevel, 0.f);
		}
	}
}

QColor VolumeMeter::getPeakColor(float peakHold) const
{
	if (peakHold < minimumInputLevel)
		return backgroundNominalColor;
	else if (peakHold < warningLevel)
		return foregroundNominalColor;
	else if (peakHold < errorLevel)
		return foregroundWarningColor;
	else if (peakHold < clipLevel)
		return foregroundErrorColor;
	else
		return clipColor;
}

/* Ported from OBS 32 VolumeMeter::paintHTicks() */
void VolumeMeter::paintHTicks(QPainter &painter, int x, int y, int width)
{
	qreal scale = width / minimumLevel;

	painter.setFont(font());
	QFontMetrics metrics(font());
	painter.setPen(majorTickColor);

	for (int i = 0; i >= minimumLevel; i -= TICK_DB_INTERVAL) {
		int position = int(x + width - (i * scale) - 1);
		QString str = QString::number(i);

		QRect textBounds = metrics.boundingRect(str);
		int pos;
		if (i == 0) {
			pos = position - textBounds.width();
		} else {
			pos = position - (textBounds.width() / 2);
			if (pos < 0)
				pos = 0;
		}
		painter.drawText(pos, y + 4 + metrics.capHeight(), str);

		painter.drawLine(position, y, position, y + TICK_SIZE);
	}
}

void VolumeMeter::updateTickLabelTokenSize()
{
	QFontMetrics metrics(font());
	tickTextTokenRect = metrics.size(Qt::TextSingleLine, TICK_LABEL_TOKEN);
}

/* Ported from OBS 32 VolumeMeter::updateBackgroundCache(), horizontal case */
void VolumeMeter::updateBackgroundCache(bool force)
{
	if (!force && size().isEmpty())
		return;

	if (!force && backgroundCache.size() == size() * devicePixelRatioF() && !backgroundCache.isNull())
		return;

	QColor backgroundColor = palette().color(QPalette::Window);

	backgroundCache = QPixmap(size() * devicePixelRatioF());
	backgroundCache.setDevicePixelRatio(devicePixelRatioF());
	backgroundCache.fill(backgroundColor);

	QPainter bg{&backgroundCache};
	QRect widgetRect = rect();

	paintHTicks(bg, INDICATOR_THICKNESS + 3, kChannels * (meterThickness + 1) - 1,
		    widgetRect.width() - (INDICATOR_THICKNESS + 3));

	int meterStart = INDICATOR_THICKNESS + 2;
	int meterLength = widgetRect.width() - (INDICATOR_THICKNESS + 2);

	qreal scale = meterLength / minimumLevel;

	int warningPosition = meterLength - convertToInt((float)(warningLevel * scale));
	int errorPosition = meterLength - convertToInt((float)(errorLevel * scale));

	int nominalLength = warningPosition;
	int warningLength = nominalLength + (errorPosition - warningPosition);

	for (int ch = 0; ch < kChannels; ch++) {
		int channelOffset = ch * (meterThickness + 1);

		bg.fillRect(meterStart, channelOffset, meterLength, meterThickness, backgroundErrorColor);
		bg.fillRect(meterStart, channelOffset, warningLength, meterThickness, backgroundWarningColor);
		bg.fillRect(meterStart, channelOffset, nominalLength, meterThickness, backgroundNominalColor);
	}
}

int VolumeMeter::convertToInt(float number) const
{
	constexpr int min = std::numeric_limits<int>::min();
	constexpr int max = std::numeric_limits<int>::max();

	if (number >= (float)max)
		return max;
	else if (number < min)
		return min;
	else
		return int(number);
}

QRect VolumeMeter::getBarRect() const
{
	QRect barRect = rect();
	barRect.setHeight(kChannels * (meterThickness + 1) - 1);
	return barRect;
}

/* Ported from OBS 32 VolumeMeter::paintEvent(), horizontal case */
void VolumeMeter::paintEvent(QPaintEvent *event)
{
	UNUSED_PARAMETER(event);

	uint64_t ts = os_gettime_ns();
	qreal timeSinceLastRedraw = (ts - lastRedrawTime) * 0.000000001;
	calculateBallistics(ts, timeSinceLastRedraw);
	bool idle = detectIdle(ts);

	QPainter painter(this);

	int meterStart = INDICATOR_THICKNESS + 2;
	int meterLength = rect().width() - (INDICATOR_THICKNESS + 2);

	const qreal scale = meterLength / minimumLevel;

	painter.drawPixmap(0, 0, backgroundCache);

	int warningPosition = meterLength - convertToInt((float)(warningLevel * scale));
	int errorPosition = meterLength - convertToInt((float)(errorLevel * scale));
	int clipPosition = meterLength - convertToInt((float)(clipLevel * scale));

	int nominalLength = warningPosition;
	int warningLength = nominalLength + (errorPosition - warningPosition);

	for (int ch = 0; ch < kChannels; ch++) {
		QMutexLocker locker(&dataMutex);
		float peak = displayPeak[ch];
		float peakHold = displayPeakHold[ch];
		float magnitude = displayMagnitude[ch];
		float inputPeakHold = displayInputPeakHold[ch];
		locker.unlock();

		int peakPosition = meterLength - convertToInt(peak * (float)scale);
		int peakHoldPosition = meterLength - convertToInt(peakHold * (float)scale);
		int magnitudePosition = meterLength - convertToInt(magnitude * (float)scale);

		if (clipping)
			peakPosition = meterLength;

		int channelOffset = ch * (meterThickness + 1);

		auto fill = [&](int length, const QColor &color) {
			painter.fillRect(meterStart, channelOffset, length, meterThickness, color);
		};

		if (peakPosition >= clipPosition) {
			if (!clipping) {
				QTimer::singleShot(CLIP_FLASH_DURATION_MS, this, [this]() { clipping = false; });
				clipping = true;
			}

			fill(meterLength, foregroundErrorColor);
		} else {
			if (peakPosition > errorPosition)
				fill(std::min(peakPosition, meterLength), foregroundErrorColor);
			if (peakPosition > warningPosition)
				fill(std::min(peakPosition, warningLength), foregroundWarningColor);
			if (peakPosition > meterStart)
				fill(std::min(peakPosition, nominalLength), foregroundNominalColor);
		}

		QColor peakHoldColor = foregroundNominalColor;
		if (peakHoldPosition >= errorPosition)
			peakHoldColor = foregroundErrorColor;
		else if (peakHoldPosition >= warningPosition)
			peakHoldColor = foregroundWarningColor;

		if (peakHoldPosition - 3 > 0)
			painter.fillRect(meterStart + peakHoldPosition - 3, channelOffset, 3, meterThickness,
					 peakHoldColor);

		if (magnitudePosition - 3 >= 0)
			painter.fillRect(meterStart + magnitudePosition - 3, channelOffset, 3, meterThickness,
					 magnitudeColor);

		if (idle)
			continue;

		painter.fillRect(0, channelOffset, INDICATOR_THICKNESS, meterThickness, getPeakColor(inputPeakHold));
	}

	lastRedrawTime = ts;
}

void VolumeMeter::doLayout()
{
	meterThickness = std::clamp((int)std::floor(22 / kChannels), 3, 6);

	updateBackgroundCache();
	resetLevels();

	updateGeometry();
}

void VolumeMeter::resizeEvent(QResizeEvent *event)
{
	updateBackgroundCache();
	QWidget::resizeEvent(event);
}

void VolumeMeter::changeEvent(QEvent *e)
{
	if (e->type() == QEvent::StyleChange || e->type() == QEvent::FontChange ||
	    e->type() == QEvent::PaletteChange) {
		updateTickLabelTokenSize();
		updateBackgroundCache(true);
		doLayout();
		update();
	}

	QWidget::changeEvent(e);
}

QSize VolumeMeter::minimumSizeHint() const
{
	return sizeHint();
}

QSize VolumeMeter::sizeHint() const
{
	QRect meterRect = getBarRect();
	int labelTotal = (int)(std::abs(minimumLevel) / TICK_DB_INTERVAL) + 1;

	int width = (labelTotal * tickTextTokenRect.width()) + INDICATOR_THICKNESS;
	int height = meterRect.height() + tickTextTokenRect.height();

	return QSize(width, height);
}
