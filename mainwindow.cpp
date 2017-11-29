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

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "utility.h"
#include "keithley236.h"
#include "lakeshore330.h"
#include "plot2d.h"
#include "math.h"

#include <QMessageBox>
#include <QDebug>
#include <QSettings>
#include <QFile>
#include <QThread>
#include <QLayout>
#include <QSerialPortInfo>


MainWindow::MainWindow(QWidget *parent)
  : QMainWindow(parent)
  , ui(new Ui::MainWindow)
  , pOutputFile(Q_NULLPTR)
  , pKeithley(Q_NULLPTR)
  , pLakeShore(Q_NULLPTR)
  , LAMP_ON(1)
  , LAMP_OFF(0)
  , gpibBoardID(0)
  , currentLampStatus(LAMP_OFF)
  , pPlotMeasurements(Q_NULLPTR)
  , pPlotTemperature(Q_NULLPTR)
  , maxPlotPoints(3000)
  , maxReachingTTime(120)// In seconds
  , timeBetweenMeasurements(10000)
  , iPlotDark(1)
  , iPlotPhoto(2)
  , isK236ReadyForTrigger(false)
  , bRunning(false)
  // For Arduino Serial Port
  , baudRate(QSerialPort::Baud115200)
  , waitTimeout(1000)
  , nSweepPoints(100)

{
  ui->setupUi(this);
  // To remove the resize-handle in the lower right corner
  ui->statusBar->setSizeGripEnabled(false);
  // To make the size of the window fixed
  setFixedSize(size());

  ui->startRvsTButton->show();
  ui->startIvsVButton->show();

  QSettings settings;
  restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
  restoreState(settings.value("mainWindowState").toByteArray());
}


MainWindow::~MainWindow() {
  if(pKeithley != Q_NULLPTR) delete pKeithley;
  pKeithley = Q_NULLPTR;
  if(pLakeShore != Q_NULLPTR) delete pLakeShore;
  pLakeShore = Q_NULLPTR;

  if(pPlotMeasurements) delete pPlotMeasurements;
  pPlotMeasurements = Q_NULLPTR;
  if(pPlotTemperature) delete pPlotTemperature;
  pPlotTemperature = Q_NULLPTR;

  delete ui;
}


void
MainWindow::closeEvent(QCloseEvent *event) {
  Q_UNUSED(event)
  QSettings settings;
  settings.setValue("mainWindowGeometry", saveGeometry());
  settings.setValue("mainWindowState", saveState());
  serialPort.close();
  if(bRunning) {
    waitingTStartTimer.stop();
    stabilizingTimer.stop();
    readingTTimer.stop();
    measuringTimer.stop();
    disconnect(&waitingTStartTimer, 0, 0, 0);
    disconnect(&stabilizingTimer, 0, 0, 0);
    disconnect(&readingTTimer, 0, 0, 0);
    disconnect(&measuringTimer, 0, 0, 0);
    if(pOutputFile) {
      if(pOutputFile->isOpen())
        pOutputFile->close();
      pOutputFile->deleteLater();
      pOutputFile = Q_NULLPTR;
    }
    if(pKeithley) pKeithley->endVvsT();
    if(pLakeShore) pLakeShore->switchPowerOff();
  }
}


