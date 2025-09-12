#include "Settings.h"

#include <iostream>
#include <map>

Settings::Settings(QObject *parent) : QSettings(parent) {}

Settings::Settings(const QString &organization, const QString &application,
                   QObject *parent)
    : QSettings(organization, application, parent) {}

void Settings::init() { load(); }

void Settings::load() {
  // Load settings from storage using QSettings methods
  m_windowHeight = value("window/height", m_windowHeight).toInt();
  m_windowWidth = value("window/width", m_windowWidth).toInt();
  m_windowX = value("window/x", m_windowX).toInt();
  m_windowY = value("window/y", m_windowY).toInt();
  m_device = value("hardware/device", m_device).toString();
  m_rate = stringToRate(value("rate", rateToString(m_rate)).toString(),
                        m_rate); // Use m_rate as default
  m_beep_short = value("beep_short", m_beep_short).toBool();
  m_beep_diode = value("beep_diode", m_beep_diode).toBool();
  m_beep_resistance = value("beep_threshold", m_beep_resistance).toInt();
}

void Settings::save() {
  // Save settings to storage using QSettings methods
  setValue("window/height", m_windowHeight);
  setValue("window/width", m_windowWidth);
  setValue("window/x", m_windowX);
  setValue("window/y", m_windowY);
  setValue("hardware/device", m_device);
  setValue("rate", rateToString(m_rate)); // Save rate
  setValue("beep_short", m_beep_short);
  setValue("beep_diode", m_beep_diode);
  setValue("beep_threshold", m_beep_resistance);

  // Ensure settings are written to disk
  std::cerr << "Settings saved." << std::endl;
  sync();
}

// Setter methods with automatic value updates
void Settings::setWindowHeight(const int height) {
  m_windowHeight = height;
  setValue("window/height", height);
}

void Settings::setWindowWidth(const int width) {
  m_windowWidth = width;
  setValue("window/width", width);
}

void Settings::setWindowX(const int x) {
  m_windowX = x;
  setValue("window/x", x);
}

void Settings::setWindowY(const int y) {
  m_windowY = y;
  setValue("window/y", y);
}

void Settings::setDevice(const QString &device) {
  m_device = device;
  setValue("hardware/device", device);
}

void Settings::setRate(Rate rate) {
  // Added implementation for setRate
  if (m_rate != rate) {
    m_rate = rate;
    setValue("rate", rateToString(rate));
    // You might want to call sync() here if you want immediate persistence
    // or rely on the global save() method.
  }
}

void Settings::setBeepShort(bool enabled) {
  if (m_beep_short != enabled) {
    m_beep_short = enabled;
    setValue("beep_short", m_beep_short);
  }
}

void Settings::setBeepDiode(bool enabled) {
  if (m_beep_diode != enabled) {
    m_beep_diode = enabled;
    setValue("beep_diode", m_beep_diode);
  }
}

void Settings::setBeepResistance(int threshold) {
  if (m_beep_resistance != threshold) {
    m_beep_resistance = threshold;
    setValue("beep_threshold", m_beep_resistance);
  }
}

Settings::Rate Settings::stringToRate(QString value, Rate dflt) {
  static const std::map<std::string, Rate> enumMap = {
      {"slow", Rate::SLOW}, {"medium", Rate::MEDIUM}, {"fast", Rate::FAST}};
  std::string lower = value.toLower().toStdString();

  auto it = enumMap.find(lower);
  if (it != enumMap.end()) {
    return it->second; // Found, return the enum value
  }
  return dflt;
}

QString Settings::rateToString(Rate rate) {
  switch (rate) {
  case Rate::SLOW:
    return "slow";
  case Rate::MEDIUM:
    return "medium";
  case Rate::FAST:
    return "fast";
  default:
    return "unknown"; // Should ideally not happen
  }
}
