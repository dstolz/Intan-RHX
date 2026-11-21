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

#include <QtWidgets>
#include <QSettings>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include "spikesortingdialog.h"

SpikeSortingDialog:: SpikeSortingDialog(SystemState* state_, ControllerInterface* controllerInterface_, QWidget *parent) :
    QDialog(parent),
    state(state_),
    controllerInterface(controllerInterface_)
{
    setAcceptDrops(true);
    connect(state, SIGNAL(stateChanged()), this, SLOT(updateFromState()));

    channelName = new QLabel("", this);

    lockScopeCheckbox = new QCheckBox(tr("Lock Plot to Selected"), this);
    connect(lockScopeCheckbox, SIGNAL(clicked()), this, SLOT(toggleLock()));

    setToSelectedButton = new QPushButton(tr("Set to Selected"), this);
    connect(setToSelectedButton, SIGNAL(clicked()), this, SLOT(setToSelected()));

    thresholdSpinBox = new QSpinBox(this);
    thresholdSpinBox->setSingleStep(5);
    thresholdSpinBox->setSuffix(" " + MicroVoltsSymbol);

    // Auto threshold controls. Parameters (not the on/off state) are remembered between sessions.
    QSettings settings;
    const QString AutoThresholdToolTip =
            tr("Periodically set each channel's detection threshold to k") + QChar(0x00B7) + QChar(0x03C3) +
            tr(", where ") + QChar(0x03C3) + tr(" = median(|x|) / 0.6745 of that channel's highpass-filtered signal over the noise window. ") +
            tr("This sets the real threshold: it affects TCP spike output and saved spike events (if enabled).");

    autoThresholdCheckBox = new QCheckBox(tr("Auto Threshold (k") + QChar(0x00B7) + QChar(0x03C3) + ")", this);
    autoThresholdCheckBox->setToolTip(AutoThresholdToolTip);
    autoThresholdCheckBox->setChecked(false);  // Always start off so thresholds never change unexpectedly.
    connect(autoThresholdCheckBox, SIGNAL(clicked(bool)), this, SLOT(toggleAutoThreshold(bool)));

    autoScopeComboBox = new QComboBox(this);
    autoScopeComboBox->addItem(tr("This Channel"));          // AutoScopeThisChannel
    autoScopeComboBox->addItem(tr("All Enabled Channels"));  // AutoScopeAllEnabledChannels
    autoScopeComboBox->setCurrentIndex(qBound(0, settings.value("spikeScopeAutoThreshold/scope", 0).toInt(), 1));
    autoScopeComboBox->setToolTip(tr("Apply to the Spike Scope channel only, or to every enabled amplifier channel ") +
                                  tr("(each channel uses its own ") + QChar(0x03C3) + ")");
    connect(autoScopeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(autoThresholdScopeChanged(int)));

    autoStatusLabel = new QLabel("", this);

    autoMultiplierSpinBox = new QDoubleSpinBox(this);
    autoMultiplierSpinBox->setRange(1.0, 20.0);
    autoMultiplierSpinBox->setSingleStep(0.5);
    autoMultiplierSpinBox->setDecimals(1);
    autoMultiplierSpinBox->setValue(settings.value("spikeScopeAutoThreshold/multiplier", 4.0).toDouble());
    autoMultiplierSpinBox->setToolTip(AutoThresholdToolTip);
    connect(autoMultiplierSpinBox, SIGNAL(valueChanged(double)), this, SLOT(autoThresholdParametersChanged()));

    autoNegativeCheckBox = new QCheckBox(tr("Negative"), this);
    autoNegativeCheckBox->setChecked(settings.value("spikeScopeAutoThreshold/negative", true).toBool());
    connect(autoNegativeCheckBox, SIGNAL(clicked(bool)), this, SLOT(autoThresholdParametersChanged()));

    autoWindowSpinBox = new QDoubleSpinBox(this);
    autoWindowSpinBox->setRange(0.5, 10.0);
    autoWindowSpinBox->setSingleStep(0.5);
    autoWindowSpinBox->setDecimals(1);
    autoWindowSpinBox->setSuffix(" s");
    autoWindowSpinBox->setValue(settings.value("spikeScopeAutoThreshold/windowSeconds", 1.0).toDouble());
    autoWindowSpinBox->setToolTip(tr("Length of the most recent data used to estimate ") + QChar(0x03C3));
    connect(autoWindowSpinBox, SIGNAL(valueChanged(double)), this, SLOT(autoThresholdParametersChanged()));

    autoIntervalSpinBox = new QDoubleSpinBox(this);
    autoIntervalSpinBox->setRange(0.5, 10.0);
    autoIntervalSpinBox->setSingleStep(0.5);
    autoIntervalSpinBox->setDecimals(1);
    autoIntervalSpinBox->setSuffix(" s");
    autoIntervalSpinBox->setValue(settings.value("spikeScopeAutoThreshold/intervalSeconds", 1.0).toDouble());
    autoIntervalSpinBox->setToolTip(tr("How often the threshold is recomputed and applied"));
    connect(autoIntervalSpinBox, SIGNAL(valueChanged(double)), this, SLOT(autoThresholdParametersChanged()));

    voltageScaleComboBox = new QComboBox(this);
    state->yScaleSpikeScope->setupComboBox(voltageScaleComboBox);
    connect(voltageScaleComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setVoltageScale(int)));

    timeScaleComboBox = new QComboBox(this);
    state->tScaleSpikeScope->setupComboBox(timeScaleComboBox);
    connect(timeScaleComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setTimeScale(int)));

    showSpikesComboBox = new QComboBox(this);
    state->numSpikesDisplayed->setupComboBox(showSpikesComboBox);
    connect(showSpikesComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setNumSpikesDisplayed(int)));

    clearScopeButton = new QPushButton(tr("Clear Scope"), this);
    connect(clearScopeButton, SIGNAL(clicked(bool)), this, SLOT(clearScope()));

    takeSnapshotButton = new QPushButton(tr("Take Snapshot"), this);
    connect(takeSnapshotButton, SIGNAL(clicked(bool)), this, SLOT(takeSnapshot()));

    clearSnapshotButton = new QPushButton(tr("Clear Snapshot"), this);
    connect(clearSnapshotButton, SIGNAL(clicked(bool)), this, SLOT(clearSnapshot()));

    QGroupBox *artifactSuppressionBox = new QGroupBox(tr("Artifact Suppression"), this);

    suppressionCheckBox = new QCheckBox(tr("Enable Suppression"), this);
    connect(suppressionCheckBox, SIGNAL(clicked(bool)), this, SLOT(toggleSuppressionEnabled(bool)));

    artifactsShownCheckBox = new QCheckBox(tr("Show Artifacts"), this);
    connect(artifactsShownCheckBox, SIGNAL(clicked(bool)), this, SLOT(toggleArtifactsShown(bool)));

    suppressionThresholdLabel1 = new QLabel(tr("Artifact Threshold"), this);
    suppressionThresholdSpinBox = new QSpinBox(this);
    suppressionThresholdSpinBox->setSingleStep(10);
    suppressionThresholdSpinBox->setSuffix(" " + MicroVoltsSymbol);
    state->suppressionThreshold->setupSpinBox(suppressionThresholdSpinBox);
    connect(suppressionThresholdSpinBox, SIGNAL(valueChanged(int)), this, SLOT(setSuppressionThreshold()));

    QVBoxLayout *artifactSuppressionLayout = new QVBoxLayout;
    QHBoxLayout *suppressionEnabledRow = new QHBoxLayout;
    suppressionEnabledRow->addWidget(suppressionCheckBox);
    QHBoxLayout *artifactsShownRow = new QHBoxLayout;
    artifactsShownRow->addWidget(artifactsShownCheckBox);
    QHBoxLayout *suppressionThresholdRow = new QHBoxLayout;
    suppressionThresholdRow->addWidget(suppressionThresholdLabel1);
    suppressionThresholdRow->addWidget(suppressionThresholdSpinBox);
    suppressionThresholdRow->addStretch(1);
    artifactSuppressionLayout->addLayout(suppressionEnabledRow);
    artifactSuppressionLayout->addLayout(artifactsShownRow);
    artifactSuppressionLayout->addLayout(suppressionThresholdRow);

    artifactSuppressionBox->setLayout(artifactSuppressionLayout);

    // Currently, deactivated any user hoop selection.
    //QGroupBox *hoopSpikeSortingBox = new QGroupBox("Hoop Spike Sorting", this);
    //QCheckBox *useHoopsCheckBox = new QCheckBox("Hoop Sorting Enabled", this);