bool
MainWindow::CheckInstruments() {
  Addr4882_t padlist[31];
  Addr4882_t resultlist[31];
  for(short i=0; i<30; i++) padlist[i] = i+1;
  padlist[30] = NOADDR;

  // Enable assertion of REN when System Controller
  // This is required by the Keithley 236
  ibconfig(gpibBoardID, IbcSRE, 1);
  if(isGpibError("MainWindow::CheckInstruments(): ibconfig() Unable to set REN When SC"))
    return false;
  SendIFC(gpibBoardID);
  if(isGpibError("MainWindow::CheckInstruments(): SendIFC Error"))
    return false;

  // If addrlist contains only the constant NOADDR,
  // the Universal Device Clear (DCL) message is sent
  // to all the devices on the bus
  Addr4882_t addrlist;
  addrlist = NOADDR;
  DevClearList(gpibBoardID, &addrlist);
  if(isGpibError("MainWindow::CheckInstruments() - DevClearList() failed. Are the Instruments Connected and Switced On ?"))
    return false;

  FindLstn(gpibBoardID, padlist, resultlist, 30);
  if(isGpibError("MainWindow::CheckInstruments() - FindLstn() failed. Are the Instruments Connected and Switced On ?"))
    return false;
  int nDevices = ThreadIbcntl();
//  qInfo() << QString("Found %1 Instruments connected to the GPIB Bus").arg(nDevices);
  // Identify the instruments connected to the GPIB Bus
  char readBuf[257];
  QString sCommand = "*IDN?";
  for(int i=0; i<nDevices; i++) {
    Send(gpibBoardID, resultlist[i], sCommand.toUtf8().constData(), sCommand.length(), DABend);
    if(isGpibError("MainWindow::CheckInstruments() - *IDN? Failed"))
      return false;
    Receive(gpibBoardID, resultlist[i], readBuf, 256, STOPend);
    if(isGpibError("MainWindow::CheckInstruments() - Receive() Failed"))
      return false;
    readBuf[ibcnt] = 0;
    QString sInstrumentID = QString(readBuf);
//    qDebug() << QString("InstrumentID= %1").arg(sInstrumentID);
    // La source Measure Unit K236 non risponde al comando di identificazione !!!!
    if(sInstrumentID.contains("NSDC", Qt::CaseInsensitive)) {
      if(pKeithley == Q_NULLPTR) {
        pKeithley = new Keithley236(gpibBoardID, resultlist[i], this);
      }
    } else if(sInstrumentID.contains("MODEL330", Qt::CaseInsensitive)) {
      if(pLakeShore == NULL)
        pLakeShore = new LakeShore330(gpibBoardID, resultlist[i], this);
    }
  }

  if(pKeithley == Q_NULLPTR) {
    int iAnswer = QMessageBox::warning(this,
                                       "Warning",
                                       "Source Measure Unit not Connected",
                                       QMessageBox::Abort|QMessageBox::Ignore,
                                       QMessageBox::Abort);
    if(iAnswer == QMessageBox::Abort)
      return false;
  }

  if(pLakeShore == Q_NULLPTR) {
    int iAnswer = QMessageBox::warning(this,
                                       "Warning",
                                       "Lake Shore not Connected",
                                       QMessageBox::Abort|QMessageBox::Ignore,
                                       QMessageBox::Abort);
    if(iAnswer == QMessageBox::Abort)
      return false;
  }
  return true;
}


bool
MainWindow::switchLampOn() {
  requestData = QByteArray(1, SwitchON);
  return writeToArduino(requestData);
}


bool
MainWindow::switchLampOff() {
  requestData = QByteArray(1, SwitchOFF);
  return writeToArduino(requestData);
}


void
MainWindow::stopRvsT() {
    bRunning = false;
    waitingTStartTimer.stop();
    stabilizingTimer.stop();
    readingTTimer.stop();
    measuringTimer.stop();
    disconnect(&waitingTStartTimer, 0, 0, 0);
    disconnect(&stabilizingTimer, 0, 0, 0);
    disconnect(&readingTTimer, 0, 0, 0);
    disconnect(&measuringTimer, 0, 0, 0);
    if(pOutputFile) {
      pOutputFile->close();
      pOutputFile->deleteLater();
      pOutputFile = Q_NULLPTR;
    }
    pKeithley->endVvsT();
    disconnect(pKeithley, SIGNAL(complianceEvent()),
               this, SLOT(onComplianceEvent()));
    disconnect(pKeithley, SIGNAL(readyForTrigger()),
               this, SLOT(onKeithleyReadyForTrigger()));
    disconnect(pKeithley, SIGNAL(newReading(QDateTime, QString)),
               this, SLOT(onNewKeithleyReading(QDateTime, QString)));
    pLakeShore->switchPowerOff();
    pKeithley->deleteLater();
    pKeithley = Q_NULLPTR;
    pLakeShore->deleteLater();
    pLakeShore = Q_NULLPTR;
    switchLampOff();
}


