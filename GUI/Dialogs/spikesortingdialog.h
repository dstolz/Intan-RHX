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

#ifndef SPIKESORTINGDIALOG_H
#define SPIKESORTINGDIALOG_H

#include <QDialog>
#include <QElapsedTimer>

#include "systemstate.h"
#include "xmlinterface.h"
#include "spikeplot.h"

class QLabel;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QPushButton;
class QCheckBox;
class QTabWidget;
class WaveformFifo;
class ControllerInterface;

class SpikeSortingDialog : public QDialog
{
    Q_OBJECT
public:
    SpikeSortingDialog(SystemState* state_, ControllerInterface* controllerInterface_, QWidget *parent = nullptr);
    ~SpikeSortingDialog();

    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    void updateForRun();
    void updateForLoad();
    void updateForStop();
    void updateForChangeHeadstages();

    void updateSpikeScope(WaveformFifo *waveformFifo, int numSamples);
    void activate();

    // True while auto threshold is on and applied to all enabled channels (ControlWindow greys out
    // Set Spike Detection Thresholds in that case, since auto would overwrite its result).
    bool autoThresholdAllChannelsActive() const;

private slots:
    void updateFromState();
    void setVoltageThreshold(int threshold)
        { state->signalSources->channelByName(state->spikeScopeChannel->getValue())->setSpikeThreshold(threshold); }
    void setVoltageScale(int index)
        { state->yScaleSpikeScope->setIndex(index); }
    void setTimeScale(int index)
        { state->tScaleSpikeScope->setIndex(index); }
    void setNumSpikesDisplayed(int index)
        { state->numSpikesDisplayed->setIndex(index); }
    void loadSpikeSortingParameters();
    void saveSpikeSortingParameters();
    void clearScope() { spikePlot->clearSpikes(); spikePlot->setFocus(); }
    void takeSnapshot() { spikePlot->takeSnapshot(); spikePlot->clearSpikes(); spikePlot->setFocus(); }
    void clearSnapshot() { spikePlot->clearSnapshot(); spikePlot->setFocus(); }
    void changeCurrentChannel(const QString& nativeChannelName)
        { state->spikeScopeChannel->setValue(nativeChannelName); }
    void toggleLock() { updateFromState(); }
    void setToSelected();
    void toggleSuppressionEnabled(bool enabled);
    void toggleArtifactsShown(bool enabled)
        { state->artifactsShown->setValue(enabled); }
    void setSuppressionThreshold()
        { state->suppressionThreshold->setValue(suppressionThresholdSpinBox->value()); }
    void toggleAutoThreshold(bool enabled);
    void autoThresholdScopeChanged(int index);
    void autoThresholdParametersChanged();

private:   
    SystemState* state;
    ControllerInterface* controllerInterface;

    XMLInterface *spikeSettingsInterface;

    QPushButton *loadSpikeSortingParametersButton;
    QPushButton *saveSpikeSortingParametersButton;

    QLabel *channelName;

    QCheckBox *lockScopeCheckbox;
    QPushButton *setToSelectedButton;

    QSpinBox *thresholdSpinBox;

    // Auto threshold: for each channel c, Thr_c = k * sigma_c, sigma_c = median(|x_c|) / 0.6745 on that
    // channel's own |HIGH signal. Writes the channels' real spike thresholds, so it affects detection,
    // TCP spike output, and saved spike events (if enabled) exactly like setting thresholds by hand.
    enum AutoThresholdScope { AutoScopeThisChannel = 0, AutoScopeAllEnabledChannels = 1 };
    QCheckBox *autoThresholdCheckBox;
    QComboBox *autoScopeComboBox;
    QDoubleSpinBox *autoMultiplierSpinBox;
    QCheckBox *autoNegativeCheckBox;
    QDoubleSpinBox *autoWindowSpinBox;
    QDoubleSpinBox *autoIntervalSpinBox;
    QLabel *autoStatusLabel;
    QElapsedTimer autoThresholdTimer;
    std::vector<uint16_t> noiseChunkBuffer;
    std::vector<uint32_t> noiseHistograms;

    void updateAutoThreshold(WaveformFifo *waveformFifo);
    bool measureNoiseSigmas(WaveformFifo *waveformFifo, const std::vector<GpuWaveformAddress> &addresses,
                            int numWords, std::vector<double> &sigmas);
    void saveAutoThresholdSettings() const;
    void autoThresholdModeChanged();

    QComboBox *voltageScaleComboBox;

    QComboBox *timeScaleComboBox;

    QComboBox *showSpikesComboBox;

    QPushButton *clearScopeButton;
    QPushButton *takeSnapshotButton;
    QPushButton *clearSnapshotButton;

    QCheckBox *suppressionCheckBox;
    QCheckBox *artifactsShownCheckBox;
    QLabel *suppressionThresholdLabel1;
    QSpinBox *suppressionThresholdSpinBox;

    QTabWidget *unitTabWidget;
    QVector<QWidget*> unitTabs;
    QVector<QCheckBox*> hoopEnabledCheckBoxes;
    QVector<QLabel*> tALabels;
    QVector<QLabel*> vALabels;
    QVector<QLabel*> tBLabels;
    QVector<QLabel*> vBLabels;

    SpikePlot *spikePlot;

    void updateTitle();
};

#endif // SPIKESORTINGDIALOG_H
