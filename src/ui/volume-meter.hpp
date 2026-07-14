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

#include <QMutex>
#include <QPixmap>
#include <QWidget>

#include <obs.h>

/* Horizontal true-peak level meter for one OBS audio track (stereo, i.e.
 * exactly what the Hearthis output sends).
 *
 * Deliberately named "VolumeMeter" with the same Q_PROPERTY set as the OBS
 * frontend widget: OBS applies its theme stylesheets application-wide, and
 * the type selector "VolumeMeter { qproperty-... }" in themes like Yami then
 * styles this meter identically to the native mixer meters. Drawing,
 * ballistics and the true-peak algorithm are ported from the OBS 32 sources
 * (frontend/components/VolumeMeter.cpp, libobs/obs-audio-controls.c),
 * reduced to the horizontal, always-enabled case. */
class VolumeMeter : public QWidget {
	Q_OBJECT
	Q_PROPERTY(QColor backgroundNominalColor READ getBackgroundNominalColor WRITE setBackgroundNominalColor
			   DESIGNABLE true)
	Q_PROPERTY(QColor backgroundWarningColor READ getBackgroundWarningColor WRITE setBackgroundWarningColor
			   DESIGNABLE true)
	Q_PROPERTY(
		QColor backgroundErrorColor READ getBackgroundErrorColor WRITE setBackgroundErrorColor DESIGNABLE true)
	Q_PROPERTY(QColor foregroundNominalColor READ getForegroundNominalColor WRITE setForegroundNominalColor
			   DESIGNABLE true)
	Q_PROPERTY(QColor foregroundWarningColor READ getForegroundWarningColor WRITE setForegroundWarningColor
			   DESIGNABLE true)
	Q_PROPERTY(
		QColor foregroundErrorColor READ getForegroundErrorColor WRITE setForegroundErrorColor DESIGNABLE true)
	Q_PROPERTY(QColor backgroundNominalColorDisabled MEMBER backgroundNominalColorDisabled DESIGNABLE true)
	Q_PROPERTY(QColor backgroundWarningColorDisabled MEMBER backgroundWarningColorDisabled DESIGNABLE true)
	Q_PROPERTY(QColor backgroundErrorColorDisabled MEMBER backgroundErrorColorDisabled DESIGNABLE true)
	Q_PROPERTY(QColor foregroundNominalColorDisabled MEMBER foregroundNominalColorDisabled DESIGNABLE true)
	Q_PROPERTY(QColor foregroundWarningColorDisabled MEMBER foregroundWarningColorDisabled DESIGNABLE true)
	Q_PROPERTY(QColor foregroundErrorColorDisabled MEMBER foregroundErrorColorDisabled DESIGNABLE true)
	Q_PROPERTY(QColor magnitudeColor READ getMagnitudeColor WRITE setMagnitudeColor DESIGNABLE true)
	Q_PROPERTY(QColor majorTickColor READ getMajorTickColor WRITE setMajorTickColor DESIGNABLE true)
	Q_PROPERTY(QColor minorTickColor READ getMinorTickColor WRITE setMinorTickColor DESIGNABLE true)

public:
	explicit VolumeMeter(QWidget *parent = nullptr);
	~VolumeMeter() override;

	/* Attaches the meter to an OBS audio track (1-based, like the config) */
	void setTrack(int track);

	QColor getBackgroundNominalColor() const { return p_backgroundNominalColor; }
	void setBackgroundNominalColor(QColor c);
	QColor getBackgroundWarningColor() const { return p_backgroundWarningColor; }
	void setBackgroundWarningColor(QColor c);
	QColor getBackgroundErrorColor() const { return p_backgroundErrorColor; }
	void setBackgroundErrorColor(QColor c);
	QColor getForegroundNominalColor() const { return p_foregroundNominalColor; }
	void setForegroundNominalColor(QColor c);
	QColor getForegroundWarningColor() const { return p_foregroundWarningColor; }
	void setForegroundWarningColor(QColor c);
	QColor getForegroundErrorColor() const { return p_foregroundErrorColor; }
	void setForegroundErrorColor(QColor c);
	QColor getMagnitudeColor() const { return magnitudeColor; }
	void setMagnitudeColor(QColor c);
	QColor getMajorTickColor() const { return majorTickColor; }
	void setMajorTickColor(QColor c);
	QColor getMinorTickColor() const { return minorTickColor; }
	void setMinorTickColor(QColor c);

	QSize minimumSizeHint() const override;
	QSize sizeHint() const override;

protected:
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void changeEvent(QEvent *e) override;

private:
	static void audioCallback(void *param, size_t mixIdx, struct audio_data *data);
	void processAudio(const struct audio_data *data);

	void doLayout();
	void resetLevels();
	bool detectIdle(uint64_t ts);
	void calculateBallistics(uint64_t ts, qreal timeSinceLastRedraw);
	void updateTickLabelTokenSize();
	void updateBackgroundCache(bool force = false);
	void paintHTicks(QPainter &painter, int x, int y, int width);
	QColor getPeakColor(float peakHold) const;
	QRect getBarRect() const;
	int convertToInt(float number) const;

	static const int kChannels = 2; /* meter shows the stereo mix sent to Hearthis */

	int connectedTrack = 0; /* 0 = not connected; otherwise 1-based track */

	QMutex dataMutex;
	uint64_t currentLastUpdateTime = 0;
	float currentMagnitude[kChannels];
	float currentPeak[kChannels];
	float currentInputPeak[kChannels];
	float truePeakPrevSamples[kChannels][4];

	float displayMagnitude[kChannels];
	float displayPeak[kChannels];
	float displayPeakHold[kChannels];
	uint64_t displayPeakHoldLastUpdateTime[kChannels];
	float displayInputPeakHold[kChannels];
	uint64_t displayInputPeakHoldLastUpdateTime[kChannels];

	QTimer *redrawTimer;
	uint64_t lastRedrawTime = 0;
	bool clipping = false;

	QPixmap backgroundCache;
	QSize tickTextTokenRect;

	QColor backgroundNominalColor;
	QColor backgroundWarningColor;
	QColor backgroundErrorColor;
	QColor foregroundNominalColor;
	QColor foregroundWarningColor;
	QColor foregroundErrorColor;
	QColor backgroundNominalColorDisabled;
	QColor backgroundWarningColorDisabled;
	QColor backgroundErrorColorDisabled;
	QColor foregroundNominalColorDisabled;
	QColor foregroundWarningColorDisabled;
	QColor foregroundErrorColorDisabled;
	QColor clipColor;
	QColor magnitudeColor;
	QColor majorTickColor;
	QColor minorTickColor;

	QColor p_backgroundNominalColor;
	QColor p_backgroundWarningColor;
	QColor p_backgroundErrorColor;
	QColor p_foregroundNominalColor;
	QColor p_foregroundWarningColor;
	QColor p_foregroundErrorColor;

	int meterThickness;
	qreal minimumLevel;
	qreal warningLevel;
	qreal errorLevel;
	qreal clipLevel;
	qreal minimumInputLevel;
	qreal peakDecayRate;
	qreal magnitudeIntegrationTime;
	qreal peakHoldDuration;
	qreal inputPeakHoldDuration;
};
