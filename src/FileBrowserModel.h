#ifndef FILEBROWSERMODEL_H
#define FILEBROWSERMODEL_H

#include <QAbstractListModel>
#include <QStringList>

class FileBrowserModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString displayPath READ displayPath NOTIFY pathChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY pathChanged)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY pathChanged)

public:
    explicit FileBrowserModel(QObject *parent = nullptr);

    enum Roles { FileNameRole = Qt::UserRole + 1, FilePathRole, FileUrlRole, IsDirRole, EntryTypeRole };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString displayPath() const;
    bool canGoBack() const { return m_historyIdx > 0; }
    bool canGoForward() const { return m_historyIdx < m_history.size() - 1; }

    Q_INVOKABLE void goTo(const QString &path);
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void goForward();
    Q_INVOKABLE void goUp();
    Q_INVOKABLE void goHome();

signals:
    void pathChanged();

private:
    struct Entry { QString name, path, type; bool isDir; };
    void navigate(const QString &path, bool addToHistory);
    void load();

    QList<Entry> m_entries;
    QStringList m_history;
    int m_historyIdx = -1;
    QString m_path;
};

#endif