//    QVBoxLayout *hoopSpikeSortingLayout = new QVBoxLayout;
//    hoopSpikeSortingLayout->addWidget(useHoopsCheckBox);
//    hoopSpikeSortingBox->setLayout(hoopSpikeSortingLayout);

//    unitTabWidget = new QTabWidget;

//    for (int unit = 0; unit < 4; unit++) {
//        QWidget *page = new QWidget(this);
//        QVBoxLayout *pageLayout = new QVBoxLayout;
//        for (int hoop = 0; hoop < 4; hoop++) {
//            QGroupBox *hoopGroupBox = new QGroupBox(this);

//            hoopEnabledCheckBoxes.append(new QCheckBox("Hoop " + QString::number(hoop + 1), this));

//            QHBoxLayout *rowA = new QHBoxLayout;
//            QLabel *labelA = new QLabel("A: ");
//            rowA->addWidget(labelA);

//            tALabels.append(new QLabel("tA"));
//            rowA->addWidget(tALabels[unit * 4 + hoop]);

//            vALabels.append(new QLabel("vA"));
//            rowA->addWidget(vALabels[unit * 4 + hoop]);

//            QHBoxLayout *rowB = new QHBoxLayout;
//            QLabel *labelB = new QLabel("B: ");
//            rowB->addWidget(labelB);

