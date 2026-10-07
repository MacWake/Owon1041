#pragma once

#include <wx/wx.h>

#include <string>

#include "SerialPort.h"
#include "Settings.h"

class ConnectDialog final : public wxDialog {
public:
  ConnectDialog(wxWindow *parent, Settings *settings);
  ~ConnectDialog() override;

  std::string getSelectedPort() const;
  // Transfers ownership of the configured port to the caller (if the last
  // try/test succeeded); returns nullptr otherwise.
  SerialPort *getConfiguredSerialPort();

  bool tryPortByName(const std::string &portName);

private:
  void onRefresh(wxCommandEvent &event);
  void onConnect(wxCommandEvent &event);
  void onTest(wxCommandEvent &event);

  void setupUi();
  void populatePortsList();
  bool configureSerialPort(const std::string &device);
  void tryPort();
  void tryConfiguredPort();
  void loadSettings();
  void saveSettings();

  Settings *m_settings; // not owned
  wxComboBox *portComboBox = nullptr;
  wxCheckBox *m_beep_short = nullptr;
  wxTextCtrl *m_short_threshold = nullptr;
  wxCheckBox *m_beep_diode = nullptr;
  wxRadioBox *m_rateBox = nullptr;
  wxButton *refreshButton = nullptr;
  wxButton *connectButton = nullptr;
  wxStaticText *statusLabel = nullptr;

  SerialPort *serialPort = nullptr;
  bool m_check_ok = false;
};
