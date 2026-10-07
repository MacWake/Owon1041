#include "SerialPort.h"

#include <algorithm>
#include <chrono>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <glob.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

#ifdef _WIN32

namespace {
std::string WideToUtf8(const wchar_t *w) {
  char buf[512];
  int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, buf, sizeof(buf), nullptr, nullptr);
  return n > 0 ? std::string(buf) : std::string();
}
} // namespace

std::vector<SerialPortInfo> EnumerateSerialPorts() {
  std::vector<SerialPortInfo> ports;
  wchar_t target[512];
  for (int i = 1; i <= 256; ++i) {
    wchar_t name[16];
    swprintf(name, 16, L"COM%d", i);
    if (QueryDosDeviceW(name, target, 512) != 0) {
      char narrow[16];
      snprintf(narrow, sizeof(narrow), "COM%d", i);
      ports.push_back({narrow, "", ""});
    }
  }
  return ports;
}

SerialPort::SerialPort(const std::string &portName) : m_portName(portName) {}
SerialPort::~SerialPort() { close(); }

bool SerialPort::open() {
  if (m_handle) {
    return true;
  }
  std::string dev = "\\\\.\\" + m_portName;
  HANDLE h = CreateFileA(dev.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                         OPEN_EXISTING, 0, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    m_error = "Could not open " + m_portName;
    return false;
  }
  DCB dcb{};
  dcb.DCBlength = sizeof(dcb);
  if (!GetCommState(h, &dcb)) {
    m_error = "GetCommState failed for " + m_portName;
    CloseHandle(h);
    return false;
  }
  dcb.BaudRate = CBR_115200;
  dcb.ByteSize = 8;
  dcb.Parity = NOPARITY;
  dcb.StopBits = ONESTOPBIT;
  dcb.fBinary = TRUE;
  dcb.fParity = FALSE;
  dcb.fOutxCtsFlow = FALSE;
  dcb.fOutxDsrFlow = FALSE;
  dcb.fDtrControl = DTR_CONTROL_ENABLE;
  dcb.fRtsControl = RTS_CONTROL_ENABLE;
  dcb.fInX = FALSE;
  dcb.fOutX = FALSE;
  if (!SetCommState(h, &dcb)) {
    m_error = "SetCommState failed for " + m_portName;
    CloseHandle(h);
    return false;
  }
  COMMTIMEOUTS timeouts{};
  timeouts.ReadIntervalTimeout = 50;
  timeouts.ReadTotalTimeoutMultiplier = 10;
  timeouts.ReadTotalTimeoutConstant = 50;
  timeouts.WriteTotalTimeoutMultiplier = 10;
  timeouts.WriteTotalTimeoutConstant = 500;
  SetCommTimeouts(h, &timeouts);
  SetupComm(h, 4096, 4096);
  m_handle = h;
  return true;
}

void SerialPort::close() {
  if (m_handle) {
    CloseHandle(static_cast<HANDLE>(m_handle));
    m_handle = nullptr;
  }
}

bool SerialPort::isOpen() const { return m_handle != nullptr; }

bool SerialPort::write(const std::string &data) { return write(data.data(), data.size()); }

bool SerialPort::write(const char *data, std::size_t size) {
  if (!m_handle) {
    m_error = "Port not open";
    return false;
  }
  HANDLE h = static_cast<HANDLE>(m_handle);
  std::size_t done = 0;
  while (done < size) {
    DWORD n = 0;
    if (!WriteFile(h, data + done, static_cast<DWORD>(size - done), &n, nullptr) || n == 0) {
      m_error = "Write failed on " + m_portName;
      return false;
    }
    done += n;
  }
  return true;
}

bool SerialPort::waitForBytesWritten(int /*timeoutMs*/) { return isOpen(); }

bool SerialPort::waitForReadyRead(int timeoutMs) {
  if (!m_handle) {
    return false;
  }
  HANDLE h = static_cast<HANDLE>(m_handle);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  for (;;) {
    DWORD errors = 0;
    COMSTAT stat{};
    if (!ClearCommError(h, &errors, &stat)) {
      return false;
    }
    if (stat.cbInQue > 0) {
      return true;
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      return false;
    }
    Sleep(5);
  }
}

std::size_t SerialPort::bytesAvailable() const {
  if (!m_handle) {
    return 0;
  }
  DWORD errors = 0;
  COMSTAT stat{};
  if (!ClearCommError(static_cast<HANDLE>(m_handle), &errors, &stat)) {
    return 0;
  }
  return stat.cbInQue;
}

bool SerialPort::readChunk(std::string &out, int timeoutMs) {
  if (!waitForReadyRead(timeoutMs)) {
    return false;
  }
  HANDLE h = static_cast<HANDLE>(m_handle);
  char buf[256];
  DWORD n = 0;
  if (!ReadFile(h, buf, sizeof(buf), &n, nullptr) || n == 0) {
    return false;
  }
  out.assign(buf, n);
  return true;
}

std::string SerialPort::readAll() {
  std::string out;
  while (bytesAvailable() > 0) {
    std::string chunk;
    if (!readChunk(chunk, 50)) {
      break;
    }
    out += chunk;
  }
  return out;
}

bool SerialPort::readLine(std::string &line, int timeoutMs) {
  line.clear();
  std::string acc;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  for (;;) {
    int remaining =
        static_cast<int>((deadline - std::chrono::steady_clock::now()).count() / 1000000);
    if (remaining <= 0) {
      return !acc.empty() && acc.find('\n') != std::string::npos;
    }
    std::string chunk;
    if (!readChunk(chunk, remaining)) {
      return false;
    }
    acc += chunk;
    auto pos = acc.find('\n');
    if (pos != std::string::npos) {
      line = acc.substr(0, pos);
      while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
        line.pop_back();
      }
      return true;
    }
  }
}

