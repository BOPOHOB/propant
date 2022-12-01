#include <QApplication>
#include <QFileDialog>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QStack>
#include <QJsonValue>

#include "propant.h"

/* находит JsonValue в объекте d по пути path и применяет к нему функцию apply,
 * возвращает новый объект совпадающий с d но вместо значения по пути path лежит результат работы apply
 */
QJsonObject insertInto(QJsonObject d, const QStringList& path, const std::function<QJsonValue(const QJsonValue&)>& apply)
{
  QStack<QJsonValue> v;
  v.push(d);
  for (const QString& key: path) {
    Q_ASSERT(v.top().isObject());
    QJsonObject pos(v.top().toObject());
    Q_ASSERT(pos.contains(key));
    v.push(pos[key]);
  }
  QJsonValue result(apply(v.pop()));
  for (QStringList::const_reverse_iterator it(path.rbegin()); it != path.rend(); ++it) {
    QJsonObject acceptor(v.pop().toObject());
    acceptor[*it] = result;
    result = acceptor;
  }
  Q_ASSERT(v.empty());
  return result.toObject();
}

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);

  // при разработке можно сюда вписывать путь к отладочному файлу чтобы не указываеть его постоянно руками
  QString f("C:\\Users\\voronov.ps\\Downloads\\Telegram Desktop\\Пропанты РН-ГРИД 2022.prp");
  if (!QFile::exists(f)) {
      f = QFileDialog::getOpenFileName(nullptr, "Select FracProFile", QString(), "FracPro (*.prp)");
  }

  QString data([&f]()->QString{
    QFile file(f);
    file.open(QFile::ReadOnly);
    QByteArray raw(file.readAll());
    return QStringDecoder(QStringConverter::Encoding::System).decode(raw);
  }());

  QVector<QStringList> chunks(1);

  for (const QString& row: data.split('\n')) {
    if (row.isEmpty()) {
      if (!chunks.last().isEmpty()) {
        chunks.push_back(QStringList());
      }
    } else if (!row.startsWith("NOTE**")) {
      chunks.last().push_back(row);
    }
  }

  QJsonArray result;
  QSet<QString> vendors;
  QVector<Propant> propants;

  for (const QStringList& chunk: chunks) if (!chunk.isEmpty()) {
    propants.emplaceBack(chunk);
    vendors.insert(propants.last().vendor);
    result.push_back(propants.last().toJSON());
  }

  QFile outputFile(QProcessEnvironment::systemEnvironment().value("LOCALAPPDATA") + "\\CyberFrac\\Databases\\databases.json");

  if (!outputFile.exists()) {
    outputFile.setFileName(QFileDialog::getOpenFileName(nullptr, "Select database file", QString(), "JSON (*.json)"));
  }

  // добавляем сами пропанты
  outputFile.open(QFile::ReadWrite);
  QJsonDocument forInsertations(QJsonDocument::fromJson(outputFile.readAll()));
  QJsonObject updated(insertInto(forInsertations.object(), QString("DataBases/ProppantModel/data").split('/'), [&result](const QJsonValue& v)->QJsonValue{
    Q_ASSERT(v.isArray());
    QJsonArray point(v.toArray());
    for (const QJsonValue& itm: result) {
      point.push_back(itm);
    }
    return point;
  }));
  updated = insertInto(updated, QString("DataBases/ProppantModel/rowCount").split('/'), [&result](const QJsonValue& v)->QJsonValue{
    Q_ASSERT(v.isDouble());
    return v.toInt() + result.size();
  });

  // Раскладываем пропанты по группам
  QMap<QString, int> vendorsIds;
  updated = insertInto(updated, QString("DataBases/ProppantModel/groups").split('/'), [&result, &vendors, &vendorsIds](const QJsonValue& v)->QJsonValue{
    Q_ASSERT(v.isArray());
    QVector<QString> knownVendors;
    for (QJsonValue knownVendor: v.toArray()) {
      knownVendors.push_back(knownVendor.toString());
    }
    for (const QString& vendor: vendors) {
      int id(knownVendors.indexOf(vendor));
      if (id == -1) {
        id = knownVendors.size();
        knownVendors.push_back(vendor);
      }
      vendorsIds.insert(vendor, id);
    }
    QJsonArray allVendors;
    for (const QString &vendor: knownVendors) {
      allVendors.push_back(vendor);
    }
    return allVendors;
  });
  updated = insertInto(updated, QString("DataBases/ProppantModel/groupsIndex").split('/'), [&result, &propants, &vendorsIds](const QJsonValue& v)->QJsonValue{
    Q_ASSERT(v.isArray());
    QJsonArray index(v.toArray());
    for (const Propant& prop: propants) {
      index.push_back(prop.key());
      index.push_back(vendorsIds[prop.vendor]);
    }
    return index;
  });

  outputFile.seek(0);
  outputFile.write(QJsonDocument(updated).toJson());
  outputFile.close();

  return 0;
}
