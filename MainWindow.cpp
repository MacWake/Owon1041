#include "MainWindow.h"

#include "ConnectDialog.h"

#include <cstdio>
#include <ctime>
#include <iostream>
#include <thread>

namespace {
// Timer IDs for the poll and autoconnect timers.
constexpr int ID_POLL_TIMER = wxID_HIGHEST + 1;
constexpr int ID_AUTOCONNECT_TIMER = wxID_HIGHEST + 2;

std::string Trim(const std::string &s) {
  const auto b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) {
    return "";
  }
  const auto e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

// Insert a space between a digit and a directly attached unit suffix,
// mirroring the Qt behaviour: (?<=\d)(?=[^\d.eE+\-\s])
std::string SpaceDigitUnit(const std::string &s) {
  auto isUnitBoundary = [](char prev, char next) {
    if (prev < '0' || prev > '9') {
      return false;
    }
    if (next >= '0' && next <= '9') {
      return false;
    }
    return next != '.' && next != 'e' && next != 'E' && next != '+' &&
           next != '-' && next != ' ' && next != '\t' && next != '\r' &&
           next != '\n';
  };
  std::string out;
  out.reserve(s.size() + 2);
  for (std::size_t i = 0; i < s.size(); ++i) {
    out.push_back(s[i]);
    if (i + 1 < s.size() && isUnitBoundary(s[i], s[i + 1])) {
      out.push_back(' ');
    }
  }
  return out;
}

// The meter sends GB2312-ish byte pairs for special units; map them to UTF-8.
void FixMeterEncoding(std::string &s) {
  const std::pair<std::string, std::string> table[] = {
      {std::string("\xa6\xb8", 2), "\xce\xa9"},     // Ohm sign
      {std::string("\xa6\xcc", 2), "\xc2\xb5"},     // Micro sign
      {std::string("\xa1\xe6", 2), "\xc2\xb0" "C"}, // degree C
      {std::string("\xa8\x48", 2), "\xc2\xb0" "F"}, // degree F
  };
  for (const auto &[from, to] : table) {
    std::string::size_type pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
      s.replace(pos, from.size(), to);
      pos += to.size();
    }
  }
}

std::string JsonEscape(const std::string &s) {
  std::string out;
  for (char c : s) {
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      if (static_cast<unsigned char>(c) < 0x20) {
        char buf[8];
        snprintf(buf, sizeof(buf), "\\u%04x", c);
        out += buf;
      } else {
        out += c;
      }
    }
  }
  return out;
}

std::string ToIsoUtcMs(std::chrono::system_clock::time_point tp) {
  const auto ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;
  std::time_t t = std::chrono::system_clock::to_time_t(tp);
  std::tm tm{};
#ifdef _WIN32
  gmtime_s(&tm, &t);
#else
  gmtime_r(&t, &tm);
#endif
  char buf[32];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", tm.tm_year + 1900,
           tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
           static_cast<int>(ms.count()));
  return buf;
}
} // namespace

MainWindow::MainWindow() : wxFrame(nullptr, wxID_ANY, "") {
  m_settings.load();
  if (m_settings.windowWidth() > 0 && m_settings.windowHeight() > 0) {
    SetSize(m_settings.windowX(), m_settings.windowY(), m_settings.windowWidth(),
            m_settings.windowHeight());
  }

  setupUi();

  m_proxy = new ScpiProxy(
      [this](const std::string &command) { return handleProxyQuery(command); });
  if (m_proxy->start()) {
    measurement->SetToolTip("Read-only SCPI: 127.0.0.1:5025");
  } else {
    std::cerr << "SCPI proxy could not listen: " << m_proxy->errorString() << "\n";
    measurement->SetToolTip("SCPI proxy unavailable: " + m_proxy->errorString());
  }

  m_pollTimer.SetOwner(this, ID_POLL_TIMER);
  Bind(wxEVT_TIMER, &MainWindow::onPollTimer, this, ID_POLL_TIMER);
  m_autoconnectTimer.SetOwner(this, ID_AUTOCONNECT_TIMER);
  Bind(wxEVT_TIMER, &MainWindow::onAutoconnectTimer, this, ID_AUTOCONNECT_TIMER);
  m_autoconnectTimer.Start(2000, wxTIMER_ONE_SHOT);
}

MainWindow::~MainWindow() {
  m_pollTimer.Stop();
  m_autoconnectTimer.Stop();
  delete m_proxy;
  if (m_port) {
    m_port->close();
    delete m_port;
  }
  m_settings.save();
}