void
MainWindow::on_startRvsTButton_clicked() {
  if(ui->startRvsTButton->text().contains("Stop")) {
    stopRvsT();
    ui->startRvsTButton->setText("Start R vs T");
    ui->startIvsVButton->setEnabled(true);
    return;
  }
  // else
  if(configureRvsTDialog.exec() == QDialog::Rejected)
    return;

  QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
  // Start the Tasks to switch the lamp on or off
  ui->statusBar->showMessage("Checking for the Presence of Lamp Switch");
  if(!connectToArduino()) {
    qCritical() << QString("No Arduino Ready to Use !");
    exit(-1);
  }
  switchLampOff();
  // Are the GPIB instruments connectd and ready to start ?
  ui->statusBar->showMessage("Checking for the GPIB Instruments");
  if(!CheckInstruments()) {
    ui->statusBar->showMessage("GPIB Instruments not found");
    QApplication::restoreOverrideCursor();
    return;
  }
  if(pKeithley  != Q_NULLPTR)  {
    ui->statusBar->showMessage("Initializing Keithley 236...");
    if(pKeithley->init()) {
      ui->statusBar->showMessage("Unable to Initialize Keithley 236...");
      QApplication::restoreOverrideCursor();
      return;
    }
    isK236ReadyForTrigger = false;
    connect(pKeithley, SIGNAL(complianceEvent()),
            this, SLOT(onComplianceEvent()));
    connect(pKeithley, SIGNAL(readyForTrigger()),
            this, SLOT(onKeithleyReadyForTrigger()));
    connect(pKeithley, SIGNAL(newReading(QDateTime, QString)),
            this, SLOT(onNewKeithleyReading(QDateTime, QString)));
  }
  if(pLakeShore != Q_NULLPTR) {
    ui->statusBar->showMessage("Initializing LakeShore 330...");
    if(pLakeShore->init()) {
      ui->statusBar->showMessage("Unable to Initialize LakeShore 330...");
      QApplication::restoreOverrideCursor();
      return;
    }
  }

  // Open the Output file
  ui->statusBar->showMessage("Opening Output file...");
  if(!prepareOutputFile(configureRvsTDialog.sBaseDir,
                        configureRvsTDialog.sOutFileName))
  {
    ui->statusBar->showMessage("Unable to Open the Output file...");
    QApplication::restoreOverrideCursor();
    return;
  }
  // Write the header
  pOutputFile->write(QString("%1 %2 %3 %4 %5 %6")
                     .arg("T-Dark[K]", 12)
                     .arg("V-Dark[V]", 12)
                     .arg("I-Dark[A]", 12)
                     .arg("T-Photo[K]", 12)
                     .arg("V-Photo[V]", 12)
                     .arg("I-Photo[A]\n", 12)
                     .toLocal8Bit());
  pOutputFile->write(configureRvsTDialog.sSampleInfo.toLocal8Bit());
  pOutputFile->write("\n");
  pOutputFile->flush();
  // Init the Plots
  initRvsTPlots();
  // Configure Source-Measure Unit
  double dAppliedCurrent = configureRvsTDialog.dSourceValue;
  double dVoltageCompliance = 10.0;
  pKeithley->initVvsT(dAppliedCurrent, dVoltageCompliance);
  // Configure Thermostat
  pLakeShore->setTemperature(configureRvsTDialog.dTempStart);
  pLakeShore->switchPowerOn(3);
  // Configure the needed timers
  connect(&waitingTStartTimer, SIGNAL(timeout()),
          this, SLOT(onTimeToCheckReachedT()));
  connect(&readingTTimer, SIGNAL(timeout()),
          this, SLOT(onTimeToReadT()));
  waitingTStartTime = QDateTime::currentDateTime();
  // Read and plot initial value of Temperature
  startReadingTTime = waitingTStartTime;
  onTimeToReadT();
  readingTTimer.start(5000);
  // Start the reaching of the Initial Temperature
  waitingTStartTimer.start(5000);
  // All done...
  // now we are waiting for reaching the initial temperature
  ui->startIvsVButton->setDisabled(true);
  ui->startRvsTButton->setText("Stop R vs T");
  ui->statusBar->showMessage(QString("%1 Waiting Initial T[%2K]")
                             .arg(waitingTStartTime.toString())
                             .arg(configureRvsTDialog.dTempStart));
  QApplication::restoreOverrideCursor();
}


