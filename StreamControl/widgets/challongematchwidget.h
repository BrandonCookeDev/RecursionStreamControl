/**********************************************************************************

Copyright (c) 2015, Antony Clarke
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

**********************************************************************************/

#ifndef WIDGETS_CHALLONGEMATCHWIDGET_H
#define WIDGETS_CHALLONGEMATCHWIDGET_H

#include <QWidget>

class QPushButton;
class QLabel;
class QComboBox;
class QGridLayout;
class QNetworkAccessManager;
class QNetworkReply;
class QAuthenticator;

class ChallongeMatchWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ChallongeMatchWidget(QWidget *parent,
                                  QMap<QString, QObject*>& widgets,
                                  const QMap<QString, QString>& settings,
                                  QString playerOneWidget,
                                  QString playerTwoWidget);

signals:

private:
    QGridLayout     *layout;

    QComboBox       *tournamentsBox;
    QLabel          *tournamentLabel;
    QPushButton     *tournamentFetchButton;

    QComboBox       *matchesBox;
    QLabel          *matchLabel;
    QPushButton     *matchFetchButton;

    QPushButton     *setDataButton;

    QNetworkAccessManager   *manager;

    // So we can set the target widgets with the bracket data
    QMap<QString, QObject*>& widgetList;

    // Needed to get the challonge username/api key
    const QMap<QString, QString>& settings;

    const QString playerOneWidgetId, playerTwoWidgetId;

    QByteArray getAuthHeader() const;

public slots:
    void fetchTournaments();
    void fetchMatches();
    void processTournamentListJson();
    void processTournamentJson();
    void setData();
};

#endif // WIDGETS_CHALLONGEMATCHWIDGET_H