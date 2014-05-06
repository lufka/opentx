#include "expodialog.h"
#include "ui_expodialog.h"
#include "helpers.h"

ExpoDialog::ExpoDialog(QWidget *parent, ModelData & model, ExpoData *expoData, GeneralSettings & generalSettings, FirmwareInterface * firmware, char * inputName) :
  QDialog(parent),
  ui(new Ui::ExpoDialog),
  model(model),
  generalSettings(generalSettings),
  firmware(firmware),
  ed(expoData),
  inputName(inputName)
{
  ui->setupUi(this);
  QLabel * lb_fp[] = {ui->lb_FP0,ui->lb_FP1,ui->lb_FP2,ui->lb_FP3,ui->lb_FP4,ui->lb_FP5,ui->lb_FP6,ui->lb_FP7,ui->lb_FP8 };
  QCheckBox * cb_fp[] = {ui->cb_FP0,ui->cb_FP1,ui->cb_FP2,ui->cb_FP3,ui->cb_FP4,ui->cb_FP5,ui->cb_FP6,ui->cb_FP7,ui->cb_FP8 };

  setWindowTitle(tr("Edit %1").arg(getInputStr(model, ed->chn)));
  QRegExp rx(CHAR_FOR_NAMES_REGEX);

  if (IS_TARANIS(GetEepromInterface()->getBoard()))
    gvGroup = new GVarGroup(ui->weightGV, ui->weightSB, ui->weightCB, ed->weight, 100, -100, 100);
  else
    gvGroup = new GVarGroup(ui->weightGV, ui->weightSB, ui->weightCB, ed->weight, 100, 0, 100);

  curveGroup = new CurveGroup(ui->curveTypeCB, ui->curveGVarCB, ui->curveValueCB, ui->curveValueSB, ed->curve);

  populateSwitchCB(ui->switchesCB, ed->swtch, generalSettings);

  ui->sideCB->setCurrentIndex(ed->mode-1);

  if (!firmware->getCapability(FlightModes)) {
    ui->label_phases->hide();
    for (int i=0; i<9; i++) {
      lb_fp[i]->hide();
      cb_fp[i]->hide();
    }
  }
  else {
    int mask=1;
    for (int i=0; i<9 ; i++) {
      if ((ed->phases & mask)==0) {
        cb_fp[i]->setChecked(true);
      }
      mask <<= 1;
    }
    for (int i=firmware->getCapability(FlightModes); i<9;i++) {
      lb_fp[i]->hide();
      cb_fp[i]->hide();
    }
  }

  if (firmware->getCapability(VirtualInputs)) {
    ui->inputName->setMaxLength(4);
    populateSourceCB(ui->sourceCB, ed->srcRaw, model, POPULATE_SOURCES | POPULATE_SWITCHES | POPULATE_TRIMS | POPULATE_TELEMETRY);
    ui->sourceCB->removeItem(0);
  }
  else {
    ui->inputNameLabel->hide();
    ui->inputName->hide();
    ui->sourceLabel->hide();
    ui->sourceCB->hide();
    ui->trimLabel->hide();
    ui->trimCB->hide();
  }

  ui->trimCB->addItem(tr("Rud"), 1);
  ui->trimCB->addItem(tr("Ele"), 2);
  ui->trimCB->addItem(tr("Thr"), 3);
  ui->trimCB->addItem(tr("Ail"), 4);
  ui->trimCB->setCurrentIndex(1 - ed->carryTrim);

  int expolength = firmware->getCapability(HasExpoNames);
  if (!expolength) {
    ui->lineNameLabel->hide();
    ui->lineName->hide();
  }
  else {
    ui->lineName->setMaxLength(expolength);
  }

  ui->inputName->setValidator(new QRegExpValidator(rx, this));
  ui->inputName->setText(inputName);

  ui->lineName->setValidator(new QRegExpValidator(rx, this));
  ui->lineName->setText(ed->name);

  updateScale();
  valuesChanged();

  connect(ui->lineName, SIGNAL(editingFinished()), this, SLOT(valuesChanged()));
  connect(ui->sourceCB, SIGNAL(currentIndexChanged(int)), this, SLOT(valuesChanged()));
  connect(ui->scale, SIGNAL(editingFinished()), this, SLOT(valuesChanged()));
  connect(ui->trimCB, SIGNAL(currentIndexChanged(int)), this, SLOT(valuesChanged()));
  connect(ui->switchesCB, SIGNAL(currentIndexChanged(int)), this, SLOT(valuesChanged()));
  connect(ui->sideCB, SIGNAL(currentIndexChanged(int)), this, SLOT(valuesChanged()));
  for (int i=0; i<9; i++) {
    connect(cb_fp[i], SIGNAL(toggled(bool)), this, SLOT(valuesChanged()));
  }
  if (firmware->getCapability(VirtualInputs))
    connect(ui->inputName, SIGNAL(editingFinished()), this, SLOT(valuesChanged()));

  QTimer::singleShot(0, this, SLOT(shrink()));
}

