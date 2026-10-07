#pragma once

#include <string>

// Plain-C++ application settings stored as an INI file in the platform
// config location. Replaces the QSettings-based implementation; the key
// names (window/height, hardware/device, rate, ...) are unchanged.
class Settings {
public:
  enum class Rate { SLOW, MEDIUM, FAST };

  Settings();

  void init();
  void load();
  void save();

  int windowHeight() const { return m_windowHeight; }
  int windowWidth() const { return m_windowWidth; }
  int windowX() const { return m_windowX; }
  int windowY() const { return m_windowY; }
  std::string device() const { return m_device; }
  Rate getRate() const { return m_rate; }
  bool getBeepShort() const { return m_beep_short; }
  bool getBeepDiode() const { return m_beep_diode; }
  int getBeepResistance() const { return m_beep_resistance; }

  void setWindowHeight(int height);
  void setWindowWidth(int width);
  void setWindowX(int x);
  void setWindowY(int y);
  void setDevice(const std::string &device);
  void setRate(Rate rate);
  void setBeepShort(bool enabled);
  void setBeepDiode(bool enabled);
  void setBeepResistance(int threshold);

  static Rate stringToRate(const std::string &value, Rate dflt);
  static std::string rateToString(Rate rate);

  static std::string configFilePath();

private:
  int m_windowHeight = 162;
  int m_windowWidth = 580;
  int m_windowX = 100;
  int m_windowY = 100;
  std::string m_device;
  Rate m_rate = Rate::FAST;
  bool m_beep_short = true;
  bool m_beep_diode = true;
  int m_beep_resistance = 50;
};
