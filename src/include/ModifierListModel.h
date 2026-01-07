#pragma once

#include <QObject>
#include <QAbstractListModel>
#include <QQmlEngine>
#include "ModifierParser.h"

/**
 * ModifierListModel - 修改器列表数据模型
 * 为 QML ListView/TableView 提供数据
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

    // 数据操作
    Q_INVOKABLE void clear();
    Q_INVOKABLE void setModifiers(const QList<ModifierInfo>& modifiers);
    Q_INVOKABLE ModifierInfo getModifier(int index) const;
    Q_INVOKABLE QString getModifierName(int index) const;  // QML 可直接访问
    Q_INVOKABLE QList<ModifierInfo> getAllModifiers() const { return m_modifiers; }
    Q_INVOKABLE int count() const { return m_modifiers.size(); }

signals:
    void countChanged();

private:
    QList<ModifierInfo> m_modifiers;
};