void MainWindow::setupUi() {
  SetTitle(wxString("MacWake OWON XDM-1041 v") + APP_VERSION_STRING);

  m_panel = new wxPanel(this, wxID_ANY);

  measurement =
      new wxStaticText(m_panel, wxID_ANY, "not connected", wxDefaultPosition,
                       wxDefaultSize, wxALIGN_RIGHT | wxALIGN_CENTRE_VERTICAL | wxBORDER_SUNKEN);

  wxFont font(48, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
#if defined(__WXMSW__)
  font.SetFaceName("Consolas");
#elif defined(__WXOSX__)
  font.SetFaceName("Menlo");
#else
  font.SetFaceName("Liberation Mono");
#endif
  measurement->SetFont(font);
  measurement->Bind(wxEVT_LEFT_UP, &MainWindow::onMeasurementClick, this);

  auto makeButton = [this](const char *label) {
    return new wxButton(m_panel, wxID_ANY, wxString::FromUTF8(label));
  };
  btn_50_v = makeButton("50 V");
  btn_auto_v = makeButton("Auto V");
  btn_short = makeButton("Short");
  btn_diode = makeButton("Diode");
  btn_50_kr = makeButton("50 k\xce\xa9");
  btn_auto_r = makeButton("Auto \xce\xa9");
  btn_50_f = makeButton("50 \xc2\xb5" "F");
  btn_auto_f = makeButton("Auto F");
  btn_freq = makeButton("Hz");
  btn_period = makeButton("Period");

  btn_50_v->Bind(wxEVT_BUTTON, &MainWindow::onVoltage50V, this);
  btn_auto_v->Bind(wxEVT_BUTTON, &MainWindow::onVoltageAuto, this);
  btn_short->Bind(wxEVT_BUTTON, &MainWindow::onShort, this);
  btn_diode->Bind(wxEVT_BUTTON, &MainWindow::onDiode, this);
  btn_50_kr->Bind(wxEVT_BUTTON, &MainWindow::onResistance50K, this);
  btn_auto_r->Bind(wxEVT_BUTTON, &MainWindow::onResistanceAuto, this);
  btn_50_f->Bind(wxEVT_BUTTON, &MainWindow::onCapacitance50uF, this);
  btn_auto_f->Bind(wxEVT_BUTTON, &MainWindow::onCapacitanceAuto, this);
  btn_freq->Bind(wxEVT_BUTTON, &MainWindow::onFrequency, this);
  btn_period->Bind(wxEVT_BUTTON, &MainWindow::onPeriod, this);

  m_connect_dialog = new ConnectDialog(this, &m_settings);

  Bind(wxEVT_SIZE, &MainWindow::onFrameSize, this);
  Bind(wxEVT_CLOSE_WINDOW, &MainWindow::onClose, this);

  const wxSize client = GetClientSize();
  m_panel->SetSize(client);
  setupPositions(client.GetWidth(), client.GetHeight());
}

void MainWindow::onFrameSize(wxSizeEvent &event) {
  event.Skip();
  const wxSize client = GetClientSize();
  if (m_panel) {
    m_panel->SetSize(client);
    setupPositions(client.GetWidth(), client.GetHeight());
  }
  m_settings.setWindowWidth(client.GetWidth());
  m_settings.setWindowHeight(client.GetHeight());
}

void MainWindow::onClose(wxCloseEvent &event) {
  m_pollTimer.Stop();
  m_autoconnectTimer.Stop();
  event.Skip(); // proceeds to destructor, which saves settings
}

void MainWindow::setupPositions(const int width, const int height) {
  const int btn_width = 70;
  const int btn_height = 32;
  const int btnbar_w = 350;
  const int btn_x = (width - btnbar_w) / 2;

  const int measureHeight = measurement->GetCharHeight();
  measurement->SetSize(2, 0, width - 4, measureHeight);
  const int btngroup_y1 = measureHeight + 2;
  const int btngroup_y2 = btngroup_y1 + btn_height + 1;

  btn_50_v->SetSize(btn_x, btngroup_y1, btn_width, btn_height);
  btn_auto_v->SetSize(btn_x, btngroup_y2, btn_width, btn_height);
  btn_short->SetSize(btn_x + 70, btngroup_y1, btn_width, btn_height);
  btn_diode->SetSize(btn_x + 70, btngroup_y2, btn_width, btn_height);
  btn_50_kr->SetSize(btn_x + 140, btngroup_y1, btn_width, btn_height);
  btn_auto_r->SetSize(btn_x + 140, btngroup_y2, btn_width, btn_height);
  btn_50_f->SetSize(btn_x + 210, btngroup_y1, btn_width, btn_height);
  btn_auto_f->SetSize(btn_x + 210, btngroup_y2, btn_width, btn_height);
  btn_freq->SetSize(btn_x + 280, btngroup_y1, btn_width, btn_height);
  btn_period->SetSize(btn_x + 280, btngroup_y2, btn_width, btn_height);
  (void)height;
}

void MainWindow::onAutoconnectTimer(wxTimerEvent &) { connectSerial(); }

void MainWindow::onPollTimer(wxTimerEvent &) { updateMeasurement(); }

void MainWindow::connectSerial() {
  std::cerr << "Connecting to serial port (auto)" << std::endl;
  if (m_settings.device().empty()) {
    openConnectDialog();
  } else {
    if (!m_connect_dialog->tryPortByName(m_settings.device())) {
      std::cerr << "Could not connect to serial port " << m_settings.device() << std::endl;
    } else {
      m_port = m_connect_dialog->getConfiguredSerialPort();
      onConnect();
    }
  }
}

std::string MainWindow::rateToSerial(Settings::Rate rate) {
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
  writeSCPIStatement("RATE " + rateToSerial(m_settings.getRate()));
  writeSCPIStatement("SYST:BEEP:STAT OFF");
  wxCommandEvent dummy;
  onVoltage50V(dummy);
  m_pollTimer.Start(100);
}

bool MainWindow::openConnectDialog() {
  m_pollTimer.Stop();
  if (m_port) {
    m_port->close();
    delete m_port;
    m_port = nullptr;
  }
  m_lastDisplay.clear();
  m_haveDisplay = false;
  measurement->SetLabel("not connected");
  if (m_connect_dialog->ShowModal() == wxID_OK) {
    SerialPort *port = m_connect_dialog->getConfiguredSerialPort();
    if (port && port->isOpen()) {
      m_port = port;
      m_settings.setDevice(port->portName());
      m_settings.save();
      onConnect();
      return true;
    }
    delete port;
  }
  return false;
}

void MainWindow::updateMeasurement() {
  if (!m_port || !m_port->isOpen()) {
    m_pollTimer.Stop();
    measurement->SetLabel("not connected");
    m_lastDisplay.clear();
    m_haveDisplay = false;
    return;
  }
  std::string reading = writeSCPICommand("MEAS1:SHOW?");
  if (reading.empty()) {
    return;
  }
  reading = SpaceDigitUnit(reading);
  m_lastDisplay = reading;
  m_lastDisplayAt = std::chrono::system_clock::now();
  m_haveDisplay = true;
  measurement->SetLabel(wxString::FromUTF8(reading.c_str()));
}

void MainWindow::onVoltage50V(wxCommandEvent &) {
  m_unit = "V";
  m_mode = "VOLT:DC";
  writeSCPIStatement("CONF:VOLT:DC 50");
}

void MainWindow::onVoltageAuto(wxCommandEvent &) {
  m_unit = "V";
  m_mode = "VOLT:DC";
  writeSCPIStatement("CONF:VOLT:DC AUTO");
}

void MainWindow::onShort(wxCommandEvent &) {
  m_unit = "\xce\xa9"; // Ohm sign, UTF-8
  m_mode = "CONT";
  writeSCPIStatement("CONF:CONT");
  if (m_settings.getBeepShort()) {
    std::cerr << "Beep resistance: " << m_settings.getBeepResistance() << "\n";
    writeSCPIStatement("CONT:THRE " + std::to_string(m_settings.getBeepResistance()));
    writeSCPIStatement("SYST:BEEP:STAT ON");
  } else {
    writeSCPIStatement("SYST:BEEP:STAT OFF");
  }
}

void MainWindow::onDiode(wxCommandEvent &) {
  m_unit = "V";
  m_mode = "DIOD";
  if (m_settings.getBeepDiode()) {
    writeSCPIStatement("SYST:BEEP:STAT ON");
  } else {
    writeSCPIStatement("SYST:BEEP:STAT OFF");
  }
  writeSCPIStatement("CONF:DIOD");
}

void MainWindow::onResistance50K(wxCommandEvent &) {
  m_unit = "\xce\xa9";
  m_mode = "RES";
  writeSCPIStatement("CONF:RES 50E3");
}

void MainWindow::onResistanceAuto(wxCommandEvent &) {
  m_unit = "\xce\xa9";
  m_mode = "RES";
  writeSCPIStatement("CONF:RES AUTO");
}

void MainWindow::onCapacitance50uF(wxCommandEvent &) {
  m_unit = "F";
  m_mode = "CAP";
  writeSCPIStatement("CONF:CAP 50E-6");
}

void MainWindow::onCapacitanceAuto(wxCommandEvent &) {
  m_unit = "F";
  m_mode = "CAP";
  writeSCPIStatement("CONF:CAP AUTO");
}

void MainWindow::onFrequency(wxCommandEvent &) {
  m_unit = "Hz";
  m_mode = "FREQ";
  writeSCPIStatement("CONF:FREQ");
}

void MainWindow::onPeriod(wxCommandEvent &) {
  m_unit = "s";
  m_mode = "PER";
  writeSCPIStatement("CONF:PER");
}

std::string MainWindow::readSCPI() {
  std::lock_guard<std::mutex> lock(m_serialMutex);
  if (!m_port || !m_port->isOpen()) {
    std::cerr << "Serial port not open" << std::endl;
    return {};
  }
  if (!m_port->waitForReadyRead(500)) {
    std::cerr << "Serial port not ready" << std::endl;
    return {};
  }
  std::string line;
  if (!m_port->readLine(line, 100)) {
    std::cerr << "Read timeout occurred" << std::endl;
    return {};
  }
  FixMeterEncoding(line);
  return Trim(line);
}

void MainWindow::writeSCPIStatement(const std::string &command) {
  std::lock_guard<std::mutex> lock(m_serialMutex);
  if (!m_port || !m_port->isOpen()) {
    std::cerr << "No port open, refusing writeSCPI\n";
    return;
  }
  m_port->write(command + "\r\n");
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

std::string MainWindow::writeSCPICommand(const std::string &command) {
  // writeSCPIStatement and readSCPI each take the mutex; kept as two steps
  // like the Qt version (write, 10ms pause, then read).
  writeSCPIStatement(command);
  {
    std::lock_guard<std::mutex> lock(m_serialMutex);
    if (!m_port || !m_port->isOpen()) {
      return {};
    }
  }
  return readSCPI();
}

std::string MainWindow::handleProxyQuery(const std::string &command) {
  std::unique_lock<std::mutex> lock(m_serialMutex);
  const bool connected = m_port && m_port->isOpen();
  const std::string port = connected ? m_port->portName() : "";
  const std::string mode = connected ? m_mode : "";
  const std::string unit = connected ? m_unit : "";
  const std::string display = connected ? m_lastDisplay : "";
  const std::string displayAt =
      (connected && m_haveDisplay) ? ToIsoUtcMs(m_lastDisplayAt) : "";
  if (command == "PROX:STATE?") {
    return "{\"connected\":" + std::string(connected ? "true" : "false") + ",\"port\":\"" +
           JsonEscape(port) + "\",\"mode\":\"" + JsonEscape(mode) +
           "\",\"mode_source\":\"app\",\"unit\":\"" + JsonEscape(unit) + "\",\"display\":\"" +
           JsonEscape(display) + "\",\"display_at\":\"" + displayAt + "\"}";
  }
  if (!connected) {
    return "ERR:DISCONNECTED";
  }
  std::string meterCommand = command;
  if (command == "READ?" || command == "PROX:READ?") {
    meterCommand = "MEAS1?";
  }
  // Snapshot for the response while still holding the lock; the blocking
  // meter I/O below runs unlocked so the poll timer is not stalled.
  const std::string snapDisplay = m_lastDisplay;
  const bool snapHaveDisplay = m_haveDisplay;
  const std::string snapDisplayAt = snapHaveDisplay ? ToIsoUtcMs(m_lastDisplayAt) : "";
  lock.unlock();
  const std::string response = writeSCPICommand(meterCommand);
  if (response.empty()) {
    return "ERR:TIMEOUT";
  }
  if (command == "PROX:READ?") {
    const std::string now = ToIsoUtcMs(std::chrono::system_clock::now());
    return "{\"connected\":true,\"port\":\"" + JsonEscape(port) + "\",\"mode\":\"" +
           JsonEscape(mode) + "\",\"mode_source\":\"app\",\"unit\":\"" + JsonEscape(unit) +
           "\",\"display\":\"" + JsonEscape(snapDisplay) + "\",\"display_at\":\"" +
           snapDisplayAt + "\",\"value\":\"" + JsonEscape(response) +
           "\",\"measured_at\":\"" + now + "\"}";
  }
  return response;
}

void MainWindow::onMeasurementClick(wxMouseEvent &) {
  m_settings.save();
  openConnectDialog();
}