void
MainWindow::on_startIvsVButton_clicked() {
  if(ui->startIvsVButton->text().contains("Stop")) {
    if(pOutputFile) {
      pOutputFile->close();
      pOutputFile->deleteLater();
      pOutputFile = Q_NULLPTR;
    }
    ui->startIvsVButton->setText("Start I vs V");
    ui->startRvsTButton->setEnabled(true);
    ui->statusBar->showMessage("Measure (I vs V) Halted");
    return;
  }
  //else
  if(configureIvsVDialog.exec() == QDialog::Rejected)
    return;

  QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
  ui->startRvsTButton->setDisabled(true);
  ui->startIvsVButton->setText("Stop I vs V");

  // Are the GPIB instruments connectd and ready to start ?
  ui->statusBar->showMessage("Checking for the GPIB Instruments");
  if(!CheckInstruments()) {
    ui->statusBar->showMessage("GPIB Instruments not found");
    QApplication::restoreOverrideCursor();
    return;
  }
  if(pKeithley != Q_NULLPTR)  {
    ui->statusBar->showMessage("Initializing Keithley 236...");
    if(pKeithley->init()) {
      ui->statusBar->showMessage("Unable to Initialize Keithley 236...");
      QApplication::restoreOverrideCursor();
      return;
    }
    isK236ReadyForTrigger = false;
    connect(pKeithley, SIGNAL(complianceEvent()),
            this, SLOT(onComplianceEvent()));
    connect(pKeithley, SIGNAL(readyForTrigger()),
            this, SLOT(onKeithleyReadyForSweepTrigger()));
    connect(pKeithley, SIGNAL(sweepDone(QDateTime,QString)),
            this, SLOT(onKeithleySweepDone(QDateTime, QString)));
  }
  if(pLakeShore != Q_NULLPTR) {
    ui->statusBar->showMessage("Initializing LakeShore 330...");
    if(pLakeShore->init()) {
      ui->statusBar->showMessage("Unable to Initialize LakeShore 330...");
      QApplication::restoreOverrideCursor();
      return;
    }
  }

  // Open the Output file
  ui->statusBar->showMessage("Opening Output file...");
  if(!prepareOutputFile(configureIvsVDialog.sBaseDir,
                        configureIvsVDialog.sOutFileName))
  {
    QApplication::restoreOverrideCursor();
    return;
  }
  pOutputFile->write(QString("%1 %2\n").arg("Voltage[V]", 12). arg("Current[A]", 12).toLocal8Bit());
  pOutputFile->write(configureIvsVDialog.sSampleInfo.toLocal8Bit());
  pOutputFile->write("\n");
  pOutputFile->flush();

  ui->statusBar->showMessage("Checking the presence of a Junction...");
  double vStart = configureIvsVDialog.dVStart;
  double vStop =  configureIvsVDialog.dVStop;
  int junctionDirection = pKeithley->junctionCheck(vStart, vStop);
  if(junctionDirection == pKeithley->ERROR_JUNCTION) {
    ui->statusBar->showMessage("Error Checking the presence of a Junction...");
    QApplication::restoreOverrideCursor();
    return;
  }
  // Now we know how to proceed... (maybe...)
  initIvsVPlots();
  isK236ReadyForTrigger = false;
  if(junctionDirection == 0) {
    // No diode junction
    ui->statusBar->showMessage("Sweeping...Please Wait");
    double dIStart = configureIvsVDialog.dIStart;
    double dIStop = configureIvsVDialog.dIStop;
    double dIStep = (dIStop - dIStart) / double(nSweepPoints);
    pKeithley->initISweep(dIStart, dIStop, dIStep, 1000.0);
  }
  else if(junctionDirection > 0) {
    qDebug() << "Forward Direction Handling";
    // Forward junction
  }
  else {
    // Reverse junction
    qDebug() << "Reverse Direction Handling";
  }
  QApplication::restoreOverrideCursor();
}


