/*
 *
Copyright (C) 2016  Gabriele Salvato

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/
#include "configureIvsVdialog.h"
#include "ui_configureIvsVdialog.h"


#include <QSettings>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>


ConfigureIvsVDialog::ConfigureIvsVDialog(QWidget *parent)
  : QDialog(parent)
  , sBaseDir(QDir::homePath())
  , sOutFileName("junction.dat")
  , currentMin(-1.0e-3)
  , currentMax(1.0e-3)
  , voltageMin(-10.0)
  , voltageMax(10.0)
  , temperatureMin(0.0)
  , temperatureMax(450.0)
  , ui(new Ui::ConfigureIvsVDialog)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);
  ui->setupUi(this);

  sNormalStyle = ui->IStartEdit->styleSheet();
  sErrorStyle  = "QLineEdit { color: rgb(255, 255, 255); background: rgb(255, 0, 0); selection-background-color: rgb(128, 128, 255); }";

  restoreSettings();
  setToolTips();
  // Measurement parameters
  if(!isCurrentValid(dIStart))
    dIStart = 0.0;
  ui->IStartEdit->setText(QString("%1").arg(dIStart, 0, 'g', 2));
  //
  if(!isCurrentValid(dIStop))
    dIStop = 0.0;
  ui->IStopEdit->setText(QString("%1").arg(dIStop, 0, 'g', 2));
  //
  if(!isVoltageValid(dVStart))
    dVStart = 0.0;
  ui->VStartEdit->setText(QString("%1").arg(dVStart, 0, 'g', 2));
  //
  if(!isVoltageValid(dVStop))
    dVStop = 0.0;
  ui->VStopEdit->setText(QString("%1").arg(dVStop, 0, 'g', 2));

  // Temperature Control
  if(!isTemperatureValid(dTemperature))
    dTemperature = 300.0;
  ui->TValueEdit->setText(QString("%1").arg(dTemperature, 0, 'f', 2));

  if(bUseThermostat) {
    ui->ThermostatCheckBox->setChecked(true);
    ui->TValueEdit->setEnabled(true);
  }
  else {
    ui->ThermostatCheckBox->setChecked(false);
    ui->TValueEdit->setEnabled(false);
  }

  // Sample information
  ui->sampleInformationEdit->setPlainText(sSampleInfo);

  // Output Path
  ui->outPathEdit->setText(sBaseDir);

  // Output File Name
  ui->outFileEdit->setText(sOutFileName);
}


ConfigureIvsVDialog::~ConfigureIvsVDialog() {
  delete ui;
}


void
ConfigureIvsVDialog::closeEvent(QCloseEvent *event) {
  Q_UNUSED(event)
  saveSettings();
}


void
ConfigureIvsVDialog::restoreSettings() {
  QSettings settings;

  restoreGeometry(settings.value("ConfigureIvsVDialogGeometry").toByteArray());

  dIStart        = settings.value("ConfigureIvsVIStart", 0.0).toDouble();
  dIStop         = settings.value("ConfigureIvsVIStop", 0.0).toDouble();
  dVStart        = settings.value("ConfigureIvsVVStart", 0.0).toDouble();
  dVStop         = settings.value("ConfigureIvsVVStop", 0.0).toDouble();

  dTemperature   = settings.value("ConfigureIvsVTemperture", 300.0).toDouble();
  bUseThermostat = settings.value("ConfigureIvsVUseThermostat", false).toBool();

  sSampleInfo    = settings.value("ConfigureIvsVSampleInfo", "").toString();
  sBaseDir       = settings.value("ConfigureIvsVBaseDir", sBaseDir).toString();
  sOutFileName   = settings.value("ConfigureIvsVOutFileName", sOutFileName).toString();
}


void
ConfigureIvsVDialog::saveSettings() {
  QSettings settings;

  settings.setValue("ConfigureIvsVIStart", dIStart);
  settings.setValue("ConfigureIvsVIStop", dIStop);
  settings.setValue("ConfigureIvsVVStart", dVStart);
  settings.setValue("ConfigureIvsVVStop", dVStop);

  settings.setValue("ConfigureIvsVTemperture", dTemperature);
  settings.setValue("ConfigureIvsVUseThermostat", bUseThermostat);

  sSampleInfo = ui->sampleInformationEdit->toPlainText();
  settings.setValue("ConfigureIvsVSampleInfo", sSampleInfo);
  settings.setValue("ConfigureIvsVBaseDir", sBaseDir);
  settings.setValue("ConfigureIvsVOutFileName", sOutFileName);
}


void
ConfigureIvsVDialog::setToolTips() {
  QString sHeader = QString("Enter values in range [%1 : %2]");

  ui->IStartEdit->setToolTip(sHeader.arg(currentMin).arg(currentMax));
  ui->IStopEdit->setToolTip(sHeader.arg(currentMin).arg(currentMax));
  ui->VStartEdit->setToolTip(sHeader.arg(voltageMin).arg(voltageMax));
  ui->VStopEdit->setToolTip(sHeader.arg(voltageMin).arg(voltageMax));

  ui->ThermostatCheckBox->setToolTip(QString("Enable/Disable Thermostat Use"));
  ui->TValueEdit->setToolTip(sHeader.arg(temperatureMin).arg(temperatureMax));

  ui->sampleInformationEdit->setToolTip(QString("Enter Sample description (multiline)"));
  ui->outPathEdit->setToolTip(QString("Output File Folder"));
  ui->outFileEdit->setToolTip(QString("Enter Output File Name"));
  ui->outFilePathButton->setToolTip((QString("Press to Change Output File Folder")));
  ui->doneButton->setToolTip(QString("Press when Done"));
}


bool
ConfigureIvsVDialog::isCurrentValid(double dCurrent) {
  return (dCurrent >= currentMin) &&
         (dCurrent <= currentMax);
}


bool
ConfigureIvsVDialog::isVoltageValid(double dVoltage) {
  return (dVoltage >= voltageMin) &&
         (dVoltage <= voltageMax);
}


bool
ConfigureIvsVDialog::isTemperatureValid(double dTemperature) {
  return (dTemperature >= temperatureMin) &&
         (dTemperature <= temperatureMax);
}


void
ConfigureIvsVDialog::on_outFilePathButton_clicked() {
  QFileDialog chooseDirDialog;
  QDir outDir(sBaseDir);
  chooseDirDialog.setFileMode(QFileDialog::DirectoryOnly);
  if(outDir.exists()) {
    chooseDirDialog.setDirectory(outDir);
  }
  else {
    chooseDirDialog.setDirectory(QDir::homePath());
  }
  if(chooseDirDialog.exec() == QDialog::Accepted) {
    sBaseDir = chooseDirDialog.selectedFiles().at(0);
  }
  ui->outPathEdit->setText(sBaseDir);
}


void
ConfigureIvsVDialog::on_ThermostatCheckBox_stateChanged(int arg1) {
  if(arg1) {
    ui->labelTValue->setEnabled(true);
    ui->TValueEdit->setEnabled(true);
  }
  else {
    ui->labelTValue->setDisabled(true);
    ui->TValueEdit->setDisabled(true);
  }
}


void
ConfigureIvsVDialog::on_doneButton_clicked() {
  if(sOutFileName == QString()) {
    QMessageBox::information(
          this,
          QString("Empty Output Filename"),
          QString("Please enter a Valid Output File Name"));
    ui->outFileEdit->setFocus();
    return;
  }
  if(QDir(sBaseDir).exists(sOutFileName)) {
    int iAnswer = QMessageBox::question(
                    this,
                    QString("File Already exists"),
                    QString("Overwrite %1 ?").arg(sOutFileName),
                    QMessageBox::Yes,
                    QMessageBox::No,
                    QMessageBox::NoButton);
    if(iAnswer == QMessageBox::No) {
      ui->outFileEdit->setFocus();
      return;
    }
  }
  saveSettings();
  accept();
}


void
ConfigureIvsVDialog::on_cancelButton_clicked() {
  reject();
}


void
ConfigureIvsVDialog::on_IStartEdit_textChanged(const QString &arg1) {
  if(isCurrentValid(arg1.toDouble())){
    dIStart = arg1.toDouble();
    ui->IStartEdit->setStyleSheet(sNormalStyle);
  }
  else {
    ui->IStartEdit->setStyleSheet(sErrorStyle);
  }
}


void
ConfigureIvsVDialog::on_IStopEdit_textChanged(const QString &arg1) {
  if(isCurrentValid(arg1.toDouble())){
    dIStop = arg1.toDouble();
    ui->IStopEdit->setStyleSheet(sNormalStyle);
  }
  else {
    ui->IStopEdit->setStyleSheet(sErrorStyle);
  }
}


void
ConfigureIvsVDialog::on_VStartEdit_textChanged(const QString &arg1) {
  if(isVoltageValid(arg1.toDouble())){
    dVStart = arg1.toDouble();
    ui->VStartEdit->setStyleSheet(sNormalStyle);
  }
  else {
    ui->VStartEdit->setStyleSheet(sErrorStyle);
  }
}


void
ConfigureIvsVDialog::on_VStopEdit_textChanged(const QString &arg1) {
  if(isVoltageValid(arg1.toDouble())){
    dVStop = arg1.toDouble();
    ui->VStopEdit->setStyleSheet(sNormalStyle);
  }
  else {
    ui->VStopEdit->setStyleSheet(sErrorStyle);
  }
}


void
ConfigureIvsVDialog::on_TValueEdit_textChanged(const QString &arg1) {
  if(isTemperatureValid(arg1.toDouble())){
    dTemperature = arg1.toDouble();
    ui->TValueEdit->setStyleSheet(sNormalStyle);
  }
  else {
    ui->TValueEdit->setStyleSheet(sErrorStyle);
  }
}
