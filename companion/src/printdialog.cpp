#include "printdialog.h"
#include "ui_printdialog.h"
#include "helpers.h"
#include "eeprominterface.h"
#include <QtGui>
#include <QImage>
#include <QColor>
#include <QPainter>

#if !defined WIN32 && defined __GNUC__
#include <unistd.h>
#endif

#define ISIZE 200 // curve image size
#define ISIZEW 400 // curve image size

PrintDialog::PrintDialog(QWidget *parent, FirmwareInterface * firmware, GeneralSettings *gg, ModelData *gm, QString filename) :
  QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint),
  firmware(firmware),
  g_eeGeneral(gg),
  g_model(gm),
  printfilename(filename),
  ui(new Ui::PrintDialog),
  gvars(gvarsEnabled()),
  gvarnum (0)
{
  if (gvars) {
    gvarnum=firmware->getCapability(Gvars); 
  } 

  ui->setupUi(this);
  this->setWindowIcon(CompanionIcon("print.png"));
  te = ui->textEdit;

  setWindowTitle(tr("Setup for: ") + g_model->name);
  te->clear();
  QString modelname=g_model->name;
  if (modelname.isEmpty()) {
    curvefile5=QString("%1/curve5.png").arg(qd->tempPath());
    curvefile9=QString("%1/curve9.png").arg(qd->tempPath());      
  }
  else {
    curvefile5=QString("%1/%2-curve5.png").arg(qd->tempPath()).arg(modelname);
    curvefile9=QString("%1/%2-curve9.png").arg(qd->tempPath()).arg(modelname);
  }
  printSetup(); 
  if (gvars) {
    te->append(printFlightModes()+"<br>");
  }
  printInputs();
  printMixes();
  printLimits();
  printCurves();
  printGvars();
  printSwitches();
  printFSwitches();
  printFrSky();
  
  te->scrollToAnchor("1");
  if (!printfilename.isEmpty()) {
    printToFile();
    QTimer::singleShot(0, this, SLOT(autoClose()));
  }
}

void PrintDialog::closeEvent(QCloseEvent *event) 
{
  if (printfilename.isEmpty()) {
    QByteArray ba = curvefile5.toLatin1();
    char *name = ba.data(); 
    unlink(name);
    ba = curvefile9.toLatin1();
    name = ba.data(); 
    unlink(name);
  }
}

PrintDialog::~PrintDialog()
{
    delete ui;
}

QString doTC(const QString s, const QString color="", bool bold=false)
{
    QString str = s;
    if(bold) str = "<b>" + str + "</b>";
    if(!color.isEmpty()) str = "<font color=" + color + ">" + str + "</font>";
    return "<td align=center>" + str + "</td>";
}

QString doTR(const QString s, const QString color="", bool bold=false)
{
    QString str = s;
    if(bold) str = "<b>" + str + "</b>";
    if(!color.isEmpty()) str = "<font color=" + color + ">" + str + "</font>";
    return "<td align=right>" + str + "</td>";
}

QString doTL(const QString s, const QString color="", bool bold=false)
{
    QString str = s;
    if(bold) str = "<b>" + str + "</b>";
    if(!color.isEmpty()) str = "<font color=" + color + ">" + str + "</font>";
    return "<td align=left>" + str + "</td>";
}

QString PrintDialog::fv(const QString name, const QString value)
{
    return "<b>" + name + ": </b><font color=green>" + value + "</font><br>";
}

void PrintDialog::printSetup()
{
    QString str = "<a name=1></a><table border=1 cellspacing=0 cellpadding=3 width=\"100%\">";
    str.append(QString("<tr><td colspan=%1 ><table border=0 width=\"100%\"><tr><td><h1>").arg((firmware->getCapability(FlightModes) && !gvars) ? 2 : 1));
    str.append(g_model->name);
    str.append("&nbsp;(");
    str.append(firmware->getEepromInterface()->getName());
    str.append(")</h1></td><td align=right valign=top NOWRAP><font size=-1>"+tr("printed on: %1").arg(QDateTime::currentDateTime().toString(Qt::SystemLocaleShortDate))+"</font></td></tr></table></td></tr><tr><td><table border=0 cellspacing=0 cellpadding=3>");
    str.append("<tr><td><h2>"+tr("General Model Settings")+"</h2></td></tr>");
    str.append("<tr><td>");
    str.append(fv(tr("Name"), g_model->name));
    str.append(fv(tr("EEprom Size"), QString("%1").arg(firmware->getEepromInterface()->getSize(*g_model))));
    str.append(fv(tr("Timer1"), getTimerStr(g_model->timers[0])));  //value, mode, count up/down
    str.append(fv(tr("Timer2"), getTimerStr(g_model->timers[1])));  //value, mode, count up/down
    str.append(fv(tr("Protocol"), getProtocol(g_model))); //proto, numch, delay,
    str.append(fv(tr("Pulse Polarity"), g_model->moduleData[0].ppmPulsePol ? "NEG" : "POS"));
    str.append(fv(tr("Throttle Trim"), g_model->thrTrim ? tr("Enabled") : tr("Disabled")));
    str.append(fv(tr("Throttle Expo"), g_model->thrExpo ? tr("Enabled") : tr("Disabled")));
    // TODO   str.append(fv(tr("Trim Switch"), getSWName(g_model->trimSw)));
    str.append(fv(tr("Trim Increment"), getTrimInc(g_model)));
    str.append(fv(tr("Center Beep"), getCenterBeep(g_model))); // specify which channels beep
    str.append("</td></tr></table></td>");
    if (!gvars) {
      str.append("<td width=\"380\">");
      str.append(printFlightModes());
      str.append("</td>");
    }
    str.append("</tr></table><br>");
    te->append(str);
}