void
MainWindow::stopIvsV() {
    if(pOutputFile) {
      pOutputFile->close();
      pOutputFile->deleteLater();
      pOutputFile = Q_NULLPTR;
    }
    disconnect(pKeithley, SIGNAL(complianceEvent()),
               this, SLOT(onComplianceEvent()));
    disconnect(pKeithley, SIGNAL(readyForTrigger()),
              this, SLOT(onKeithleyReadyForSweepTrigger()));
    disconnect(pKeithley, SIGNAL(sweepDone(QDateTime,QString)),
              this, SLOT(onKeithleySweepDone(QDateTime, QString)));
    pKeithley->endISweep();
    pLakeShore->switchPowerOff();
    ui->startIvsVButton->setText("Start I vs V");
    ui->startRvsTButton->setEnabled(true);
    ui->statusBar->showMessage("Sweep Done");
    pKeithley->deleteLater();
    pKeithley = Q_NULLPTR;
}


bool
MainWindow::prepareOutputFile(QString sBaseDir, QString sFileName) {
  if(pOutputFile) {
    pOutputFile->close();
    pOutputFile->deleteLater();
    pOutputFile = Q_NULLPTR;
  }
  pOutputFile = new QFile(sBaseDir + "/" + sFileName);
  if(!pOutputFile->open(QIODevice::Text|QIODevice::WriteOnly)) {
    QMessageBox::critical(this,
                          "Error: Unable to Open Output File",
                          QString("%1/%2")
                          .arg(sBaseDir)
                          .arg(sFileName));
    ui->statusBar->showMessage("Unable to Open Output file...");
    return false;
  }
  return true;
}


void
MainWindow::initRvsTPlots() {
  // Plot of Condicibility vs Temperature
  if(pPlotMeasurements) delete pPlotMeasurements;
  sMeasurementPlotLabel = QString("log(S) [Ohm^-1] -vs- 1000/T [K^-1]");
  pPlotMeasurements = new Plot2D(this, sMeasurementPlotLabel);
  pPlotMeasurements->setMaxPoints(maxPlotPoints);
  pPlotMeasurements->SetLimits(0.0, 1.0, 0.1, 1.0, true, true, false, true);

  pPlotMeasurements->NewDataSet(iPlotDark,//Id
                                3, //Pen Width
                                QColor(192, 192, 192),// Color
                                pPlotMeasurements->ipoint,// Symbol
                                "Dark"// Title
                                );
  pPlotMeasurements->SetShowDataSet(iPlotDark, true);
  pPlotMeasurements->SetShowTitle(iPlotDark, true);

  pPlotMeasurements->NewDataSet(iPlotPhoto,//Id
                                3, //Pen Width
                                QColor(255, 255, 0),// Color
                                pPlotMeasurements->ipoint,// Symbol
                                "Photo"// Title
                                );
  pPlotMeasurements->SetShowDataSet(iPlotPhoto, true);
  pPlotMeasurements->SetShowTitle(iPlotPhoto, true);

  pPlotMeasurements->UpdatePlot();
  pPlotMeasurements->show();

  // Plot of Temperature vs Time
  if(pPlotTemperature) delete pPlotTemperature;
  sTemperaturePlotLabel = QString("T [K] vs t [s]");
  pPlotTemperature = new Plot2D(this, sTemperaturePlotLabel);
  pPlotTemperature->setMaxPoints(maxPlotPoints);
  pPlotTemperature->SetLimits(0.0, 1.0, 0.0, 1.0, true, true, false, false);

  pPlotTemperature->NewDataSet(1,//Id
                               3, //Pen Width
                               QColor(255, 0, 0),// Color
                               pPlotTemperature->ipoint,// Symbol
                               "T"// Title
                               );
  pPlotTemperature->SetShowDataSet(1, true);
  pPlotTemperature->SetShowTitle(1, true);

  pPlotTemperature->UpdatePlot();
  pPlotTemperature->show();
  iCurrentTPlot = 1;
}


