/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial Usage
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
** $QT_END_LICENSE$
**
****************************************************************************/

#include "mdichild.h"
#include "ui_mdichild.h"
#include "xmlinterface.h"
#include "hexinterface.h"
#include "mainwindow.h"
#include "modeledit/modeledit.h"
#include "generaledit.h"
#include "avroutputdialog.h"
#include "burnconfigdialog.h"
#include "printdialog.h"
#include "burndialog.h"
#include "helpers.h"
#include "appdata.h"
#include "wizarddialog.h"
#include "taranisnotfound.h"
#include <QFileInfo>

#if defined WIN32 || !defined __GNUC__
#include <windows.h>
#define sleep(x) Sleep(x*1000)
#else
#include <unistd.h>
#endif

MdiChild::MdiChild():
  QWidget(),
  ui(new Ui::mdiChild),
  firmware(GetCurrentFirmware()),
  isUntitled(true),
  fileChanged(false)
{
  ui->setupUi(this);
  this->setWindowIcon(CompanionIcon("open.png"));
  ui->SimulateTxButton->setIcon(CompanionIcon("simulate.png"));
  setAttribute(Qt::WA_DeleteOnClose);

  eepromInterfaceChanged();

  if(!(this->isMaximized() || this->isMinimized())) {
    adjustSize();
  }
}

MdiChild::~MdiChild() 
{
  delete ui;
}

void MdiChild::qSleep(int ms)
{
  if (ms<0)
    return;

#if defined WIN32 || !defined __GNUC__
    Sleep(uint(ms));
#else
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000 * 1000 };
    nanosleep(&ts, NULL);
#endif
}

void MdiChild::eepromInterfaceChanged()
{
  ui->modelsList->refreshList();
  ui->SimulateTxButton->setEnabled(GetCurrentFirmware()/*firmware*/->getCapability(Simulation));
  updateTitle();
}

void MdiChild::cut()
{
  ui->modelsList->cut();
}

void MdiChild::copy()
{
  ui->modelsList->copy();
}

void MdiChild::paste()
{
  ui->modelsList->paste();
}

bool MdiChild::hasPasteData()
{
  return ui->modelsList->hasPasteData();
}

bool MdiChild::hasSelection()
{
    return ui->modelsList->hasSelection();
}

void MdiChild::updateTitle()
{
  QString title = userFriendlyCurrentFile() + "[*]"+" ("+GetEepromInterface()->getName()+QString(")");
  if (!IS_SKY9X(GetEepromInterface()->getBoard()))
    title += QString(" - %1 ").arg(EEPromAvail) + tr("free bytes");
  setWindowTitle(title);
}

void MdiChild::setModified()
{
  ui->modelsList->refreshList();
  fileChanged = true;
  updateTitle();
  documentWasModified();
}

void MdiChild::on_SimulateTxButton_clicked()
{
  startSimulation(this, radioData, -1);
}

void MdiChild::checkAndInitModel(int row)
{
  ModelData &model = radioData.models[row - 1];
  if (model.isempty()) {
    model.setDefaultValues(row - 1, radioData.generalSettings);
    setModified();
  }
}

void MdiChild::generalEdit()
{
  GeneralEdit *t = new GeneralEdit(radioData, this);
  connect(t, SIGNAL(modelValuesChanged()), this, SLOT(setModified()));
  t->show();
}

void MdiChild::modelEdit()
{
  int row = ui->modelsList->currentRow();

  if (row == 0){
    generalEdit();
  } 
  else {
    QApplication::setOverrideCursor(Qt::WaitCursor);
    checkAndInitModel( row );
    ModelData &model = radioData.models[row - 1];
    ModelEdit *t = new ModelEdit(this, radioData, (row - 1), GetCurrentFirmware()/*firmware*/);
    t->setWindowTitle(tr("Editing model %1: ").arg(row) + model.name);
    connect(t, SIGNAL(modified()), this, SLOT(setModified()));
    t->show();
    QApplication::restoreOverrideCursor();
  }
}

void MdiChild::wizardEdit()
{
  int row = ui->modelsList->currentRow();
  if (row > 0) {
    checkAndInitModel(row);
    WizardDialog * wizard = new WizardDialog(radioData.generalSettings, row, this);
    wizard->exec();
    if (wizard->mix.complete /*TODO rather test the exec() result?*/) {
      radioData.models[row - 1] = wizard->mix;
      setModified();
    }
  }
}

void MdiChild::openEditWindow()
{
  int row = ui->modelsList->currentRow();
  if (row == 0){
    generalEdit();
  }
  else{
    ModelData &model = radioData.models[row - 1];
    if (model.isempty() && g.useWizard()) {
      wizardEdit();
    }
    else {
      modelEdit();
    }
  }
}