//            tBLabels.append(new QLabel("tB"));
//            rowB->addWidget(tBLabels[unit * 4 + hoop]);

//            vBLabels.append(new QLabel("vB"));
//            rowB->addWidget(vBLabels[unit * 4 + hoop]);

//            QVBoxLayout *hoopLayout = new QVBoxLayout;
//            hoopLayout->addWidget(hoopEnabledCheckBoxes[unit * 4 + hoop]);
//            hoopLayout->addLayout(rowA);
//            hoopLayout->addLayout(rowB);

//            hoopGroupBox->setLayout(hoopLayout);
//            pageLayout->addWidget(hoopGroupBox);
//        }
//        page->setLayout(pageLayout);
//        unitTabWidget->addTab(page, "Unit " + QString::number(unit + 1));
//    }

    loadSpikeSortingParametersButton = new QPushButton(tr("Load Detection Parameters"), this);
    connect(loadSpikeSortingParametersButton, SIGNAL(clicked(bool)), this, SLOT(loadSpikeSortingParameters()));

    saveSpikeSortingParametersButton = new QPushButton(tr("Save Detection Parameters"), this);
    connect(saveSpikeSortingParametersButton, SIGNAL(clicked(bool)), this, SLOT(saveSpikeSortingParameters()));

    spikePlot = new SpikePlot(state, this);

    QHBoxLayout *channelRow = new QHBoxLayout;
    channelRow->addWidget(channelName);
    channelRow->addStretch(1);

    QHBoxLayout *lockRow = new QHBoxLayout;
    lockRow->addWidget(lockScopeCheckbox);

    QHBoxLayout *setRow = new QHBoxLayout;
    setRow->addWidget(setToSelectedButton);

    QVBoxLayout *channelColumn = new QVBoxLayout;
    channelColumn->addLayout(channelRow);
    channelColumn->addLayout(lockRow);
    channelColumn->addLayout(setRow);
    QGroupBox *channelGroup = new QGroupBox(tr("Channel"), this);
    channelGroup->setLayout(channelColumn);

    QHBoxLayout *thresholdRow = new QHBoxLayout;
    thresholdRow->addWidget(new QLabel(tr("Detection Threshold"), this));
    thresholdRow->addWidget(thresholdSpinBox);
    thresholdRow->addStretch(1);

    QHBoxLayout *autoEnableRow = new QHBoxLayout;
    autoEnableRow->addWidget(autoThresholdCheckBox);
    autoEnableRow->addStretch(1);

    QHBoxLayout *autoScopeRow = new QHBoxLayout;
    autoScopeRow->addWidget(new QLabel(tr("Apply To"), this));
    autoScopeRow->addWidget(autoScopeComboBox);
    autoScopeRow->addStretch(1);

    QHBoxLayout *autoMultiplierRow = new QHBoxLayout;
    autoMultiplierRow->addWidget(new QLabel(tr("Multiplier k"), this));
    autoMultiplierRow->addWidget(autoMultiplierSpinBox);
    autoMultiplierRow->addWidget(autoNegativeCheckBox);
    autoMultiplierRow->addStretch(1);

    QHBoxLayout *autoWindowRow = new QHBoxLayout;
    autoWindowRow->addWidget(new QLabel(tr("Noise Window"), this));
    autoWindowRow->addWidget(autoWindowSpinBox);
    autoWindowRow->addStretch(1);

    QHBoxLayout *autoIntervalRow = new QHBoxLayout;
    autoIntervalRow->addWidget(new QLabel(tr("Update Every"), this));
    autoIntervalRow->addWidget(autoIntervalSpinBox);
    autoIntervalRow->addStretch(1);

    QVBoxLayout *detectionSettingsColumn = new QVBoxLayout;
    detectionSettingsColumn->addLayout(thresholdRow);
    detectionSettingsColumn->addLayout(autoEnableRow);
    detectionSettingsColumn->addLayout(autoScopeRow);
    detectionSettingsColumn->addLayout(autoMultiplierRow);
    detectionSettingsColumn->addLayout(autoWindowRow);
    detectionSettingsColumn->addLayout(autoIntervalRow);
    detectionSettingsColumn->addWidget(autoStatusLabel);
    QGroupBox *detectionSettingsGroup = new QGroupBox(tr("Spike Detection Settings"), this);
    detectionSettingsGroup->setLayout(detectionSettingsColumn);

    QHBoxLayout *voltageScaleRow = new QHBoxLayout;
    voltageScaleRow->addWidget(new QLabel(tr("Voltage Scale"), this));
    voltageScaleRow->addWidget(voltageScaleComboBox);
    voltageScaleRow->addStretch(1);

    QHBoxLayout *timeScaleRow = new QHBoxLayout;
    timeScaleRow->addWidget(new QLabel(tr("Time Scale"), this));
    timeScaleRow->addWidget(timeScaleComboBox);
    timeScaleRow->addStretch(1);

    QHBoxLayout *showSpikesRow = new QHBoxLayout;
    showSpikesRow->addWidget(new QLabel(tr("Show"), this));
    showSpikesRow->addWidget(showSpikesComboBox);
    showSpikesRow->addWidget(new QLabel(tr("spikes"), this));
    showSpikesRow->addStretch(1);

    QHBoxLayout *snapshotRow = new QHBoxLayout;
    snapshotRow->addWidget(takeSnapshotButton);
    snapshotRow->addWidget(clearSnapshotButton);

    QVBoxLayout *displaySettingsColumn = new QVBoxLayout;
    displaySettingsColumn->addLayout(voltageScaleRow);
    displaySettingsColumn->addLayout(timeScaleRow);
    displaySettingsColumn->addLayout(showSpikesRow);
    displaySettingsColumn->addWidget(clearScopeButton);
    displaySettingsColumn->addLayout(snapshotRow);
    QGroupBox *displaySettingsGroup = new QGroupBox(tr("Display Settings"), this);
    displaySettingsGroup->setLayout(displaySettingsColumn);

    QVBoxLayout *leftColumn = new QVBoxLayout;
    leftColumn->addWidget(channelGroup);
    leftColumn->addWidget(displaySettingsGroup);
    leftColumn->addWidget(detectionSettingsGroup);
    leftColumn->addWidget(artifactSuppressionBox);
