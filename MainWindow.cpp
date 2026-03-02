#include "MainWindow.h"

#include "ConnectDialog.h"
#include <QDebug>
#include <QMouseEvent>
#include <QThread>
#include <QTimer>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>
#include <iostream>

Settings *MainWindow::settings = nullptr;

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) { // NOLINT(*-pro-type-member-init)
  if (!MainWindow::settings) {
    MainWindow::settings = new Settings("MacWake", "Owon1041", this);
    MainWindow::settings->load();
  }

  if (MainWindow::settings->windowWidth() > 0 &&
      MainWindow::settings->windowHeight() > 0) {
    setGeometry(MainWindow::settings->windowX(),
                MainWindow::settings->windowY(),
                MainWindow::settings->windowWidth(),
                MainWindow::settings->windowHeight());
  }

  setupUi(this);

  QTimer::singleShot(2000, this, &MainWindow::connectSerial);
}

MainWindow::~MainWindow() {
  // No need to delete UI elements as they are deleted when parent is deleted
  if (m_port) {
    if (m_port->isOpen()) {
      m_port->close();
    }
    delete m_port;
    m_port = nullptr;
  }

  if (m_timer) {
    m_timer->stop();
    delete m_timer;
    m_timer = nullptr;
  }
}

void MainWindow::setupUi(QMainWindow *MainWindow) {
  if (MainWindow->objectName().isEmpty())
    MainWindow->setObjectName("MainWindow");
  setGeometry(MainWindow::settings->windowX(), MainWindow::settings->windowY(),
              MainWindow::settings->windowWidth(), MainWindow::settings->windowHeight());
  MainWindow->setMinimumSize(QSize(580, 162));
  MainWindow->setWindowTitle("MacWake OWON XDM-1041 v" APP_VERSION_STRING);

  // ReSharper disable once CppDFAMemoryLeak
  const auto centralwidget = new QWidget(MainWindow);
  centralwidget->setObjectName("centralwidget");

  measurement = new QLabel("not connected", centralwidget);
  measurement->setObjectName("measurement");

  QFont font;
  font.setStyleHint(QFont::Monospace);
  font.setFamily("monospace");
  font.setFixedPitch(true);
  font.setPointSize(72);

#if defined(Q_OS_WIN)
  font.setFamily("Consolas");
#elif defined(Q_OS_MAC)
  font.setFamily("Menlo");
#else
  font.setFamily("Liberation Mono");
#endif

  measurement->setFont(font);

  measurement->setFrameShape(QFrame::StyledPanel);
  measurement->setFrameShadow(QFrame::Raised);
  measurement->setTextFormat(Qt::PlainText);
  measurement->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  measurement->setMargin(0);
  measurement->setContentsMargins(0, 0, 0, 0);
  measurement->setStyleSheet("QLabel { padding: 0px; margin: 0px; }");

  measurement->installEventFilter(this);

  measurement->setMouseTracking(true);
  measurement->setAttribute(Qt::WA_Hover, true);
  measurement->setFocusPolicy(Qt::StrongFocus);

  btn_50_v = new QPushButton("50 V", centralwidget);
  btn_50_v->setObjectName("btn_50_v");

  btn_auto_v = new QPushButton("Auto V", centralwidget);
  btn_auto_v->setObjectName("btn_auto_v");

  btn_short = new QPushButton("Short", centralwidget);
  btn_short->setObjectName("btn_short");

  btn_diode = new QPushButton("Diode", centralwidget);
  btn_diode->setObjectName("btn_diode");

  btn_50_kr = new QPushButton("50 kΩ", centralwidget);
  btn_50_kr->setObjectName("btn_50_kr");

  btn_auto_r = new QPushButton("Auto Ω", centralwidget);
  btn_auto_r->setObjectName("btn_auto_r");

  btn_50_f = new QPushButton("50 µF", centralwidget);
  btn_50_f->setObjectName("btn_50_f");

  btn_auto_f = new QPushButton("Auto F", centralwidget);
  btn_auto_f->setObjectName("btn_auto_f");

  btn_freq = new QPushButton("Hz", centralwidget);
  btn_freq->setObjectName("btn_freq");

  btn_period = new QPushButton("Period", centralwidget);
  btn_period->setObjectName("btn_period");

  connect(btn_50_v, &QPushButton::clicked, this, &MainWindow::onVoltage50V);
  connect(btn_auto_v, &QPushButton::clicked, this, &MainWindow::onVoltageAuto);
  connect(btn_short, &QPushButton::clicked, this, &MainWindow::onShort);
  connect(btn_diode, &QPushButton::clicked, this, &MainWindow::onDiode);
  connect(btn_50_kr, &QPushButton::clicked, this, &MainWindow::onResistance50K);
  connect(btn_auto_r, &QPushButton::clicked, this,
          &MainWindow::onResistanceAuto);
  connect(btn_50_f, &QPushButton::clicked, this,
          &MainWindow::onCapacitance50uF);
  connect(btn_auto_f, &QPushButton::clicked, this,
          &MainWindow::onCapacitanceAuto);
  connect(btn_freq, &QPushButton::clicked, this, &MainWindow::onFrequency);
  connect(btn_period, &QPushButton::clicked, this, &MainWindow::onPeriod);

  setupPositions(MainWindow->width(), MainWindow->height());
  MainWindow->setCentralWidget(centralwidget);

  m_connect_dialog = new ConnectDialog(this);

  this->m_timer = new QTimer(this);
  this->m_timer->setInterval(100);
  this->m_timer->setSingleShot(false);
  connect(this->m_timer, &QTimer::timeout, this,
          &MainWindow::updateMeasurement);
}