void MdiChild::newFile()
{
  static int sequenceNumber = 1;

  isUntitled = true;
  curFile = QString("document%1.eepe").arg(sequenceNumber++);
  updateTitle();
}

bool MdiChild::loadFile(const QString &fileName, bool resetCurrentFile)
{
    QFile file(fileName);

    if (!file.exists()) {
      QMessageBox::critical(this, tr("Error"), tr("Unable to find file %1!").arg(fileName));
      return false;
    }

    int fileType = getFileType(fileName);

    if (fileType==FILE_TYPE_XML) {
      if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {  //reading HEX TEXT file
        QMessageBox::critical(this, tr("Error"),
            tr("Error opening file %1:\n%2.")
            .arg(fileName)
            .arg(file.errorString()));
        return false;
      }
      QTextStream inputStream(&file);
      XmlInterface(inputStream).load(radioData);
    }
    else if (fileType==FILE_TYPE_HEX || fileType==FILE_TYPE_EEPE) { //read HEX file
      if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {  //reading HEX TEXT file
          QMessageBox::critical(this, tr("Error"),
                               tr("Error opening file %1:\n%2.")
                               .arg(fileName)
                               .arg(file.errorString()));
          return false;
      }
        
      QDomDocument doc(ER9X_EEPROM_FILE_TYPE);
      bool xmlOK = doc.setContent(&file);
      if(xmlOK) {
        if (LoadEepromXml(radioData, doc)){
          ui->modelsList->refreshList();
          if(resetCurrentFile) setCurrentFile(fileName);
          return true;
        }
      }
      file.reset();
      
      QTextStream inputStream(&file);

      if (fileType==FILE_TYPE_EEPE) {  // read EEPE file header
        QString hline = inputStream.readLine();
        if (hline!=EEPE_EEPROM_FILE_HEADER) {
          file.close();
          return false;
        }
      }

      uint8_t eeprom[EESIZE_RLC_MAX];
      int eeprom_size = HexInterface(inputStream).load(eeprom, EESIZE_RLC_MAX);
      if (!eeprom_size) {
        QMessageBox::critical(this, tr("Error"),
            tr("Invalid EEPROM File %1")
            .arg(fileName));
        file.close();
        return false;
      }

      file.close();

      if (!LoadEeprom(radioData, eeprom, eeprom_size)) {
        QMessageBox::critical(this, tr("Error"),
            tr("Invalid EEPROM File %1")
            .arg(fileName));
        return false;
      }

      ui->modelsList->refreshList();
      if(resetCurrentFile) setCurrentFile(fileName);

      return true;
    }
    else if (fileType==FILE_TYPE_BIN) { //read binary
      int eeprom_size = file.size();

      if (!file.open(QFile::ReadOnly)) {  //reading binary file   - TODO HEX support
          QMessageBox::critical(this, tr("Error"),
                               tr("Error opening file %1:\n%2.")
                               .arg(fileName)
                               .arg(file.errorString()));
          return false;
      }
      uint8_t *eeprom = (uint8_t *)malloc(eeprom_size);
      memset(eeprom, 0, eeprom_size);
      long result = file.read((char*)eeprom, eeprom_size);
      file.close();

      if (result != eeprom_size) {
          QMessageBox::critical(this, tr("Error"),
                               tr("Error reading file %1:\n%2.")
                               .arg(fileName)
                               .arg(file.errorString()));

          return false;
      }

      if (!LoadEeprom(radioData, eeprom, eeprom_size) && !LoadBackup(radioData, eeprom, eeprom_size, 0)) {
        QMessageBox::critical(this, tr("Error"),
            tr("Invalid binary EEPROM File %1")
            .arg(fileName));
        return false;
      }

      ui->modelsList->refreshList();
      if(resetCurrentFile) setCurrentFile(fileName);

      free(eeprom);
      return true;
    }

    return false;
}

bool MdiChild::save()
{
  if (isUntitled) {
    return saveAs(true);
  }
  else {
    return saveFile(curFile);
  }
}