//    leftColumn->addWidget(hoopSpikeSortingBox);
//    leftColumn->addWidget(unitTabWidget);
    leftColumn->addStretch();
    leftColumn->addWidget(loadSpikeSortingParametersButton);
    leftColumn->addWidget(saveSpikeSortingParametersButton);

    QVBoxLayout *rightColumn = new QVBoxLayout;
    rightColumn->addWidget(spikePlot);

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->addLayout(leftColumn);
    mainLayout->addLayout(rightColumn);
    mainLayout->setStretch(0, 0);
    mainLayout->setStretch(1, 1);

    setLayout(mainLayout);

    spikeSettingsInterface = new XMLInterface(state, controllerInterface, XMLIncludeSpikeSortingParameters);

    state->updateForChangeHeadstages();

    state->signalSources->channelByName(state->spikeScopeChannel->getValue())->setupSpikeThresholdSpinBox(thresholdSpinBox);
    connect(thresholdSpinBox, SIGNAL(valueChanged(int)), this, SLOT(setVoltageThreshold(int)));

    if (state->running) updateForRun();
    else updateForStop();
}

SpikeSortingDialog::~SpikeSortingDialog()
{
    delete spikeSettingsInterface;
    delete spikePlot;
}

void SpikeSortingDialog::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        // Only accept events with a single local file url
        if (event->mimeData()->urls().size() == 1) {
            if (event->mimeData()->urls().at(0).isLocalFile())
                event->accept();
        }
    }
}

