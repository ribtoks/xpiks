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

#include "spellchecksuggestionmodel.h"
#include <QQmlEngine>

#include "spellsuggestionsitem.h"
#include "../Models/artworkmetadata.h"
#include "spellcheckerservice.h"
#include "ispellcheckable.h"
#include "../Commands/commandmanager.h"

namespace SpellCheck {
    SpellCheckSuggestionModel::SpellCheckSuggestionModel():
        QAbstractListModel(),
        Common::BaseEntity()
    {
    }

    SpellCheckSuggestionModel::~SpellCheckSuggestionModel() {
        qDeleteAll(m_SuggestionsList);
    }

    QObject *SpellCheckSuggestionModel::getSuggestionItself(int index) const {
        SpellSuggestionsItem *item = NULL;

        if (0 <= index && index < m_SuggestionsList.length()) {
            item = m_SuggestionsList.at(index);
            QQmlEngine::setObjectOwnership(item, QQmlEngine::CppOwnership);
        }

        return item;
    }

    void SpellCheckSuggestionModel::clearModel() {
        beginResetModel();
        qDeleteAll(m_SuggestionsList);
        m_SuggestionsList.clear();
        endResetModel();
    }

    void SpellCheckSuggestionModel::submitCorrections() const {
        foreach (SpellSuggestionsItem *item, m_SuggestionsList) {
            if (item->getIsSelected()) {
                item->replaceToSuggested(m_CurrentItem);
            }
        }
    }

    void SpellCheckSuggestionModel::setupModel(SpellCheckerService *service, SpellCheck::ISpellCheckable *item) {
        Q_ASSERT(service != NULL);
        Q_ASSERT(item != NULL);

        QVector<SpellSuggestionsItem*> suggestionsRequests = item->createKeywordsSuggestionsList();
        QVector<SpellSuggestionsItem*> executedRequests = setupSuggestions(suggestionsRequests);

        beginResetModel();
        m_CurrentItem = item;
        qDeleteAll(m_SuggestionsList);
        m_SuggestionsList.clear();
        m_SuggestionsList << executedRequests;
        endResetModel();
    }

    QVector<SpellSuggestionsItem *> SpellCheckSuggestionModel::setupSuggestions(const QVector<SpellSuggestionsItem *> &items) {
        SpellCheckerService *service = m_CommandManager->getSpellCheckerService();
        // another vector for requests with available suggestions
        QVector<SpellSuggestionsItem*> executedRequests;
        executedRequests.reserve(items.length());

        foreach (SpellSuggestionsItem* item, items) {
            QStringList suggestions = service->suggestCorrections(item->getWord());
            if (!suggestions.isEmpty()) {
                item->setSuggestions(suggestions);
                executedRequests.append(item);
            } else {
                delete item;
            }
        }

        return executedRequests;
    }

    void SpellCheckSuggestionModel::setAllSelected(bool selected) {
        int size = m_SuggestionsList.length();

        for (int i = 0; i < size; ++i) {
            SpellSuggestionsItem *item = m_SuggestionsList.at(i);
            item->setIsSelected(selected);
        }

        emit dataChanged(index(0), index(size - 1), QVector<int>() << IsSelectedRole);
        emit selectAllChanged();
    }

    int SpellCheckSuggestionModel::rowCount(const QModelIndex &parent) const {
        Q_UNUSED(parent);
        return m_SuggestionsList.length();
    }

    QVariant SpellCheckSuggestionModel::data(const QModelIndex &index, int role) const {
        int row = index.row();
        if (row < 0 || row >= m_SuggestionsList.length()) { return QVariant(); }

        SpellSuggestionsItem *item = m_SuggestionsList.at(row);

        switch (role) {
        case WordRole:
            return item->getWord();
        case ReplacementIndexRole:
            return item->getReplacementIndex();
        case IsSelectedRole:
            return item->getIsSelected();
        case ReplacementOriginRole:
            return item->getReplacementOrigin();
        default:
            return QVariant();
        }
    }

    Qt::ItemFlags SpellCheckSuggestionModel::flags(const QModelIndex &index) const {
        int row = index.row();
        if (row < 0 || row >= m_ArtworkList.length()) {
            return Qt::ItemIsEnabled;
        }

        return QAbstractItemModel::flags(index) | Qt::ItemIsEditable;
    }

    bool SpellCheckSuggestionModel::setData(const QModelIndex &index, const QVariant &value, int role) {
        int row = index.row();
        if (row < 0 || row >= m_SuggestionsList.length()) { return false; }
        int roleToUpdate = 0;

        switch (role) {
        case EditIsSelectedRole:
            m_SuggestionsList.at(row)->setIsSelected(value.toBool());
            roleToUpdate = IsSelectedRole;
            break;
        default:
            return false;
        }

        emit dataChanged(index, index, QVector<int>() << roleToUpdate);

        return true;
    }

    QHash<int, QByteArray> SpellCheckSuggestionModel::roleNames() const {
        QHash<int, QByteArray> roles;
        roles[WordRole] = "word";
        roles[ReplacementIndexRole] = "replacementindex";
        roles[IsSelectedRole] = "isselected";
        roles[EditIsSelectedRole] = "editisselected";
        roles[ReplacementOriginRole] = "replacementorigin";
        return roles;
    }
}