bool MdiChild::saveAs(bool isNew)
{
    QString fileName;
    if (IS_SKY9X(GetEepromInterface()->getBoard())) {
      curFile.replace(".eepe", ".bin");
      QFileInfo fi(curFile);
#ifdef __APPLE__
      fileName = QFileDialog::getSaveFileName(this, tr("Save As"), g.eepromDir() + "/" +fi.fileName());
#else
      fileName = QFileDialog::getSaveFileName(this, tr("Save As"), g.eepromDir() + "/" +fi.fileName(), tr(BIN_FILES_FILTER));
#endif      
    }
    else {
      QFileInfo fi(curFile);
#ifdef __APPLE__
      fileName = QFileDialog::getSaveFileName(this, tr("Save As"), g.eepromDir() + "/" +fi.fileName());
#else
      fileName = QFileDialog::getSaveFileName(this, tr("Save As"), g.eepromDir() + "/" +fi.fileName(), tr(EEPROM_FILES_FILTER));
#endif      
    }
    if (fileName.isEmpty())
      return false;
    if (fileName.contains("rev4a")) {
      g.rev4aSupport( true );
    }
    if (fileName.contains("norev4a")) {
      g.rev4aSupport( false );
    }
    g.eepromDir( QFileInfo(fileName).dir().absolutePath() );
    if (isNew)
      return saveFile(fileName);
    else 
      return saveFile(fileName,true);
}

bool MdiChild::saveFile(const QString &fileName, bool setCurrent)
{
    QString myFile;
    myFile = fileName;
    if (IS_SKY9X(GetEepromInterface()->getBoard())) {
      myFile.replace(".eepe", ".bin");
    }
    QFile file(myFile);

    int fileType = getFileType(myFile);

    uint8_t *eeprom = (uint8_t*)malloc(GetEepromInterface()->getEEpromSize());
    int eeprom_size = 0;

    if (fileType != FILE_TYPE_XML) {
      eeprom_size = GetEepromInterface()->save(eeprom, radioData, GetCurrentFirmwareVariant(), 0/*last version*/);
      if (!eeprom_size) {
        QMessageBox::warning(this, tr("Error"),tr("Cannot write file %1:\n%2.").arg(myFile).arg(file.errorString()));
        return false;
      }
    }

    if (!file.open(fileType == FILE_TYPE_BIN ? QIODevice::WriteOnly : (QIODevice::WriteOnly | QIODevice::Text))) {
      QMessageBox::warning(this, tr("Error"),tr("Cannot write file %1:\n%2.").arg(myFile).arg(file.errorString()));
      return false;
    }

    QTextStream outputStream(&file);

    if (fileType==FILE_TYPE_XML) {
      if (!XmlInterface(outputStream).save(radioData)) {
        QMessageBox::warning(this, tr("Error"),tr("Cannot write file %1:\n%2.").arg(myFile).arg(file.errorString()));
        file.close();
        return false;
      }
    }
    else if (fileType==FILE_TYPE_HEX || fileType==FILE_TYPE_EEPE) { // write hex
      if (fileType==FILE_TYPE_EEPE)
        outputStream << EEPE_EEPROM_FILE_HEADER << "\n";

      if (!HexInterface(outputStream).save(eeprom, eeprom_size)) {
          QMessageBox::warning(this, tr("Error"),tr("Cannot write file %1:\n%2.").arg(myFile).arg(file.errorString()));
          file.close();
          return false;
      }
    }
    else if (fileType==FILE_TYPE_BIN) // write binary
    {
      long result = file.write((char*)eeprom, eeprom_size);
      if(result!=eeprom_size) {
        QMessageBox::warning(this, tr("Error"),tr("Error writing file %1:\n%2.").arg(myFile).arg(file.errorString()));
        return false;
      }
    }
    else {
      QMessageBox::warning(this, tr("Error"),tr("Error writing file %1:\n%2.").arg(myFile).arg("Unknown format"));
      return false;
    }

    free(eeprom); // TODO free in all cases ...
    file.close();
    if(setCurrent) setCurrentFile(myFile);

    return true;
}

QString MdiChild::userFriendlyCurrentFile()
{
  return strippedName(curFile);
}

void MdiChild::closeEvent(QCloseEvent *event)
{
  if (maybeSave()) {
    event->accept();
  }
  else {
    event->ignore();
  }
}

void MdiChild::documentWasModified()
{
  setWindowModified(fileChanged);
}