QString PrintDialog::printFlightModes()
{      
    QString str="";
    str.append(QString("<table border=1 cellspacing=0 cellpadding=3 width=\"100%\"><tr><td colspan=%1><h2>").arg(!gvars ? 8+firmware->getCapability(RotaryEncoders) : 8+gvarnum+firmware->getCapability(RotaryEncoders)));
    str.append(tr("Flight modes"));
    str.append("</h2></td></tr><tr><td style=\"border-style:none;\">&nbsp;</td><td colspan=2 align=center><b>");
    str.append(tr("Fades")+"</b></td>");
    str.append("<td colspan=4 align=center><b>"+tr("Trims")+"</b></td>");
    if (gvars) {
      str.append(QString("<td colspan=%1 align=center><b>").arg(gvarnum)+tr("Gvars")+"</b></td>");
    }
    if (firmware->getCapability(RotaryEncoders)) {
      str.append(QString("<td colspan=%1 align=center><b>").arg(firmware->getCapability(RotaryEncoders))+tr("Rot.Enc.")+"</b></td>");
    }
    str.append("<td rowspan=2 align=\"center\" valign=\"bottom\"><b>"+tr("Switch")+"</b></td></tr><tr><td align=center nowrap><b>"+tr("Flight mode name"));
    str.append("</b></td><td align=center ><b>"+tr("IN")+"</b></td><td align=center ><b>"+tr("OUT")+"</b></td>");
    QString labels[] = { tr("Rud"), tr("Ele"), tr("Thr"), tr("Ail") }; // TODO is elsewhere for sure
    for (int i=0; i<4; i++) {
      GeneralSettings generalSettings = *g_eeGeneral;
      str.append(QString("<td  align=\"center\" nowrap><b>%1</b></td>").arg(labels[CONVERT_MODE(i+1)-1]));
    }
    if (gvars) {
      for (unsigned int i=0; i<gvarnum; i++) {
        str.append(QString("<td  align=\"center\" nowrap><b>GV%1</b><br>%2</td>").arg(i+1).arg(g_model->gvars_names[i]));
      }      
    }
    for (int i=0; i<firmware->getCapability(RotaryEncoders); i++) {
      str.append(QString("<td align=\"center\"><b>RE%1</b></td>").arg((i==0 ? 'A': 'B')));
    }
    str.append("</tr>");
    for (int i=0; i<firmware->getCapability(FlightModes); i++) {
      PhaseData *pd=&g_model->phaseData[i];
      str.append("<tr><td><b>"+tr("FM")+QString("%1</b> <font size=+1 face='Courier New' color=green>%2</font></td>").arg(i).arg(pd->name));
      str.append(QString("<td  align=\"right\"><font size=+1 face='Courier New' color=green>%1</font></td>").arg((qreal)pd->fadeIn/firmware->getCapability(SlowScale)));
      str.append(QString("<td width=\"30\" align=\"right\"><font size=+1 face='Courier New' color=green>%1</font></td>").arg((qreal)pd->fadeOut/firmware->getCapability(SlowScale)));
      for (int k=0; k<4; k++) {
        //TODO trim values
        if (pd->trimRef[k]==-1) {
          str.append(QString("<td align=\"right\"><font size=+1 face='Courier New' color=green>%1</font></td>").arg(pd->trim[k]));
        } else {
          str.append("<td align=\"right\" ><font size=+1 face='Courier New' color=green>"+tr("FM")+QString("%1</font></td>").arg(pd->trimRef[k]));
        }
      }
      if (gvars) {
        for (unsigned int k=0; k<gvarnum; k++) {
          if (pd->gvars[k]<=1024) {
            str.append(QString("<td align=\"right\"><font size=+1 face='Courier New' color=green>%1").arg(pd->gvars[k])+"</font></td>");
          }
          else {
            int num = pd->gvars[k] - 1025;
            if (num>=i) num++;
            str.append("<td align=\"right\" ><font size=+1 face='Courier New' color=green>"+tr("FM")+QString("%1</font></td>").arg(num));
          }
        }
      }
      for (int k=0; k<firmware->getCapability(RotaryEncoders); k++) {
        if (pd->rotaryEncoders[k]<=1024) {
          str.append(QString("<td align=\"right\"><font size=+1 face='Courier New' color=green>%1").arg(pd->rotaryEncoders[k])+"</font></td>");
        }
        else {
          int num = pd->rotaryEncoders[k] - 1025;
          if (num>=i) num++;
          str.append(QString("<td align=\"right\"><font size=+1 face='Courier New' color=green>")+tr("FM")+QString("%1</font></td>").arg(num));
        }
      }      
      str.append(QString("<td align=center><font size=+1 face='Courier New' color=green>%1</font></td>").arg(pd->swtch.toString()));
      str.append("</tr>");
    }
    str.append("</table>");
    return(str);
}

