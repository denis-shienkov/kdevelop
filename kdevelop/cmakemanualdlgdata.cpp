/**********************************************************************
            --- KDevelop (KDlgEdit)  generated file ---                

            Last generated: Wed Jul 14 20:29:59 1999

            DO NOT EDIT!!!  This file will be automatically
            regenerated by KDevelop.  All changes will be lost.

**********************************************************************/
#include <qpixmap.h>
#include "cmakemanualdlg.h"

void  CMakeManualDlg::initDialog(){
  this->resize(350,220);
  this->setMinimumSize(0,0);
  QGroupBox_1->setGeometry(10,10,330,70);
  QGroupBox_1->setMinimumSize(0,0);
  QGroupBox_1->setTitle("Program");

  ok_button->setGeometry(50,170,100,30);
  ok_button->setMinimumSize(0,0);
  ok_button->setText("OK");

  cancel_button->setGeometry(190,170,100,30);
  cancel_button->setMinimumSize(0,0);
  cancel_button->setText("Cancel");

  file_edit->setGeometry(10,110,280,30);
  file_edit->setMinimumSize(0,0);
  file_edit->setText("");

  QLabel_1->setGeometry(10,80,170,30);
  QLabel_1->setMinimumSize(0,0);
  QLabel_1->setText("Manual-File:");

  sgtml2html_radiobutton->setGeometry(200,30,100,30);
  sgtml2html_radiobutton->setMinimumSize(0,0);
  sgtml2html_radiobutton->setText("sgml2html");

  ksgml2html_radiobutton->setGeometry(40,30,100,30);
  ksgml2html_radiobutton->setMinimumSize(0,0);
  ksgml2html_radiobutton->setText("ksgml2html");

  file_button->setGeometry(310,110,30,30);
  file_button->setMinimumSize(0,0);
  file_button->setText("Button");
  file_button->setPixmap(QPixmap("/opt/kde/share/icons/mini/mini-ofolder.xpm"));

}