void SpikeSortingDialog::dropEvent(QDropEvent *event)
{
    QSettings settings;
    QString filename = event->mimeData()->urls().at(0).toLocalFile();

    QFileInfo fileInfo(filename);
    settings.setValue("spikeSortingDirectory", fileInfo.absolutePath());

    QString errorMessage;
    bool loadSuccess = spikeSettingsInterface->loadFile(filename, errorMessage);

    if (!loadSuccess) {
        QMessageBox::critical(this, tr("Error: Loading from XML"), errorMessage);
        return;
    } else if (!errorMessage.isEmpty()) {
        QMessageBox::warning(this, tr("Warning: Loading from XML"), errorMessage);
    }
}

void SpikeSortingDialog::updateFromState()
{
    // Check if single channel selection has changed at all.
    QString channelNativeName = state->signalSources->singleSelectedAmplifierChannelName();
    if (channelNativeName != state->spikeScopeChannel->getValue()) {
        if (!channelNativeName.isEmpty()) {
            setToSelectedButton->setEnabled(true);
            if (lockScopeCheckbox->isChecked()) {
                changeCurrentChannel(channelNativeName);
            }
        } else {
            setToSelectedButton->setEnabled(false); // (a) Disable setToSelectedButton.
        }
    }

    // Update channel name label.
    if (channelName->text() != state->spikeScopeChannel->getValue()) {
        channelName->setText(state->signalSources->getNativeAndCustomNames(state->spikeScopeChannel->getValue()));
    }
    if (channelName->text().isEmpty()) {
        channelName->setText(tr("N/A"));
    }

    // Update spike plot if channel has changed.
    if (spikePlot->getWaveform() != state->spikeScopeChannel->getValue()) {
        spikePlot->setWaveform(state->spikeScopeChannel->getValue().toStdString());
        autoThresholdTimer.invalidate();  // New channel: compute its auto threshold right away.
    }

    // Update spike threshold if spikeScopeChannel exists.
    QString channelName = state->spikeScopeChannel->getValue();

    if (channelName != "N/A") {
        if (thresholdSpinBox->value() != state->signalSources->channelByName(channelName)->getSpikeThreshold()) {
            thresholdSpinBox->setValue(state->signalSources->channelByName(channelName)->getSpikeThreshold());
        }
    }

    if (voltageScaleComboBox->currentIndex() != state->yScaleSpikeScope->getIndex())
        voltageScaleComboBox->setCurrentIndex(state->yScaleSpikeScope->getIndex());

    if (timeScaleComboBox->currentIndex() != state->tScaleSpikeScope->getIndex())
        timeScaleComboBox->setCurrentIndex(state->tScaleSpikeScope->getIndex());

    if (showSpikesComboBox->currentIndex() != state->numSpikesDisplayed->getIndex())
        showSpikesComboBox->setCurrentIndex(state->numSpikesDisplayed->getIndex());

    if (suppressionCheckBox->isChecked() != state->suppressionEnabled->getValue())
        suppressionCheckBox->setChecked(state->suppressionEnabled->getValue());

    if (artifactsShownCheckBox->isChecked() != state->artifactsShown->getValue())
        artifactsShownCheckBox->setChecked(state->artifactsShown->getValue());

    if (suppressionThresholdSpinBox->value() != state->suppressionThreshold->getValue())
        suppressionThresholdSpinBox->setValue(state->suppressionThreshold->getValue());

    updateTitle();
}

