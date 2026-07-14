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

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;

class SettingsDialog : public QDialog {
	Q_OBJECT

public:
	explicit SettingsDialog(QWidget *parent = nullptr);

	/* Opens the dialog modally; returns true if the settings were saved */
	static bool edit(QWidget *parent);

private:
	void loadConfig();
	void saveConfig();

	QLineEdit *usernameEdit;
	QLineEdit *passwordEdit;
	QLineEdit *nameEdit;
	QLineEdit *descriptionEdit;
	QCheckBox *onAirCheck;
	QCheckBox *recordingCheck;
	QComboBox *trackCombo;
	QComboBox *bitrateCombo;
	QLineEdit *serverEdit;
	QSpinBox *portSpin;
	QCheckBox *autoStartCheck;
};