void
MainWindow::initIvsVPlots() {
  // Plot of Current vs Voltage
  if(pPlotMeasurements) delete pPlotMeasurements;
  sMeasurementPlotLabel = QString("I [A] vs V [V]");

  pPlotMeasurements = new Plot2D(this, sMeasurementPlotLabel);
  pPlotMeasurements->setMaxPoints(maxPlotPoints);
  pPlotMeasurements->NewDataSet(1,//Id
                                3, //Pen Width
                                QColor(255, 255, 0),// Color
                                StripChart::ipoint,// Symbol
                                "IvsV"// Title
                                );
  pPlotMeasurements->SetShowDataSet(1, true);
  pPlotMeasurements->SetShowTitle(1, true);
  pPlotMeasurements->SetLimits(0.0, 1.0, 0.0, 1.0, true, true, false, false);
  pPlotMeasurements->UpdatePlot();
  pPlotMeasurements->show();
//  // Plot of Temperature vs Time
//  if(pPlotTemperature) delete pPlotTemperature;
//  sTemperaturePlotLabel = QString("T [K] vs t [s]");
//  pPlotTemperature = new Plot2D(this, sTemperaturePlotLabel);
//  pPlotTemperature->setMaxPoints(maxPlotPoints);
//  pPlotTemperature->NewDataSet(1,//Id
//                               3, //Pen Width
//                               QColor(255, 255, 0),// Color
//                               StripChart::ipoint,// Symbol
//                               "T"// Title
//                               );
//  pPlotTemperature->SetShowDataSet(1, true);
//  pPlotTemperature->SetShowTitle(1, true);
//  pPlotTemperature->SetLimits(0.0, 1.0, 0.0, 1.0, true, true, false, false);
//  pPlotTemperature->UpdatePlot();
//  pPlotTemperature->show();
}


void
MainWindow::onTimeToCheckReachedT() {
  double T = pLakeShore->getTemperature();
  if(fabs(T-configureRvsTDialog.dTempStart) < 0.1)//configureRvsTDialog->dTolerance)
  {
    disconnect(&waitingTStartTimer, 0, 0, 0);
    waitingTStartTimer.stop();

    connect(&stabilizingTimer, SIGNAL(timeout()),
            this, SLOT(onTimerStabilizeT()));
    stabilizingTimer.start(configureRvsTDialog.iStabilizingTime*60*1000);
//    qDebug() << QString("Starting T Reached: Thermal Stabilization...");
    ui->statusBar->showMessage(QString("Starting T Reached: Thermal Stabilization for %1 min.").arg(configureRvsTDialog.iStabilizingTime));
  }
  else {
    currentTime = QDateTime::currentDateTime();
    quint64 elapsedMsec = waitingTStartTime.secsTo(currentTime);
//    qDebug() << "Elapsed:" << elapsedMsec << "Maximum:" << quint64(configureRvsTDialog.iReachingTime)*60*1000;
    if(elapsedMsec > quint64(configureRvsTDialog.iReachingTime)*60*1000) {
      disconnect(&waitingTStartTimer, 0, 0, 0);
      waitingTStartTimer.stop();
      connect(&stabilizingTimer, SIGNAL(timeout()),
              this, SLOT(onTimerStabilizeT()));
      stabilizingTimer.start(configureRvsTDialog.iStabilizingTime*60*1000);
//      qDebug() << QString("Max Reaching Time Exceed...Thermal Stabilization...");
      ui->statusBar->showMessage(QString("Max Reaching Time Exceed...Thermal Stabilization for %1 min.").arg(configureRvsTDialog.iStabilizingTime));
    }
  }
}


