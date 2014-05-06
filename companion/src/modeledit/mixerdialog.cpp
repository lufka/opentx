#include "mixerdialog.h"
#include "ui_mixerdialog.h"
#include "eeprominterface.h"
#include "helpers.h"

MixerDialog::MixerDialog(QWidget *parent, ModelData & model, MixData *mixdata, GeneralSettings & generalSettings, FirmwareInterface * firmware) :
  QDialog(parent),
  ui(new Ui::MixerDialog),
  model(model),
  generalSettings(generalSettings),
  firmware(firmware),
  md(mixdata),
  lock(false)
{
    ui->setupUi(this);
    QRegExp rx(CHAR_FOR_NAMES_REGEX);
    QLabel * lb_fp[] = {ui->lb_FP0,ui->lb_FP1,ui->lb_FP2,ui->lb_FP3,ui->lb_FP4,ui->lb_FP5,ui->lb_FP6,ui->lb_FP7,ui->lb_FP8 };
    QCheckBox * cb_fp[] = {ui->cb_FP0,ui->cb_FP1,ui->cb_FP2,ui->cb_FP3,ui->cb_FP4,ui->cb_FP5,ui->cb_FP6,ui->cb_FP7,ui->cb_FP8 };

    this->setWindowTitle(tr("DEST -> CH%1").arg(md->destCh));

    populateSourceCB(ui->sourceCB, md->srcRaw, model, POPULATE_SOURCES | POPULATE_VIRTUAL_INPUTS | POPULATE_SWITCHES | POPULATE_TRIMS);
    ui->sourceCB->removeItem(0);

    int limit = firmware->getCapability(OffsetWeight);

    gvWeightGroup = new GVarGroup(ui->weightGV, ui->weightSB, ui->weightCB, md->weight, 100, -limit, limit);
    gvOffsetGroup = new GVarGroup(ui->offsetGV, ui->offsetSB, ui->offsetCB, md->sOffset, 0, -limit, limit);
    curveGroup = new CurveGroup(ui->curveTypeCB, ui->curveGVarCB, ui->curveValueCB, ui->curveValueSB, md->curve);

    ui->MixDR_CB->setChecked(md->noExpo==0);

    if (firmware->getCapability(VirtualInputs) || !firmware->getCapability(MixesWithoutExpo)) {
      ui->MixDR_CB->hide();
      ui->label_MixDR->hide();
    }

    if (!firmware->getCapability(VirtualInputs)) {
      ui->trimCB->addItem(tr("Rud"));
      ui->trimCB->addItem(tr("Ele"));
      ui->trimCB->addItem(tr("Thr"));
      ui->trimCB->addItem(tr("Ail"));
    }

    ui->trimCB->setCurrentIndex(1 - md->carryTrim);

    int namelength = firmware->getCapability(HasMixerNames);
    if (!namelength) {
      ui->label_name->hide();
      ui->mixerName->hide();
    }
    else {
      ui->mixerName->setMaxLength(namelength);
    }
    ui->mixerName->setValidator(new QRegExpValidator(rx, this));
    ui->mixerName->setText(md->name);

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
        if ((md->phases & mask)==0) {
          cb_fp[i]->setChecked(true);
        }
        mask <<= 1;
      }
      for (int i=firmware->getCapability(FlightModes); i<9;i++) {
        lb_fp[i]->hide();
        cb_fp[i]->hide();
      }
    }

    populateSwitchCB(ui->switchesCB, md->swtch, generalSettings);
    ui->warningCB->setCurrentIndex(md->mixWarn);
    ui->mltpxCB->setCurrentIndex(md->mltpx);
    int scale=firmware->getCapability(SlowScale);  
    float range=firmware->getCapability(SlowRange);  
    ui->slowDownSB->setMaximum(range/scale);
    ui->slowDownSB->setSingleStep(1.0/scale);
    ui->slowDownSB->setDecimals((scale==1 ? 0 :1));
    ui->slowDownSB->setValue((float)md->speedDown/scale);
    ui->slowUpSB->setMaximum(range/scale);
    ui->slowUpSB->setSingleStep(1.0/scale);
    ui->slowUpSB->setDecimals((scale==1 ? 0 :1));
    ui->slowUpSB->setValue((float)md->speedUp/scale);
    ui->delayDownSB->setMaximum(range/scale);
    ui->delayDownSB->setSingleStep(1.0/scale);
    ui->delayDownSB->setDecimals((scale==1 ? 0 :1));
    ui->delayDownSB->setValue((float)md->delayDown/scale);
    ui->delayUpSB->setMaximum(range/scale);
    ui->delayUpSB->setSingleStep(1.0/scale);
    ui->delayUpSB->setDecimals((scale==1 ? 0 :1));
    ui->delayUpSB->setValue((float)md->delayUp/scale);
    QTimer::singleShot(0, this, SLOT(shrink()));

    valuesChanged();
    connect(ui->mixerName,SIGNAL(editingFinished()),this,SLOT(valuesChanged()));
    connect(ui->sourceCB,SIGNAL(currentIndexChanged(int)),this,SLOT(valuesChanged()));
    connect(ui->trimCB,SIGNAL(currentIndexChanged(int)),this,SLOT(valuesChanged()));
    connect(ui->MixDR_CB,SIGNAL(toggled(bool)),this,SLOT(valuesChanged()));
    connect(ui->switchesCB,SIGNAL(currentIndexChanged(int)),this,SLOT(valuesChanged()));
    connect(ui->warningCB,SIGNAL(currentIndexChanged(int)),this,SLOT(valuesChanged()));
    connect(ui->mltpxCB,SIGNAL(currentIndexChanged(int)),this,SLOT(valuesChanged()));
    connect(ui->delayDownSB,SIGNAL(editingFinished()),this,SLOT(valuesChanged()));
    connect(ui->delayUpSB,SIGNAL(editingFinished()),this,SLOT(valuesChanged()));
    connect(ui->slowDownSB,SIGNAL(editingFinished()),this,SLOT(valuesChanged()));
    connect(ui->slowUpSB,SIGNAL(editingFinished()),this,SLOT(valuesChanged()));
    for (int i=0; i<9; i++) {
      connect(cb_fp[i],SIGNAL(toggled(bool)),this,SLOT(valuesChanged()));
    }
}

