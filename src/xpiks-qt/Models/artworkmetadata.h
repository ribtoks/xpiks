#ifndef IMAGEMETADATA_H
#define IMAGEMETADATA_H

#include <QStringListModel>
#include <QStringList>
#include <QString>
#include <QSet>

namespace Models {
    class ArtworkMetadata {
    public:
        ArtworkMetadata(const QString &imageDescription, const QString &imageFileName,
                      const QString &rawKeywords);

    public:
        const QString &getImageDescription() const { return m_ImageDescription; }
        const QString &getImageFileName() const { return m_ImageFileName; }
        const QStringList &getKeywords() const { return m_KeywordsList; }
        const QSet<QString> &getKeywordsSet() const { return m_KeywordsSet; }
        bool isInDirectory(const QString &directory) const { return m_ImageFileName.startsWith(directory); }
        bool isModified() const { return m_IsModified; }
        bool getIsSelected() const { return m_IsSelected; }

    public:
        void setImageDescription(const QString &value) {
            if (m_ImageDescription != value) {
                m_ImageDescription = value;
                setModified();
            }
        }
        void resetModified() { m_IsModified = false; }
        void setIsSelected(bool isSelected) { m_IsSelected = isSelected; }

    public:
        bool removeKeywordAt(int index);
        bool removeLastKeyword() { return removeKeywordAt(m_KeywordsList.length() - 1); }
        bool appendKeyword(const QString &keyword);

    private:
        void parseKeywords(const QString& rawKeywords);
        void setModified() { m_IsModified = true; }

    private:
        QStringList m_KeywordsList;
        QSet<QString> m_KeywordsSet;
        QString m_ImageFileName;
        QString m_ImageDescription;
        bool m_IsModified;
        bool m_IsSelected;
    };
}

#endif // IMAGEMETADATA_H
