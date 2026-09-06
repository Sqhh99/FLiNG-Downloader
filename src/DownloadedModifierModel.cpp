#include "DownloadedModifierModel.h"

DownloadedModifierModel::DownloadedModifierModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int DownloadedModifierModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_modifiers.size();
}

QVariant DownloadedModifierModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_modifiers.size())
        return QVariant();

    const DownloadedModifierInfo& modifier = m_modifiers.at(index.row());

    switch (role) {
    case NameRole:
        return modifier.name;
    case VersionRole:
        return modifier.version;
    case GameVersionRole:
        return modifier.gameVersion;
    case DownloadDateRole:
        return modifier.downloadDate.toString("yyyy-MM-dd HH:mm");
    case FilePathRole:
        return modifier.filePath;
    case UrlRole:
        return modifier.url;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> DownloadedModifierModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[VersionRole] = "version";
    roles[GameVersionRole] = "gameVersion";
    roles[DownloadDateRole] = "downloadDate";
    roles[FilePathRole] = "filePath";
    roles[UrlRole] = "url";
    return roles;
}

void DownloadedModifierModel::clear()
{
    beginResetModel();
    m_modifiers.clear();
    endResetModel();
    emit countChanged();
}

void DownloadedModifierModel::setModifiers(const QList<DownloadedModifierInfo>& modifiers)
{
    beginResetModel();
    m_modifiers = modifiers;
    endResetModel();
    emit countChanged();
}

void DownloadedModifierModel::addModifier(const DownloadedModifierInfo& modifier)
{
    beginInsertRows(QModelIndex(), m_modifiers.size(), m_modifiers.size());
    m_modifiers.append(modifier);
    endInsertRows();
    emit countChanged();
}

void DownloadedModifierModel::updateModifier(int index, const DownloadedModifierInfo& modifier)
{
    if (index < 0 || index >= m_modifiers.size())
        return;

    m_modifiers[index] = modifier;
    const QModelIndex changed = createIndex(index, 0);
    emit dataChanged(changed, changed);
}

void DownloadedModifierModel::removeModifier(int index)
{
    if (index < 0 || index >= m_modifiers.size())
        return;

    beginRemoveRows(QModelIndex(), index, index);
    m_modifiers.removeAt(index);
    endRemoveRows();
    emit countChanged();
}

DownloadedModifierInfo DownloadedModifierModel::getModifier(int index) const
{
    if (index >= 0 && index < m_modifiers.size()) {
        return m_modifiers.at(index);
    }
    return DownloadedModifierInfo();
}