MixerDialog::~MixerDialog()
{
  delete gvWeightGroup;
  delete gvOffsetGroup;
  delete curveGroup;
  delete ui;
}

void MixerDialog::changeEvent(QEvent *e)
{
  QDialog::changeEvent(e);

  switch (e->type()) {
    case QEvent::LanguageChange:
      ui->retranslateUi(this);
      break;
    default:
      break;
  }
}

void MixerDialog::valuesChanged()
{
  if (!lock) {
    lock = true;
    QCheckBox * cb_fp[] = {ui->cb_FP0,ui->cb_FP1,ui->cb_FP2,ui->cb_FP3,ui->cb_FP4,ui->cb_FP5,ui->cb_FP6,ui->cb_FP7,ui->cb_FP8 };
    md->srcRaw  = RawSource(ui->sourceCB->itemData(ui->sourceCB->currentIndex()).toInt(), &model);
    if (!firmware->getCapability(VirtualInputs) && firmware->getCapability(MixesWithoutExpo)) {
      bool drVisible = (md->srcRaw.type == SOURCE_TYPE_STICK && md->srcRaw.index < NUM_STICKS);
      ui->MixDR_CB->setEnabled(drVisible);
      ui->label_MixDR->setEnabled(drVisible);
    }
    md->carryTrim = -(ui->trimCB->currentIndex()-1);
    md->noExpo = ui->MixDR_CB->checkState() ? 0 : 1;
    md->swtch     = RawSwitch(ui->switchesCB->itemData(ui->switchesCB->currentIndex()).toInt());
    md->mixWarn   = ui->warningCB->currentIndex();
    md->mltpx     = (MltpxValue)ui->mltpxCB->currentIndex();
    int scale=firmware->getCapability(SlowScale);
    md->delayDown = round(ui->delayDownSB->value()*scale);
    md->delayUp   = round(ui->delayUpSB->value()*scale);
    md->speedDown = round(ui->slowDownSB->value()*scale);
    md->speedUp   = round(ui->slowUpSB->value()*scale);

    int i=0;
    for (i=0; i<ui->mixerName->text().toAscii().length(); i++) {
      md->name[i]=ui->mixerName->text().toAscii().at(i);
    }
    md->name[i]=0;
    md->phases=0;
    for (int i=8; i>=0 ; i--) {
      if (!cb_fp[i]->checkState()) {
        md->phases+=1;
      }
      md->phases<<=1;
    }
    md->phases>>=1;

    lock = false;
  }
}

void MixerDialog::shrink()
{
  resize(0, 0);
}