void PrintDialog::printInputs()
{
    QString str = "<table border=1 cellspacing=0 cellpadding=3 width=\"100%\"><tr><td><h2>";
    str.append(tr("Inputs"));
    str.append("</h2></td></tr><tr><td><table border=0 cellspacing=0 cellpadding=3>");
    int ec=0;
    unsigned int lastCHN = 255;
    for(int i=0; i<C9X_MAX_EXPOS; i++) {
      ExpoData *ed=&g_model->expoData[i];
      if(ed->mode==0)
        continue;
      ec++;
      str.append("<tr><td><font size=+1 face='Courier New'>");
      if(lastCHN!=ed->chn) {
        lastCHN=ed->chn;
        str.append("<b>"+getInputStr(*g_model, ed->chn)+"</b>");
      }
      str.append("</font></td>");
      str.append("<td><font size=+1 face='Courier New' color=green>");

      switch(ed->mode) {
        case (1): 
          str += "&lt;-";
          break;
        case (2): 
          str += "-&gt;";
          break;
        default:
          str += "&nbsp;&nbsp;";
          break;
      };

      str += "&nbsp;" + tr("Weight") + QString("(%1)").arg(getGVarString(ed->weight,true));
  
      if (firmware->getCapability(VirtualInputs)) {
        str += " " + tr("Source") + QString("(%1)").arg(ed->srcRaw.toString());
        if (ed->carryTrim>0) str += " " + tr("NoTrim");
        else if (ed->carryTrim<0) str += " " + RawSource(SOURCE_TYPE_TRIM, (-(ed->carryTrim)-1)).toString();
      }
      if (ed->curve.value) str += " " + Qt::escape(ed->curve.toString());

      if (firmware->getCapability(FlightModes)) {
        if(ed->phases) {
          if (ed->phases!=(unsigned int)(1<<firmware->getCapability(FlightModes))-1) {
            unsigned int mask=1;
            bool first = true;
            bool multiple = false;
            QString strModes;
            for (int j=0; j<firmware->getCapability(FlightModes);j++) {
              if (!(ed->phases & mask)) {
                //PhaseData *pd = &g_model->phaseData[j];
                const char * pdName = g_model->phaseData[j].name;
                if (first) {
                  strModes += Qt::escape(QString("%1").arg(getPhaseName(j+1,pdName)));
                  first = false;
                } else {
                  strModes += Qt::escape(QString(", %1").arg(getPhaseName(j+1, pdName)));
                  multiple = true;
                }
              }
              mask <<= 1;
            }
            if (!strModes.isEmpty()) {
              str += " " + tr(multiple?"Flight modes":"Flight mode") + "(" + strModes + ")";
            }
          } else {
            str += tr("DISABLED")+QString(" !!!");
          }
        }
      } 
      if (ed->swtch.type) str += " " + tr("Switch") + QString("(%1)").arg(ed->swtch.toString());
      if (firmware->getCapability(HasExpoNames) && ed->name[0]) str += Qt::escape(QString(" [%1]").arg(ed->name));
      str += "</font></td></tr>";
    }
    str += "</table></td></tr></table><br>";
    if (ec>0)
      te->append(str);
}


void PrintDialog::printMixes()
{
    QString str = "<table border=1 cellspacing=0 cellpadding=3 style=\"page-break-after:always;\" width=\"100%\"><tr><td><h2>";
    str.append(tr("Mixers"));
    str.append("</h2></td></tr><tr><td><table border=0 cellspacing=0 cellpadding=3>");

    unsigned int lastCHN = 255;
    for(int i=0; i<firmware->getCapability(Mixes); i++) {
      MixData *md = &g_model->mixData[i];
      if(!md->destCh || md->destCh>(unsigned int)firmware->getCapability(Outputs) ) break;
      str.append("<tr><td><font size=+1 face='Courier New'><b>");
      if(lastCHN!=md->destCh) {
        lastCHN=md->destCh;

        QString chname = QObject::tr("CH%1").arg(lastCHN);
        // TODO not nice, Qt brings a function for that, I don't remember right now
        (chname.length() < 4) ? chname.append("  ") : chname.append(" ");
        if (firmware->getCapability(HasChNames)) {
          QString name = g_model->limitData[lastCHN-1].name;
          if (!name.isEmpty()) {
            name = QString("(") + name + QString(")");
          }
          name.append("        ");
          chname += name.left(8);
        }
        chname = Qt::escape(chname);
        str.append(chname.replace(" ", "&nbsp;")); 
      } 
      else {
        str.append("&nbsp;");
      }
      str.append("</b></font></td>");
      str.append("<td><font size=+1 face='Courier New' color=green>");
      switch(md->mltpx) {
        case (1): str += "&nbsp;*"; break;
        case (2): str += "&nbsp;R"; break;
        default:  str += "&nbsp;&nbsp;"; break;
      };
        //set mixer src model if it is unset (srcRaw needs this to generate proper toString() for input source type)
        if (md->srcRaw.model == 0) 
          md->srcRaw.model = g_model;  
      str += " " + md->srcRaw.toString();

      str += " " + Qt::escape(tr("Weight(%1)").arg(getGVarString(md->weight, true)));

      QString phasesStr = getPhasesStr(md->phases, *g_model);
      if (!phasesStr.isEmpty()) str += " " + Qt::escape(phasesStr);

      if (md->swtch.type != SWITCH_TYPE_NONE) {
        str += " " + Qt::escape(tr("Switch(%1)").arg(md->swtch.toString()));
      }

      if (md->carryTrim) {
        str += " " + Qt::escape(tr("NoTrim"));
      }

      if (md->noExpo)      str += " " + Qt::escape(tr("No DR/Expo"));
      if (md->sOffset)     str += " " + Qt::escape(tr("Offset(%1)").arg(getGVarString(md->sOffset)));
      if (md->curve.value) str += " " + Qt::escape(md->curve.toString());

  
      int scale = firmware->getCapability(SlowScale);
      if (scale == 0)
        scale = 1;
      if (md->delayDown || md->delayUp)
        str += Qt::escape(tr(" Delay(u%1:d%2)").arg((double)md->delayUp/scale).arg((double)md->delayDown/scale));
      if (md->speedDown || md->speedUp)
        str += Qt::escape(tr(" Slow(u%1:d%2)").arg((double)md->speedUp/scale).arg((double)md->speedDown/scale));
      if (md->mixWarn)  str += Qt::escape(tr(" Warn(%1)").arg(md->mixWarn));
      if (firmware->getCapability(HasMixerNames)) {
        QString MixerName;
        MixerName.append(md->name);
        if (!MixerName.isEmpty()) {
          str += " " + Qt::escape(QString("(%1)").arg(MixerName));
        }
      }
      str.append("</font></td></tr>");
    }
    str.append("</table></td></tr></table><br>");
    te->append(str);
}

