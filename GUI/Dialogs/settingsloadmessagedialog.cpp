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

#include <algorithm>
#include <QtWidgets>
#include <QRegularExpression>

#include "settingsloadmessagedialog.h"

namespace {
const int ContentWidth = 460;     // Width of the wrapped message text, in pixels.
const int MaxContentHeight = 340; // Taller messages scroll.
}

SettingsLoadMessageDialog::SettingsLoadMessageDialog(const QString &headline, const QString &filename,
                                                     const QString &message, Severity severity, bool offerRescan,
                                                     QWidget *parent) :
    QDialog(parent),
    rescan(false)
{
    setWindowTitle(severity == Error ? tr("Settings File Error") : tr("Settings File Warning"));

    // Icon column.
    QLabel *iconLabel = new QLabel(this);
    QStyle::StandardPixmap pixmapType = (severity == Error) ? QStyle::SP_MessageBoxCritical : QStyle::SP_MessageBoxWarning;
    int iconSize = style()->pixelMetric(QStyle::PM_MessageBoxIconSize, nullptr, this);
    iconLabel->setPixmap(style()->standardIcon(pixmapType, nullptr, this).pixmap(iconSize, iconSize));
    iconLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    // Headline and file name.
    QLabel *headlineLabel = new QLabel(headline, this);
    QFont headlineFont = headlineLabel->font();
    headlineFont.setBold(true);
    headlineFont.setPointSizeF(headlineFont.pointSizeF() * 1.15);
    headlineLabel->setFont(headlineFont);
    headlineLabel->setWordWrap(true);

    QFileInfo fileInfo(filename);
    QLabel *fileLabel = new QLabel(fileInfo.fileName(), this);
    fileLabel->setTextFormat(Qt::PlainText);
    fileLabel->setToolTip(QDir::toNativeSeparators(filename));
    fileLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QPalette dimPalette = fileLabel->palette();
    dimPalette.setColor(QPalette::WindowText, dimPalette.color(QPalette::PlaceholderText));
    fileLabel->setPalette(dimPalette);

    // Message body: one paragraph per problem, with channel lists collapsed into ranges.
    QWidget *body = new QWidget(this);
    QVBoxLayout *bodyLayout = new QVBoxLayout;
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(12);

    const QList<Section> sections = parseMessage(message);
    for (const Section &section : sections) {
        QVBoxLayout *sectionLayout = new QVBoxLayout;
        sectionLayout->setSpacing(4);

        QLabel *textLabel = new QLabel(friendlyText(section.text, section.items.size()), body);
        textLabel->setTextFormat(Qt::PlainText);
        textLabel->setWordWrap(true);
        textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        sectionLayout->addWidget(textLabel);

        if (section.isList && !section.items.isEmpty()) {
            QLabel *listLabel = new QLabel(compressChannelNames(section.items).join(", "), body);
            listLabel->setTextFormat(Qt::PlainText);
            listLabel->setWordWrap(true);
            listLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
            QFont listFont = listLabel->font();
            listFont.setBold(true);
            listLabel->setFont(listFont);
            listLabel->setContentsMargins(16, 0, 0, 0);
            sectionLayout->addWidget(listLabel);
        }
        bodyLayout->addLayout(sectionLayout);
    }
    bodyLayout->addStretch();
    body->setLayout(bodyLayout);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidget(body);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Size the scroll area to the wrapped text, up to MaxContentHeight.
    int contentHeight = bodyLayout->totalHeightForWidth(ContentWidth);
    bool needsScroll = contentHeight > MaxContentHeight;
    int scrollBarWidth = needsScroll ? style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, this) + 4 : 0;
    scrollArea->setFixedWidth(ContentWidth + scrollBarWidth);
    scrollArea->setFixedHeight(needsScroll ? MaxContentHeight : std::max(contentHeight, 0));

    QVBoxLayout *textColumn = new QVBoxLayout;
    textColumn->setSpacing(2);
    textColumn->addWidget(headlineLabel);
    textColumn->addWidget(fileLabel);
    textColumn->addSpacing(10);
    textColumn->addWidget(scrollArea);

    QHBoxLayout *topRow = new QHBoxLayout;
    topRow->setSpacing(14);
    topRow->addWidget(iconLabel, 0, Qt::AlignTop);
    topRow->addLayout(textColumn, 1);

    // Buttons.
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    if (offerRescan) {
        QPushButton *rescanButton = buttonBox->addButton(tr("Rescan Ports"), QDialogButtonBox::ActionRole);
        rescanButton->setToolTip(tr("Scan the ports for connected headstages now (same as Rescan Ports on the "
                                    "Configure tab). Skipped channel settings are applied to any newly detected channels."));
        connect(rescanButton, &QPushButton::clicked, this, [this]() {
            rescan = true;
            accept();
        });
    }
    buttonBox->button(QDialogButtonBox::Ok)->setDefault(true);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(topRow);
    mainLayout->addSpacing(8);
    mainLayout->addWidget(buttonBox);
    mainLayout->setSizeConstraint(QLayout::SetFixedSize);
    setLayout(mainLayout);
}