void SpikeSortingDialog::updateForRun()
{
    loadSpikeSortingParametersButton->setEnabled(false);
    saveSpikeSortingParametersButton->setEnabled(false);
}

void SpikeSortingDialog::updateForLoad()
{
    updateForRun();
}

void SpikeSortingDialog::updateForStop()
{
    loadSpikeSortingParametersButton->setEnabled(true);
    saveSpikeSortingParametersButton->setEnabled(true);
}

void SpikeSortingDialog::updateForChangeHeadstages()
{
    spikePlot->setWaveform(state->spikeScopeChannel->getValue().toStdString());
    autoThresholdTimer.invalidate();
}

void SpikeSortingDialog::activate()
{
    updateFromState();
    show();
    raise();
    activateWindow();
}

void SpikeSortingDialog::updateSpikeScope(WaveformFifo *waveformFifo, int numSamples)
{
    // Spike plot is only drawn while visible, but auto threshold (if on) keeps running when the
    // window is closed, since it sets real detection thresholds.
    if (!isHidden()) spikePlot->updateWaveforms(waveformFifo, numSamples);
    updateAutoThreshold(waveformFifo);
}

bool SpikeSortingDialog::autoThresholdAllChannelsActive() const
{
    return autoThresholdCheckBox->isChecked() && autoScopeComboBox->currentIndex() == AutoScopeAllEnabledChannels;
}

void SpikeSortingDialog::updateAutoThreshold(WaveformFifo *waveformFifo)
{
    if (!autoThresholdCheckBox->isChecked()) return;

    // Recompute at most once per user-set interval. An invalid timer means "update at next opportunity".
    if (autoThresholdTimer.isValid() &&
            autoThresholdTimer.elapsed() < (qint64) round(1000.0 * autoIntervalSpinBox->value())) return;

    int numWords = (int) ceil(autoWindowSpinBox->value() * state->sampleRate->getNumericValue());
    if (numWords <= 0) return;
    if (waveformFifo->numWordsInMemory(WaveformFifo::ReaderDisplay) < numWords) {
        autoStatusLabel->setText(tr("Auto: waiting for data..."));
        return;  // Wait until the full noise window is in memory; retry next refresh.
    }

    // Build list of channels to update.
    QElapsedTimer computeTimer;
    computeTimer.start();

    QString scopeChannelName = state->spikeScopeChannel->getValue();
    std::vector<Channel*> channels;
    std::vector<GpuWaveformAddress> addresses;
    auto addChannel = [&](const std::string &name) {
        Channel* channel = state->signalSources->channelByName(name);
        if (!channel || !channel->isEnabled() || channel->getSignalType() != AmplifierSignal) return;
        std::string waveName = name + "|HIGH";
        if (!waveformFifo->gpuWaveformPresent(waveName)) return;
        GpuWaveformAddress address = waveformFifo->getGpuWaveformAddress(waveName);
        if (address.waveformIndex < 0) return;
        channels.push_back(channel);
        addresses.push_back(address);
    };
    if (autoScopeComboBox->currentIndex() == AutoScopeAllEnabledChannels) {
        std::vector<std::string> names = state->signalSources->amplifierChannelsNameList();
        for (const std::string &name : names) addChannel(name);
    } else {
        addChannel(scopeChannelName.toStdString());
    }
    if (channels.empty()) {
        autoStatusLabel->setText(tr("Auto: no enabled channel"));
        autoThresholdTimer.start();
        return;
    }

    std::vector<double> sigmas;
    if (!measureNoiseSigmas(waveformFifo, addresses, numWords, sigmas)) return;
    autoThresholdTimer.start();

    // Apply each channel's own threshold, batching state updates into one.
    const double k = autoMultiplierSpinBox->value();
    const bool negative = autoNegativeCheckBox->isChecked();
    int numChanged = 0;
    state->holdUpdate();
    for (int i = 0; i < (int) channels.size(); ++i) {
        if (channels[i]->getNativeName() == scopeChannelName) spikePlot->setLatestSigma(sigmas[i]);
        if (sigmas[i] <= 0.0) continue;  // Flat signal (or median beyond histogram range); leave threshold alone.
        double threshold = negative ? -k * sigmas[i] : k * sigmas[i];
        int thresholdInt = qBound(-5000, (int) round(threshold), 5000);  // Range of Channel's SpikeThresholdMicroVolts.
        if (thresholdInt == 0) continue;  // A zero threshold would trigger on nearly every sample.
        if (thresholdInt != channels[i]->getSpikeThreshold()) {
            channels[i]->setSpikeThreshold(thresholdInt);
            ++numChanged;
        }
    }
    state->releaseUpdate();

    autoStatusLabel->setText(tr("Auto: ") + QString::number(channels.size()) +
                             (channels.size() == 1 ? tr(" channel, ") : tr(" channels, ")) +
                             QString::number((double) computeTimer.nsecsElapsed() / 1.0e6, 'f', 1) + tr(" ms"));
}

