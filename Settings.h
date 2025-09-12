#ifndef SETTINGS_H
#define SETTINGS_H

#include <QSettings>
#include <QString>

class Settings : public QSettings {
  Q_OBJECT

public:
  enum class Rate { SLOW, MEDIUM, FAST };

  explicit Settings(QObject *parent = nullptr);

  Settings(const QString &organization, const QString &application,
           QObject *parent = nullptr);

  void init();

  void load();

  void save();

  // Getter methods
  int windowHeight() const { return m_windowHeight; }
  int windowWidth() const { return m_windowWidth; }
  int windowX() const { return m_windowX; }
  int windowY() const { return m_windowY; }
  QString device() const { return m_device; }
  Rate getRate() const { return m_rate; }
  bool getBeepShort() const { return m_beep_short; }
  bool getBeepDiode() const { return m_beep_diode; }
  int getBeepResistance() const { return m_beep_resistance; }

  // Setter methods
  void setWindowHeight(int height);

  void setWindowWidth(int width);

  void setWindowX(int x);

  void setWindowY(int y);

  void setDevice(const QString &device);

  void setRate(Rate rate); // Added setter for rate
  void setBeepShort(bool enabled);

  void setBeepDiode(bool enabled);

  void setBeepResistance(int threshold);

  static Rate stringToRate(QString value, Rate dflt);

  static QString rateToString(Rate rate);

private:
  // Default values (can be overridden by loaded settings)
  int m_windowHeight = 162;
  int m_windowWidth = 580;
  int m_windowX = 100;
  int m_windowY = 100;
  QString m_device = "";
  Rate m_rate = Rate::FAST;
  bool m_beep_short = true;
  bool m_beep_diode = true;
  int m_beep_resistance = 50;
};

#endif // SETTINGS_H
