#include "modeledit.h"
#include "ui_modeledit.h"
#include "setup.h"
#include "heli.h"
#include "flightmodes.h"
#include "inputs.h"
#include "mixes.h"
#include "channels.h"
#include "curves.h"
#include "../helpers.h"
#include "logicalswitches.h"
#include "customfunctions.h"
#include "telemetry.h"
#include "appdata.h"
#include <QScrollArea>

ModelEdit::ModelEdit(QWidget * parent, RadioData & radioData, int modelId, FirmwareInterface * firmware) :
  QDialog(parent),
  ui(new Ui::ModelEdit),
  modelId(modelId),
  model(radioData.models[modelId]),
  generalSettings(radioData.generalSettings),
  firmware(firmware)
{
  ui->setupUi(this);
  setWindowIcon(CompanionIcon("edit.png"));
  restoreGeometry(g.modelEditGeo());  
  ui->pushButton->setIcon(CompanionIcon("simulate.png"));
  SetupPanel * setupPanel = new SetupPanel(this, model, generalSettings, firmware);
  addTab(setupPanel, tr("Setup"));
  addTab(new HeliPanel(this, model, generalSettings, firmware), tr("Heli"));
  addTab(new FlightModesPanel(this, model, generalSettings, firmware), tr("Flight Modes"));
  addTab(new InputsPanel(this, model, generalSettings, firmware), tr("Inputs"));
  addTab(new MixesPanel(this, model, generalSettings, firmware), tr("Mixes"));
  Channels * chnPanel = new Channels(this, model, generalSettings, firmware);
  addTab(chnPanel, tr("Servos"));
  addTab(new Curves(this, model, generalSettings, firmware), tr("Curves"));
  addTab(new LogicalSwitchesPanel(this, model, generalSettings, firmware), tr("Logical Switches"));
  if (firmware->getCapability(CustomFunctions))
    addTab(new CustomFunctionsPanel(this, model, generalSettings, firmware), tr("Special Functions"));
  if (firmware->getCapability(Telemetry) & TM_HASTELEMETRY)
    addTab(new TelemetryPanel(this, model, generalSettings, firmware), tr("Telemetry"));
    
  connect(setupPanel, SIGNAL(extendedLimitsToggled()), chnPanel, SLOT(refreshExtendedLimits()));
}

ModelEdit::~ModelEdit()
{
  delete ui;
}

void ModelEdit::closeEvent(QCloseEvent *event)
{
  g.modelEditGeo( saveGeometry() );
}

class VerticalScrollArea : public QScrollArea
{
  public:
    VerticalScrollArea(QWidget * parent, ModelPanel * panel);

  protected:
    virtual bool eventFilter(QObject *o, QEvent *e);

  private:
    ModelPanel * panel;
    QWidget * parent;
};

VerticalScrollArea::VerticalScrollArea(QWidget * parent, ModelPanel * panel):
  QScrollArea(parent),
  panel(panel),
  parent(parent)
{
  setWidgetResizable(true);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setWidget(panel);
  panel->installEventFilter(this);
}

bool VerticalScrollArea::eventFilter(QObject *o, QEvent *e)
{
  if (o == panel && e->type() == QEvent::Resize) {
    setMinimumWidth(panel->minimumSizeHint().width() + verticalScrollBar()->width());
  }
  return false;
}

void ModelEdit::addTab(ModelPanel *panel, QString text)
{
  panels << panel;
  QWidget * widget = new QWidget(ui->tabWidget);
  QVBoxLayout *baseLayout = new QVBoxLayout(widget);
  VerticalScrollArea * area = new VerticalScrollArea(widget, panel);
  baseLayout->addWidget(area);
  ui->tabWidget->addTab(widget, text);
  connect(panel, SIGNAL(modified()), this, SLOT(onTabModified()));
}

void ModelEdit::onTabModified()
{
  emit modified();
}

void ModelEdit::on_tabWidget_currentChanged(int index)
{
  panels[index]->update();
}

void ModelEdit::on_pushButton_clicked()
{
  launchSimulation();
}

void ModelEdit::launchSimulation()
{
  RadioData *simuData = new RadioData();
  simuData->generalSettings = generalSettings;
  simuData->models[0] = model;
  startSimulation(this, *simuData, 0);
  delete simuData;
}


