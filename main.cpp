#include <QtWidgets>
#include <MainWindow.h>

#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include "sio.h"
#include "ptable.h"
#include "flasher.h"
#include "usbloader.h"
#include "fwsave.h"

#include "hexeditor/qhexedit.h"

// ссылка на селектор портов
QComboBox* pselector;

// Таблица разделов
ptable_list* ptable;
int npart=0;

QString* fwfilename=0;

// Методы класса главного окна
//=============================================
//*****************************************
//* Конструктор класса
//*****************************************
MainWindow::MainWindow(QMainWindow *parent) : QMainWindow(parent) {
    
// Настройка элементов окна
setupUi(this);

RefreshPorts->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));

pselector=PortSelector;
// создание класса таблицы разделов
ptable=new ptable_list;

// заполнение списка портов
find_ports();

// создание окна hex-редактора
hexedit=new QHexEdit(centralwidget);
hexedit->setObjectName(QStringLiteral("HexEditor"));
hexedit->setGeometry(QRect(230, 100, 600, 470));
hexedit->setAddressWidth(8);
hexedit->setOverwriteMode(true);
hexedit->hide();

if (fwfilename != 0) {
  OpenFwFile(*fwfilename);
}
}


//*****************************************
//* Деструктор класса
//*****************************************
MainWindow::~MainWindow() {

delete ptable;  
}

//*************************************************
//  Поиск ttyUSB портов и сбор их имен в таблицу
//*************************************************
void MainWindow::find_ports() {

QDir fdir("/dev");

PortSelector->clear();
PortSelector->addItems(fdir.entryList((QStringList)"ttyUSB*",QDir::System,QDir::Name));
PortSelector->setCurrentIndex(0);
}

//*****************************************
//*  Открытие файла прошивки
//*****************************************
void MainWindow::OpenFwFile(QString filename) {
  
FILE* in;

in=fopen(filename.toLocal8Bit(),"r");
if (in == 0) {
  QMessageBox::critical(0,"Ошибка","Ошибка открытия файла");
  return;
}  

// Поиск разделов и формирование таблицы разделов

ptable->findparts(in); 
regenerate_partlist();
partlist->setCurrentRow(0);
SelectPart();  
EnableMenu();
  
}  
  
//*****************************************
//*  Добавление разделов из файла прошивки
//*****************************************
void MainWindow::AppendFwFile() {
  
static QString fwname;

QFileDialog* qf=new QFileDialog(this);
fwname=qf->getOpenFileName(0,"Выбор файла прошивки","","*.exe *.bin *.fw");
delete qf;
if (fwname.isEmpty()) return;
OpenFwFile(fwname);

// если это первый открываемый файл - делаем его имя именем по умолчанию
if (fwfilename == 0) fwfilename=&fwname;  
}


//********************************************
//*  Формирование экранного списка разделов
//********************************************
void MainWindow::regenerate_partlist() {

int i;
QString str;
partlist->clear();
for (i=0;i<ptable->index();i++) {
  str.sprintf("%06x - %s",ptable->code(i),ptable->name(i));
  partlist->addItem(str);
}  
}  

//*****************************************
//*  Выбор нового файла прошивки
//*****************************************
void MainWindow::SelectFwFile() {
  
menu_part->setEnabled(0);
Menu_Oper_flash->setEnabled(0);
fileappend->setEnabled(0);
filesave->setEnabled(0);
ptable->clear();
fwfilename=0;
AppendFwFile();
EnableMenu();
}

//*****************************************
//*  Разрешение пунктов меню
//*****************************************
void MainWindow::EnableMenu(){ 
  
if (ptable->index() != 0) { 
  menu_part->setEnabled(1);
  Menu_Oper_flash->setEnabled(1);
  fileappend->setEnabled(1);
  filesave->setEnabled(1);
}
}

//*****************************************
//*  Запись на диск полного файла прошивки
//*****************************************
void MainWindow::SaveFwFile() {

fw_saver();  
}