void
MainWindow::onTimerStabilizeT() {
  // It's time to start measurements
  stabilizingTimer.stop();
  disconnect(&stabilizingTimer, 0, 0, 0);
  startMeasuringTime = QDateTime::currentDateTime();
  pPlotTemperature->NewDataSet(2,//Id
                               3, //Pen Width
                               QColor(255, 255, 0),// Color
                               StripChart::ipoint,// Symbol
                               "Tm"// Title
                               );
  pPlotTemperature->SetShowDataSet(2, true);
  pPlotTemperature->SetShowTitle(2, true);
  pPlotTemperature->UpdatePlot();
  iCurrentTPlot = 2;
//  qDebug() << "Thermal Stabilization Reached: Start of the Measure";
  ui->statusBar->showMessage(QString("Thermal Stabilization Reached: Start of the Measure"));
  connect(&measuringTimer, SIGNAL(timeout()),
          this, SLOT(onTimeToGetNewMeasure()));
  if(!pLakeShore->startRamp(configureRvsTDialog.dTempEnd, configureRvsTDialog.dTRate)) {
    ui->statusBar->showMessage(QString("Error Starting the Measure"));
    return;
  }
  measuringTimer.start(timeBetweenMeasurements);
  bRunning = true;
}


void
MainWindow::onTimeToGetNewMeasure() {
  getNewSigmaMeasure();
  if(!pLakeShore->isRamping()) {// Ramp is Done
    measuringTimer.stop();
    bRunning = false;
    disconnect(&measuringTimer, 0, 0, 0);
    if(pOutputFile) {
      if(pOutputFile->isOpen())
        pOutputFile->close();
      pOutputFile->deleteLater();
      pOutputFile = Q_NULLPTR;
    }
    pKeithley->endVvsT();
    pLakeShore->switchPowerOff();
    switchLampOff();
    ui->startRvsTButton->setText("Start R vs T");
    ui->startIvsVButton->setEnabled(true);
//    qDebug() << "End Temperature Reached: Measure is Done";
    ui->statusBar->showMessage(QString("Measurements Completed !"));
    return;
  }
}


void
MainWindow::onTimeToReadT() {
  double currentTemperature = pLakeShore->getTemperature();
  currentTime = QDateTime::currentDateTime();
  ui->temperatureEdit->setText(QString("%1").arg(currentTemperature));
  pPlotTemperature->NewPoint(iCurrentTPlot,
                             double(startReadingTTime.secsTo(currentTime)),
                             currentTemperature);
  pPlotTemperature->UpdatePlot();
}


void
MainWindow::onComplianceEvent() {
  qCritical() << "MainWindow::onComplianceEvent()";
  on_startRvsTButton_clicked();
}


void
MainWindow::onKeithleyReadyForTrigger() {
  isK236ReadyForTrigger = true;
}


void
MainWindow::onKeithleyReadyForSweepTrigger() {
  disconnect(pKeithley, SIGNAL(readyForTrigger()),
             this, SLOT(onKeithleyReadyForSweepTrigger()));
  pKeithley->triggerSweep();
}


void
MainWindow::onNewKeithleyReading(QDateTime dataTime, QString sDataRead) {
  // Decode readings
  QStringList sMeasures = QStringList(sDataRead.split(",", QString::SkipEmptyParts));
  if(sMeasures.count() < 2) {
    qDebug() << "MainWindow::onNewKeithleyReading: Measurement Format Error";
    return;
  }
  double currentTemperature = pLakeShore->getTemperature();
  double t = double(startMeasuringTime.msecsTo(dataTime))/1000.0;
  Q_UNUSED(t)
  double current = sMeasures.at(1).toDouble();
  double voltage = sMeasures.at(0).toDouble();
  ui->temperatureEdit->setText(QString("%1").arg(currentTemperature));
  ui->currentEdit->setText(QString("%1").arg(current, 12, 'g', 6, ' '));
  ui->voltageEdit->setText(QString("%1").arg(voltage, 12, 'g', 6, ' '));

  if(!bRunning)
    return;

  pOutputFile->write(QString("%1 %2 %3")
                     .arg(currentTemperature, 12, 'g', 6, ' ')
                     .arg(current, 12, 'g', 6, ' ')
                     .arg(voltage, 12, 'g', 6, ' ')
                     .toLocal8Bit());
  pOutputFile->flush();
  if(currentLampStatus == LAMP_OFF) {
    pPlotMeasurements->NewPoint(iPlotDark, 1000.0/currentTemperature, current/voltage);
    pPlotMeasurements->UpdatePlot();
    currentLampStatus = LAMP_ON;
    switchLampOn();
  }
  else {
    pPlotMeasurements->NewPoint(iPlotPhoto, 1000.0/currentTemperature, current/voltage);
    pPlotMeasurements->UpdatePlot();
    currentLampStatus = LAMP_OFF;
    pOutputFile->write("\n");
    switchLampOff();
  }
}


