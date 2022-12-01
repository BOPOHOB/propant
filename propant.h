#ifndef PROPANT_H
#define PROPANT_H

#include <QString>
#include <QVector>

class QJsonValue;

struct Propant
{
  QString name;
  QString description;
  QString vendor;
  QVector<double> stress;
  QVector<double> permability[3];
  QVector<double> poros;
  double diameter;
  double PSG;
  explicit Propant(const QStringList&);

  QJsonValue toJSON() const;

  QJsonValue key() const;
};

#endif // PROPANT_H