//*****************************************
//*  Выбор раздела из списка
//*****************************************
void MainWindow::SelectPart() {

QString txt;  

int idx=partlist->currentRow();

// Вывод значений заголовка
txt.sprintf("%-8.8s",ptable->platform(idx));
Platform_input->setText(txt);

txt.sprintf("%-16.16s",ptable->date(idx));
Date_input->setText(txt);

txt.sprintf("%-16.16s",ptable->time(idx));
Time_input->setText(txt);

txt.sprintf("%-32.32s",ptable->version(idx));
Version_input->setText(txt);

txt.sprintf("%04x",ptable->code(idx)>>16);
pcode->setText(txt);

// формирование окна hex-редактора
// printf("\n idx=%i",idx); 
// printf("\n data adr=%08x  data size=%08x",(char*)ptable->iptr(idx),ptable->psize(idx)); fflush(stdout);
hexcup.setRawData((char*)ptable->iptr(idx),ptable->psize(idx));
hexedit->setData(hexcup);
hexedit->setCursorPosition(0);
hexedit->show();
  
}  


//*****************************************
//*  Сохранение раздела на диск
//*****************************************
void MainWindow::Menu_Part_Store() {
  
int np=partlist->currentRow();
QString filename;
QString str;
FILE* out;
uint8_t hdr[92];


// записываем образ раздела
filename.sprintf("%02i-%08x-%s.fw",np,ptable->code(np),ptable->name(np));
filename=QFileDialog::getSaveFileName(this,"Имя файла",filename,"firmware (*.fw);;All files (*.*)");
if (filename.isEmpty()) return;
out=fopen(filename.toLocal8Bit(),"w");
if (out == 0) {
  QMessageBox::critical(0,"Ошибка","Ошибка создания файла");
  return;
}

// записываем заголовок - upgrade state
bzero(hdr,sizeof(hdr));
hdr[0]=0x0d;
fwrite(hdr,1,sizeof(hdr),out);

ptable->save_part(np,out,0);
fclose(out);
}

//*****************************************
//*  Извлечение образа раздела на диск
//*****************************************
void MainWindow::Menu_Part_Extract() {
  
int np=partlist->currentRow();
QString filename;
QString str;
FILE* out;

filename.sprintf("%02i-%08x-%s.bin",np,ptable->code(np),ptable->name(np));
filename=QFileDialog::getSaveFileName(this,"Имя извлекаемого файла",filename,"image (*.bin);;All files (*.*)");
if (filename.isEmpty()) return;
// qDebug()  << filename;
out=fopen(filename.toLocal8Bit().data(),"w");
if (out == 0) {
  QMessageBox::critical(0,"Ошибка","Ошибка открытия файла");
  return;
}
fwrite(ptable->iptr(np),1,ptable->psize(np),out);
fclose(out);
}


//*****************************************
//*  Замена образа раздела 
//*****************************************
void MainWindow::Menu_Part_Replace() {

int np=partlist->currentRow();
QString filename;
QString str;
char fileselector[100];
FILE* in;
// Выбираем подходящие расширения файлов
enum parttypes ptype=ptable->ptype(np);
printf("\n ptype = %i",ptype);
switch (ptype) {
  case part_bin:
    strcpy(fileselector,"image (*.bin)");
    break;
    
  case part_cpio:
    strcpy(fileselector,"CPIO archive (*.cpio)");
    break;
      
  case part_nvram:
    strcpy(fileselector,"NVDLOAD image (*.nvd)");
    break;

  case part_iso:
    strcpy(fileselector,"ISO image (*.iso)");
    break;

  case part_ptable:
    strcpy(fileselector,"Partition table (*.ptable)");
    break;
}
strcat(fileselector,";;All files (*.*)");
      

filename.sprintf("%02i-%08x-%s.bin",np,ptable->code(np),ptable->name(np));
filename=QFileDialog::getOpenFileName(this,"Имя файла с образом раздела",filename,fileselector);
if (filename.isEmpty()) return;
in=fopen(filename.toLocal8Bit(),"r");
if (in == 0) {
  QMessageBox::critical(0,"Ошибка","Ошибка открытия файла");
  printf("\n file %s",filename.toLocal8Bit().data());
  return;
}
ptable->loadimage(np,in);
SelectPart();
  
}

//*****************************************
//*  Удаление раздела 
//*****************************************
void MainWindow::Menu_Part_Delete() {

int32_t ci=partlist->currentRow(); 

if (ptable->index() == 1) return; // последний раздел удалять нельзя
ptable->delpart(ci);
regenerate_partlist();
if (ci< (ptable->index()-1)) partlist->setCurrentRow(ci);
else partlist->setCurrentRow(ptable->index()-1);
SelectPart();  
}


//*****************************************
//*  Перемещение раздела вверх 
//*****************************************
void MainWindow::Menu_Part_MoveUp() {
  
int32_t ci=partlist->currentRow(); 
  
ptable->moveup(ci);
regenerate_partlist();
if (ci>0) partlist->setCurrentRow(ci-1);
else partlist->setCurrentRow(0);
}