void PrintDialog::printLimits()
{
    QString str = "<table border=1 cellspacing=0 cellpadding=3 width=\"100%\">";
    int numcol;
    numcol=(firmware->getCapability(Outputs)+1)>17 ? 17:firmware->getCapability(Outputs)+1;
    str.append(QString("<tr><td colspan=%1><h2>").arg(numcol)+tr("Limits")+"</h2></td></tr>");
    str.append("<tr><td>&nbsp;</td>");
    if (firmware->getCapability(Outputs)<17) {
      for(int i=0; i<firmware->getCapability(Outputs); i++) {
        str.append(doTC(tr("CH")+QString(" %1").arg(i+1,2,10,QChar('0')),"",true));
      }
      str.append("</tr>");
      if (firmware->getCapability(HasChNames)) {
        str.append("<tr><td><b>"+tr("Name")+"</b></td>");
        for(int i=0; i<firmware->getCapability(Outputs); i++) {
          str.append(doTR(g_model->limitData[i].name,"green"));
        }
      }
      str.append("<tr><td><b>"+tr("Offset")+"</b></td>");
      for(int i=0; i<firmware->getCapability(Outputs); i++) {
        str.append(doTR(QString::number((qreal)g_model->limitData[i].offset/10, 'f', 1),"green"));
      }
      str.append("</tr>");
      str.append("<tr><td><b>"+tr("Min")+"</b></td>");
      for(int i=0; i<firmware->getCapability(Outputs); i++) {
        str.append(doTR(QString::number((qreal)g_model->limitData[i].min/10),"green"));
      }
      str.append("</tr>");
      str.append("<tr><td><b>"+tr("Max")+"</b></td>");
      for(int i=0; i<firmware->getCapability(Outputs); i++) {
        str.append(doTR(QString::number((qreal)g_model->limitData[i].max/10),"green"));
      }
      str.append("</tr>");
      str.append("<tr><td><b>"+tr("Invert")+"</b></td>");
      for(int i=0; i<firmware->getCapability(Outputs); i++) {
        str.append(doTR(QString(g_model->limitData[i].revert ? tr("INV") : tr("NOR")),"green"));
      }
    } else {
      for(int i=0; i<16; i++) {
        str.append(doTC(tr("CH")+QString(" %1").arg(i+1,2,10,QChar('0')),"",true));
      }
      str.append("</tr>");
      if (firmware->getCapability(HasChNames)) {
        str.append("<tr><td><b>"+tr("Name")+"</b></td>");
        for(int i=0; i<16; i++) {
          str.append(doTR(g_model->limitData[i].name,"green"));
        }
      }
      str.append("<tr><td><b>"+tr("Offset")+"</b></td>");
      for(int i=0; i<16; i++) {
        str.append(doTR(QString::number((qreal)g_model->limitData[i].offset/10, 'f', 1),"green"));
      }
      str.append("</tr>");
      str.append("<tr><td><b>"+tr("Min")+"</b></td>");
      for(int i=0; i<16; i++) {
        str.append(doTR(QString::number((qreal)g_model->limitData[i].min/10),"green"));
      }
      str.append("</tr>");
      str.append("<tr><td><b>"+tr("Max")+"</b></td>");
      for(int i=0; i<16; i++) {
        str.append(doTR(QString::number((qreal)g_model->limitData[i].max/10),"green"));
      }
      str.append("</tr>");
      str.append("<tr><td><b>"+tr("Invert")+"</b></td>");
      for(int i=0; i<16; i++) {
        str.append(doTR(QString(g_model->limitData[i].revert ? tr("INV") : tr("NOR")),"green"));
      }
      str.append("</tr>");
      str.append(QString("<tr><td colspan=%1>&nbsp;").arg(numcol)+"</td></tr>");
      str.append("<tr><td>&nbsp;</td>");
      for(int i=16; i<firmware->getCapability(Outputs); i++) {
        str.append(doTC(tr("CH")+QString(" %1").arg(i+1,2,10,QChar('0')),"",true));
      }
      str.append("</tr>");
      if (firmware->getCapability(HasChNames)) {
        str.append("<tr><td><b>"+tr("Name")+"</b></td>");
        for(int i=16; i<firmware->getCapability(Outputs); i++) {
          str.append(doTR(g_model->limitData[i].name,"green"));
        }
      }
      str.append("<tr><td><b>"+tr("Offset")+"</b></td>");
      for(int i=16; i<firmware->getCapability(Outputs); i++) {
        str.append(doTR(QString::number((qreal)g_model->limitData[i].offset/10, 'f', 1),"green"));
      }
      str.append("</tr>");
      str.append("<tr><td><b>"+tr("Min")+"</b></td>");
      for(int i=16; i<firmware->getCapability(Outputs); i++) {
        str.append(doTR(QString::number((qreal)g_model->limitData[i].min/10),"green"));
      }
      str.append("</tr>");
      str.append("<tr><td><b>"+tr("Max")+"</b></td>");
      for(int i=16; i<firmware->getCapability(Outputs); i++) {
        str.append(doTR(QString::number((qreal)g_model->limitData[i].max/10),"green"));
      }
      str.append("</tr>");
      str.append("<tr><td><b>"+tr("Invert")+"</b></td>");
      for(int i=16; i<firmware->getCapability(Outputs); i++) {
        str.append(doTR(QString(g_model->limitData[i].revert ? tr("INV") : tr("NOR")),"green"));
      }
    }
    str.append("</tr>");
    str.append("</table>");
    str.append("<br>");
    te->append(str);
}

