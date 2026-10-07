#pragma once

#include <cstddef>
#include <string>
#include <vector>

// Minimal serial-port abstraction replacing QSerialPort/QSerialPortInfo.
// POSIX implementation uses termios; Windows uses the Win32 COM API.
struct SerialPortInfo {
  std::string portName;    // OS device path, e.g. /dev/cu.usbserial-110 or COM3
  std::string description; // Human-readable label (may be empty)
  std::string manufacturer; // May be empty
};

// List candidate serial devices on this machine.
std::vector<SerialPortInfo> EnumerateSerialPorts();

class SerialPort {
public:
  explicit SerialPort(const std::string &portName);
  ~SerialPort();

  SerialPort(const SerialPort &) = delete;
  SerialPort &operator=(const SerialPort &) = delete;

  const std::string &portName() const { return m_portName; }

  // Open read/write at 115200 8N1. Returns false on failure; see errorString().
  bool open();
  void close();
  bool isOpen() const;

  std::string errorString() const { return m_error; }

  // Write the whole buffer. Returns false on failure.
  bool write(const std::string &data);
  bool write(const char *data, std::size_t size);

  // True once input is available or timeoutMs elapses (false = timeout/error).
  bool waitForReadyRead(int timeoutMs);
  // True once output was accepted or timeoutMs elapses.
  bool waitForBytesWritten(int timeoutMs);

  // Number of bytes that can be read without blocking (0 on error).
  std::size_t bytesAvailable() const;

  // Read one line (trailing CR/LF stripped). Returns false on timeout/error.
  bool readLine(std::string &line, int timeoutMs);
  // Read whatever is currently available (may be empty).
  std::string readAll();

private:
  bool readChunk(std::string &out, int timeoutMs);

  std::string m_portName;
  std::string m_error;
#ifdef _WIN32
  void *m_handle = nullptr; // HANDLE
#else
  int m_fd = -1;
#endif
};
