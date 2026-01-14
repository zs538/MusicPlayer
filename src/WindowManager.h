#ifndef WINDOWMANAGER_H
#define WINDOWMANAGER_H

#include <QObject>
#include <QQmlEngine>
#include <QAbstractListModel>
#include <QVariantMap>
#include <QVector>

/**
 * @brief WindowListModel exposes the list of floating windows to QML.
 */
class WindowListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        WindowIdRole = Qt::UserRole + 1,
        WindowTypeRole,
        TitleRole,
        PanelStateRole,
        GeometryRole
    };
    Q_ENUM(Roles)

    explicit WindowListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    struct WindowInfo {
        QString windowId;
        QString windowType;
        QString title;
        QVariantMap panelState;  // {filter, groupBy, panelContextType}
        QVariantMap geometry;
    };

    void addWindow(const WindowInfo &info);
    void removeWindow(const QString &windowId);
    void updateGeometry(const QString &windowId, const QVariantMap &geometry);
    int indexOf(const QString &windowId) const;
    const QVector<WindowInfo> &windows() const { return m_windows; }

private:
    QVector<WindowInfo> m_windows;
};

/**
 * @brief WindowManager manages floating collection windows.
 * 
 * This is a QML singleton that owns the list of floating windows.
 * QML creates actual Window components based on this model.
 */
class WindowManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(WindowListModel* windowsModel READ windowsModel CONSTANT)

public:
    explicit WindowManager(QObject *parent = nullptr);

    static WindowManager *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);
    static WindowManager *instance();

    WindowListModel *windowsModel() const { return m_windowsModel; }

    Q_INVOKABLE QString openCollectionWindow(const QVariantMap &panelState);
    Q_INVOKABLE void closeWindow(const QString &windowId);
    Q_INVOKABLE void updateWindowGeometry(const QString &windowId, int x, int y, int width, int height);

    // Session persistence
    QVariantList windowsToVariant() const;
    void restoreWindowsFromVariant(const QVariantList &list);

signals:
    void windowOpened(const QString &windowId, const QVariantMap &panelState);
    void windowClosed(const QString &windowId);
    void focusWindowRequested(const QString &windowId);

private:
    static WindowManager *s_instance;
    WindowListModel *m_windowsModel;
    int m_nextWindowId = 1;
};

#endif // WINDOWMANAGER_H