// Split XMLInterface's message into sections. Each "Warning:" / "Error:" starts a new section (the prefix is dropped,
// since the dialog's icon and title already say it). A section whose text ends in ':' is followed by one item per line;
// otherwise further lines are wrapped continuation text.
QList<SettingsLoadMessageDialog::Section> SettingsLoadMessageDialog::parseMessage(const QString &message)
{
    // Some warnings are appended without a newline separator; give each its own line.
    QString normalized = message;
    static const QRegularExpression inlinePrefix("(?<=\\S)\\s*(?=(Warning|Error): )");
    normalized.replace(inlinePrefix, "\n");

    QList<Section> sections;
    const QStringList lines = normalized.split('\n');
    for (const QString &rawLine : lines) {
        QString line = rawLine.trimmed();
        if (line.isEmpty()) continue;

        bool startsSection = false;
        for (const QString &prefix : { QStringLiteral("Warning:"), QStringLiteral("Error:") }) {
            if (line.startsWith(prefix)) {
                line = line.mid(prefix.length()).trimmed();
                startsSection = true;
                break;
            }
        }

        if (startsSection || sections.isEmpty()) {
            Section section;
            section.text = line;
            section.isList = line.endsWith(':');
            sections.append(section);
        } else if (sections.last().isList) {
            sections.last().items.append(line);
        } else {
            sections.last().text += " " + line;
            sections.last().isList = line.endsWith(':');
        }
    }
    return sections;
}

// Reword the most common messages more plainly. Anything not recognized is shown as written.
QString SettingsLoadMessageDialog::friendlyText(const QString &text, int itemCount)
{
    if (text.startsWith("Settings for the following channels were not loaded")) {
        QString result = (itemCount == 1) ?
                    tr("Settings for 1 channel were skipped because the controller doesn't detect it right now "
                       "(for example, no headstage is connected). Everything else was loaded.") :
                    tr("Settings for %1 channels were skipped because the controller doesn't detect them right now "
                       "(for example, no headstage is connected). Everything else was loaded.").arg(itemCount);
        if (text.contains("applied automatically")) {
            result += " " + tr("Connect the headstage and rescan the ports, and these settings will be applied. "
                               "They are kept until you load another settings file.");
        }
        return result;
    }
    if (text.startsWith("The following channels are currently detected by the Intan controller but are not included")) {
        return (itemCount == 1) ?
                    tr("1 detected channel is not in this settings file, so its settings were left unchanged:") :
                    tr("%1 detected channels are not in this settings file, so their settings were left unchanged:")
                    .arg(itemCount);
    }
    if (text.startsWith("This file is from a different version of the IntanRHX software")) {
        return tr("This file was saved by a different version of IntanRHX, so full compatibility can't be guaranteed. "
                  "Re-save it from this version to update it and stop this warning.");
    }
    return text;
}

// Collapse runs of consecutively numbered channels with the same prefix (A-000, A-001, ... A-063) into ranges.
QStringList SettingsLoadMessageDialog::compressChannelNames(const QStringList &names)
{
    static const QRegularExpression numbered("^(.*?)(\\d+)$");
    const QString dash = QString(" ") + QChar(0x2013) + " ";

    QStringList ranges;
    QString firstName, lastName, prefix;
    int width = 0;
    int lastNumber = 0;

    auto flush = [&]() {
        if (firstName.isEmpty()) return;
        ranges.append(firstName == lastName ? firstName : firstName + dash + lastName);
    };

    for (const QString &name : names) {
        QRegularExpressionMatch match = numbered.match(name);
        if (match.hasMatch() && !firstName.isEmpty() && width > 0 && match.captured(1) == prefix &&
                match.captured(2).length() == width && match.captured(2).toInt() == lastNumber + 1) {
            lastName = name;
            lastNumber = match.captured(2).toInt();
            continue;
        }
        flush();
        firstName = name;
        lastName = name;
        if (match.hasMatch()) {
            prefix = match.captured(1);
            width = match.captured(2).length();
            lastNumber = match.captured(2).toInt();
        } else {
            prefix.clear();
            width = 0;
            lastNumber = 0;
        }
    }
    flush();
    return ranges;
}