// Median-based noise estimate for each channel in 'addresses', over the most recent numWords samples of
// the display reader's memory: sigma = median(|x|) / 0.6745, with x the highpass-filtered signal.
// Uses one pass over the raw (uint16, offset 32768, 0.195 uV/LSB) interleaved data with a per-channel
// histogram of |x|, so cost is linear in samples and memory is read in order. Channels whose median
// falls beyond the histogram range get sigma = -1 (not applied).
bool SpikeSortingDialog::measureNoiseSigmas(WaveformFifo *waveformFifo, const std::vector<GpuWaveformAddress> &addresses,
                                            int numWords, std::vector<double> &sigmas)
{
    const int numChannels = (int) addresses.size();
    const int NumBins = 4096;        // |x| in LSB: 0 ... 4095 (0 ... ~799 uV); last bin collects anything larger.
    const int ChunkSamples = 2048;   // Copy in chunks to bound memory use for long windows / many channels.
    const double MicroVoltsPerLsb = 0.195;

    noiseHistograms.assign((size_t) numChannels * NumBins, 0U);
    noiseChunkBuffer.resize((size_t) numChannels * ChunkSamples);

    for (int t = -numWords; t < 0; t += ChunkSamples) {
        int n = std::min(ChunkSamples, -t);
        waveformFifo->copyGpuAmplifierDataArrayRaw(WaveformFifo::ReaderDisplay, noiseChunkBuffer.data(), addresses, t, n);
        const uint16_t* p = noiseChunkBuffer.data();
        for (int s = 0; s < n; ++s) {
            for (int c = 0; c < numChannels; ++c) {
                int value = std::abs((int) *p++ - 32768);
                if (value >= NumBins) value = NumBins - 1;
                ++noiseHistograms[(size_t) c * NumBins + value];
            }
        }
    }

    // Grouped median with linear interpolation inside the median bin. Integer value v covers [v - 0.5, v + 0.5),
    // except v = 0 which (as an absolute value) covers [0, 0.5).
    sigmas.assign(numChannels, -1.0);
    const double half = 0.5 * (double) numWords;
    for (int c = 0; c < numChannels; ++c) {
        const uint32_t* hist = &noiseHistograms[(size_t) c * NumBins];
        double cumulative = 0.0;
        for (int v = 0; v < NumBins - 1; ++v) {
            double count = (double) hist[v];
            if (cumulative + count >= half && count > 0.0) {
                double lower = (v == 0) ? 0.0 : v - 0.5;
                double width = (v == 0) ? 0.5 : 1.0;
                double medianLsb = lower + width * (half - cumulative) / count;
                sigmas[c] = medianLsb * MicroVoltsPerLsb / 0.6745;
                break;
            }
            cumulative += count;
        }
    }
    return true;
}