//*****************************************
//*  Перемещение раздела вниз
//*****************************************
void MainWindow::Menu_Part_MoveDown() {

int32_t ci=partlist->currentRow(); 
  
ptable->movedown(ci);
regenerate_partlist();
if (ci<(ptable->index()-1)) partlist->setCurrentRow(ci+1);
else partlist->setCurrentRow(ptable->index()-1);
}


//********************************************
// Разрешение редактирования полей заголовка
//********************************************
void MainWindow::Menu_Part_EditHeader() {
  
  
Platform_input->setReadOnly(0);
Date_input->setReadOnly(0);
Time_input->setReadOnly(0);
Version_input->setReadOnly(0);
}


//********************************************
//* Установка текущей даты изменения раздела
//********************************************
void MainWindow::set_date() {

QString str;
time_t actime=time(0);
struct tm* mtime=localtime(&actime);

str.sprintf("%4i.%02i.%02i",mtime->tm_year+1900,mtime->tm_mon+1,mtime->tm_mday);
Date_input->setText(str);
Date_input->setModified(true);

str.sprintf("%02i:%02i:%02i",mtime->tm_hour,mtime->tm_min,mtime->tm_sec);
Time_input->setText(str);
Time_input->setModified(true);
}


//********************************************
//* Запись областей редактирования заголовка
//********************************************
void MainWindow::HeaderChanged() {

int32_t ci=partlist->currentRow(); 
uint32_t code;

if (Platform_input->isModified())  strncpy((char*)ptable->hptr(ci)->unlock,Platform_input->text().toLocal8Bit(),8);
if (Date_input->isModified())  strncpy((char*)ptable->hptr(ci)->date,Date_input->text().toLocal8Bit(),16);
if (Time_input->isModified())  strncpy((char*)ptable->hptr(ci)->time,Time_input->text().toLocal8Bit(),16);
if (Version_input->isModified())  strncpy((char*)ptable->hptr(ci)->version,Version_input->text().toLocal8Bit(),32);
if (pcode -> isModified()) {
  sscanf(pcode->text().toLocal8Bit(),"%x",&code);
  ptable->hptr(ci)->code=code<<16;
  printf("\n code=%x",ptable->hptr(ci)->code);
}
ptable->calc_hd_crc16(ci);
}

//********************************************
// Запрещение редактирования полей заголовка
//********************************************
void MainWindow::Disable_EditHeader() {
  
  
Platform_input->setReadOnly(0);
Date_input->setReadOnly(0);
Time_input->setReadOnly(0);
Version_input->setReadOnly(0);
}

//********************************************
// Запуск диалога прошивальщика
//********************************************
void MainWindow::Start_Flasher() {

if (PortSelector->count() == 0) {
   QMessageBox::critical(0,"Ошибка","Не найдены последовательне порты");
   return;
}
  
flasher* fl=new flasher;
fl->show();
}

//********************************************
//  Перезагрузка модема
//********************************************
void MainWindow::Reboot_modem() {

if (PortSelector->count() == 0) {
   QMessageBox::critical(0,"Ошибка","Не найдены последовательне порты");
   return;
}
open_port();
modem_reboot();  
close_port();
QMessageBox::information(0,"ОK","Команда перезагрузки передана в модем");
}


//********************************************
// Запуск udb-загрузчика
//********************************************
void MainWindow::usbdload() {

if (PortSelector->count() == 0) {
   QMessageBox::critical(0,"Ошибка","Не найдены последовательне порты");
   return;
}
  
usbloader* ul=new usbloader;
ul->show();
}


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@222

int main(int argc, char* argv[]) {
  
QApplication app(argc, argv);

QCoreApplication::setApplicationName("Qt linux huawei flasher");
QCoreApplication::setApplicationVersion("1.0");


QCommandLineParser parser;
    parser.setApplicationDescription("Программа для прошивки и восстановления устройств на чипсете Hisilicon Balong v7");
    parser.addHelpOption();
    parser.addPositionalArgument("firmware", QCoreApplication::translate("main", "Файл прошивки"));

parser.process(app);    
QStringList args = parser.positionalArguments();
    
if (args.size() > 0) fwfilename=&args[0];
else fwfilename=0;

MainWindow* mw = new  MainWindow(0);
mw->show();
return app.exec();
}
