#ifndef BURNDIALOG_H
#define BURNDIALOG_H

#include <QtGui>
#include <QDialog>
#include "eeprominterface.h"
#include "flashinterface.h"
#include "xmlinterface.h"

#define HEX_FILES_FILTER     "HEX files (*.hex);;"
#define BIN_FILES_FILTER     "BIN files (*.bin);;"
#define EEPE_FILES_FILTER    "EEPE EEPROM files (*.eepe);;"
#define EEPROM_FILES_FILTER  "EEPE files (*.eepe *.bin *.hex);;" EEPE_FILES_FILTER BIN_FILES_FILTER HEX_FILES_FILTER
#define FLASH_FILES_FILTER   "FLASH files (*.bin *.hex);;" BIN_FILES_FILTER HEX_FILES_FILTER
#define EXTERNAL_EEPROM_FILES_FILTER   "EEPROM files (*.bin *.hex);;" BIN_FILES_FILTER HEX_FILES_FILTER
#define ER9X_EEPROM_FILE_TYPE        "ER9X_EEPROM_FILE"
#define EEPE_EEPROM_FILE_HEADER  "EEPE EEPROM FILE"
#define EEPE_MODEL_FILE_HEADER   "EEPE MODEL FILE"
#define EEPROM_FILE_TYPE 1
#define FLASH_FILE_TYPE 2


namespace Ui
{
  class burnDialog;
}

class burnDialog : public QDialog
{
  Q_OBJECT

public:
  explicit burnDialog(QWidget *parent = 0, int Type = 2, QString * fileName = NULL, bool * backupEE=NULL, QString docname="");
  ~burnDialog();

private slots:
  void on_FlashLoadButton_clicked();
  void on_BurnFlashButton_clicked();
  void on_cancelButton_clicked();
  void on_EEbackupCB_clicked();
  void on_EEpromCB_toggled(bool checked);  
  bool checkeEprom(QString fileName);
  void on_useProfileImageCB_clicked();
  void on_useFwImageCB_clicked();
  void on_useLibraryImageCB_clicked();
  void on_useAnotherImageCB_clicked();

  void checkFw(QString fileName);
  void displaySplash();
  void updateUI();
  void shrink();


private:
  Ui::burnDialog *ui;
  QString * hexfileName;
  bool * backup;
  int hexType;
  RadioData radioData;
  bool burnraw;
  enum ImageSource {FIRMWARE, PROFILE, LIBRARY, ANOTHER};
  ImageSource imageSource;
  QString imageFile;
};

#endif // BURNDIALOG_H