void SpikeSortingDialog::toggleAutoThreshold(bool enabled)
{
    autoThresholdModeChanged();
    if (enabled) autoStatusLabel->setText(tr("Auto: starts when running"));
    else autoStatusLabel->setText("");
}

void SpikeSortingDialog::autoThresholdScopeChanged(int /* index */)
{
    saveAutoThresholdSettings();
    autoThresholdModeChanged();
}

void SpikeSortingDialog::autoThresholdModeChanged()
{
    bool enabled = autoThresholdCheckBox->isChecked();
    thresholdSpinBox->setEnabled(!enabled);
    spikePlot->setAutoThresholdActive(enabled);
    autoThresholdTimer.invalidate();  // Apply as soon as enough data is available.
    state->forceUpdate();             // Let ControlWindow re-evaluate whether Set Spike Detection Thresholds is enabled.
}

void SpikeSortingDialog::autoThresholdParametersChanged()
{
    saveAutoThresholdSettings();
    autoThresholdTimer.invalidate();  // Apply new parameters at the next display refresh.
}

void SpikeSortingDialog::saveAutoThresholdSettings() const
{
    QSettings settings;
    settings.setValue("spikeScopeAutoThreshold/scope", autoScopeComboBox->currentIndex());
    settings.setValue("spikeScopeAutoThreshold/multiplier", autoMultiplierSpinBox->value());
    settings.setValue("spikeScopeAutoThreshold/negative", autoNegativeCheckBox->isChecked());
    settings.setValue("spikeScopeAutoThreshold/windowSeconds", autoWindowSpinBox->value());
    settings.setValue("spikeScopeAutoThreshold/intervalSeconds", autoIntervalSpinBox->value());
}

void SpikeSortingDialog::loadSpikeSortingParameters()
{
    QSettings settings;
    QString defaultDirectory = settings.value("spikeSortingDirectory", ".").toString();
    QString filename = QFileDialog::getOpenFileName(this, "Select Parameter File", defaultDirectory, "XML files (*.xml)");

    // If user clicked 'cancel', then abort.
    if (filename.isEmpty()) return;

    QFileInfo fileInfo(filename);
    settings.setValue("spikeSortingDirectory", fileInfo.absolutePath());

    QString errorMessage;
    bool loadSuccess = spikeSettingsInterface->loadFile(filename, errorMessage);

    if (!loadSuccess) {
        QMessageBox::critical(this, tr("Error: Loading from XML"), errorMessage);
        return;
    } else if (!errorMessage.isEmpty()) {
        QMessageBox::warning(this, tr("Warning: Loading from XML"), errorMessage);
    }
}

void SpikeSortingDialog::saveSpikeSortingParameters()
{
    QSettings settings;
    QString defaultDirectory = settings.value("spikeSortingDirectory", ".").toString();
    QString filename = QFileDialog::getSaveFileName(this, "Save Spike Sorting Parameters As", defaultDirectory, "XML files (*.xml)");

    // If user clicked 'cancel', then abort.
    if (filename.isEmpty()) return;

    QFileInfo fileInfo(filename);
    settings.setValue("spikeSortingDirectory", fileInfo.absolutePath());

    if (!spikeSettingsInterface->saveFile(filename)) qDebug() << "Failure writing XML Spike Sorting Parameters";
}

void SpikeSortingDialog::setToSelected()
{
    QString channelNativeName = state->signalSources->singleSelectedAmplifierChannelName();
    if (channelNativeName.isEmpty()) {
        qDebug() << "Error: Somehow this was triggered without a single channel being selected";
        return;
    }
    changeCurrentChannel(channelNativeName);
    spikePlot->setFocus();
}

void SpikeSortingDialog::toggleSuppressionEnabled(bool enabled)
{
    artifactsShownCheckBox->setEnabled(enabled);
    suppressionThresholdLabel1->setEnabled(enabled);
    suppressionThresholdSpinBox->setEnabled(enabled);
    state->suppressionEnabled->setValue(enabled);
}

void SpikeSortingDialog::updateTitle()
{
    setWindowTitle(tr("Spike Scope") + " (" + state->spikeScopeChannel->getValue() + ")");
}