bool MdiChild::maybeSave()
{
  if (fileChanged) {
    QMessageBox::StandardButton ret;
    ret = QMessageBox::warning(this, tr("Companion"),
        tr("%1 has been modified.\n"
           "Do you want to save your changes?").arg(userFriendlyCurrentFile()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (ret == QMessageBox::Save)
      return save();
    else if (ret == QMessageBox::Cancel)
      return false;
  }
  return true;
}

void MdiChild::setCurrentFile(const QString &fileName)
{
  curFile = QFileInfo(fileName).canonicalFilePath();
  isUntitled = false;
  fileChanged = false;
  setWindowModified(false);
  updateTitle();
  int MaxRecentFiles = g.historySize();
  QStringList files = g.recentFiles();
  files.removeAll(fileName);
  files.prepend(fileName);
  while (files.size() > MaxRecentFiles)
      files.removeLast();
 
  g.recentFiles( files );
}

QString MdiChild::strippedName(const QString &fullFileName)
{
  return QFileInfo(fullFileName).fileName();
}

void MdiChild::writeEeprom()  // write to Tx
{
  bool backupEnable=g.enableBackup();
  QString backupPath=g.backupDir();
  if (!backupPath.isEmpty()) {
    if (!QDir(backupPath).exists()) {
      if (backupEnable) {
        QMessageBox::warning(this, tr("Backup is impossible"), tr("The backup dir set in preferences does not exist"));
      }
      backupEnable=false;
    }
  }
  else {
    backupEnable=false;
  }
  QString stickCal=g.profile[g.id()].stickPotCalib();
  burnConfigDialog bcd;
  QString tempDir    = QDir::tempPath();
  QString tempFile = tempDir + "/temp.bin";
  saveFile(tempFile, false);
  if(!QFileInfo(tempFile).exists()) {
    QMessageBox::critical(this,tr("Error"), tr("Cannot write temporary file!"));
    return;
  }
  EEPROMInterface *eepromInterface = GetEepromInterface();
  bool backup=false;
  burnDialog *cd = new burnDialog(this, 1, &tempFile, &backup,strippedName(curFile));
  cd->exec();
  if (!tempFile.isEmpty()) {
    if (backup) {
      if (backupEnable) {
        QString backupFile=backupPath+"/backup-"+QDateTime().currentDateTime().toString("yyyy-MM-dd-HHmmss")+".bin";
        if (IS_TARANIS(eepromInterface->getBoard())) {
          QString path=((MainWindow *)this->parent())->FindTaranisPath();
          if (path.isEmpty()) {
            taranisNotFoundDialog *tnfd = new taranisNotFoundDialog(this);
            tnfd->exec();
            return;
          }
          else {
            QStringList str;
            str << path << backupFile;
            avrOutputDialog *ad = new avrOutputDialog(this,"", str, tr("Backup EEPROM From Radio")); //, AVR_DIALOG_KEEP_OPEN);
            ad->setWindowIcon(CompanionIcon("read_eeprom.png"));
            ad->exec();
            delete ad;
            sleep(1);
          }
        }
        else {
          QStringList str = ((MainWindow *)this->parent())->GetReceiveEEpromCommand(backupFile);
          avrOutputDialog *ad = new avrOutputDialog(this, ((MainWindow *)this->parent())->GetAvrdudeLocation(), str, tr("Backup EEPROM From Radio"));
          ad->setWindowIcon(CompanionIcon("read_eeprom.png"));
          ad->exec();
          delete ad;
          sleep(1);
        }
      }
      int oldrev=((MainWindow *)this->parent())->getEpromVersion(tempFile);
      QString tempFlash=tempDir + "/flash.bin";
      QStringList str = ((MainWindow *)this->parent())->GetReceiveFlashCommand(tempFlash);
      avrOutputDialog *ad = new avrOutputDialog(this, ((MainWindow *)this->parent())->GetAvrdudeLocation(), str, "Read Flash From Radio");
      ad->setWindowIcon(CompanionIcon("read_flash.png"));
      ad->exec();
      delete ad;
      sleep(1);
      QString restoreFile = tempDir + "/compat.bin";
      if (!((MainWindow *)this->parent())->convertEEPROM(tempFile, restoreFile, tempFlash)) {
        int ret = QMessageBox::question(this, tr("Error"), tr("Cannot check eeprom compatibility! Continue anyway?"), QMessageBox::Yes | QMessageBox::No);
        if (ret == QMessageBox::No)
          return;
      }
      else {
        int rev=((MainWindow *)this->parent())->getEpromVersion(restoreFile);
        if ((rev/100)!=(oldrev/100)) {
          QMessageBox::warning(this,tr("Warning"), tr("Firmware in radio is of a different family of eeprom written, check file and preferences!"));
        }
        if (rev<oldrev) {
          QMessageBox::warning(this,tr("Warning"), tr("Firmware in flash is outdated, please upgrade!"));
        }
        tempFile=restoreFile;
      }
      QByteArray ba = tempFlash.toLatin1();
      char *name = ba.data(); 
      unlink(name);
    }
    else {
      if (backupEnable) {
        QString backupFile=backupPath+"/backup-"+QDateTime().currentDateTime().toString("yyyy-MM-dd-hhmmss")+".bin";
        if (IS_TARANIS(eepromInterface->getBoard())) {
          QString path=((MainWindow *)this->parent())->FindTaranisPath();
          if (path.isEmpty()) {
            taranisNotFoundDialog *tnfd = new taranisNotFoundDialog(this);
            tnfd->exec();
            return;
          }
          else {
            QFile::copy(path,backupFile);
          }
        }
        else {
          QStringList str = ((MainWindow *)this->parent())->GetReceiveEEpromCommand(backupFile);
          avrOutputDialog *ad = new avrOutputDialog(this, ((MainWindow *)this->parent())->GetAvrdudeLocation(), str, tr("Backup EEPROM From Radio"));
          ad->setWindowIcon(CompanionIcon("read_eeprom.png"));
          ad->exec();
          delete ad;
          sleep(1);
        }
      }
    }

    if (IS_TARANIS(eepromInterface->getBoard())) {
      QString path=((MainWindow *)this->parent())->FindTaranisPath();
      if (path.isEmpty()) {
        taranisNotFoundDialog *tnfd = new taranisNotFoundDialog(this);
        tnfd->exec();
        return;
      }
      else {
        QStringList str;
        str << tempFile << path;
        avrOutputDialog *ad = new avrOutputDialog(this,"", str, tr("Write EEPROM To Radio"), AVR_DIALOG_SHOW_DONE); //, AVR_DIALOG_KEEP_OPEN);
        ad->setWindowIcon(CompanionIcon("read_eeprom.png"));
        ad->exec();
        delete ad;
        sleep(1);
      }
    }
    else {
      QStringList str = ((MainWindow *)this->parent())->GetSendEEpromCommand(tempFile);
      avrOutputDialog *ad = new avrOutputDialog(this, ((MainWindow *)this->parent())->GetAvrdudeLocation(), str, "Write EEPROM To Radio", AVR_DIALOG_SHOW_DONE);
      ad->setWindowIcon(CompanionIcon("write_eeprom.png"));
      ad->exec();
      delete ad;
    }
  }
}

void MdiChild::simulate()
{
  if (ui->modelsList->currentRow() >= 1) {
    startSimulation(this, radioData, ui->modelsList->currentRow()-1);
  }
}

void MdiChild::print(int model, QString filename)
{
  PrintDialog * pd = NULL;

  if (model>=0 && !filename.isEmpty()) {
    pd = new PrintDialog(this, GetCurrentFirmware()/*firmware*/, &radioData.generalSettings, &radioData.models[model], filename);
  }
  else if (ui->modelsList->currentRow() > 0) {
    pd = new PrintDialog(this, GetCurrentFirmware()/*firmware*/, &radioData.generalSettings, &radioData.models[ui->modelsList->currentRow()-1]);
  }
    
  if (pd) {
    pd->setAttribute(Qt::WA_DeleteOnClose, true);
    pd->show();
  }
}

void MdiChild::viableModelSelected(bool viable)
{
  emit copyAvailable(viable);
}

void MdiChild::setEEpromAvail(int eavail)
{
  EEPromAvail=eavail;
}

bool MdiChild::loadBackup()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open backup Models and Settings file"), g.eepromDir(),tr(EEPROM_FILES_FILTER));
    if (fileName.isEmpty())
      return false;
    QFile file(fileName);

    if (!file.exists()) {
      QMessageBox::critical(this, tr("Error"), tr("Unable to find file %1!").arg(fileName));
      return false;
    }
    if(ui->modelsList->currentRow()<1) return false;
    int index=ui->modelsList->currentRow()-1;

    int eeprom_size = file.size();
    if (!file.open(QFile::ReadOnly)) {  //reading binary file   - TODO HEX support
        QMessageBox::critical(this, tr("Error"),
                              tr("Error opening file %1:\n%2.")
                              .arg(fileName)
                              .arg(file.errorString()));
        return false;
    }
    QByteArray eeprom(eeprom_size, 0);
    long result = file.read((char*)eeprom.data(), eeprom_size);
    file.close();

    if (result != eeprom_size) {
        QMessageBox::critical(this, tr("Error"),
                              tr("Error reading file %1:\n%2.")
                              .arg(fileName)
                              .arg(file.errorString()));

        return false;
    }

    if (!LoadBackup(radioData, (uint8_t *)eeprom.data(), eeprom_size, index)) {
      QMessageBox::critical(this, tr("Error"),
          tr("Invalid binary backup File %1")
          .arg(fileName));
      return false;
    }

    ui->modelsList->refreshList();

    return true;
}
