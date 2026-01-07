#pragma once

#include <QObject>
#include <QAbstractListModel>
#include <QQmlEngine>
#include "ModifierParser.h"

/**
 * ModifierListModel - Modifier list data model
 * Provides data for QML ListView/TableView
 */
class ModifierListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

public:
    enum ModifierRoles {
        NameRole = Qt::UserRole + 1,
        GameVersionRole,
        LastUpdateRole,
        OptionsCountRole,
        UrlRole
    };
    Q_ENUM(ModifierRoles)

    explicit ModifierListModel(QObject* parent = nullptr);

    // QAbstractListModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Data operations
    Q_INVOKABLE void clear();
    Q_INVOKABLE void setModifiers(const QList<ModifierInfo>& modifiers);
    Q_INVOKABLE ModifierInfo getModifier(int index) const;
    Q_INVOKABLE QString getModifierName(int index) const;  // Directly accessible from QML
    Q_INVOKABLE QList<ModifierInfo> getAllModifiers() const { return m_modifiers; }
    Q_INVOKABLE int count() const { return m_modifiers.size(); }

signals:
    void countChanged();

private:
    QList<ModifierInfo> m_modifiers;
};
