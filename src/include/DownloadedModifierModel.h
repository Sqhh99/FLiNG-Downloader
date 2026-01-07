#pragma once

#include <QObject>
#include <QAbstractListModel>
#include <QQmlEngine>
#include "ModifierManager.h"

/**
 * DownloadedModifierModel - Downloaded modifier list data model
 */
class DownloadedModifierModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

public:
    enum DownloadedRoles {
        NameRole = Qt::UserRole + 1,
        VersionRole,
        GameVersionRole,
        DownloadDateRole,
        FilePathRole,
        UrlRole
    };
    Q_ENUM(DownloadedRoles)

    explicit DownloadedModifierModel(QObject* parent = nullptr);

    // QAbstractListModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Data operations
    Q_INVOKABLE void clear();
    Q_INVOKABLE void setModifiers(const QList<DownloadedModifierInfo>& modifiers);
    Q_INVOKABLE void addModifier(const DownloadedModifierInfo& modifier);
    Q_INVOKABLE void removeModifier(int index);
    Q_INVOKABLE DownloadedModifierInfo getModifier(int index) const;
    Q_INVOKABLE int count() const { return m_modifiers.size(); }

signals:
    void countChanged();

private:
    QList<DownloadedModifierInfo> m_modifiers;
};