#else // POSIX

namespace {
void GlobInto(const char *pattern, std::vector<std::string> &out) {
  glob_t g{};
  if (glob(pattern, GLOB_NOSORT, nullptr, &g) == 0) {
    for (std::size_t i = 0; i < g.gl_pathc; ++i) {
      out.emplace_back(g.gl_pathv[i]);
    }
  }
  globfree(&g);
}
} // namespace

std::vector<SerialPortInfo> EnumerateSerialPorts() {
  std::vector<std::string> paths;
#if defined(__APPLE__)
  GlobInto("/dev/cu.*", paths);
#else
  GlobInto("/dev/serial/by-id/*", paths);
  if (paths.empty()) {
    GlobInto("/dev/ttyUSB*", paths);
    GlobInto("/dev/ttyACM*", paths);
  }
#endif
  std::sort(paths.begin(), paths.end());
  paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
  std::vector<SerialPortInfo> ports;
  for (const auto &p : paths) {
    std::string base = p.substr(p.find_last_of('/') + 1);
    ports.push_back({p, base, ""});
  }
  return ports;
}

SerialPort::SerialPort(const std::string &portName) : m_portName(portName) {}
SerialPort::~SerialPort() { close(); }

bool SerialPort::open() {
  if (m_fd >= 0) {
    return true;
  }
  int fd = ::open(m_portName.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd < 0) {
    m_error = "Could not open " + m_portName + ": " + std::strerror(errno);
    return false;
  }
  // Clear O_NONBLOCK; blocking I/O is driven by select() timeouts.
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags >= 0) {
    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
  }
  termios tio{};
  if (tcgetattr(fd, &tio) != 0) {
    m_error = "tcgetattr failed for " + m_portName;
    ::close(fd);
    return false;
  }
  cfmakeraw(&tio);
  cfsetspeed(&tio, B115200);
  tio.c_cflag |= CS8 | CLOCAL | CREAD;
  tio.c_cflag &= ~(PARENB | CSTOPB | CRTSCTS);
  tio.c_iflag &= ~(IXON | IXOFF | IXANY);
  tio.c_cc[VMIN] = 0;
  tio.c_cc[VTIME] = 1; // 0.1s inter-byte timeout
  if (tcsetattr(fd, TCSANOW, &tio) != 0) {
    m_error = "tcsetattr failed for " + m_portName;
    ::close(fd);
    return false;
  }
  tcflush(fd, TCIOFLUSH);
  m_fd = fd;
  return true;
}

void SerialPort::close() {
  if (m_fd >= 0) {
    ::close(m_fd);
    m_fd = -1;
  }
}

bool SerialPort::isOpen() const { return m_fd >= 0; }

bool SerialPort::write(const std::string &data) { return write(data.data(), data.size()); }

bool SerialPort::write(const char *data, std::size_t size) {
  if (m_fd < 0) {
    m_error = "Port not open";
    return false;
  }
  std::size_t done = 0;
  while (done < size) {
    ssize_t n = ::write(m_fd, data + done, size - done);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      m_error = "Write failed on " + m_portName;
      return false;
    }
    done += static_cast<std::size_t>(n);
  }
  return true;
}

bool SerialPort::waitForBytesWritten(int /*timeoutMs*/) { return isOpen(); }

bool SerialPort::waitForReadyRead(int timeoutMs) {
  if (m_fd < 0) {
    return false;
  }
  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(m_fd, &rfds);
  timeval tv{};
  tv.tv_sec = timeoutMs / 1000;
  tv.tv_usec = (timeoutMs % 1000) * 1000;
  int rc = select(m_fd + 1, &rfds, nullptr, nullptr, &tv);
  return rc > 0 && FD_ISSET(m_fd, &rfds);
}

std::size_t SerialPort::bytesAvailable() const {
  if (m_fd < 0) {
    return 0;
  }
  int n = 0;
  if (ioctl(m_fd, FIONREAD, &n) != 0 || n < 0) {
    return 0;
  }
  return static_cast<std::size_t>(n);
}

bool SerialPort::readChunk(std::string &out, int timeoutMs) {
  if (!waitForReadyRead(timeoutMs)) {
    return false;
  }
  char buf[256];
  ssize_t n = ::read(m_fd, buf, sizeof(buf));
  if (n <= 0) {
    return false;
  }
  out.assign(buf, static_cast<std::size_t>(n));
  return true;
}

std::string SerialPort::readAll() {
  std::string out;
  while (bytesAvailable() > 0) {
    std::string chunk;
    if (!readChunk(chunk, 50)) {
      break;
    }
    out += chunk;
  }
  return out;
}

bool SerialPort::readLine(std::string &line, int timeoutMs) {
  line.clear();
  std::string acc;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  for (;;) {
    auto now = std::chrono::steady_clock::now();
    if (now >= deadline && acc.find('\n') == std::string::npos) {
      return false;
    }
    int remaining = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());
    if (remaining < 0) {
      remaining = 0;
    }
    std::string chunk;
    if (!readChunk(chunk, remaining)) {
      return false;
    }
    acc += chunk;
    auto pos = acc.find('\n');
    if (pos != std::string::npos) {
      line = acc.substr(0, pos);
      while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
        line.pop_back();
      }
      return true;
    }
  }
}

#endif
