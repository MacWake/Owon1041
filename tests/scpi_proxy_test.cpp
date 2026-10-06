#include "../ScpiProxy.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QTcpSocket>
#include <QThread>
#include <functional>
#include <iostream>

namespace {
bool waitUntil(const std::function<bool()> &condition, int timeoutMs = 1000) {
  QElapsedTimer timer;
  timer.start();
  while (!condition() && timer.elapsed() < timeoutMs) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    QThread::msleep(1);
  }
  return condition();
}

bool expect(bool condition, const char *message) {
  if (!condition)
    std::cerr << message << '\n';
  return condition;
}
} // namespace

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  QByteArray forwarded;
  ScpiProxy proxy([&forwarded](const QByteArray &command) {
    forwarded += command + '\n';
    if (command == "READ?")
      return QByteArray("1.230E-3");
    return QByteArray("{\"connected\":true}");
  });
  if (!expect(proxy.start(0), "Could not start SCPI proxy"))
    return 1;

  QTcpSocket first;
  QTcpSocket second;
  first.connectToHost(QHostAddress::LocalHost, proxy.serverPort());
  second.connectToHost(QHostAddress::LocalHost, proxy.serverPort());
  if (!expect(waitUntil([&] {
                return first.state() == QAbstractSocket::ConnectedState &&
                       second.state() == QAbstractSocket::ConnectedState;
              }),
              "Clients did not connect"))
    return 1;

  first.write("rea");
  second.write("PROX:STATE?\r\n");
  if (!expect(waitUntil([&] { return second.canReadLine(); }),
              "Second client did not receive a status response"))
    return 1;
  if (!expect(second.readLine() == "{\"connected\":true}\n",
              "Status response was incorrect") ||
      !expect(forwarded == "PROX:STATE?\n",
              "A partial command was forwarded"))
    return 1;

  first.write("d?\nCONF:VOLT:DC 50\n");
  if (!expect(waitUntil([&] {
                return first.peek(first.bytesAvailable()).count('\n') >= 2;
              }),
              "First client did not receive both responses"))
    return 1;
  if (!expect(first.readAll() == "1.230E-3\nERR:UNSUPPORTED\n",
              "Measurement or unsupported-command response was incorrect") ||
      !expect(forwarded == "PROX:STATE?\nREAD?\n",
              "A write command reached the meter handler"))
    return 1;

  first.write(QByteArray(257, 'A'));
  if (!expect(waitUntil([&] { return first.state() == QAbstractSocket::UnconnectedState; }),
              "Overlong command did not close the connection"))
    return 1;
  return 0;
}
