// Qt-free test for the BSD-socket ScpiProxy. Uses raw TCP sockets.
#include "../ScpiProxy.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using test_socket_t = SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using test_socket_t = int;
#endif

namespace {
bool waitUntil(const std::function<bool()> &condition, int timeoutMs = 1000) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  while (!condition() && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return condition();
}

bool expect(bool condition, const char *message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

void CloseTestSocket(test_socket_t fd) {
#ifdef _WIN32
  closesocket(fd);
#else
  ::close(fd);
#endif
}

bool SendAll(test_socket_t fd, const std::string &data) {
  std::size_t done = 0;
  while (done < data.size()) {
#ifdef _WIN32
    int n = send(fd, data.data() + done, static_cast<int>(data.size() - done), 0);
#else
    ssize_t n = send(fd, data.data() + done, data.size() - done, 0);
#endif
    if (n <= 0) {
      return false;
    }
    done += static_cast<std::size_t>(n);
  }
  return true;
}

// Read until delim appears or timeout; appends to buf. Returns bytes in buf
// that are new since the call is not tracked - caller inspects buf.
bool WaitForBytes(test_socket_t fd, std::string &buf, std::size_t want, int timeoutMs = 1000) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  while (buf.size() < want && std::chrono::steady_clock::now() < deadline) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    timeval tv{0, 10000};
    if (select(static_cast<int>(fd) + 1, &rfds, nullptr, nullptr, &tv) <= 0) {
      continue;
    }
    char tmp[256];
#ifdef _WIN32
    int n = recv(fd, tmp, sizeof(tmp), 0);
#else
    ssize_t n = recv(fd, tmp, sizeof(tmp), 0);
#endif
    if (n <= 0) {
      break; // closed or error
    }
    buf.append(tmp, static_cast<std::size_t>(n));
  }
  return buf.size() >= want;
}

bool WaitClosed(test_socket_t fd, int timeoutMs = 1000) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  char tmp[64];
  for (;;) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    timeval tv{0, 10000};
    if (select(static_cast<int>(fd) + 1, &rfds, nullptr, nullptr, &tv) > 0) {
#ifdef _WIN32
      int n = recv(fd, tmp, sizeof(tmp), 0);
#else
      ssize_t n = recv(fd, tmp, sizeof(tmp), 0);
#endif
      if (n <= 0) {
        return true;
      }
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      return false;
    }
  }
}

std::size_t CountLines(const std::string &s) {
  return static_cast<std::size_t>(std::count(s.begin(), s.end(), '\n'));
}
} // namespace

int main() {
#ifdef _WIN32
  WSADATA wsa{};
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
    std::cerr << "WSAStartup failed\n";
    return 1;
  }
#endif
  std::string forwarded;
  ScpiProxy proxy([&forwarded](const std::string &command) {
    forwarded += command + '\n';
    if (command == "READ?") {
      return std::string("1.230E-3");
    }
    return std::string("{\"connected\":true}");
  });
  if (!expect(proxy.start(0), "Could not start SCPI proxy")) {
    return 1;
  }

  auto connectClient = [&]() -> test_socket_t {
#ifdef _WIN32
    test_socket_t fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == INVALID_SOCKET) {
      return INVALID_SOCKET;
    }
#else
    test_socket_t fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
      return -1;
    }
#endif
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(proxy.serverPort());
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
      CloseTestSocket(fd);
#ifdef _WIN32
      return INVALID_SOCKET;
#else
      return -1;
#endif
    }
    return fd;
  };

  test_socket_t first = connectClient();
  test_socket_t second = connectClient();
#ifdef _WIN32
  const bool connected = first != INVALID_SOCKET && second != INVALID_SOCKET;
#else
  const bool connected = first >= 0 && second >= 0;
#endif
  if (!expect(connected, "Clients did not connect")) {
    return 1;
  }

  std::string secondBuf;
  SendAll(second, "PROX:STATE?\r\n");
  if (!expect(WaitForBytes(second, secondBuf, 1), "Second client got no data") ||
      !expect(secondBuf == "{\"connected\":true}\n", "Status response was incorrect") ||
      !expect(forwarded == "PROX:STATE?\n", "A partial command was forwarded")) {
    return 1;
  }

  std::string firstBuf;
  SendAll(first, "rea");
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  SendAll(first, "d?\nCONF:VOLT:DC 50\n");
  if (!expect(waitUntil([&] {
                WaitForBytes(first, firstBuf, firstBuf.size() + 1, 50);
                return CountLines(firstBuf) >= 2;
              }),
              "First client did not receive both responses")) {
    return 1;
  }
  if (!expect(firstBuf == "1.230E-3\nERR:UNSUPPORTED\n",
              "Measurement or unsupported-command response was incorrect") ||
      !expect(forwarded == "PROX:STATE?\nREAD?\n",
              "A write command reached the meter handler")) {
    return 1;
  }

  SendAll(first, std::string(257, 'A'));
  if (!expect(WaitClosed(first), "Overlong command did not close the connection")) {
    return 1;
  }

  CloseTestSocket(first);
  CloseTestSocket(second);
  proxy.stop();
#ifdef _WIN32
  WSACleanup();
#endif
  std::cout << "scpi_proxy_test passed\n";
  return 0;
}
