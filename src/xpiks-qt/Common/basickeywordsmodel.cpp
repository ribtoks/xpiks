/*
 * This file is a part of Xpiks - cross platform application for
 * keywording and uploading images for microstocks
 * Copyright (C) 2014-2015 Taras Kushnir <kushnirTV@gmail.com>
 *
 * Xpiks is distributed under the GNU General Public License, version 3.0
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "basickeywordsmodel.h"
#include <QDebug>
#include "../SpellCheck/spellcheckitem.h"
#include "../SpellCheck/spellsuggestionsitem.h"
#include "../SpellCheck/spellcheckiteminfo.h"
#include "../Helpers/keywordvalidator.h"

namespace Common {
    BasicKeywordsModel::BasicKeywordsModel(QObject *parent):
        QAbstractListModel(parent),
        m_SpellCheckInfo(NULL)
    {
    }

    QVariant BasicKeywordsModel::data(const QModelIndex &index, int role) const {
        int row = index.row();
        if (row < 0 || row >= m_KeywordsList.length()) {
            return QVariant();
        }

        switch (role) {
        case KeywordRole:
            return m_KeywordsList.at(index.row());
        case IsCorrectRole:
            return m_SpellCheckResults.at(index.row());
        default:
            return QVariant();
        }
    }

    bool BasicKeywordsModel::appendKeyword(const QString &keyword) {
        bool added = false;
        const QString &sanitizedKeyword = keyword.simplified().toLower();
        bool isValid = Helpers::isValidKeyword(sanitizedKeyword);

        if (isValid && !m_KeywordsSet.contains(sanitizedKeyword)) {
            int keywordsCount = m_KeywordsList.length();

            m_KeywordsSet.insert(sanitizedKeyword);
            m_SpellCheckResults.append(true);

            beginInsertRows(QModelIndex(), keywordsCount, keywordsCount);
            m_KeywordsList.append(sanitizedKeyword);
            endInsertRows();
            added = true;
        }

        return added;
    }

    bool BasicKeywordsModel::takeKeywordAt(int index, QString &removedKeyword) {
        bool removed = false;

        if (0 <= index && index < m_KeywordsList.length()) {
            const QString &keyword = m_KeywordsList.at(index);
            m_KeywordsSet.remove(keyword);

            beginRemoveRows(QModelIndex(), index, index);
            removedKeyword = m_KeywordsList.takeAt(index);
            m_SpellCheckResults.removeAt(index);
            endRemoveRows();

            removed = true;
        }

        return removed;
    }

    void BasicKeywordsModel::setKeywords(const QStringList &keywordsList) {
        clearKeywords();
        appendKeywords(keywordsList);
    }

    int BasicKeywordsModel::appendKeywords(const QStringList &keywordsList) {
        int appendedCount = 0, size = keywordsList.length();

        for (int i = 0; i < size; ++i) {
            if (appendKeyword(keywordsList.at(i))) {
                appendedCount += 1;
            }
        }

        return appendedCount;
    }

    bool BasicKeywordsModel::setDescription(const QString &value) {
        bool result = value != m_Description;
        if (result) { m_Description = value; }
        return result;
    }

    bool BasicKeywordsModel::setTitle(const QString &value) {
        bool result = value != m_Title;
        if (result) { m_Title = value; }
        return result;
    }

    bool BasicKeywordsModel::isEmpty() const {
        return m_KeywordsList.isEmpty() || m_Description.simplified().isEmpty();
    }

    bool BasicKeywordsModel::isTitleEmpty() const {
        return m_Title.simplified().isEmpty();
    }

    bool BasicKeywordsModel::isDescriptionEmpty() const {
        return m_Description.simplified().isEmpty();
    }

    bool BasicKeywordsModel::containsKeyword(const QString &searchTerm, bool exactMatch) {
        bool hasMatch = false;
        int length = m_KeywordsList.length();

        if (exactMatch) {
            for (int i = 0; i < length; ++i) {
                if (m_KeywordsList.at(i) == searchTerm) {
                    hasMatch = true;
                    break;
                }
            }
        } else {
            for (int i = 0; i < length; ++i) {
                if (m_KeywordsList.at(i).contains(searchTerm, Qt::CaseInsensitive)) {
                    hasMatch = true;
                    break;
                }
            }
        }

        return hasMatch;
    }

    bool BasicKeywordsModel::hasKeywordsSpellError() const {
        bool anyError = false;
        int length = m_SpellCheckResults.length();

        for (int i = 0; i < length; ++i) {
            if (!m_SpellCheckResults[i]) {
                anyError = true;
                break;
            }
        }

        return anyError;
    }

    bool BasicKeywordsModel::hasDescriptionSpellError() const {
        bool hasError = m_SpellCheckInfo->anyDescriptionError();
        return hasError;
    }

    bool BasicKeywordsModel::hasTitleSpellError() const {
        bool hasError = m_SpellCheckInfo->anyTitleError();
        return hasError;
    }

    void BasicKeywordsModel::setSpellStatuses(const QVector<bool> &statuses) {
        Q_ASSERT(statuses.length() == m_SpellCheckResults.length());

        int size = statuses.length();
        for (int i = 0; i < size; ++i) {
            m_SpellCheckResults[i] = statuses[i];
        }
    }

    void BasicKeywordsModel::clearModel() {
        setDescription("");
        setTitle("");
        clearKeywords();
    }

    void BasicKeywordsModel::clearKeywords() {
        beginResetModel();
        m_KeywordsList.clear();
        endResetModel();

        m_SpellCheckResults.clear();
        m_KeywordsSet.clear();
    }

    void BasicKeywordsModel::resetKeywords(const QStringList &keywords) {
        clearKeywords();
        appendKeywords(keywords);
    }

    void BasicKeywordsModel::notifySpellCheckResults() {
        emit spellCheckResultsReady();
        qDebug() << "spellCheckResultsReady() emited";
    }

    void BasicKeywordsModel::updateDescriptionSpellErrors(const QHash<QString, bool> &results) {
        QSet<QString> descriptionErrors;
        QStringList descriptionWords = getDescriptionWords();
        foreach (const QString &word, descriptionWords) {
            if (results.value(word, true) == false) {
                descriptionErrors.insert(word);
            }
        }

        m_SpellCheckInfo->setDescriptionErrors(descriptionErrors);
    }

    void BasicKeywordsModel::updateTitleSpellErrors(const QHash<QString, bool> &results) {
        QSet<QString> titleErrors;
        QStringList titleWords = getTitleWords();
        foreach (const QString &word, titleWords) {
            if (results.value(word, true) == false) {
                titleErrors.insert(word);
            }
        }

        m_SpellCheckInfo->setTitleErrors(titleErrors);
    }

    void BasicKeywordsModel::setSpellCheckResult(SpellCheck::SpellCheckQueryItem *result) {
        int index = result->m_Index;

        if (0 <= index && index < m_SpellCheckResults.length()) {
            if (m_KeywordsList[index] == result->m_Word) {
                m_SpellCheckResults[index] = result->m_IsCorrect;
            }
        }
    }

    QString BasicKeywordsModel::retrieveKeyword(int wordIndex) {
        QString keyword;
        if (0 <= wordIndex && wordIndex < m_KeywordsList.length()) {
            keyword = m_KeywordsList.at(wordIndex);
        }

        return keyword;
    }

    QStringList BasicKeywordsModel::getKeywords() {
        return m_KeywordsList;
    }

    void BasicKeywordsModel::setSpellCheckResults(const QVector<SpellCheck::SpellCheckQueryItem *> &items) {
        Q_ASSERT(m_KeywordsList.length() == m_SpellCheckResults.length());

        int size = items.length();
        for (int i = 0; i < size; ++i) {
            SpellCheck::SpellCheckQueryItem *item = items.at(i);
            int index = item->m_Index;
            if (0 <= index && index < m_KeywordsList.length()) {
                if (m_KeywordsList[index] == item->m_Word) {
                    m_SpellCheckResults[index] = item->m_IsCorrect;
                }
            }
        }

        int indexToUpdate = -1;

        if (items.length() == 1) {
            indexToUpdate = items.first()->m_Index;
        }

        emitSpellCheckChanged(indexToUpdate);
    }

    void BasicKeywordsModel::setSpellCheckResults(const QHash<QString, bool> &results) {
        updateDescriptionSpellErrors(results);
        updateTitleSpellErrors(results);

        notifySpellCheckResults();
    }

    QVector<SpellCheck::SpellSuggestionsItem *> BasicKeywordsModel::createKeywordsSuggestionsList() {
        QVector<SpellCheck::SpellSuggestionsItem *> spellCheckSuggestions;
        int length = m_KeywordsList.length();
        spellCheckSuggestions.reserve(length/2);

        for (int i = 0; i < length; ++i) {
            if (!m_SpellCheckResults[i]) {
                const QString &keyword = m_KeywordsList[i];
                SpellCheck::KeywordSpellSuggestions *suggestionsItem = new SpellCheck::KeywordSpellSuggestions(keyword, i);
                spellCheckSuggestions.append(suggestionsItem);
            }
        }

        return spellCheckSuggestions;
    }

    QVector<SpellCheck::SpellSuggestionsItem *> BasicKeywordsModel::createDescriptionSuggestionsList() {
        QStringList descriptionErrors = m_SpellCheckInfo->retrieveDescriptionErrors();
        int length = descriptionErrors.length();

        QVector<SpellCheck::SpellSuggestionsItem *> spellCheckSuggestions;
        spellCheckSuggestions.reserve(length);

        for (int i = 0; i < length; ++i) {
            const QString &word = descriptionErrors.at(i);
            SpellCheck::DescriptionSpellSuggestions *suggestionsItem = new SpellCheck::DescriptionSpellSuggestions(word);
            spellCheckSuggestions.append(suggestionsItem);
        }

        return spellCheckSuggestions;
    }

    QVector<SpellCheck::SpellSuggestionsItem *> BasicKeywordsModel::createTitleSuggestionsList() {
        QStringList titleErrors = m_SpellCheckInfo->retrieveTitleErrors();
        int length = titleErrors.length();

        QVector<SpellCheck::SpellSuggestionsItem *> spellCheckSuggestions;
        spellCheckSuggestions.reserve(length);

        for (int i = 0; i < length; ++i) {
            const QString &word = titleErrors.at(i);
            SpellCheck::TitleSpellSuggestions *suggestionsItem = new SpellCheck::TitleSpellSuggestions(word);
            spellCheckSuggestions.append(suggestionsItem);
        }

        return spellCheckSuggestions;
    }

    void BasicKeywordsModel::replaceKeyword(int index, const QString &existing, const QString &replacement) {
        if (0 <= index && index < m_KeywordsList.length()) {
            const QString &internal = m_KeywordsList.at(index);
            if (internal == existing) {
                m_KeywordsList[index] = replacement;
                m_SpellCheckResults[index] = true;
                QModelIndex i = this->index(index);
                emit dataChanged(i, i, QVector<int>() << IsCorrectRole << KeywordRole);
            }
        }
    }

    void BasicKeywordsModel::replaceWordInDescription(const QString &word, const QString &replacement) {
        m_Description.replace(word, replacement);
    }

    void BasicKeywordsModel::replaceWordInTitle(const QString &word, const QString &replacement) {
        m_Title.replace(word, replacement);
    }

    void BasicKeywordsModel::connectSignals(SpellCheck::SpellCheckItem *item) {
        QObject::connect(item, SIGNAL(resultsReady(int)), this, SLOT(spellCheckRequestReady(int)));
    }

    QStringList BasicKeywordsModel::getDescriptionWords() const {
        QStringList words = m_Description.split(" ", QString::SkipEmptyParts);
        return words;
    }

    QStringList BasicKeywordsModel::getTitleWords() const {
        QStringList words = m_Title.split(" ", QString::SkipEmptyParts);
        return words;
    }

    void BasicKeywordsModel::spellCheckRequestReady(int index) {
        qDebug() << "SpellCheck results ready at index" << index;
        emitSpellCheckChanged(index);
    }

    void BasicKeywordsModel::emitSpellCheckChanged(int index) {
        int count = m_KeywordsList.length();

        if (index == -1) {
            if (count > 0) {
                QModelIndex start = this->index(0);
                QModelIndex end = this->index(count - 1);
                emit dataChanged(start, end, QVector<int>() << IsCorrectRole);
            }
        } else {
            if (0 <= index && index < count) {
                QModelIndex i = this->index(index);
                emit dataChanged(i, i, QVector<int>() << IsCorrectRole);
            }
        }
    }

    QHash<int, QByteArray> BasicKeywordsModel::roleNames() const {
        QHash<int, QByteArray> roles;
        roles[KeywordRole] = "keyword";
        roles[IsCorrectRole] = "iscorrect";
        return roles;
    }

    void BasicKeywordsModel::resetKeywords() {
        m_KeywordsList.clear();
        m_KeywordsSet.clear();
        m_SpellCheckResults.clear();
    }

    void BasicKeywordsModel::addKeywords(const QString &rawKeywords) {
        QStringList keywordsList = rawKeywords.split(",", QString::SkipEmptyParts);
        int size = keywordsList.size();

        for (int i = 0; i < size; ++i) {
            const QString &keyword = keywordsList.at(i).simplified();
            m_KeywordsList.append(keyword);
            m_SpellCheckResults.append(true);
            m_KeywordsSet.insert(keyword.toLower());
        }
    }

    void BasicKeywordsModel::freeSpellCheckInfo() {
        if (m_SpellCheckInfo != NULL) {
            delete m_SpellCheckInfo;
        }
    }
}