void
MainWindow::onKeithleySweepDone(QDateTime dataTime, QString sData) {
  Q_UNUSED(dataTime)
  ui->statusBar->showMessage("Sweep Done: Decoding readings...Please wait");
  QStringList sMeasures = QStringList(sData.split(",", QString::SkipEmptyParts));
  if(sMeasures.count() < 2) {
    qCritical() << "no Sweep Values ";
    return;
  }
  ui->statusBar->showMessage("Sweep Done: Updating Plot...Please wait");
  double current, voltage;
  for(int i=0; i<sMeasures.count(); i+=2) {
    current = sMeasures.at(i).toDouble();
    voltage = sMeasures.at(i+1).toDouble();
    pOutputFile->write(QString("%1 %2\n")
                       .arg(voltage, 12, 'g', 6, ' ')
                       .arg(current, 12, 'g', 6, ' ')
                       .toLocal8Bit());
    pPlotMeasurements->NewPoint(1, voltage, current);
  }
  pPlotMeasurements->UpdatePlot();
  pOutputFile->flush();
  pOutputFile->close();
  stopIvsV();
  ui->statusBar->showMessage("Measure Done");
}


bool
MainWindow::getNewSigmaMeasure() {
  if(!isK236ReadyForTrigger)
    return false;
  isK236ReadyForTrigger = false;
  return pKeithley->sendTrigger();
}


bool
MainWindow::connectToArduino() {
  bool found = false;
  QList<QSerialPortInfo> serialPorts = QSerialPortInfo::availablePorts();
  if(serialPorts.isEmpty()) {
    qCritical() << QString("Empty COM port list: No Arduino connected !");
  }
  else {
    QSerialPortInfo info;
    for(int i=0; i<serialPorts.size()&& !found; i++) {
      info = serialPorts.at(i);
//      qInfo() << "Conntecting to: " << info.portName();
      serialPort.setPortName(info.portName());
      serialPort.setBaudRate(QSerialPort::Baud115200);
      serialPort.setDataBits(QSerialPort::Data8);
      serialPort.setParity(QSerialPort::NoParity);
      serialPort.setStopBits(QSerialPort::OneStop);
      if(serialPort.open(QIODevice::ReadWrite)) {
        // Arduino will reset upon opening the seial port
        // so we need to give it time to boot
        QThread::sleep(3);
        requestData = QByteArray(2, AreYouThere);
        found = writeToArduino(requestData);
        if(found) break;
        serialPort.close();
      }
    }
  }
  return found;
}


bool
MainWindow::writeToArduino(QByteArray requestData) {
  serialPort.write(requestData.append(uchar(EOS)));
  if (serialPort.waitForBytesWritten(waitTimeout)) {
    if (serialPort.waitForReadyRead(waitTimeout)) {
      QByteArray responseData = serialPort.readAll();
      while(serialPort.waitForReadyRead(10))
        responseData += serialPort.readAll();
      if(responseData.at(0) != uchar(ACK)) {
        qCritical() << "MainWindow::writeRequest(): not an ACK";
        return false;
      }
    }
    else {
      qCritical() << "MainWindow::writeRequest(): Wait read response timeout";
      return false;
    }
  }
  else {
    qCritical() <<"MainWindow::writeRequest(): Wait write request timeout %1";
    return false;
  }
  return true;
}
