#include "propant.h"

#include <QStringConverter>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QVariant>
#include <QUuid>
#include <QJsonDocument>

#include <QDebug>

namespace {

// превращает строку из чисел разделённых пробелами в массив чисел
QVector<double> fromString(const QString& s) {
  QVector<double> v;
  for (const QString& c: s.split(' ')) {
    bool isOk;
    v.emplaceBack(c.toDouble(&isOk));
    Q_ASSERT(isOk);
  }
  return v;
}
}

/*
 * генератор ключа для пропанта. чтобы сделать такой генератор для других типов данных нужно подменить idNamespace
 * для кислот он:    "3dccf42b-2566-4002-962f-26be5a942830"
 * для оборудования: "24182182-3c65-45aa-b105-8856f6de1eb9"
 * для жидкостей:    "da2fd6d0-2595-4335-bf4b-cad4d825e310"
 * для литотипов:    "77aad063-cbe8-483b-be6f-43473925e3c8"
 */

QJsonValue Propant::key() const
{
  const auto code = QVariant(name).toByteArray().toBase64();
  QUuid idNamespace("5f70df1f-1a9f-4473-b63b-71b778e09fad");
  return QVariant(QUuid::createUuidV3(idNamespace, code).toString()).toJsonValue();
}

Propant::Propant(const QStringList & list)
{
  QMap<QString, QString> note;
  for (const QString& row: list) {
    const size_t split(row.indexOf(' '));
    QString name(row.mid(0, split));
    QString data(row.mid(split + 1));
    note[name] = data;
  }

  Q_ASSERT(note.contains("PROPNAME"));
  Q_ASSERT(note.contains("PROPDESC"));
  Q_ASSERT(note.contains("PROPPOROS"));
  Q_ASSERT(note.contains("PROPDIAM"));
  Q_ASSERT(note.contains("PROPPERM"));
  Q_ASSERT(note.contains("PROPSTRESS"));
  Q_ASSERT(note.contains("PROPVENDOR"));
  Q_ASSERT(note.contains("PROPSG"));

  // тут просто раскладываем это всё хозяйство по объекту чтобы было удобнее писать конвертор в json
  name = note["PROPNAME"];
  description = note["PROPDESC"];
  vendor = note["PROPVENDOR"];
  bool parseCheck;
  diameter = note["PROPDIAM"].toDouble(&parseCheck);
  Q_ASSERT(parseCheck);
  PSG = note["PROPSG"].toDouble(&parseCheck);
  Q_ASSERT(parseCheck);
  stress = fromString(note["PROPSTRESS"]);
  poros = fromString(note["PROPPOROS"]);
  QVector<double> perm(fromString(note["PROPPERM"].mid(note["PROPPERM"].indexOf(' ') + 1)));
  Q_ASSERT(stress.size() == poros.size());
  Q_ASSERT(stress.size() == note["PROPPERM"].mid(0, note["PROPPERM"].indexOf(' ')).toInt());
  Q_ASSERT(stress.size() * 3 == perm.size());
  for (int i(0); i != stress.size(); ++i) {
    permability[0].push_back(perm[i]);
    permability[1].push_back(perm[i + stress.size()]);
    permability[2].push_back(perm[i + stress.size() * 2]);
  }
}

QJsonValue Propant::toJSON() const
{
  QJsonArray result;
  result.push_back(key());
  result.push_back(name);
  result.push_back(description);
  static const QRegularExpression sizeInferer("\\d{1,2}/\\d{1,2}");
  QRegularExpressionMatch match(sizeInferer.match(description));
  result.push_back(match.hasMatch() ? match.capturedTexts().first() : "-/-");// размер
  result.push_back(diameter);
  result.push_back(PSG);// относительная плотность
  result.push_back(1.55);// относительная насыпная плотность
  result.push_back(0.64);// CriticalDensity
  result.push_back(750);// удельная теплоёмкость
  result.push_back(2.5);// к-во частиц заклинивания
  result.push_back(1);// коэффициент скорости
  result.push_back(1);// коэффициент скорости
  QJsonObject matrix;
  matrix.insert("value", "");
  matrix.insert("rowCount", 3);
  matrix.insert("columnCount", 1);
  QJsonArray matrixData;
  for (int row(0); row < 3; ++row) {
    QJsonObject matrixRow;
    static const double vvv[3] = {2.44, 4.88, 9.77};
    matrixRow.insert("value", vvv[row]);// в интерфейсе игнорируется но солверы могут использовать. Это давление для которого указана проницаемость, должна указываться в кг/м^2
    matrixRow.insert("rowCount", stress.size());
    matrixRow.insert("columnCount", 6);

    QJsonArray matrixRowData;
    for (int i(0); i < stress.size(); ++i) {
      QJsonArray matrixCell;
      matrixCell.push_back(stress[i] / 1e5);
      matrixCell.push_back(permability[row][i]);
      matrixCell.push_back(0);// тут в кибере лежит какой-то мусор, возможно Феликс что-то про это знает
      matrixCell.push_back(0);
      matrixCell.push_back(0);
      matrixCell.push_back(poros[i]);
      matrixRowData.push_back(matrixCell);
    }
    matrixRow.insert("data", matrixRowData);
    QJsonArray matrixRowWrap;
    matrixRowWrap.push_back(matrixRow);
    matrixData.push_back(matrixRowWrap);
  }
  matrix.insert("data", matrixData);
  result.push_back(matrix);
  return result;
}
