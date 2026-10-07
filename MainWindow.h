#pragma once

#include <wx/wx.h>

#include <chrono>
#include <mutex>
#include <string>

#include "ScpiProxy.h"
#include "SerialPort.h"
#include "Settings.h"

class ConnectDialog;

class MainWindow final : public wxFrame {
public:
  explicit MainWindow();
  ~MainWindow() override;

private:
  void setupUi();
  void setupPositions(int width, int height);

  void onFrameSize(wxSizeEvent &event);
  void onClose(wxCloseEvent &event);
  void onAutoconnectTimer(wxTimerEvent &event);
  void onPollTimer(wxTimerEvent &event);
  void onMeasurementClick(wxMouseEvent &event);

  void onVoltage50V(wxCommandEvent &event);
  void onVoltageAuto(wxCommandEvent &event);
  void onShort(wxCommandEvent &event);
  void onDiode(wxCommandEvent &event);
  void onResistance50K(wxCommandEvent &event);
  void onResistanceAuto(wxCommandEvent &event);
  void onCapacitance50uF(wxCommandEvent &event);
  void onCapacitanceAuto(wxCommandEvent &event);
  void onFrequency(wxCommandEvent &event);
  void onPeriod(wxCommandEvent &event);

  void connectSerial();
  void onConnect();
  bool openConnectDialog();
  void updateMeasurement();

  static std::string rateToSerial(Settings::Rate rate);

  // Blocking meter I/O; serialized by m_serialMutex because the SCPI proxy
  // handler runs on worker threads while the poll timer runs on the GUI thread.
  std::string readSCPI();
  void writeSCPIStatement(const std::string &command);
  std::string writeSCPICommand(const std::string &command);
  std::string handleProxyQuery(const std::string &command);

  Settings m_settings;
  wxPanel *m_panel = nullptr;
  wxStaticText *measurement = nullptr;
  wxButton *btn_50_v = nullptr;
  wxButton *btn_auto_v = nullptr;
  wxButton *btn_short = nullptr;
  wxButton *btn_diode = nullptr;
  wxButton *btn_50_kr = nullptr;
  wxButton *btn_auto_r = nullptr;
  wxButton *btn_50_f = nullptr;
  wxButton *btn_auto_f = nullptr;
  wxButton *btn_freq = nullptr;
  wxButton *btn_period = nullptr;

  ConnectDialog *m_connect_dialog = nullptr;
  wxTimer m_pollTimer;
  wxTimer m_autoconnectTimer;

  std::mutex m_serialMutex;
  SerialPort *m_port = nullptr;
  ScpiProxy *m_proxy = nullptr;

  std::string m_unit;
  std::string m_mode;
  std::string m_lastDisplay;
  std::chrono::system_clock::time_point m_lastDisplayAt{};
  bool m_haveDisplay = false;
};
