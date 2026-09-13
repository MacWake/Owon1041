#include "MainWindow.h"
#include <QApplication>
#include <QIcon>
#include <QPushButton>

int main(int argc, char *argv[]) {
  QApplication a(argc, argv);
  a.setWindowIcon(QIcon(":/app_icon.png"));
  MainWindow mainWindow;
  mainWindow.show();
  return QApplication::exec();
}