ExpoDialog::~ExpoDialog()
{
  delete gvGroup;
  delete curveGroup;
  delete ui;
}

void ExpoDialog::updateScale()
{
  if (firmware->getCapability(VirtualInputs) && ed->srcRaw.type == SOURCE_TYPE_TELEMETRY) {
    RawSourceRange range = ed->srcRaw.getRange();
    ui->scaleLabel->show();
    ui->scale->show();
    ui->scale->setDecimals(range.decimals);
    ui->scale->setMinimum(range.min);
    ui->scale->setMaximum(range.max);
    ui->scale->setValue(round(range.step * ed->scale));
  }
  else {
    ui->scaleLabel->hide();
    ui->scale->hide();
  }
}

void ExpoDialog::valuesChanged()
{
    QCheckBox * cb_fp[] = {ui->cb_FP0,ui->cb_FP1,ui->cb_FP2,ui->cb_FP3,ui->cb_FP4,ui->cb_FP5,ui->cb_FP6,ui->cb_FP7,ui->cb_FP8 };

    RawSource srcRaw = RawSource(ui->sourceCB->itemData(ui->sourceCB->currentIndex()).toInt());
    if (ed->srcRaw != srcRaw) {
      ed->srcRaw = srcRaw;
      updateScale();
    }

    RawSourceRange range = srcRaw.getRange();
    ed->scale = round(float(ui->scale->value()) / range.step);
    ed->carryTrim = 1 - ui->trimCB->currentIndex();
    ed->swtch  = RawSwitch(ui->switchesCB->itemData(ui->switchesCB->currentIndex()).toInt());
    ed->mode   = ui->sideCB->currentIndex() + 1;

    strcpy(ed->name, ui->lineName->text().toAscii().data());
    strcpy(inputName, ui->inputName->text().toAscii().data());

    ed->phases=0;
    for (int i=8; i>=0 ; i--) {
      if (!cb_fp[i]->checkState()) {
        ed->phases+=1;
      }
      ed->phases<<=1;
    }
    ed->phases>>=1;
    if (firmware->getCapability(FlightModes)) {
      int zeros=0;
      int ones=0;
      int phtemp=ed->phases;
      for (int i=0; i<firmware->getCapability(FlightModes); i++) {
        if (phtemp & 1) {
          ones++;
        }
        else {
          zeros++;
        }
        phtemp >>=1;
      }
      if (zeros==1) {
        phtemp=ed->phases;
        for (int i=0; i<firmware->getCapability(FlightModes); i++) {
          if ((phtemp & 1)==0) {
            break;
          }
          phtemp >>=1;
        }
      }
      else if (ones==1) {
        phtemp=ed->phases;
        for (int i=0; i<firmware->getCapability(FlightModes); i++) {
          if (phtemp & 1) {
            break;
          }
          phtemp >>=1;
        }
      }
    }
    else {
      ed->phases=0;
    }  
}

void ExpoDialog::shrink()
{
  resize(0, 0);
}
