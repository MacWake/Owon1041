#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
using socket_t = SOCKET;
#else
using socket_t = int;
#endif

// Loopback-only, line-oriented SCPI query endpoint. The handler runs on
// worker threads; it must be thread-safe (MainWindow serializes meter access
// with a mutex because the GUI timer shares the port).
class ScpiProxy final {
public:
  using QueryHandler = std::function<std::string(const std::string &)>;

  explicit ScpiProxy(QueryHandler handler);
  ~ScpiProxy();

  ScpiProxy(const ScpiProxy &) = delete;
  ScpiProxy &operator=(const ScpiProxy &) = delete;

  bool start(std::uint16_t port = 5025);
  void stop();
  std::string errorString() const { return m_error; }
  std::uint16_t serverPort() const { return m_port; }

private:
  void acceptLoop();
  void serveClient(socket_t fd);
  void closeListenSocket();

  QueryHandler m_handler;
  std::thread m_acceptThread;
  std::mutex m_mutex;
  std::vector<std::thread> m_clientThreads;
  std::vector<socket_t> m_clientFds;
  std::atomic<bool> m_running{false};
  socket_t m_listenFd;
  std::string m_error;
  std::uint16_t m_port = 0;
};