void PrintDialog::printCurves()
{
    int i,r,g,b,c,count;
    char buffer[16];
    QPen pen(Qt::black, 2, Qt::SolidLine);
    
    QString str = "<table border=1 cellspacing=0 cellpadding=3 style=\"page-break-before:auto;\" width=\"100%\"><tr><td><h2>";
    str.append(tr("Curves"));
    str.append("</h2></td></tr><tr><td>");
    int numcurves=firmware->getCapability(NumCurves);
    if (numcurves==0) {
      numcurves=16;
    }
    {
      QImage qi(ISIZEW+1,ISIZEW+1,QImage::Format_RGB32);
      QPainter painter(&qi);
      painter.setBrush(QBrush("#FFFFFF"));
      painter.setPen(QColor(0,0,0));
      painter.drawRect(0,0,ISIZEW,ISIZEW);
      str.append("<table border=0 cellspacing=0 cellpadding=3 width=\"100%\">"+QString("<tr><td width=\"400\"><img src=\"%1\" border=0></td><td><table border=1 cellspacing=0 cellpadding=3 width=\"100%\">").arg(curvefile5));
      for(i=0; i<numcurves; i++) {
        pen.setColor(colors[i]);
        painter.setPen(pen);
        colors[i].getRgb(&r,&g,&b);
        c=r;
        c*=256;
        c+=g;
        c*=256;
        c+=b;
        sprintf(buffer,"%06x",c);
        if(i%2 == 0) str.append("<tr>");
        str.append(QString("<td width=\"70\"><font color=#%1><b>").arg(buffer)+tr("Curve")+QString(" %1</b></font></td>").arg(i+1));
        if(i%2) str.append("</tr>");
      }
      str.append("</table></td></tr><tr><td colspan=2><table border=1 cellspacing=0 cellpadding=3 width=\"100%\">");
      str.append("<tr>");
      str.append(doTC("&nbsp;"));
      str.append(doTC("&nbsp;"));
      int numpoint=0;
      for(i=0; i<numcurves; i++) {
        if (g_model->curves[i].count>numpoint)
          numpoint=g_model->curves[i].count;
      }
      for(i=0; i<numpoint; i++) 
          str.append(doTC(tr("pt %1").arg(i+1), "", true));
      str.append("</tr>");
      for(i=0; i<numcurves; i++) {
        pen.setColor(colors[i]);
        painter.setPen(pen);
        colors[i].getRgb(&r,&g,&b);
        c=r;
        c*=256;
        c+=g;
        c*=256;
        c+=b;
        sprintf(buffer,"%06x",c);
        str.append("<tr>");
        int curvepoints=g_model->curves[i].count;
        if (g_model->curves[i].type == CurveData::CURVE_TYPE_CUSTOM)
          str.append(QString("<td width=\"70\" rowspan=2 valign=middle><font color=#%1><b>").arg(buffer)+tr("Curve")+QString(" %1</b></font></td><td width=5>Y</td>").arg(i+1));
        else
          str.append(QString("<td width=\"70\"><font color=#%1><b>").arg(buffer)+tr("Curve")+QString(" %1</b></font></td><td width=5>Y</td>").arg(i+1));
        count=0;
        for(int j=0; j<curvepoints; j++) {
          if (g_model->curves[i].points[j].y!=0)
            count++;
        }
        for(int j=0; j<curvepoints; j++) {    
          str.append(doTR(QString::number(g_model->curves[i].points[j].y),"green"));
          if (j>0 && count!=0) {
            if (g_model->curves[i].type == CurveData::CURVE_TYPE_CUSTOM)
              painter.drawLine(ISIZEW/2+(ISIZEW*g_model->curves[i].points[j-1].x)/200,ISIZEW/2-(ISIZEW*g_model->curves[i].points[j-1].y)/200,ISIZEW/2+(ISIZEW*g_model->curves[i].points[j].x)/200,ISIZEW/2-(ISIZEW*g_model->curves[i].points[j].y)/200);
            else
              painter.drawLine(ISIZEW*(j-1)/(curvepoints-1),ISIZEW/2-(ISIZEW*g_model->curves[i].points[j-1].y)/200,ISIZEW*(j)/(curvepoints-1),ISIZEW/2-(ISIZEW*g_model->curves[i].points[j].y)/200);
          }
        }
        for(int j=curvepoints; j<numpoint; j++) {
          str.append(doTR("","green"));
        }     
        str.append("</tr>");
        if (g_model->curves[i].type == CurveData::CURVE_TYPE_CUSTOM) {
          str.append("<tr><td width=5>X</td>");
          for(int j=0; j<curvepoints; j++) {    
            str.append(doTR(QString::number(g_model->curves[i].points[j].x),"green"));
          }
          for(int j=curvepoints; j<numpoint; j++) {
            str.append(doTR("","green"));
          }     
          str.append("</tr>");  
        }
      }
      str.append("</table></td></tr></table></td></tr></table>");
      str.append("<br>");
      painter.setPen(QColor(0,0,0));
      painter.drawLine(0,ISIZEW/2,ISIZEW,ISIZEW/2);
      painter.drawLine(ISIZEW/2,0,ISIZEW/2,ISIZEW);
      for(i=0; i<21; i++) {
        painter.drawLine(ISIZEW/2-5,(ISIZEW*i)/(20),ISIZEW/2+5,(ISIZEW*i)/(20));
        painter.drawLine((ISIZEW*i)/(20),ISIZEW/2-5,(ISIZEW*i)/(20),ISIZEW/2+5);
      }

      qi.save(curvefile5, "png",100); 
      
    }
    te->append(str);
}

void PrintDialog::printSwitches()
{
    int sc=0;
    QString str = "<table border=1 cellspacing=0 cellpadding=3 width=\"100%\">";
    str.append("<tr><td><h2>"+tr("Logical Switches")+"</h2></td></tr>");
    str.append("<tr><td><table border=0 cellspacing=0 cellpadding=3>");

    for (int i=0; i<firmware->getCapability(LogicalSwitches); i++) {
      if (g_model->customSw[i].func) {
        str.append("<tr>");
        str.append("<td width=\"60\"><b>"+tr("L")+QString("%1</b></td>").arg(i+1));
        QString tstr = g_model->customSw[i].toString(*g_model);
        str.append(doTL(tstr,"green"));
        str.append("</tr>");
        sc++;
      }
    }
    str.append("</table></td></tr></table>");
    str.append("<br>");
    if (sc!=0)
      te->append(str);
}

void PrintDialog::printGvars()
{
  if (!firmware->getCapability(GvarsFlightModes) && (gvars && firmware->getCapability(Gvars))) {
    QString str = "<table border=1 cellspacing=0 cellpadding=3 width=\"100%\">";
    str.append("<tr><td><h2>"+tr("Global Variables")+"</h2></td></tr>");
    str.append("<tr><td><table border=1 cellspacing=0 cellpadding=3 width=100>");
    PhaseData *pd=&g_model->phaseData[0];
    int width=100/gvarnum;
    str.append("<tr>");
    for(unsigned int i=0; i<gvarnum; i++) {        
        str.append(QString("<td width=\"%1%\" align=\"center\"><b>").arg(width)+tr("GV")+QString("%1</b></td>").arg(i+1));
    }
    str.append("</tr>");
    str.append("<tr>");
    for(unsigned int i=0; i<gvarnum; i++) {        
        str.append(QString("<td width=\"%1%\" align=\"center\"><font color=green>").arg(width)+QString("%1</font></td>").arg(pd->gvars[i]));
    }
    str.append("</tr>");
    str.append("</table></td></tr></table>");
    str.append("<br>");
    te->append(str);
  }
}

