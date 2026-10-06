#ifndef SCPIPROXY_H
#define SCPIPROXY_H

#include <QHash>
#include <QObject>
#include <QTcpServer>
#include <functional>

class QTcpSocket;

// A loopback-only, line-oriented SCPI query endpoint. The handler runs on the
// GUI thread, which also owns the serial port, so replies cannot interleave.
class ScpiProxy final : public QObject {
public:
  using QueryHandler = std::function<QByteArray(const QByteArray &)>;

  explicit ScpiProxy(QueryHandler handler, QObject *parent = nullptr);

  bool start(quint16 port = 5025);
  QString errorString() const;
  quint16 serverPort() const;

private:
  void acceptConnections();
  void readClient(QTcpSocket *socket);
  void handleLine(QTcpSocket *socket, const QByteArray &line);

  QTcpServer m_server;
  QHash<QTcpSocket *, QByteArray> m_buffers;
  QueryHandler m_handler;
};

#endif // SCPIPROXY_H
