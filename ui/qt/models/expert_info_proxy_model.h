/** @file
 *
 * Data model for Expert Info tap data.
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef EXPERT_INFO_PROXY_MODEL_H
#define EXPERT_INFO_PROXY_MODEL_H

#include <config.h>

#include <QSortFilterProxyModel>

class ExpertPacketItem;

class ExpertInfoProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    ExpertInfoProxyModel(QObject *parent = 0);

    enum SeverityMode { Group, Packet };
    enum ExpertProxyColumn {
        colProxySeverity = 0,
        colProxySummary,
        colProxyGroup,
        colProxyProtocol,
        colProxyCount,
        colProxyLast
    };

    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const;

    //GUI helpers
    void setSeverityMode(enum SeverityMode);
    void setSeverityFilter(int severity, bool hide);
    void setSummaryFilter(const QString &filter);

protected:
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const;
    bool filterAcceptItem(ExpertPacketItem& item) const;

    enum SeverityMode severityMode_;
    QList<int> hidden_severities_;

    QString textFilter_;

};

#endif // EXPERT_INFO_PROXY_MODEL_H
