#include "ScpiProxy.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <set>

#ifdef _WIN32
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {
constexpr std::size_t maxCommandLength = 256;
constexpr std::size_t maxClients = 8;

bool IsAllowedQuery(const std::string &command) {
  static const std::set<std::string> allowed = {
      "*IDN?",      "MEAS?",       "MEAS1?",      "MEAS2?",     "MEAS:SHOW?",
      "MEAS1:SHOW?", "MEAS2:SHOW?", "READ?",       "PROX:READ?", "PROX:STATE?",
  };
  return allowed.count(command) != 0;
}

std::string ToUpper(std::string s) {
  for (auto &c : s) {
    c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
  }
  return s;
}

std::string TrimRight(const std::string &s) {
  auto e = s.find_last_not_of(" \t\r\n");
  return e == std::string::npos ? "" : s.substr(0, e + 1);
}

void CloseSocket(socket_t fd) {
#ifdef _WIN32
  closesocket(fd);
#else
  ::close(fd);
#endif
}

void ShutdownSocket(socket_t fd) {
#ifdef _WIN32
  shutdown(fd, SD_BOTH);
#else
  shutdown(fd, SHUT_RDWR);
#endif
}

bool SendAll(socket_t fd, const std::string &data) {
  std::size_t done = 0;
  while (done < data.size()) {
#ifdef _WIN32
    int n = send(fd, data.data() + done, static_cast<int>(data.size() - done), 0);
#elif defined(__APPLE__)
    // macOS has no MSG_NOSIGNAL; SO_NOSIGPIPE is set on our sockets instead.
    ssize_t n = send(fd, data.data() + done, data.size() - done, 0);
#else
    ssize_t n = send(fd, data.data() + done, data.size() - done, MSG_NOSIGNAL);
#endif
    if (n <= 0) {
      return false;
    }
    done += static_cast<std::size_t>(n);
  }
  return true;
}
} // namespace

ScpiProxy::ScpiProxy(QueryHandler handler)
    : m_handler(std::move(handler)),
#ifdef _WIN32
      m_listenFd(INVALID_SOCKET)
#else
      m_listenFd(-1)
#endif
{
}

ScpiProxy::~ScpiProxy() { stop(); }

bool ScpiProxy::start(std::uint16_t port) {
#ifdef _WIN32
  WSADATA wsa{};
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
    m_error = "WSAStartup failed";
    return false;
  }
  m_listenFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (m_listenFd == INVALID_SOCKET) {
    m_error = "Could not create socket";
    WSACleanup();
    return false;
  }
#else
  m_listenFd = socket(AF_INET, SOCK_STREAM, 0);
  if (m_listenFd < 0) {
    m_error = "Could not create socket";
    return false;
  }
#endif
  int reuse = 1;
#ifdef _WIN32
  setsockopt(m_listenFd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&reuse),
             sizeof(reuse));
#else
  setsockopt(m_listenFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
#ifdef __APPLE__
  // No MSG_NOSIGNAL on macOS: suppress SIGPIPE per-socket instead.
  {
    int one = 1;
    setsockopt(m_listenFd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
  }
#endif
  if (bind(m_listenFd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0 ||
      listen(m_listenFd, 8) != 0) {
    m_error = "Could not listen on 127.0.0.1";
    closeListenSocket();
    return false;
  }
  sockaddr_in bound{};
  socklen_t len = sizeof(bound);
  if (getsockname(m_listenFd, reinterpret_cast<sockaddr *>(&bound), &len) == 0) {
    m_port = ntohs(bound.sin_port);
  } else {
    m_port = port;
  }
  m_running = true;
  m_acceptThread = std::thread(&ScpiProxy::acceptLoop, this);
  return true;
}

void ScpiProxy::closeListenSocket() {
#ifdef _WIN32
  if (m_listenFd != INVALID_SOCKET) {
    CloseSocket(m_listenFd);
    m_listenFd = INVALID_SOCKET;
  }
#else
  if (m_listenFd >= 0) {
    CloseSocket(m_listenFd);
    m_listenFd = -1;
  }
#endif
}

void ScpiProxy::stop() {
  bool wasRunning = m_running.exchange(false);
  // Unblock a sleeping accept() so the accept thread can join promptly.
  socket_t listenFd;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    listenFd = m_listenFd;
  }
#ifdef _WIN32
  bool haveListen = listenFd != INVALID_SOCKET;
#else
  bool haveListen = listenFd >= 0;
#endif
  if (haveListen) {
    ShutdownSocket(listenFd);
    CloseSocket(listenFd);
  }
  {
    std::lock_guard<std::mutex> lock(m_mutex);
#ifdef _WIN32
    m_listenFd = INVALID_SOCKET;
#else
    m_listenFd = -1;
#endif
    for (socket_t fd : m_clientFds) {
      ShutdownSocket(fd);
    }
  }
  if (m_acceptThread.joinable()) {
    m_acceptThread.join();
  }
  // Closing client sockets unblocks the recv() loops; then join them.
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (socket_t fd : m_clientFds) {
      CloseSocket(fd);
    }
    m_clientFds.clear();
  }
  for (auto &t : m_clientThreads) {
    if (t.joinable()) {
      t.join();
    }
  }
  m_clientThreads.clear();
#ifdef _WIN32
  if (wasRunning) {
    WSACleanup();
  }
#else
  (void)wasRunning;
#endif
}

