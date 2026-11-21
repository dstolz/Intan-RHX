//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.5.2
//
//  Copyright (c) 2020-2026 Intan Technologies
//
//  This file is part of the Intan Technologies RHX Data Acquisition Software.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
//  This software is provided 'as-is', without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from
//  the use of this software.
//
//  See <https://www.intantech.com> for documentation and product information.
//
//------------------------------------------------------------------------------

#ifndef SETTINGSLOADMESSAGEDIALOG_H
#define SETTINGSLOADMESSAGEDIALOG_H

#include <QDialog>
#include <QStringList>

// Compact dialog that shows the warnings or errors produced while loading a settings file. The raw message from
// XMLInterface (one "Warning: ..." / "Error: ..." paragraph per problem, channel lists one name per line) is reformatted
// into short wrapped paragraphs, and channel lists are collapsed into ranges (e.g. "A-000 - A-063"). Optionally offers a
// Rescan Ports button; the caller checks rescanRequested() after exec() and performs the rescan.
class SettingsLoadMessageDialog : public QDialog
{
    Q_OBJECT
public:
    enum Severity { Warning, Error };

    SettingsLoadMessageDialog(const QString &headline, const QString &filename, const QString &message,
                              Severity severity, bool offerRescan, QWidget *parent = nullptr);

    bool rescanRequested() const { return rescan; }

private:
    struct Section {
        QString text;
        QStringList items;
        bool isList = false;
    };

    static QList<Section> parseMessage(const QString &message);
    static QString friendlyText(const QString &text, int itemCount);
    static QStringList compressChannelNames(const QStringList &names);

    bool rescan;
};

#endif // SETTINGSLOADMESSAGEDIALOG_H
