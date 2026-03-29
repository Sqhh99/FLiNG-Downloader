#pragma once

#include <QString>
#include <QRegularExpression>

namespace TranslationTextUtils {

inline QString normalizeLookupText(const QString& value)
{
    static const QRegularExpression kIgnoredChars(
        QStringLiteral("[\\s\\-_:：·・'\"()\\[\\]{}.,!?/\\\\]+"));

    QString normalized = value.toLower().trimmed();
    normalized.remove(kIgnoredChars);
    return normalized;
}

inline bool hasNormalizedLookupText(const QString& value)
{
    return !normalizeLookupText(value).isEmpty();
}

}  // namespace TranslationTextUtils