void PrintDialog::printFSwitches()
{
    int sc=0;
    QString str = "<table border=1 cellspacing=0 cellpadding=3 width=\"100%\">";
    str.append("<tr><td><h2>"+tr("Special Functions")+"</h2></td></tr>");
    str.append("<tr><td><table border=0 cellspacing=0 cellpadding=3><tr>");
    str.append("<td width=\"60\"><b>#</b></td>");
    str.append(doTL(tr("Switch"), "", true));
    str.append(doTL(tr("Function"), "", true));
    str.append(doTL(tr("Parameter"), "", true));
    str.append(doTL(tr("Repeat"), "", true));
    str.append(doTL(tr("Enabled"), "", true));
    str.append("</tr>");
    for(int i=0; i<firmware->getCapability(CustomFunctions); i++) {
      if (g_model->funcSw[i].swtch.type!=SWITCH_TYPE_NONE) {
          str.append("<tr>");
          str.append(doTL(tr("SF%1").arg(i+1),"", true));
          str.append(doTL(g_model->funcSw[i].swtch.toString(),"green"));
          str.append(doTL(g_model->funcSw[i].funcToString(),"green"));
          str.append(doTL(g_model->funcSw[i].paramToString(),"green"));
          int index=g_model->funcSw[i].func;
          if ((g_model->funcSw[i].repeatParam>0) && 
              (index==FuncPlaySound || index==FuncPlayHaptic || index==FuncPlayValue || index==FuncPlayPrompt || index==FuncPlayBoth || index==FuncBackgroundMusic)) {
            str.append(doTL(QString("%1").arg(g_model->funcSw[i].repeatParam),"green"));
          } else {
            str.append(doTL( "&nbsp;","green"));
          }
          if ((index<=FuncInstantTrim) || (index>FuncBackgroundMusicPause)) {
            str.append(doTL((g_model->funcSw[i].enabled ? "ON" : "OFF"),"green"));
          } else {
            str.append(doTL( "---","green"));
          }
          str.append("</tr>");
          sc++;
      }
    }
    str.append("</table></td></tr></table>");
    str.append("<br>");
    if (sc!=0)
      te->append(str);
}

