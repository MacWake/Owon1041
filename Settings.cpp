#include "Settings.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace {
std::string Trim(const std::string &s) {
  const char *ws = " \t\r\n";
  const auto b = s.find_first_not_of(ws);
  if (b == std::string::npos) {
    return "";
  }
  const auto e = s.find_last_not_of(ws);
  return s.substr(b, e - b + 1);
}

std::string ToLower(std::string s) {
  for (auto &c : s) {
    c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  }
  return s;
}
} // namespace

Settings::Settings() = default;

void Settings::init() { load(); }

std::string Settings::configFilePath() {
#ifdef _WIN32
  char *appData = nullptr;
  size_t len = 0;
  _dupenv_s(&appData, &len, "APPDATA");
  std::string base = appData ? appData : ".";
  if (appData) {
    free(appData);
  }
  return base + "\\MacWake\\Owon1041.ini";
#else
  const char *home = getenv("HOME");
  std::string homeDir = home ? home : ".";
#ifdef __APPLE__
  return homeDir + "/Library/Preferences/MacWake/Owon1041.conf";
#else
  const char *xdg = getenv("XDG_CONFIG_HOME");
  std::string base = (xdg && *xdg) ? xdg : homeDir + "/.config";
  return base + "/MacWake/Owon1041.conf";
#endif
#endif
}

void Settings::load() {
  std::ifstream in(configFilePath());
  if (!in) {
    return;
  }
  std::string section;
  std::string line;
  std::map<std::string, std::string> values;
  while (std::getline(in, line)) {
    line = Trim(line);
    if (line.empty() || line[0] == '#' || line[0] == ';') {
      continue;
    }
    if (line.front() == '[' && line.back() == ']') {
      section = Trim(line.substr(1, line.size() - 2));
      continue;
    }
    const auto eq = line.find('=');
    if (eq == std::string::npos) {
      continue;
    }
    std::string key = Trim(line.substr(0, eq));
    if (!section.empty()) {
      key = section + "/" + key;
    }
    values[key] = Trim(line.substr(eq + 1));
  }
  auto get = [&](const std::string &key, const std::string &dflt) {
    auto it = values.find(key);
    if (it != values.end()) {
      return it->second;
    }
    // Keys written by save() live under [general].
    it = values.find("general/" + key);
    return it != values.end() ? it->second : dflt;
  };
  auto getInt = [&](const std::string &key, int dflt) {
    try {
      return std::stoi(get(key, std::to_string(dflt)));
    } catch (...) {
      return dflt;
    }
  };
  m_windowHeight = getInt("window/height", m_windowHeight);
  m_windowWidth = getInt("window/width", m_windowWidth);
  m_windowX = getInt("window/x", m_windowX);
  m_windowY = getInt("window/y", m_windowY);
  const auto device = get("hardware/device", m_device);
  if (!device.empty() || values.count("hardware/device")) {
    m_device = device;
  }
  m_rate = stringToRate(get("rate", rateToString(m_rate)), m_rate);
  m_beep_short = get("beep_short", m_beep_short ? "true" : "false") == "true";
  m_beep_diode = get("beep_diode", m_beep_diode ? "true" : "false") == "true";
  m_beep_resistance = getInt("beep_threshold", m_beep_resistance);
}

void Settings::save() {
  const std::string path = configFilePath();
  const auto sep = path.find_last_of("/\\");
  if (sep != std::string::npos) {
    const std::string dir = path.substr(0, sep);
#ifdef _WIN32
    CreateDirectoryA(dir.c_str(), nullptr);
    const auto parent = dir.find_last_of("/\\");
    if (parent != std::string::npos) {
      CreateDirectoryA(dir.substr(0, parent).c_str(), nullptr);
    }
#else
    mkdir(dir.c_str(), 0755);
    if (sep > 0) {
      const auto parent = dir.find_last_of("/\\");
      if (parent != std::string::npos) {
        mkdir(dir.substr(0, parent).c_str(), 0755);
      }
    }
#endif
  }
  std::ofstream out(path, std::ios::trunc);
  if (!out) {
    std::cerr << "Could not write settings to " << path << std::endl;
    return;
  }
  out << "[window]\nheight=" << m_windowHeight << "\nwidth=" << m_windowWidth
      << "\nx=" << m_windowX << "\ny=" << m_windowY << "\n"
      << "[hardware]\ndevice=" << m_device << "\n"
      << "[general]\nrate=" << rateToString(m_rate)
      << "\nbeep_short=" << (m_beep_short ? "true" : "false")
      << "\nbeep_diode=" << (m_beep_diode ? "true" : "false")
      << "\nbeep_threshold=" << m_beep_resistance << "\n";
  std::cerr << "Settings saved." << std::endl;
}

void Settings::setWindowHeight(const int height) { m_windowHeight = height; }
void Settings::setWindowWidth(const int width) { m_windowWidth = width; }
void Settings::setWindowX(const int x) { m_windowX = x; }
void Settings::setWindowY(const int y) { m_windowY = y; }
void Settings::setDevice(const std::string &device) { m_device = device; }

void Settings::setRate(Rate rate) { m_rate = rate; }

void Settings::setBeepShort(bool enabled) { m_beep_short = enabled; }
void Settings::setBeepDiode(bool enabled) { m_beep_diode = enabled; }
void Settings::setBeepResistance(int threshold) { m_beep_resistance = threshold; }

Settings::Rate Settings::stringToRate(const std::string &value, Rate dflt) {
  const std::string lower = ToLower(Trim(value));
  if (lower == "slow") {
    return Rate::SLOW;
  }
  if (lower == "medium") {
    return Rate::MEDIUM;
  }
  if (lower == "fast") {
    return Rate::FAST;
  }
  return dflt;
}

std::string Settings::rateToString(Rate rate) {
  switch (rate) {
  case Rate::SLOW:
    return "slow";
  case Rate::MEDIUM:
    return "medium";
  case Rate::FAST:
    return "fast";
  default:
    return "unknown";
  }
}