void MainWindow::resizeEvent(QResizeEvent *event) {
  QMainWindow::resizeEvent(event);

  MainWindow::settings->setWindowWidth(event->size().width());
  MainWindow::settings->setWindowHeight(event->size().height());
  setupPositions(event->size().width(), event->size().height());
}

void MainWindow::setupPositions(const int width, const int height) const {
  const int btn_width = 70;
  const int btn_height = 32;
  const int btnbar_w = 350;
  const int btn_x = (width - btnbar_w) / 2;
  //std::cerr << "w=" << width << " h=" << height << std::endl;

  auto measureHeight = measurement->fontMetrics().height();
  // max width
  measurement->setGeometry(QRect(2, 0, width - 4, measureHeight));
  const int btngroup_y1 = measurement->y() + measurement->height() + 2;
  const int btngroup_y2 = btngroup_y1 + btn_height + 1;

  btn_50_v->setGeometry(QRect(btn_x, btngroup_y1, btn_width, btn_height));
  btn_auto_v->setGeometry(QRect(btn_x, btngroup_y2, btn_width, btn_height));
  btn_short->setGeometry(QRect(btn_x + 70, btngroup_y1, btn_width, btn_height));
  btn_diode->setGeometry(QRect(btn_x + 70, btngroup_y2, btn_width, btn_height));
  btn_50_kr->setGeometry(
      QRect(btn_x + 140, btngroup_y1, btn_width, btn_height));
  btn_auto_r->setGeometry(
      QRect(btn_x + 140, btngroup_y2, btn_width, btn_height));
  btn_50_f->setGeometry(QRect(btn_x + 210, btngroup_y1, btn_width, btn_height));
  btn_auto_f->setGeometry(
      QRect(btn_x + 210, btngroup_y2, btn_width, btn_height));
  btn_freq->setGeometry(QRect(btn_x + 280, btngroup_y1, btn_width, btn_height));
  btn_period->setGeometry(
      QRect(btn_x + 280, btngroup_y2, btn_width, btn_height));
}

void MainWindow::connectSerial() {
  std::cerr << "Connecting to serial port (auto)" << std::endl;
  if (MainWindow::settings->device().isEmpty()) {
    this->openConnectDialog();
  } else {
    if (!this->m_connect_dialog->tryPortByName(
            MainWindow::settings->device())) {
      std::cerr << "Could not connect to serial port "
                << MainWindow::settings->device().toStdString() << std::endl;
    } else {
      this->m_port = m_connect_dialog->getConfiguredSerialPort();
      this->onConnect();
    }
  }
}

QString MainWindow::rateToSerial(Settings::Rate rate) {
  switch (rate) {
  case Settings::Rate::SLOW:
    return "S";
  case Settings::Rate::MEDIUM:
    return "M";
  case Settings::Rate::FAST:
    return "F";
  }
  return "F";
}

void MainWindow::onConnect() {
  this->writeSCPIStatement(
      QString("RATE " + rateToSerial(settings->getRate())));

  this->writeSCPIStatement("SYST:BEEP:STAT OFF");
  this->onVoltage50V();

  this->m_timer->start();
}

bool MainWindow::openConnectDialog() {
  if (m_connect_dialog->exec() == QDialog::Accepted) {
    const auto serialPort = m_connect_dialog->getConfiguredSerialPort();
    if (serialPort) {
      MainWindow::settings->setDevice(serialPort->portName());
      return true;
    }
    this->m_port = serialPort;
  }
  return false;
}

void MainWindow::updateMeasurement() {
  if (!this->m_port) {
    std::cerr << "Port is NULL, stopping timer" << std::endl;
    this->m_timer->stop();
    this->m_timer->deleteLater();
    this->m_timer = nullptr;
    return;
  }
  auto reading = this->writeSCPICommand("MEAS1:SHOW?");

  QString display = reading.replace("\u00a6\u00b8", "Ω Ohm")
                        .replace("\u00aa\u00cc", "µ")
                        .replace("\u00a1\u00e6", "°C")
                        .replace("\u00a8\u0048", "°F");
  this->measurement->setText(display);
}

void MainWindow::onVoltage50V() {
  this->m_unit = "V";
  this->writeSCPIStatement("CONF:VOLT:DC 50");
}

void MainWindow::onVoltageAuto() {
  this->m_unit = "V";
  this->writeSCPIStatement("CONF:VOLT:DC AUTO");
}