void PrintDialog::printFrSky()
{
  int tc=0;
  QString str = "<table border=1 cellspacing=0 cellpadding=3 width=\"100%\">";

  if (IS_TARANIS(GetEepromInterface()->getBoard())) {
    str.append("<tr><td colspan=3><h2>"+tr("Telemetry Settings")+"</h2></td></tr>");
    str.append("<tr>");
    str.append(doTC(tr("Analog"),"", true));
    str.append(doTC(tr("Range"),"", true));
    str.append(doTC(tr("Offset"),"", true));
    str.append("</tr>");

    FrSkyData *fd=&g_model->frsky;
    for (int i=0; i<2; i++) {
      if (fd->channels[i].ratio!=0) {
        tc++;
        float ratio=(fd->channels[i].ratio/(fd->channels[i].type==0 ?10.0:1));
        QString unit = " " + getFrSkyUnits(fd->channels[i].type);
        str.append("<tr>");
        str.append(doTL(tr("A%1").arg(i+1), "", true));
        str.append(doTC(QString::number(ratio,10,(fd->channels[i].type==0 ? 1:0))+unit, "green"));
        str.append(doTC(QString::number((fd->channels[i].offset*ratio)/255,10,(fd->channels[i].type==0 ? 1:0))+unit, "green"));
        str.append("</tr>");
      }
    }

    str.append("<tr><td colspan=3 align=\"Left\" height=\"4px\"></td></tr>");
    str.append("<tr>");
    str.append(doTC(tr("Alarms"),"", true));
    str.append(doTC(tr("Low Alarm"),"", true));
    str.append(doTC(tr("Critical Alarm"),"", true));
    str.append("</tr>");
    str.append("<tr>");
    str.append(doTL(tr("RSSI"),"", true));
    str.append(doTC(QString::number(fd->rssiAlarms[0].value,10),"green"));
    str.append(doTC(QString::number(fd->rssiAlarms[1].value,10),"green"));
    str.append("</tr>");
    for (int i=0; i<2; i++) {
      if (fd->channels[i].ratio!=0) {
        float ratio=(fd->channels[i].ratio/(fd->channels[i].type==0 ?10.0:1));
        QString unit = " " + getFrSkyUnits(fd->channels[i].type);
        str.append("<tr>");
        str.append(doTL(tr("A%1").arg(i+1), "", true));
        str.append(doTC(QString::number(ratio*(fd->channels[i].alarms[0].value/255.0+fd->channels[i].offset/255.0),10,(fd->channels[i].type==0 ? 1:0))+unit, "green"));
        str.append(doTC(QString::number(ratio*(fd->channels[i].alarms[1].value/255.0+fd->channels[i].offset/255.0),10,(fd->channels[i].type==0 ? 1:0))+unit, "green"));
        str.append("</tr>");
      }
    }

    //TODO preferrably in new tables
    str.append("<tr><td colspan=3 align=\"Left\" height=\"4px\"></td></tr>");
    //str.append("<tr><td align=\"Left\"><b>"+tr("Frsky serial protocol")+"</b></td><td colspan=2 align=\"left\">"+getFrSkyProtocol(fd->usrProto)+"</td></tr>");
    //str.append("<tr><td align=\"Left\"><b>"+tr("Units system")+"</b></td><td colspan=2 align=\"left\">"+getFrSkyMeasure(fd->imperial)+"</td></tr>");
    str.append("<tr><td align=\"Left\"><b>"+tr("Blades")+"</b></td><td colspan=2 align=\"left\">"+QString("%1").arg(fd->blades)+"</td></tr>");
    str.append("<tr><td align=\"Left\" height=\"4px\"></td></tr></table>");
  }
  else {  //other boards
    str.append("<tr><td colspan=10><h2>"+tr("Telemetry Settings")+"</h2></td></tr>");
    str.append("<tr><td colspan=4 align=\"center\">&nbsp;</td><td colspan=3 align=\"center\"><b>"+tr("Alarm 1")+"</b></td><td colspan=3 align=\"center\"><b>"+tr("Alarm 2")+"</b></td></tr>");
    str.append("<tr><td align=\"center\"><b>"+tr("Analog")+"</b></td><td align=\"center\"><b>"+tr("Unit")+"</b></td><td align=\"center\"><b>"+tr("Scale")+"</b></td><td align=\"center\"><b>"+tr("Offset")+"</b></td>");
    str.append("<td width=\"40\" align=\"center\"><b>"+tr("Type")+"</b></td><td width=\"40\" align=\"center\"><b>"+tr("Condition")+"</b></td><td width=\"40\" align=\"center\"><b>"+tr("Value")+"</b></td>");
    str.append("<td width=\"40\" align=\"center\"><b>"+tr("Type")+"</b></td><td width=\"40\" align=\"center\"><b>"+tr("Condition")+"</b></td><td width=\"40\" align=\"center\"><b>"+tr("Value")+"</b></td></tr>");
    FrSkyData *fd=&g_model->frsky;
    for (int i=0; i<2; i++) {
      if (fd->channels[i].ratio!=0) {
        tc++;
        float ratio=(fd->channels[i].ratio/(fd->channels[i].type==0 ?10.0:1));
        str.append("<tr><td align=\"center\"><b>"+tr("A%1").arg(i+1)+"</b></td><td align=\"center\"><font color=green>"+getFrSkyUnits(fd->channels[i].type)+"</font></td><td align=\"center\"><font color=green>"+QString::number(ratio,10,(fd->channels[i].type==0 ? 1:0))+"</font></td><td align=\"center\"><font color=green>"+QString::number((fd->channels[i].offset*ratio)/255,10,(fd->channels[i].type==0 ? 1:0))+"</font></td>");
        str.append("<td width=\"40\" align=\"center\"><font color=green>"+getFrSkyAlarmType(fd->channels[i].alarms[0].level)+"</font></td>");
        str.append("<td width=\"40\" align=\"center\"><font color=green>");
        str.append((fd->channels[i].alarms[0].greater==1) ? "&gt;" : "&lt;");
        str.append("</font></td><td width=\"40\" align=\"center\"><font color=green>"+QString::number(ratio*(fd->channels[i].alarms[0].value/255.0+fd->channels[i].offset/255.0),10,(fd->channels[i].type==0 ? 1:0))+"</font></td>");
        str.append("<td width=\"40\" align=\"center\"><font color=green>"+getFrSkyAlarmType(fd->channels[i].alarms[1].level)+"</font></td>");
        str.append("<td width=\"40\" align=\"center\"><font color=green>");
        str.append((fd->channels[i].alarms[1].greater==1) ? "&gt;" : "&lt;");
        str.append("</font></td><td width=\"40\" align=\"center\"><font color=green>"+QString::number(ratio*(fd->channels[i].alarms[1].value/255.0+fd->channels[i].offset/255.0),10,(fd->channels[i].type==0 ? 1:0))+"</font></td></tr>");
      }
    }
    str.append("<tr><td colspan=10 align=\"Left\" height=\"4px\"></td></tr>");
    str.append("<tr><td colspan=4 align=\"center\" rowspan=2>&nbsp;</td><td colspan=3 align=\"center\"><b>"+tr("Alarm 1")+"</b></td><td colspan=3 align=\"center\"><b>"+tr("Alarm 2")+"</b></td></tr>");
    str.append("<tr><td width=\"40\" align=\"center\"><b>"+tr("Type")+"</b></td><td width=\"40\" align=\"center\"><b>"+tr("Condition")+"</b></td><td width=\"40\" align=\"center\"><b>"+tr("Value")+"</b></td>");
    str.append("<td width=\"40\" align=\"center\"><b>"+tr("Type")+"</b></td><td width=\"40\" align=\"center\"><b>"+tr("Condition")+"</b></td><td width=\"40\" align=\"center\"><b>"+tr("Value")+"</b></td></tr>");
    str.append("<tr><td align=\"Left\" colspan=4><b>"+tr("RSSI Alarm")+"</b></td>");
    str.append("<td width=\"40\" align=\"center\"><b>"+getFrSkyAlarmType(fd->rssiAlarms[0].level)+"</b></td><td width=\"40\" align=\"center\"><b>&lt;</b></td><td width=\"40\" align=\"center\"><b>"+QString::number(fd->rssiAlarms[0].value,10)+"</b></td>");
    str.append("<td width=\"40\" align=\"center\"><b>"+getFrSkyAlarmType(fd->rssiAlarms[1].level)+"</b></td><td width=\"40\" align=\"center\"><b>&lt;</b></td><td width=\"40\" align=\"center\"><b>"+QString::number(fd->rssiAlarms[1].value,10)+"</b></td></tr>");
    str.append("<tr><td colspan=10 align=\"Left\" height=\"4px\"></td></tr>");
    str.append("<tr><td colspan=2 align=\"Left\"><b>"+tr("Frsky serial protocol")+"</b></td><td colspan=8 align=\"left\">"+getFrSkyProtocol(fd->usrProto)+"</td></tr>");
    str.append("<tr><td colspan=2 align=\"Left\"><b>"+tr("Units system")+"</b></td><td colspan=8 align=\"left\">"+getFrSkyMeasure(fd->imperial)+"</td></tr>");
    str.append("<tr><td colspan=2 align=\"Left\"><b>"+tr("Blades")+"</b></td><td colspan=8 align=\"left\">"+fd->blades+"</td></tr>");
    str.append("<tr><td colspan=10 align=\"Left\" height=\"4px\"></td></tr></table>");
  }
#if 0
  if (firmware->getCapability(TelemetryBars) || (firmware->getCapability(TelemetryCSFields))) {
    int cols=firmware->getCapability(TelemetryColsCSFields);
    if (cols==0) cols=2;
    for (int j=0; j<firmware->getCapability(TelemetryCSFields)/(4*cols); j++ ) {
      if (fd->screens[j].type==0) {
        if (cols==2) {
          str.append("<table border=1 cellspacing=0 cellpadding=3 width=\"100%\"><tr><td colspan=3 align=\"Left\"><b>"+tr("Custom Telemetry View")+"</b></td></tr>");
        } else {
          str.append("<table border=1 cellspacing=0 cellpadding=3 width=\"100%\"><tr><td colspan=5 align=\"Left\"><b>"+tr("Custom Telemetry View")+"</b></td></tr>");
        }
        for (int r=0; r<4; r++) {
          str.append("<tr>");
          for (int c=0; c<cols; c++) {
            if (fd->screens[j].body.lines[r].source[c]!=0)
              tc++;
            if (cols==2) {
              str.append("<td  align=\"Center\" width=\"45%\"><b>"+getFrSkySrc(fd->screens[j].body.lines[r].source[c])+"</b></td>");
            } else {
              str.append("<td  align=\"Center\" width=\"30%\"><b>"+getFrSkySrc(fd->screens[j].body.lines[r].source[c])+"</b></td>");
            }
            if (c<(cols-1)) {
              if (cols==2) {
                str.append("<td  align=\"Center\" width=\"10%\"><b>&nbsp;</b></td>");
              } else {
                str.append("<td  align=\"Center\" width=\"5%\"><b>&nbsp;</b></td>");
              }
            }
          }
          str.append("</tr>");
        }
        str.append("</table>");
      } else {
        str.append("<table border=1 cellspacing=0 cellpadding=3 width=\"100%\"><tr><td colspan=4 align=\"Left\"><b>"+tr("Telemetry Bars")+"</b></td></tr>");
        str.append("<tr><td  align=\"Center\"><b>"+tr("Bar Number")+"</b></td><td  align=\"Center\"><b>"+tr("Source")+"</b></td><td  align=\"Center\"><b>"+tr("Min")+"</b></td><td  align=\"Center\"><b>"+tr("Max")+"</b></td></tr>");
        for (int i=0; i<4; i++) {
          if (fd->screens[j].body.bars[i].source!=0) 
            tc++;
          // TODO str.append("<tr><td  align=\"Center\"><b>"+QString::number(i+1,10)+"</b></td><td  align=\"Center\"><b>"+getFrSkySrc(fd->screens[j].body.bars[i].source)+"</b></td><td  align=\"Right\"><b>"+(fd->screens[j].body.bars[i].source>0 ? QString::number(getBarValue(fd->screens[j].body.bars[i].source, fd->screens[j].body.bars[i].barMin,fd)):"----")+"</b></td><td  align=\"Right\"><b>"+(fd->screens[j].body.bars[i].source>0 ? QString::number(getBarValue(fd->screens[j].body.bars[i].source,(255-fd->screens[j].body.bars[i].barMax),fd)) :"----")+"</b></td></tr>");
        }
        str.append("</table>");
      }
    }
  }
#endif
  if (tc>0)
      te->append(str);    
}