void ScpiProxy::acceptLoop() {
  for (;;) {
    socket_t listenFd;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      listenFd = m_listenFd;
    }
#ifdef _WIN32
    if (listenFd == INVALID_SOCKET || !m_running) {
      return;
    }
#else
    if (listenFd < 0 || !m_running) {
      return;
    }
#endif
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(listenFd, &rfds);
    timeval tv{0, 100000}; // 100ms poll so stop() joins promptly
    int rc = select(static_cast<int>(listenFd) + 1, &rfds, nullptr, nullptr, &tv);
    if (rc <= 0) {
      continue;
    }
    socket_t fd = accept(listenFd, nullptr, nullptr);
#ifdef _WIN32
    if (fd == INVALID_SOCKET) {
#else
    if (fd < 0) {
#endif
      if (!m_running) {
        return;
      }
      continue;
    }
#ifdef __APPLE__
    {
      int one = 1;
      setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
    }
#endif
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      if (m_clientFds.size() >= maxClients) {
        CloseSocket(fd);
        continue;
      }
      m_clientFds.push_back(fd);
      m_clientThreads.emplace_back(&ScpiProxy::serveClient, this, fd);
    }
  }
}

void ScpiProxy::serveClient(socket_t fd) {
  std::string buffer;
  char chunk[256];
  for (;;) {
#ifdef _WIN32
    int n = recv(fd, chunk, sizeof(chunk), 0);
#else
    ssize_t n = recv(fd, chunk, sizeof(chunk), 0);
#endif
    if (n <= 0) {
      break;
    }
    buffer.append(chunk, static_cast<std::size_t>(n));
    std::string::size_type nl;
    while ((nl = buffer.find('\n')) != std::string::npos) {
      std::string line = TrimRight(buffer.substr(0, nl));
      buffer.erase(0, nl + 1);
      if (line.size() > maxCommandLength) {
        SendAll(fd, "ERR:COMMAND_TOO_LONG\n");
        goto done;
      }
      const std::string command = ToUpper(line);
      if (command.empty()) {
        continue;
      }
      if (!IsAllowedQuery(command)) {
        SendAll(fd, "ERR:UNSUPPORTED\n");
        continue;
      }
      std::string response = m_handler(command);
      if (response.empty()) {
        response = "ERR:NO_RESPONSE";
      }
      std::replace(response.begin(), response.end(), '\r', ' ');
      std::replace(response.begin(), response.end(), '\n', ' ');
      if (!SendAll(fd, response + "\n")) {
        goto done;
      }
    }
    if (buffer.size() > maxCommandLength) {
      SendAll(fd, "ERR:COMMAND_TOO_LONG\n");
      break;
    }
  }
done:
  ShutdownSocket(fd);
  CloseSocket(fd);
  std::lock_guard<std::mutex> lock(m_mutex);
  m_clientFds.erase(std::remove(m_clientFds.begin(), m_clientFds.end(), fd), m_clientFds.end());
}