void MainWindow::onShort() {
  this->m_unit = "Ω";
  this->writeSCPIStatement("CONF:CONT");
  if (settings->getBeepShort()) {
    qDebug() << "Beep resistance: " << MainWindow::settings->getBeepResistance();
    this->writeSCPIStatement(QString("CONT:THRE ") +
                        QString::number(MainWindow::settings->getBeepResistance()));
    this->writeSCPIStatement("SYST:BEEP:STAT ON");
  } else {
    this->writeSCPIStatement("SYST:BEEP:STAT OFF");
  }
}

void MainWindow::onDiode() {
  if (MainWindow::settings->getBeepDiode()) {
    this->writeSCPIStatement("SYST:BEEP:STAT ON");
  } else {
    this->writeSCPIStatement("SYST:BEEP:STAT OFF");
  }
  this->writeSCPIStatement("CONF:DIOD");
}

void MainWindow::onResistance50K() {
  this->writeSCPIStatement("CONF:RES 50E3");
}

void MainWindow::onResistanceAuto() {
  this->m_unit = "Ω";
  this->writeSCPIStatement("CONF:RES AUTO");
}

void MainWindow::onCapacitance50uF() {
  this->m_unit = "F";
  this->writeSCPIStatement("CONF:CAP 50E-6");
}

void MainWindow::onCapacitanceAuto() {
  this->m_unit = "F";
  this->writeSCPIStatement("CONF:CAP AUTO");
}

void MainWindow::onFrequency() {
  this->m_unit = "Hz";
  this->writeSCPIStatement("CONF:FREQ");
}

void MainWindow::onPeriod() {
  this->m_unit = "%";
  this->writeSCPIStatement("CONF:PER");
}

void MainWindow::onSerialError(const QString &message) {
  if (this->m_timer) {
    this->m_timer->stop();
    qDebug() << "Stopping timer";
  }
  qDebug() << "Serial port error: " << message;
  std::cerr << "Serial port error, closing\n";
  if (this->m_port) {
    if (this->m_port->isOpen()) {
      this->m_port->close();
    }
    delete this->m_port;
    this->m_port = nullptr;
  }
}

QString MainWindow::readSCPI() const {
  if (!m_port || !m_port->isOpen()) {
    qDebug() << "Serial port not open";
    return {};
  }

  if (!m_port->waitForReadyRead(500)) {
    qDebug() << "Serial port not ready";
    return {};
  }

  QByteArray responseData;
  QElapsedTimer timer;
  timer.start();

  while (!responseData.contains('\n')) {
    if (timer.elapsed() > 100) {
      qDebug() << "Read timeout occurred";
      // emit onSerialError("readSCPI timeout");
      return {};
    }

    if (m_port->bytesAvailable() > 0) {
      responseData.append(m_port->readAll());
    } else {
      QThread::msleep(10);
    }
  }

  QString response = QString::fromLatin1(responseData);
  QByteArray data = response.toLatin1();

  if (data.contains(QByteArray("\xa6\xb8", 2))) {
    data.replace(QByteArray("\xa6\xb8", 2), "Ω");
  }
  if (data.contains(QByteArray("\xa6\xcc", 2))) {
    data.replace(QByteArray("\xa6\xcc", 2), "µ");
  }
  if (data.contains(QByteArray("\xa1\xe6", 2))) {
    data.replace(QByteArray("\xa1\xe6", 2), "°C");
  }
  if (data.contains(QByteArray("\xa8\x48", 2))) {
    data.replace(QByteArray("\xa8\x48", 2), "°F");
  }
  response = QString::fromUtf8(data);
  QRegularExpression re("([-+]?[0-9]*\\.?[0-9]+)([^0-9.]+)");
  response = response.replace(re, "\\1 \\2");

  response = response.replace("  ", " ");

  return response;
}

void MainWindow::writeSCPIStatement(const QString &command) const {
  if (!this->m_port) {
    std::cerr << "No port open, refusing writeSCPI\n";
    return;
  }
  // qDebug() << "Writing " << command;
  this->m_port->write(QString(command + "\r\n").toLocal8Bit());
  this->m_port->flush();
  QThread::msleep(10);
}

QString MainWindow::writeSCPICommand(const QString &command) const {
  if (!this->m_port) {
    std::cerr << "No port open, refusing writeSCPI\n";
    return nullptr;
  }
  // qDebug() << "Writing " << command;
  this->m_port->write(QString(command + "\r\n").toLocal8Bit());
  this->m_port->flush();
  QThread::msleep(10);
  return readSCPI();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
  if (obj == measurement) {
    if (event->type() == QEvent::MouseButtonRelease) {
      if (const QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent *>(event);
          mouseEvent->button() == Qt::LeftButton) {
        onMeasurementClicked();
        return true;
      }
    }
  }
  return QMainWindow::eventFilter(obj, event);
}

void MainWindow::onMeasurementClicked() {
  MainWindow::settings->save();
  openConnectDialog();
}