void PrintDialog::on_printButton_clicked()
{
    QPrinter printer;
    printer.setPageMargins(10.0,10.0,10.0,10.0,printer.Millimeter);
    QPrintDialog *dialog = new QPrintDialog(&printer, this);
    dialog->setWindowTitle(tr("Print Document"));
    if (dialog->exec() != QDialog::Accepted)
      return;
    te->print(&printer);
}

void PrintDialog::on_printFileButton_clicked()
{
    QString fn = QFileDialog::getSaveFileName(this,tr("Select PDF output file"),QString(),tr("ODF files (*.odt);;PDF Files(*.pdf);;HTML-Files (*.htm *.html);;All Files (*)")); 
    if (fn.isEmpty())
      return;
    if (! (fn.endsWith(".odt", Qt::CaseInsensitive) || fn.endsWith(".pdf", Qt::CaseInsensitive) || fn.endsWith(".htm", Qt::CaseInsensitive) || fn.endsWith(".html", Qt::CaseInsensitive)) )
      fn += ".pdf"; // default
    if (fn.endsWith(".pdf", Qt::CaseInsensitive)) {
      QPrinter printer;
      printer.setPageMargins(10.0,10.0,10.0,10.0,printer.Millimeter);
      printer.setOutputFormat(QPrinter::PdfFormat);
      printer.setColorMode(QPrinter::Color);
      printer.setOutputFileName(fn);
      te->print(&printer);
    } else {
      QTextDocumentWriter writer(fn);
       writer.write(te->document());      
    }
}

void PrintDialog::printToFile()
{
    if (printfilename.isEmpty())
      return;
    if (! (printfilename.endsWith(".odt", Qt::CaseInsensitive) || printfilename.endsWith(".pdf", Qt::CaseInsensitive) || printfilename.endsWith(".htm", Qt::CaseInsensitive) || printfilename.endsWith(".html", Qt::CaseInsensitive)) )
      printfilename += ".pdf"; // default
    if (printfilename.endsWith(".pdf", Qt::CaseInsensitive)) {
      QPrinter printer;
      printer.setPageMargins(10.0,10.0,10.0,10.0,printer.Millimeter);
      printer.setOutputFormat(QPrinter::PdfFormat);
      printer.setColorMode(QPrinter::Color);
      printer.setOutputFileName(printfilename);
      te->print(&printer);
    } else {
      QTextDocumentWriter writer(printfilename);
       writer.write(te->document());      
    }
}

void PrintDialog::autoClose()
{
  this->close();
}
