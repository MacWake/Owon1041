// ReSharper disable CppDFAMemoryLeak
#include "ConnectDialog.h"

#include <iostream>
#include <sstream>
#include <wx/combobox.h>
#include <wx/statline.h>

namespace {
void SetStatus(wxStaticText *label, const std::string &text, const wxColour &color) {
  label->SetLabel(wxString::FromUTF8(text.c_str()));
  label->SetForegroundColour(color);
}

std::vector<std::string> SplitComma(const std::string &s) {
  std::vector<std::string> parts;
  std::string cur;
  for (char c : s) {
    if (c == ',') {
      parts.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  parts.push_back(cur);
  for (auto &p : parts) {
    const auto b = p.find_first_not_of(" \t\r\n");
    const auto e = p.find_last_not_of(" \t\r\n");
    p = (b == std::string::npos) ? "" : p.substr(b, e - b + 1);
  }
  return parts;
}
} // namespace

ConnectDialog::ConnectDialog(wxWindow *parent, Settings *settings)
    : wxDialog(parent, wxID_ANY, "Serial Port Connection", wxDefaultPosition,
               wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_settings(settings) {
  setupUi();
  populatePortsList();
  loadSettings(); // Load settings when dialog is created
}

ConnectDialog::~ConnectDialog() { delete serialPort; }

void ConnectDialog::setupUi() {
  auto *mainSizer = new wxBoxSizer(wxVERTICAL);

  auto *defaultsBox = new wxStaticBoxSizer(wxVERTICAL, this, "Defaults");
  m_beep_short = new wxCheckBox(this, wxID_ANY, "Beep in SHORT mode");
  defaultsBox->Add(m_beep_short, 0, wxALL, 4);

  auto *thresholdSizer = new wxBoxSizer(wxHORIZONTAL);
  thresholdSizer->Add(new wxStaticText(this, wxID_ANY, "Threshold (\xce\xa9):"), 0,
                      wxALIGN_CENTRE_VERTICAL | wxRIGHT, 6);
  m_short_threshold = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition,
                                     wxSize(60, -1));
  thresholdSizer->Add(m_short_threshold, 0, wxALIGN_CENTRE_VERTICAL);
  defaultsBox->Add(thresholdSizer, 0, wxALL, 4);

  m_beep_diode = new wxCheckBox(this, wxID_ANY, "Beep in DIODE mode");
  defaultsBox->Add(m_beep_diode, 0, wxALL, 4);

  const wxString rates[] = {"Slow", "Medium", "Fast"};
  m_rateBox = new wxRadioBox(this, wxID_ANY, "Measurement Rate", wxDefaultPosition,
                             wxDefaultSize, 3, rates, 3, wxRA_SPECIFY_COLS);
  defaultsBox->Add(m_rateBox, 0, wxEXPAND | wxALL, 4);
  mainSizer->Add(defaultsBox, 0, wxEXPAND | wxALL, 6);

  auto *portBox = new wxStaticBoxSizer(wxHORIZONTAL, this, "Port Selection");
  portComboBox = new wxComboBox(this, wxID_ANY, "", wxDefaultPosition,
                                wxDefaultSize, 0, nullptr, wxCB_READONLY);
  portBox->Add(portComboBox, 1, wxEXPAND | wxALL, 4);
  refreshButton = new wxButton(this, wxID_ANY, "Refresh");
  portBox->Add(refreshButton, 0, wxALL, 4);
  mainSizer->Add(portBox, 0, wxEXPAND | wxLEFT | wxRIGHT, 6);

  statusLabel = new wxStaticText(this, wxID_ANY, "Select a port and connect.");
  statusLabel->SetForegroundColour(*wxLIGHT_GREY);
  mainSizer->Add(statusLabel, 0, wxEXPAND | wxALL, 6);

  auto *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
  buttonSizer->AddStretchSpacer(1);
  connectButton = new wxButton(this, wxID_ANY, "Connect");
  connectButton->SetDefault();
  auto *tryButton = new wxButton(this, wxID_ANY, "Test");
  auto *cancelButton = new wxButton(this, wxID_CANCEL, "Cancel");
  buttonSizer->Add(tryButton, 0, wxRIGHT, 6);
  buttonSizer->Add(cancelButton, 0, wxRIGHT, 6);
  buttonSizer->Add(connectButton, 0);
  mainSizer->Add(buttonSizer, 0, wxEXPAND | wxALL, 6);

  SetSizerAndFit(mainSizer);

  refreshButton->Bind(wxEVT_BUTTON, &ConnectDialog::onRefresh, this);
  tryButton->Bind(wxEVT_BUTTON, &ConnectDialog::onTest, this);
  connectButton->Bind(wxEVT_BUTTON, &ConnectDialog::onConnect, this);
}

void ConnectDialog::onRefresh(wxCommandEvent &) { populatePortsList(); }
void ConnectDialog::onTest(wxCommandEvent &) { tryPort(); }
void ConnectDialog::onConnect(wxCommandEvent &) {
  tryPort();
  if (m_check_ok) {
    EndModal(wxID_OK);
  } else {
    if (serialPort) {
      serialPort->close();
      delete serialPort;
      serialPort = nullptr;
    }
  }
}

void ConnectDialog::loadSettings() {
  if (!m_settings) {
    return;
  }
  m_beep_short->SetValue(m_settings->getBeepShort());
  m_beep_diode->SetValue(m_settings->getBeepDiode());
  m_short_threshold->SetValue(std::to_string(m_settings->getBeepResistance()));
  switch (m_settings->getRate()) {
  case Settings::Rate::SLOW:
    m_rateBox->SetSelection(0);
    break;
  case Settings::Rate::MEDIUM:
    m_rateBox->SetSelection(1);
    break;
  case Settings::Rate::FAST:
  default:
    m_rateBox->SetSelection(2);
    break;
  }
}

void ConnectDialog::saveSettings() {
  if (!m_settings) {
    return;
  }
  m_settings->setBeepShort(m_beep_short->GetValue());
  m_settings->setBeepDiode(m_beep_diode->GetValue());
  try {
    m_settings->setBeepResistance(
        std::stoi(m_short_threshold->GetValue().ToStdString()));
  } catch (...) {
    m_settings->setBeepResistance(0);
  }
  switch (m_rateBox->GetSelection()) {
  case 0:
    m_settings->setRate(Settings::Rate::SLOW);
    break;
  case 1:
    m_settings->setRate(Settings::Rate::MEDIUM);
    break;
  default:
    m_settings->setRate(Settings::Rate::FAST);
    break;
  }
  m_settings->save();
}

void ConnectDialog::populatePortsList() {
  portComboBox->Clear();
  const auto ports = EnumerateSerialPorts();
  if (ports.empty()) {
    SetStatus(statusLabel, "No serial ports found", *wxRED);
    connectButton->Enable(false);
    return;
  }
  for (const auto &port : ports) {
    std::string label = port.portName;
    if (!port.description.empty()) {
      label += " - " + port.description;
    }
    if (!port.manufacturer.empty()) {
      label += " (" + port.manufacturer + ")";
    }
    portComboBox->Append(wxString::FromUTF8(label.c_str()),
                         new wxStringClientData(port.portName));
  }
  portComboBox->SetSelection(0);
  SetStatus(statusLabel, "Select a port and click Connect/Test.", *wxLIGHT_GREY);
  connectButton->Enable(true);
}

bool ConnectDialog::configureSerialPort(const std::string &device) {
  delete serialPort;
  serialPort = new SerialPort(device);
  if (!serialPort->open()) {
    std::cerr << "Failed to open serial port: " << serialPort->errorString() << "\n";
    return false;
  }
  return true;
}

bool ConnectDialog::tryPortByName(const std::string &portName) {
  configureSerialPort(portName);
  saveSettings(); // Save settings before trying the port
  tryConfiguredPort();
  return m_check_ok;
}

void ConnectDialog::tryPort() {
  if (portComboBox->GetSelection() == wxNOT_FOUND) {
    SetStatus(statusLabel, "No serial port selected.", *wxRED);
    m_check_ok = false;
    return;
  }
  configureSerialPort(getSelectedPort());
  saveSettings(); // Save settings before trying the port
  tryConfiguredPort();
}

void ConnectDialog::tryConfiguredPort() {
  if (!serialPort) {
    SetStatus(statusLabel, "Serial port not initialized.", *wxRED);
    m_check_ok = false;
    return;
  }
  if (serialPort->isOpen()) {
    serialPort->close();
  }
  if (!serialPort->open()) {
    SetStatus(statusLabel, "Could not open: " + serialPort->errorString(), *wxRED);
    m_check_ok = false;
    return;
  }

  SetStatus(statusLabel, "Testing communication...", *wxBLUE);

  if (!serialPort->write("*IDN?\n") || !serialPort->waitForBytesWritten(1000)) {
    SetStatus(statusLabel, "Write timeout to " + serialPort->portName(), *wxRED);
    serialPort->close();
    m_check_ok = false;
    return;
  }
  if (!serialPort->waitForReadyRead(2000)) {
    SetStatus(statusLabel, "Read timeout from " + serialPort->portName(), *wxRED);
    serialPort->close();
    m_check_ok = false;
    return;
  }
  std::string line;
  if (!serialPort->readLine(line, 2000) || line.empty()) {
    SetStatus(statusLabel, "No response from " + serialPort->portName(), *wxRED);
    serialPort->close();
    m_check_ok = false;
    return;
  }

  const auto parts = SplitComma(line);
  if (parts.size() < 2) {
    SetStatus(statusLabel, "Invalid response: " + line.substr(0, 50), *wxRED);
    serialPort->close();
    m_check_ok = false;
    return;
  }
  const std::string model = parts[1];
  const std::string version = parts.size() > 3 ? parts[3] : "Unknown Fw";

  SetStatus(statusLabel, "Connected: " + model + " (FW: " + version + ")",
            wxColour(0, 128, 0));
  m_check_ok = true;
  // The port stays open for the caller (MainWindow) via getConfiguredSerialPort().
}

std::string ConnectDialog::getSelectedPort() const {
  const int sel = portComboBox->GetSelection();
  if (sel == wxNOT_FOUND) {
    return {};
  }
  auto *data = static_cast<wxStringClientData *>(portComboBox->GetClientObject(sel));
  return data ? data->GetData().ToStdString() : std::string();
}

SerialPort *ConnectDialog::getConfiguredSerialPort() {
  if (!m_check_ok || !serialPort) {
    return nullptr;
  }
  SerialPort *port = serialPort;
  serialPort = nullptr;
  m_check_ok = false;
  return port;
}
