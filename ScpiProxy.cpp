#include "ScpiProxy.h"

#include <QHostAddress>
#include <QSet>
#include <QTcpSocket>
#include <utility>

namespace {
constexpr qsizetype maxCommandLength = 256;
constexpr qsizetype maxClients = 8;

bool isAllowedQuery(const QByteArray &command) {
  static const QSet<QByteArray> allowed = {
      "*IDN?",         "MEAS?",        "MEAS1?",       "MEAS2?",
      "MEAS:SHOW?",   "MEAS1:SHOW?",  "MEAS2:SHOW?", "READ?",
      "PROX:READ?",   "PROX:STATE?",
  };
  return allowed.contains(command);
}
} // namespace

ScpiProxy::ScpiProxy(QueryHandler handler, QObject *parent)
    : QObject(parent), m_server(this), m_handler(std::move(handler)) {
  connect(&m_server, &QTcpServer::newConnection, this,
          [this] { acceptConnections(); });
}

bool ScpiProxy::start(quint16 port) {
  return m_server.listen(QHostAddress::LocalHost, port);
}

QString ScpiProxy::errorString() const { return m_server.errorString(); }

quint16 ScpiProxy::serverPort() const { return m_server.serverPort(); }

void ScpiProxy::acceptConnections() {
  while (m_server.hasPendingConnections()) {
    QTcpSocket *socket = m_server.nextPendingConnection();
    if (m_buffers.size() >= maxClients) {
      socket->disconnectFromHost();
      socket->deleteLater();
      continue;
    }
    m_buffers.insert(socket, {});
    connect(socket, &QTcpSocket::readyRead, this,
            [this, socket] { readClient(socket); });
    connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
      m_buffers.remove(socket);
      socket->deleteLater();
    });
  }
}

void ScpiProxy::readClient(QTcpSocket *socket) {
  auto &buffer = m_buffers[socket];
  buffer += socket->readAll();
  qsizetype newline;
  while ((newline = buffer.indexOf('\n')) >= 0) {
    const QByteArray line = buffer.left(newline);
    buffer.remove(0, newline + 1);
    handleLine(socket, line);
  }
  if (buffer.size() > maxCommandLength) {
    socket->write("ERR:COMMAND_TOO_LONG\n");
    socket->disconnectFromHost();
  }
}

void ScpiProxy::handleLine(QTcpSocket *socket, const QByteArray &line) {
  if (line.size() > maxCommandLength) {
    socket->write("ERR:COMMAND_TOO_LONG\n");
    return;
  }
  const QByteArray command = line.trimmed().toUpper();
  if (command.isEmpty())
    return;
  if (!isAllowedQuery(command)) {
    socket->write("ERR:UNSUPPORTED\n");
    return;
  }
  QByteArray response = m_handler(command);
  if (response.isEmpty())
    response = "ERR:NO_RESPONSE";
  response.replace('\r', ' ').replace('\n', ' ');
  socket->write(response + '\n');
}
