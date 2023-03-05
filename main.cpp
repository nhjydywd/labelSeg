#include "mainwindow.h"

#include <QApplication>
#include <QTranslator>
#include <QSettings>
#include <QDebug>

QTranslator trans;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.installTranslator(&trans);
    MainWindow w;

    QSettings setting;
    QString curLang = setting.value("Language","CN").toString();
    if(curLang == "EN"){
        w.changeLanguage("English");
    }else{
        w.changeLanguage("中文");
    }





    w.show();



    return a.exec();
}
