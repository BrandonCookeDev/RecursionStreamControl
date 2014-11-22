/**********************************************************************************

Copyright (c) 2012, Tan Yu Sheng
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

#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include "mainwindow.h"
#include "configwindow.h"
#include <QtDebug>
#include <QShortcut>
#include <QMessageBox>
#include <QComboBox>
#include <QAction>
#include <QLabel>
#include <QInputDialog>
#include <QToolButton>
#include <QMenu>
#include <QSpinBox>
#include <QLineEdit>
#include <QPainter>
#include <QToolbar>
#include <QPushButton>
#include <QCheckBox>
#include <QScrollArea>
#include "csv.h"
#include <QCompleter>
#include <QFileInfo>
#include <QDir>
#include "ScCompleter.h"
#include <QStringListModel>
#include "ScLineEdit.h"
#include "scradiogroup.h"
#include "sctsbutton.h"
#include "scsetbutton.h"
#include <QRadioButton>
#include "twitterwidget.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include "twitterhandler.h"

#ifdef Q_OS_WIN
#include <QTimer>
#include "windows.h"
#include "win_keyhelper.h"
#endif
#ifdef Q_OS_MAC
    #include <Carbon/Carbon.h>
#endif

MainWindow::MainWindow()
{

    if (objectName().isEmpty())
                setObjectName(QStringLiteral("MainWindow"));

    //Set up signalmappers
    resetMapper = new QSignalMapper (this) ;
    swapMapper = new QSignalMapper (this) ;

    //some defaults
    resize(50, 50);
    setFixedSize(50, 50);

    QIcon icon;
    icon.addFile(QStringLiteral(":/StreamControl/icons/r1logo2trans-crop.png"), QSize(), QIcon::Normal, QIcon::Off);
    setWindowIcon(icon);
    setAnimated(true);
    setWindowFlags(windowFlags()^Qt::WindowMaximizeButtonHint);

    cWindow = new ConfigWindow(this);

    toolBar = new QToolBar(this);
    toolBar->setObjectName(QStringLiteral("toolBar"));
    toolBar->setLayoutDirection(Qt::LeftToRight);
    toolBar->setMovable(false);
    toolBar->setFloatable(false);
    addToolBar(Qt::TopToolBarArea, toolBar);

    QAction* actionSave = new QAction(this);
    actionSave->setObjectName(QStringLiteral("actionSave"));
    QIcon icon1;
    icon1.addFile(QStringLiteral(":/StreamControl/icons/fugue/bonus/icons-24/disk.png"), QSize(), QIcon::Normal, QIcon::Off);
    actionSave->setIcon(icon1);
    actionSave->setShortcuts(QKeySequence::Save);
    connect(actionSave,SIGNAL( triggered() ),this,SLOT( saveData() ));

    QMenu *configMenu = new QMenu();
    QAction *actionConfig = new QAction("Configuration", this);
    configMenu->addAction(actionConfig);
    QAction *actionAlwaysOnTop = new QAction("Always on top", this);
    actionAlwaysOnTop->setCheckable(true);
    configMenu->addAction(actionAlwaysOnTop);


    QToolButton* configButton = new QToolButton();
    configButton->setMenu(configMenu);
    configButton->setPopupMode(QToolButton::InstantPopup);
    QIcon configIcon;
    configIcon.addFile(QString::fromUtf8(":/StreamControl/icons/fugue/bonus/icons-24/gear.png"), QSize(), QIcon::Normal, QIcon::Off);
    configButton->setIcon(configIcon);
    connect(actionConfig,SIGNAL( triggered() ),this,SLOT( openConfig() ));
    connect(actionAlwaysOnTop,SIGNAL(toggled(bool)),this,SLOT( toggleAlwaysOnTop(bool) ));

    needLink = false;
    th = new twitterHandler();

    //code to add non buttons to toolbar
    QLabel* spaceLabel = new QLabel("   ");

    toolBar->addAction(actionSave);
    toolBar->addWidget(configButton);
    toolBar->addWidget(spaceLabel);
    toolBar->setContextMenuPolicy(Qt::PreventContextMenu);

    #ifdef Q_OS_WIN
    keyPoller = new QTimer(this);
    connect(keyPoller, SIGNAL(timeout()), this, SLOT(keyPoll()));
    #endif

    hotkeyDown = false;

    loadSettings();
    loadLayout();
    loadData();
}

MainWindow::~MainWindow()
{
    deleteHotkeys();
    //delete ui;
}

void MainWindow::keyPoll() {
    #ifdef Q_OS_WIN

    bool foundHotkey = false;

    int modifier = 0;
    if(GetAsyncKeyState(VK_LWIN) & 0x8000)
        modifier |= MOD_WIN;
    if(GetAsyncKeyState(VK_CONTROL) & 0x8000)
        modifier |= MOD_CONTROL;
    if(GetAsyncKeyState(VK_MENU) & 0x8000)
        modifier |= MOD_ALT;
    if(GetAsyncKeyState(VK_SHIFT) & 0x8000)
        modifier |= MOD_SHIFT;

    for (int i = 0; i < hotkeysIndex.count(); i++) {
        int ks = hotkeysIndex.at(i);
        int hotkeyMods = win_keyhelper::getModifiers(ks);
        int hotkeyKey = win_keyhelper::getKey(ks);
        if (hotkeyMods == modifier) {
            if(GetAsyncKeyState(hotkeyKey) & 0x8000) {
                foundHotkey = true;
                if (!hotkeyDown) {
                    performHotkey(i);
                    hotkeyDown = true;
                    qDebug() << "Performing " + hotkeys.at(i).at(0);
                }
            }
        }



    }
    if (!foundHotkey) {
        hotkeyDown = false;
    }
    #endif
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
    //windows
    #ifdef Q_OS_WIN
    if(eventType=="windows_generic_MSG" && !altHotkeyHandling)
    {
        MSG* msg=static_cast<MSG*>(message);
        if(msg->message==WM_HOTKEY)
        {
            UINT fuModifiers = (UINT) LOWORD(msg->lParam);  // key-modifier flags
            UINT uVirtKey = (UINT) HIWORD(msg->lParam);     // virtual-key code

            int hotkey = win_keyhelper::getQKS(fuModifiers,uVirtKey);

            int hotkeyIndex = hotkeysIndex.indexOf(hotkey);

            if (hotkeyIndex != -1) {
                performHotkey(hotkeyIndex);
            }

            return true;

         }
    }
    #endif
    //osx
    #ifdef Q_OS_MAC
    if (eventType=="mac_generic_NSEvent")
    {
        //cast message
        EventRef* msg=static_cast<EventRef*>(message);
    }
    #endif
 return false;

}

void MainWindow::loadSettings() {

    QFile file("settings.xml");
    QString outputPath;
    QString layoutPath;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {

        QDomDocument doc;
        doc.setContent(&file);

        file.close();

        QDomElement settingsXML = doc.namedItem("settings").toElement();

        QDomElement outputPathE = settingsXML.namedItem("outputPath").toElement();
        //load old name if coming from 0.3 or lower
        if (outputPathE.isNull()) {
            outputPathE = settingsXML.namedItem("xsplitPath").toElement();
        }
        QDomElement layoutPathE = settingsXML.namedItem("layoutPath").toElement();

        outputPath = outputPathE.text();
        layoutPath = layoutPathE.text();

        QDomElement useCDATAE = settingsXML.namedItem("useCDATA").toElement();

        if (useCDATAE.text() == "1") {
            useCDATA = true;
            settings["useCDATA"] = "1";
        } else {
            useCDATA = false;
            settings["useCDATA"] = "0";
        }

        QDomElement formatE = settingsXML.namedItem("format").toElement();

        saveFormat = formatE.text().toInt();

        if(saveFormat < 1 || saveFormat > 3) {
            saveFormat = SC_XML;
        }

        QDomElement altHotKeyE = settingsXML.namedItem("altHotkeyHandling").toElement();

        if (altHotKeyE.text() == "1") {
            altHotkeyHandling = true;
            settings["altHotkeyHandling"] = "1";
        } else {
            altHotkeyHandling = false;
            settings["altHotkeyHandling"] = "0";
        }

        settings["format"] = QString::number(saveFormat);

        settings["outputPath"] = outputPath;
        settings["layoutPath"] = layoutPath;


    } else {
        QFile xsplitExe32("C:\\Program Files\\SplitMediaLabs\\XSplit\\XSplit.Core.exe");
        QFile xsplitExe("C:\\Program Files (x86)\\SplitMediaLabs\\XSplit\\XSplit.Core.exe");
        if (xsplitExe.exists() || xsplitExe32.exists()) {
            if (xsplitExe.exists()) {
                outputPath = "C:\\Program Files (x86)\\SplitMediaLabs\\XSplit\\";
            } else {
                outputPath = "C:\\Program Files\\SplitMediaLabs\\XSplit\\";
            }
            QMessageBox msgBox;

            msgBox.setText("XSplit Installation detected at default location. Saving settings.");
            msgBox.exec();

            settings["outputPath"] = outputPath;
            settings["useCDATA"] = "0";
            useCDATA = false;
            settings["format"] = QString::number(SC_XML);
            saveFormat = SC_XML;


        } else {
            QMessageBox msgBox;
            msgBox.setText("Please be sure to configure StreamControl before you start.");
            msgBox.exec();
            outputPath = "";
        }

        saveSettings();
    }


}

void MainWindow::saveSettings() {
    QFile file("settings.xml");
    file.open(QIODevice::WriteOnly | QIODevice::Text);

    QDomDocument doc ("StreamControl");

    QDomElement settingsXML = doc.createElement("settings");
    doc.appendChild(settingsXML);

    QDomElement outputPathE = doc.createElement("outputPath");
    settingsXML.appendChild(outputPathE);

    QDomCDATASection outputPathT = doc.createCDATASection(settings["outputPath"]);
    outputPathE.appendChild(outputPathT);

    QDomElement layoutPathE = doc.createElement("layoutPath");
    settingsXML.appendChild(layoutPathE);

    QDomCDATASection layoutPathT = doc.createCDATASection(settings["layoutPath"]);
    layoutPathE.appendChild(layoutPathT);

    QDomElement useCDATAE = doc.createElement("useCDATA");
    settingsXML.appendChild(useCDATAE);

    QDomText useCDATAT = doc.createTextNode(settings["useCDATA"]);
    useCDATAE.appendChild(useCDATAT);

    QDomElement formatE = doc.createElement("format");
    settingsXML.appendChild(formatE);

    QDomText formatT = doc.createTextNode(settings["format"]);
    formatE.appendChild(formatT);

    QDomElement altHotKeyE = doc.createElement("altHotkeyHandling");
    settingsXML.appendChild(altHotKeyE);

    QDomText altHotKeyT = doc.createTextNode(settings["altHotkeyHandling"]);
    altHotKeyE.appendChild(altHotKeyT);

    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << doc.toString();
    file.close();
}

void MainWindow::loadData()
{
    if (saveFormat == SC_XML || saveFormat == SC_Both) {
        loadXML();
    }
    if (saveFormat == SC_JSON) {
        loadJSON();
    }

}

void MainWindow::loadXML() {
    QFile file(settings["outputPath"] + "streamcontrol.xml");
    file.open(QIODevice::ReadOnly | QIODevice::Text);

    QDomDocument doc;
    doc.setContent(&file);

    file.close();

    QDomElement items = doc.namedItem("items").toElement();

    QMapIterator<QString, QObject *> i(widgetList);
    while (i.hasNext()) {
        i.next();
        QString wType = widgetType[i.key()];
        QDomElement currElement = items.namedItem(i.key()).toElement();

        if (wType == "lineEdit") {
            ((ScLineEdit*)widgetList[i.key()])->setText(currElement.text());
        } else if (wType == "spinBox") {
            ((QSpinBox*)widgetList[i.key()])->setValue(currElement.text().toInt());
        } else if (wType == "tsButton") {
            ((ScTSButton*)widgetList[i.key()])->setTimeStamp(currElement.text().toInt());
        } else if (wType == "checkBox") {
            if (currElement.text() == "1") {
                ((QCheckBox*)widgetList[i.key()])->setChecked(true);
            } else {
                ((QCheckBox*)widgetList[i.key()])->setChecked(false);
            }
        } else if (wType == "radioGroup") {
            ((ScRadioGroup*)widgetList[i.key()])->checkFromValue(currElement.text());
        }
    }
}

void MainWindow::loadJSON() {
    QFile file(settings["outputPath"] + "streamcontrol.json");
    file.open(QIODevice::ReadOnly | QIODevice::Text);


    //doc.setContent(&file);
    QString input = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(input.toUtf8());
    QJsonObject Obj = doc.object();
    qDebug() << doc.isNull();

    file.close();

    //QDomElement items = doc.namedItem("items").toElement();

    QMapIterator<QString, QObject *> i(widgetList);
    while (i.hasNext()) {
        i.next();
        QString wType = widgetType[i.key()];

        if (wType == "lineEdit") {
            ((ScLineEdit*)widgetList[i.key()])->setText(Obj[i.key()].toString());
        } else if (wType == "spinBox") {
            ((QSpinBox*)widgetList[i.key()])->setValue(Obj[i.key()].toInt());
        } else if (wType == "tsButton") {
            ((ScTSButton*)widgetList[i.key()])->setTimeStamp(Obj[i.key()].toInt());
        } else if (wType == "checkBox") {
            if (Obj[i.key()].toString() == "1") {
                ((QCheckBox*)widgetList[i.key()])->setChecked(true);
            } else {
                ((QCheckBox*)widgetList[i.key()])->setChecked(false);
            }
        } else if (wType == "radioGroup") {
            ((ScRadioGroup*)widgetList[i.key()])->checkFromValue(Obj[i.key()].toString());
        }
    }
}


void MainWindow::saveData()
{
    QString dataOutput;

    if (saveFormat == SC_XML || saveFormat == SC_Both) {
        QFile file(settings["outputPath"] + "streamcontrol.xml");
        file.open(QIODevice::WriteOnly | QIODevice::Text);

        dataOutput = saveXML();

        QTextStream out(&file);
        out.setCodec("UTF-8");
        out << dataOutput;
        file.close();
    }

    if (saveFormat == SC_JSON || saveFormat == SC_Both) {
        QFile file(settings["outputPath"] + "streamcontrol.json");
        file.open(QIODevice::WriteOnly | QIODevice::Text);

        dataOutput = saveJSON();

        QTextStream out(&file);
        out.setCodec("UTF-8");
        out << dataOutput;
        file.close();
    }

    //CLI
    if (cmdList.length() > 0) {
        doCLI();
    }

    //datasets

    saveDataSets();
}

QString MainWindow::saveXML() {
    QDomDocument doc ("StreamControl");

    QDomElement items = doc.createElement("items");
    doc.appendChild(items);


    QDateTime current = QDateTime::currentDateTime();
    uint timestamp_t = current.toTime_t();

    QDomElement timestamp = doc.createElement("timestamp");
    items.appendChild(timestamp);

    QDomText timestampt = doc.createTextNode(QString::number(timestamp_t));;
    timestamp.appendChild(timestampt);

    QMapIterator<QString, QObject *> i(widgetList);
    while (i.hasNext()) {
        i.next();
        QString wType = widgetType[i.key()];
        QDomElement newItem = doc.createElement(i.key());
        items.appendChild(newItem);

        if (wType == "lineEdit") {
            if (useCDATA) {
                QDomCDATASection newItemt = doc.createCDATASection(((ScLineEdit*)widgetList[i.key()])->text());
                newItem.appendChild(newItemt);
            } else {
                QDomText newItemt = doc.createTextNode(((ScLineEdit*)widgetList[i.key()])->text());
                newItem.appendChild(newItemt);
            }
        } else if (wType == "spinBox") {
            QDomText newItemt = doc.createTextNode(((QSpinBox*)widgetList[i.key()])->text());
            newItem.appendChild(newItemt);
        } else if (wType == "checkBox") {
            QString checked = "0";
            if (((QCheckBox*)widgetList[i.key()])->isChecked()) {
                checked = "1";
            }
            QDomText newItemt = doc.createTextNode(checked);
            newItem.appendChild(newItemt);
        } else if (wType == "comboBox") {
            int currentIndex = ((QComboBox*)widgetList[i.key()])->currentIndex();
            QString value;

            value = ((QComboBox*)widgetList[i.key()])->currentText();

            int valueIndex = ((QComboBox*)widgetList[i.key()])->findText(value);
            if (valueIndex != -1) {
                QVariant data = ((QComboBox*)widgetList[i.key()])->itemData(currentIndex);
                if (data.isNull())
                    value = ((QComboBox*)widgetList[i.key()])->itemText(currentIndex);
                else
                    value = data.toString();
            }

            QDomText newItemt = doc.createTextNode(value);
            newItem.appendChild(newItemt);
        } else if (wType == "radioGroup") {
            QString value = ((ScRadioGroup*)widgetList[i.key()])->getCurrentRadio();

            QDomText newItemt = doc.createTextNode(value);
            newItem.appendChild(newItemt);
        } else if (wType == "tweet") {
            QVector<QMap<QString,QString> > urlArray = ((twitterWidget*)widgetList[i.key()])->getURLs();
            QVector<QMap<QString,QString> > mediaArray = ((twitterWidget*)widgetList[i.key()])->getMedia();
            if (useCDATA) {
                QDomElement username = doc.createElement("username");
                QDomCDATASection username_t = doc.createCDATASection(((twitterWidget*)widgetList[i.key()])->getUsername());

                username.appendChild(username_t);
                newItem.appendChild(username);

                QDomElement twittername = doc.createElement("twittername");
                QDomCDATASection twittername_t = doc.createCDATASection(((twitterWidget*)widgetList[i.key()])->getTwitterName());

                twittername.appendChild(twittername_t);
                newItem.appendChild(twittername);

                QDomElement tweetText = doc.createElement("text");
                QDomCDATASection tweetText_t = doc.createCDATASection(((twitterWidget*)widgetList[i.key()])->getTweetText());

                tweetText.appendChild(tweetText_t);
                newItem.appendChild(tweetText);

                QDomElement created = doc.createElement("created");
                QDomCDATASection created_t = doc.createCDATASection(((twitterWidget*)widgetList[i.key()])->getDate());

                created.appendChild(created_t);
                newItem.appendChild(created);

                QDomElement picFileName = doc.createElement("picFileName");
                QDomCDATASection picFileName_t = doc.createCDATASection(((twitterWidget*)widgetList[i.key()])->getProfilePicFilename());

                picFileName.appendChild(picFileName_t);
                newItem.appendChild(picFileName);

                QDomElement urlContainer = doc.createElement("urls");

                for(int i = 0; i < urlArray.length();i++) {
                    QDomElement urlItem = doc.createElement("url_" + QString::number(i));

                    QDomElement urlE = doc.createElement("url");
                    QDomCDATASection urlE_t = doc.createCDATASection(urlArray[i]["url"]);
                    urlE.appendChild(urlE_t);
                    urlItem.appendChild(urlE);

                    QDomElement displayE = doc.createElement("display_url");
                    QDomCDATASection displayE_t = doc.createCDATASection(urlArray[i]["display_url"]);
                    displayE.appendChild(displayE_t);
                    urlItem.appendChild(displayE);

                    QDomElement expandedE = doc.createElement("expanded_url");
                    QDomCDATASection expandedE_t = doc.createCDATASection(urlArray[i]["expanded_url"]);
                    expandedE.appendChild(expandedE_t);
                    urlItem.appendChild(expandedE);

                    urlContainer.appendChild(urlItem);
                }
                newItem.appendChild(urlContainer);

                QDomElement mediaContainer = doc.createElement("media");

                for(int i = 0; i < mediaArray.length();i++) {
                    QDomElement mediaItem = doc.createElement("media_" + QString::number(i));

                    QDomElement urlE = doc.createElement("url");
                    QDomCDATASection urlE_t = doc.createCDATASection(mediaArray[i]["url"]);
                    urlE.appendChild(urlE_t);
                    mediaItem.appendChild(urlE);

                    QDomElement displayE = doc.createElement("display_url");
                    QDomCDATASection displayE_t = doc.createCDATASection(mediaArray[i]["display_url"]);
                    displayE.appendChild(displayE_t);
                    mediaItem.appendChild(displayE);

                    QDomElement expandedE = doc.createElement("expanded_url");
                    QDomCDATASection expandedE_t = doc.createCDATASection(mediaArray[i]["expanded_url"]);
                    expandedE.appendChild(expandedE_t);
                    mediaItem.appendChild(expandedE);

                    QDomElement typeE = doc.createElement("type");
                    QDomCDATASection typeE_t = doc.createCDATASection(mediaArray[i]["type"]);
                    typeE.appendChild(typeE_t);
                    mediaItem.appendChild(typeE);

                    QDomElement mediaUrlE = doc.createElement("media_url");
                    QDomCDATASection mediaUrlE_t = doc.createCDATASection(mediaArray[i]["media_url"]);
                    mediaUrlE.appendChild(mediaUrlE_t);
                    mediaItem.appendChild(mediaUrlE);

                    if (i == 0 && mediaArray[i]["type"] == "photo"){

                        QDomElement fileNameE = doc.createElement("filename");
                        QDomCDATASection fileNameE_t = doc.createCDATASection(mediaArray[i]["filename"]);
                        fileNameE.appendChild(fileNameE_t);
                        mediaItem.appendChild(fileNameE);
                    }

                    mediaContainer.appendChild(mediaItem);
                }
                newItem.appendChild(mediaContainer);
            } else {
                QDomElement username = doc.createElement("username");
                QDomText username_t = doc.createTextNode(((twitterWidget*)widgetList[i.key()])->getUsername());

                username.appendChild(username_t);
                newItem.appendChild(username);

                QDomElement twittername = doc.createElement("twittername");
                QDomText twittername_t = doc.createTextNode(((twitterWidget*)widgetList[i.key()])->getTwitterName());

                twittername.appendChild(twittername_t);
                newItem.appendChild(twittername);

                QDomElement tweetText = doc.createElement("text");
                QDomText tweetText_t = doc.createTextNode(((twitterWidget*)widgetList[i.key()])->getTweetText());

                tweetText.appendChild(tweetText_t);
                newItem.appendChild(tweetText);

                QDomElement created = doc.createElement("created");
                QDomText created_t = doc.createTextNode(((twitterWidget*)widgetList[i.key()])->getDate());

                created.appendChild(created_t);
                newItem.appendChild(created);

                QDomElement picFileName = doc.createElement("picFileName");
                QDomText picFileName_t = doc.createTextNode(((twitterWidget*)widgetList[i.key()])->getProfilePicFilename());

                picFileName.appendChild(picFileName_t);
                newItem.appendChild(picFileName);

                QDomElement urlContainer = doc.createElement("urls");

                for(int i = 0; i < urlArray.length();i++) {
                    QDomElement urlItem = doc.createElement("url_" + QString::number(i));

                    QDomElement urlE = doc.createElement("url");
                    QDomText urlE_t = doc.createTextNode(urlArray[i]["url"]);
                    urlE.appendChild(urlE_t);
                    urlItem.appendChild(urlE);

                    QDomElement displayE = doc.createElement("display_url");
                    QDomText displayE_t = doc.createTextNode(urlArray[i]["display_url"]);
                    displayE.appendChild(displayE_t);
                    urlItem.appendChild(displayE);

                    QDomElement expandedE = doc.createElement("expanded_url");
                    QDomText expandedE_t = doc.createTextNode(urlArray[i]["expanded_url"]);
                    expandedE.appendChild(expandedE_t);
                    urlItem.appendChild(expandedE);

                    urlContainer.appendChild(urlItem);
                }
                newItem.appendChild(urlContainer);

                QDomElement mediaContainer = doc.createElement("media");

                for(int i = 0; i < mediaArray.length();i++) {
                    QDomElement mediaItem = doc.createElement("media_" + QString::number(i));

                    QDomElement urlE = doc.createElement("url");
                    QDomText urlE_t = doc.createTextNode(mediaArray[i]["url"]);
                    urlE.appendChild(urlE_t);
                    mediaItem.appendChild(urlE);

                    QDomElement displayE = doc.createElement("display_url");
                    QDomText displayE_t = doc.createTextNode(mediaArray[i]["display_url"]);
                    displayE.appendChild(displayE_t);
                    mediaItem.appendChild(displayE);

                    QDomElement expandedE = doc.createElement("expanded_url");
                    QDomText expandedE_t = doc.createTextNode(mediaArray[i]["expanded_url"]);
                    expandedE.appendChild(expandedE_t);
                    mediaItem.appendChild(expandedE);

                    QDomElement typeE = doc.createElement("type");
                    QDomText typeE_t = doc.createTextNode(mediaArray[i]["type"]);
                    typeE.appendChild(typeE_t);
                    mediaItem.appendChild(typeE);

                    QDomElement mediaUrlE = doc.createElement("media_url");
                    QDomText mediaUrlE_t = doc.createTextNode(mediaArray[i]["media_url"]);
                    mediaUrlE.appendChild(mediaUrlE_t);
                    mediaItem.appendChild(mediaUrlE);

                    if (i == 0 && mediaArray[i]["type"] == "photo"){

                        QDomElement fileNameE = doc.createElement("filename");
                        QDomText fileNameE_t = doc.createTextNode(mediaArray[i]["filename"]);
                        fileNameE.appendChild(fileNameE_t);
                        mediaItem.appendChild(fileNameE);
                    }

                    mediaContainer.appendChild(mediaItem);
                }
                newItem.appendChild(mediaContainer);
            }
        } else if (wType == "tsButton") {
            if (((ScTSButton*)widgetList[i.key()])->isActive()) {
                ((ScTSButton*)widgetList[i.key()])->setTimeStamp(timestamp_t);
                ((ScTSButton*)widgetList[i.key()])->setActive(false);
            }

            QDomText newItemt = doc.createTextNode(QString::number(((ScTSButton*)widgetList[i.key()])->getTimeStamp()));
            newItem.appendChild(newItemt);
        }

    }

    return doc.toString();
}

QString MainWindow::saveJSON() {

    QJsonObject Obj;

    QDateTime current = QDateTime::currentDateTime();
    uint timestamp_t = current.toTime_t();

    Obj["timestamp"] = QString::number(timestamp_t);

    QMapIterator<QString, QObject *> i(widgetList);
    while (i.hasNext()) {
        i.next();
        QString wType = widgetType[i.key()];

        if (wType == "lineEdit") {
            Obj[i.key()] = ((ScLineEdit*)widgetList[i.key()])->text();
        } else if (wType == "spinBox") {
            Obj[i.key()] = ((QSpinBox*)widgetList[i.key()])->text();
        } else if (wType == "checkBox") {
            QString checked = "0";
            if (((QCheckBox*)widgetList[i.key()])->isChecked()) {
                checked = "1";
            }
            Obj[i.key()] = checked;
        } else if (wType == "comboBox") {
            int currentIndex = ((QComboBox*)widgetList[i.key()])->currentIndex();
            QString value;

            value = ((QComboBox*)widgetList[i.key()])->currentText();

            int valueIndex = ((QComboBox*)widgetList[i.key()])->findText(value);
            if (valueIndex != -1) {
                QVariant data = ((QComboBox*)widgetList[i.key()])->itemData(currentIndex);
                if (data.isNull())
                    value = ((QComboBox*)widgetList[i.key()])->itemText(currentIndex);
                else
                    value = data.toString();
            }

            Obj[i.key()] = value;
        } else if (wType == "radioGroup") {
            QString value = ((ScRadioGroup*)widgetList[i.key()])->getCurrentRadio();
            Obj[i.key()] = value;
        } else if (wType == "tweet") {
            QVector<QMap<QString,QString> > urlArray = ((twitterWidget*)widgetList[i.key()])->getURLs();
            QVector<QMap<QString,QString> > mediaArray = ((twitterWidget*)widgetList[i.key()])->getMedia();
            QJsonObject tweet;

            tweet["username"] = ((twitterWidget*)widgetList[i.key()])->getUsername();
            tweet["twittername"] = ((twitterWidget*)widgetList[i.key()])->getTwitterName();
            tweet["text"] = ((twitterWidget*)widgetList[i.key()])->getTweetText();
            tweet["created"] = ((twitterWidget*)widgetList[i.key()])->getDate();
            tweet["picFileName"] = ((twitterWidget*)widgetList[i.key()])->getProfilePicFilename();

            QJsonObject urlContainer;

            for(int it = 0; it < urlArray.length();it++) {
                QJsonObject urlItem;

                urlItem["url"] = urlArray[it]["url"];
                urlItem["display_url"] = urlArray[it]["display_url"];
                urlItem["expanded_url"] = urlArray[it]["expanded_url"];

                urlContainer["url_" + QString::number(it)] = urlItem;
            }

            tweet["urls"] = urlContainer;


            QJsonObject mediaContainer;

            for(int it = 0; it < mediaArray.length();it++) {
                QJsonObject mediaItem;

                mediaItem["url"] = mediaArray[it]["url"];
                mediaItem["display_url"] = mediaArray[it]["display_url"];
                mediaItem["expanded_url"] = mediaArray[it]["expanded_url"];
                mediaItem["type"] = mediaArray[it]["type"];
                mediaItem["media_url"] = mediaArray[it]["media_url"];

                if (it == 0 && mediaArray[it]["type"] == "photo"){

                    mediaItem["filename"] = mediaArray[it]["filename"];
                }

                mediaContainer["media_" + QString::number(it)] = mediaItem;
            }

            tweet["media"] = mediaContainer;

            Obj[i.key()] = tweet;

        } else if (wType == "tsButton") {
            if (((ScTSButton*)widgetList[i.key()])->isActive()) {
                ((ScTSButton*)widgetList[i.key()])->setTimeStamp(timestamp_t);
                ((ScTSButton*)widgetList[i.key()])->setActive(false);
            }
            Obj[i.key()] = QString::number(((ScTSButton*)widgetList[i.key()])->getTimeStamp());
        }

    }

    return QJsonDocument(Obj).toJson(QJsonDocument::Indented);

}

void MainWindow::doCLI() {

    qDebug() << "doing cli";

    QList<int> cmdQueue;
    QStringList varsChanged;

    QMapIterator<QString, QString> i(cmdOldValues);
    while (i.hasNext()) {
        i.next();

        QString var = i.key();
        QString oldVal = i.value();
        QString val = "";

        val = getValueByID(var);
        if (val != oldVal) {
            qDebug() << var + " has changed! value:" + val;
            cmdOldValues[var] = val;
            if (!varsChanged.contains(var)) {
                varsChanged.append(var);
            }
        }

    }

    QString var;
    foreach (var,varsChanged) {
        QList<int> cmds = cmdVars[var];
        int cmd;

        foreach (cmd,cmds) {
            if (!cmdQueue.contains(cmd)) {
                if(!cmdIgnores.at(cmd).contains(var))
                cmdQueue.append(cmd);
            }
        }
    }

    execCmdQueue(cmdQueue);

}

QString MainWindow::getValueByID(QString id) {
    QString newValue = "";
    QString wType = "";

    QStringList idSplit = id.split(".");

    if (idSplit.length() > 1) {
        wType = widgetType[idSplit.at(0)];
        if (wType == "tweet") {
                    newValue = "tweet";

        }
    } else {
        wType = widgetType[id];
        if (wType == "lineEdit") {
            newValue = ((ScLineEdit*)widgetList[id])->text();
        } else if (wType == "spinBox") {
            newValue = ((QSpinBox*)widgetList[id])->text();
        } else if (wType == "checkBox") {
            QString checked = "0";
            if (((QCheckBox*)widgetList[id])->isChecked()) {
                checked = "1";
            }
            newValue = checked;
        } else if (wType == "comboBox") {
            int currentIndex = ((QComboBox*)widgetList[id])->currentIndex();
            QString value;

            value = ((QComboBox*)widgetList[id])->currentText();

            int valueIndex = ((QComboBox*)widgetList[id])->findText(value);
            if (valueIndex != -1) {
                QVariant data = ((QComboBox*)widgetList[id])->itemData(currentIndex);
                if (data.isNull())
                    value = ((QComboBox*)widgetList[id])->itemText(currentIndex);
                else
                    value = data.toString();
            }

            newValue = value;
        } else if (wType == "radioGroup") {
            QString value = ((ScRadioGroup*)widgetList[id])->getCurrentRadio();
            newValue = value;
        } else if (wType == "tsButton") {
            newValue = QString::number(((ScTSButton*)widgetList[id])->getTimeStamp());
        }
    }

    return newValue;
}

void MainWindow::execCmdQueue(QList<int> cmdQueue) {
    int cmd;
    //QString cmdStr;

    foreach (cmd,cmdQueue) {
        QString cmdStr = cmdList.at(cmd);
        cmdStr = parseCmd(cmdStr);
        qDebug() << cmdStr;
        QProcess *process = new QProcess(this);
        process->start(cmdStr);
    }
}

QString MainWindow::parseCmd(QString cmdStr) {
    QRegExp rx("(\\[\\$[\\w\\.]+\\])");
    int pos = 0;
    QStringList vars;
    while ((pos = rx.indexIn(cmdStr, pos)) != -1) {
        QString var = rx.cap(0).remove(0,2);
        var.chop(1);
        vars << var;
        pos += rx.matchedLength();
    }
    QString var;
    foreach(var, vars) {
        cmdStr.replace("[$"+var+"]",cmdOldValues[var]);
    }
    return cmdStr;
}

void MainWindow::resetFields(QString widget)
{

    QListIterator<QString> i(resetList[widget]);
    while (i.hasNext()) {
        QString key = i.next();
        if (widgetType[key] == "spinBox") {
            ((QSpinBox*)widgetList[key])->setValue(0);
        } else if (widgetType[key] == "lineEdit") {
            ((ScLineEdit*)widgetList[key])->setText("");
        } else if (widgetType[key] == "checkBox") {
            ((QCheckBox*)widgetList[key])->setChecked(false);
        }
    }
}

void MainWindow::swapFields(QString widget)
{

    QString swSet1 = swapSets[widget][0];
    QString swSet2 = swapSets[widget][1];


    QList<QString> swList1 = swapList[swSet1];
    QList<QString> swList2 = swapList[swSet2];

    for (int i = 0; i < swList1.size(); ++i) {
        QString currField = swList1[i];
        QString newField = swList2[i];
        QString tempData;

        if (widgetType[currField] == "lineEdit"){
            tempData = ((ScLineEdit*)widgetList[currField])->text();
            ((ScLineEdit*)widgetList[currField])->setText(((ScLineEdit*)widgetList[newField])->text());
            ((ScLineEdit*)widgetList[newField])->setText(tempData);
        } else if (widgetType[currField] == "spinBox") {
            tempData = ((QSpinBox*)widgetList[currField])->text();
            ((QSpinBox*)widgetList[currField])->setValue(((QSpinBox*)widgetList[newField])->text().toInt());
            ((QSpinBox*)widgetList[newField])->setValue(tempData.toInt());
        } else if (widgetType[currField] == "checkBox") {
            bool tempCheck = ((QCheckBox*)widgetList[currField])->isChecked();
            ((QCheckBox*)widgetList[currField])->setChecked(((QCheckBox*)widgetList[newField])->isChecked());
            ((QCheckBox*)widgetList[newField])->setChecked(tempCheck);
        }
     }

}

void MainWindow::openConfig() {
    cWindow->setConfig(settings);
    cWindow->show();

    if (cWindow->exec() == 1) {
        QMap<QString, QString> configSettings = cWindow->getConfig();

        settings["outputPath"] = configSettings["outputPath"];
        settings["layoutPath"] = configSettings["layoutPath"];
        settings["useCDATA"] = configSettings["useCDATA"];
        settings["format"] = configSettings["format"];
        settings["altHotkeyHandling"] = configSettings["altHotkeyHandling"];

        if (settings["useCDATA"] == "1") {
            useCDATA = true;
        } else {
            useCDATA = false;
        }

        if (settings["altHotkeyHandling"] == "1") {
            altHotkeyHandling = true;
        } else {
            altHotkeyHandling = false;
        }

        saveFormat = settings["format"].toInt();

        if(saveFormat < 1 || saveFormat > 3) {
            saveFormat = SC_XML;
        }

        saveSettings();

        loadLayout();

        loadData();
    }
}

void MainWindow::toggleAlwaysOnTop(bool on_top) {

    #ifdef Q_OS_WIN
    HWND hWnd = (HWND)this->winId();

    if (on_top)
        SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    else
        SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    #else
    Qt::WindowFlags oldflags = windowFlags(), newflags;

    if( on_top )
        newflags = oldflags | Qt::WindowStaysOnTopHint;
    else
        newflags = oldflags & ~Qt::WindowStaysOnTopHint;

    setWindowFlags( newflags );
    show();
    #endif
}

void MainWindow::loadLayout() {

    layoutIterator = 0;

    clearMaps();
    keyPoller->stop();
    deleteHotkeys();
    //add global save hotkey first
    addHotkey("CTRL+ALT+SHIFT+S","Main","Save");

    QStringList errors;
    QString parseError;
    int parseErrorLine;
    int parseErrorColumn;

    QDomDocument doc;

    QFile file(settings["layoutPath"]);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {

        doc.setContent(&file,&parseError,&parseErrorLine,&parseErrorColumn);

        file.close();
    } else {

        doc = getDefaultLayout();

    }

    if (parseError.length() > 0) {
        errors << "XML Error:\n" + parseError + "\n\nLine:" + QString::number(parseErrorLine) + "\nColumn:" + QString::number(parseErrorColumn);
    } else {
        errors << checkLayout(doc);
    }

    if (errors.length() > 0) {

        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        QString errorMessage = errors.join("\n");
        errorMessage += "\n\nLoading default layout";
        msgBox.setText(errorMessage);
        msgBox.exec();

        doc = getDefaultLayout();
    }

    QDomElement layout = doc.namedItem("layout").toElement();

    //Set up the main window
    int layoutWidth = layout.attribute("width").toInt();
    int layoutHeight = layout.attribute("height").toInt();

    if (layoutWidth < 50) {
        layoutWidth = 50;
    }
    if (layoutHeight < 50) {
        layoutHeight = 50;
    }


    resize(layoutWidth, layoutHeight);
    setFixedSize(layoutWidth, layoutHeight);

    if(layout.attribute("tabbed") == "1") {
        centralWidget = new QTabWidget(this);
    } else {
        centralWidget = new QWidget(this);
    }

    setCentralWidget(centralWidget);

    if(layout.attribute("tabbed") == "1"){
        parseTabLayout(layout,centralWidget);
    } else {
        parseLayout(layout, centralWidget);
    }

    //Do ToolBar
    QDomNode toolBarNode = layout.namedItem("toolBar");

    if (!toolBarNode.isNull()) {
        parseToolBar(toolBarNode);
    }

    QDomNode cliNode = layout.namedItem("cli");

    if (!cliNode.isNull()) {
        parseCLI(cliNode);
    }

    //link o2 if needed
    if (needLink) {
        th->link();
    }

    //connect signal mappers

    connect (resetMapper, SIGNAL(mapped(QString)), this, SLOT(resetFields(QString))) ;
    connect (swapMapper, SIGNAL(mapped(QString)), this, SLOT(swapFields(QString))) ;
    connect (tsMapper, SIGNAL(mapped(QString)), this, SLOT(tsClick(QString))) ;
    connect (setButtonMapper, SIGNAL(mapped(QString)), this, SLOT(setButtonClick(QString))) ;

    //start the keyPoller
    if (altHotkeyHandling) {
        keyPoller->start(16);
    }

}

void MainWindow::parseLayout(QDomElement element, QWidget *parent) {
    QDomNode child = element.firstChildElement();
    while (!child.isNull()) {
        QString tagName = child.toElement().tagName();
        if (tagName == "label") {
            addLabel(child.toElement(), parent);
        } else if (tagName == "line") {
            addLine(child.toElement(), parent);
        } else if (tagName == "button") {
            addButton(child.toElement(), parent);
        } else if (tagName == "lineEdit") {
            addLineEdit(child.toElement(), parent);
        } else if (tagName == "spinBox") {
            addSpinBox(child.toElement(), parent);
        } else if (tagName == "checkBox") {
            addCheckBox(child.toElement(), parent);
        } else if (tagName == "radioGroup") {
            addRadioGroup(child.toElement(), parent);
        } else if (tagName == "comboBox") {
            addComboBox(child.toElement(), parent);
        } else if (tagName == "tweet") {
            addTweetWidget(child.toElement(), parent);
            needLink = true;
        } else if (tagName == "tabSet") {
            QString newTabSet = addTabWidget(child.toElement(), parent);
            parseTabLayout(child.toElement(), visualList[newTabSet]);
        }

        child = child.nextSiblingElement();
    }
}

void MainWindow::parseTabLayout(QDomElement element, QWidget *parent) {
    QDomNode child = element.firstChildElement();
    while (!child.isNull()) {
        QString tagName = child.toElement().tagName();
        if(tagName == "tab") {
            QString newTabName = child.toElement().attribute("name");
            QString newTab = "tab"+QString::number(layoutIterator);

            visualList[newTab] = new QWidget(parent);

            parseLayout(child.toElement(),visualList[newTab]);

            if (child.toElement().attribute("scrollable") == "1") {
                visualList[newTab+"Scroll"] = new QScrollArea(parent);
                visualList[newTab]->adjustSize();
                ((QScrollArea*)visualList[newTab+"Scroll"])->setWidget(((QWidget*)visualList[newTab]));
                ((QScrollArea*)visualList[newTab+"Scroll"])->setFrameShape(QFrame::NoFrame);
                ((QTabWidget*)parent)->addTab(visualList[newTab+"Scroll"],newTabName);
            } else {
                ((QTabWidget*)parent)->addTab(visualList[newTab],newTabName);
            }

            layoutIterator++;
        }

        child = child.nextSiblingElement();
    }
}

void MainWindow::parseToolBar(QDomNode toolBarNode) {

    QDomNode child = toolBarNode.firstChildElement();
    while (!child.isNull()) {
        QString tagName = child.toElement().tagName();

        if (tagName == "label") {
            //add labels
            QString newLabel = "label"+QString::number(layoutIterator);
            visualList[newLabel] = new QLabel();
            visualList[newLabel]->setObjectName(newLabel);
            ((QLabel*)visualList[newLabel])->setText(child.toElement().text());

            toolBar->addWidget(((QLabel*)visualList[newLabel]));

            layoutIterator++;
            //end labels
        } else if (tagName == "comboBox") {
            //add comboboxes
            QDomElement comboBoxElement = child.toElement();
            QString newComboBox = comboBoxElement.attribute("id");
            widgetType[newComboBox] = "comboBox";
            widgetList[newComboBox] = new QComboBox();
            widgetList[newComboBox]->setObjectName(newComboBox);

            int comboBoxWidth = 100;
            if (comboBoxElement.hasAttribute("width")) {
                comboBoxWidth = comboBoxElement.attribute("width").toInt();
            }
            ((QComboBox*)widgetList[newComboBox])->setMinimumWidth(comboBoxWidth);

            if (comboBoxElement.attribute("editable") == "1" || comboBoxElement.attribute("editable") == "true") {
                ((QComboBox*)widgetList[newComboBox])->setEditable(true);

            }

            toolBar->addWidget(((QComboBox*)widgetList[newComboBox]));

            QDomNode combochild = comboBoxElement.firstChildElement();
            int selectedIndex = 0;
            for (int itemIndex = 0; !combochild.isNull(); itemIndex++) {
                QDomElement itemElement = combochild.toElement();
                if (itemElement.tagName() == "comboItem") {

                    QString text = itemElement.text();
                    QString value = itemElement.attribute("value");
                    if (value.isNull())
                        value = text;

                    ((QComboBox*)widgetList[newComboBox])->addItem(text, value);

                    if (itemElement.attribute("selected") == "true")
                        selectedIndex = itemIndex;
                }

                combochild = combochild.nextSiblingElement();
            }

            ((QComboBox*)widgetList[newComboBox])->setCurrentIndex(selectedIndex);
            //end comboboxes
        } else if (tagName == "button") {
            QDomElement buttonElement = child.toElement();
            /*if (element.attribute("type") == "reset") {
                QString newButton = buttonElement.attribute("id");

                QList<QString> resetL = CSV::parseFromString(buttonElement.attribute("reset"))[0];

                widgetType[newButton] = "resetButton";

                resetList.insert(newButton,resetL);

                int buttonWidth = 100;
                if (buttonElement.hasAttribute("width")) {
                    buttonWidth = buttonElement.attribute("width").toInt();
                }
                visualList[newButton] = new QPushButton();
                visualList[newButton]->setObjectName(newButton);

                ((QPushButton*)visualList[newButton])->setMinimumWidth(buttonWidth);

                toolBar->addWidget(((QPushButton*)visualList[newButton]));

                ((QPushButton*)visualList[newButton])->setText(buttonElement.text());
                if (!buttonElement.attribute("tooltip").isEmpty()) {
                    ((QPushButton*)visualList[newButton])->setToolTip(buttonElement.attribute("tooltip"));
                }

                connect(((QPushButton*)visualList[newButton]), SIGNAL(clicked()), resetMapper, SLOT(map()));
                resetMapper -> setMapping (((QPushButton*)visualList[newButton]), newButton) ;
                if(!element.attribute("hotkey").isEmpty()) {
                    addHotkey(element.attribute("hotkey"),newButton,"Reset");
                }


            } else if (element.attribute("type") == "swap") {

                QString newButton = element.attribute("id");

                QList<QString> swapl1 = CSV::parseFromString(element.attribute("swapSet1"))[0];
                QList<QString> swapl2 = CSV::parseFromString(element.attribute("swapSet2"))[0];

                QList<QString> swapset;

                swapset.insert(0,newButton + "1");
                swapset.insert(1,newButton + "2");

                swapSets.insert(newButton,swapset);

                swapList.insert(newButton + "1",swapl1);

                swapList.insert(newButton + "2",swapl2);

                widgetType[newButton] = "swapButton";

                visualList[newButton] = new QPushButton(parent);
                visualList[newButton]->setObjectName(newButton);
                visualList[newButton]->setGeometry(QRect(element.attribute("x").toInt(),
                                                         element.attribute("y").toInt(),
                                                         element.attribute("width").toInt(),
                                                         element.attribute("height").toInt()));
                ((QPushButton*)visualList[newButton])->setText(element.text());
                if (!element.attribute("tooltip").isEmpty()) {
                    ((QPushButton*)visualList[newButton])->setToolTip(element.attribute("tooltip"));
                }

                connect(((QPushButton*)visualList[newButton]), SIGNAL(clicked()), swapMapper, SLOT(map()));
                swapMapper -> setMapping (((QPushButton*)visualList[newButton]), newButton) ;
                if(!element.attribute("hotkey").isEmpty()) {
                    addHotkey(element.attribute("hotkey"),newButton,"Swap");
                }

            } else if (element.attribute("type") == "timestamp") {
                bool nSaveOnClick = false;

                if (element.attribute("saveonclick") == "true" || element.attribute("saveonclick") == "1") {
                    nSaveOnClick = true;
                }
                QString newButton = element.attribute("id");
                widgetType[newButton] = "tsButton";

                widgetList[newButton] = new ScTSButton(nSaveOnClick,parent);
                widgetList[newButton]->setObjectName(newButton);
                ((ScTSButton*)widgetList[newButton])->setGeometry(QRect(element.attribute("x").toInt(),
                                                   element.attribute("y").toInt(),
                                                   element.attribute("width").toInt(),
                                                   element.attribute("height").toInt()));

                ((ScTSButton*)widgetList[newButton])->setText(element.text());

                if (!element.attribute("tooltip").isEmpty()) {
                    ((ScTSButton*)widgetList[newButton])->setToolTip(element.attribute("tooltip"));
                }

                connect(((ScTSButton*)widgetList[newButton]), SIGNAL(clicked()), tsMapper, SLOT(map()));
                tsMapper -> setMapping (((ScTSButton*)widgetList[newButton]), newButton) ;
                if(!element.attribute("hotkey").isEmpty()) {
                    addHotkey(element.attribute("hotkey"),newButton,"Timestamp");
                }
            } else*/ if (buttonElement.attribute("type") == "setButton") {
                bool nSaveOnClick = false;
                if (buttonElement.attribute("saveonclick") == "true" || buttonElement.attribute("saveonclick") == "1") {
                    nSaveOnClick = true;
                }
                QString newButton = buttonElement.attribute("id");
                QString nWidget = buttonElement.attribute("widget");
                QString nValue = buttonElement.attribute("value");
                widgetType[newButton] = "setButton";

                visualList[newButton] = new ScSetButton(nSaveOnClick,nWidget,nValue,0);
                visualList[newButton]->setObjectName(newButton);

                int buttonWidth = 100;
                if (buttonElement.hasAttribute("width")) {
                    buttonWidth = buttonElement.attribute("width").toInt();
                }

                ((ScSetButton*)visualList[newButton])->setMinimumWidth(buttonWidth);

                toolBar->addWidget(((ScSetButton*)visualList[newButton]));

                ((ScSetButton*)visualList[newButton])->setText(buttonElement.text());

                if (!buttonElement.attribute("tooltip").isEmpty()) {
                    ((ScSetButton*)visualList[newButton])->setToolTip(buttonElement.attribute("tooltip"));
                }

                connect(((ScSetButton*)visualList[newButton]), SIGNAL(clicked()), setButtonMapper, SLOT(map()));
                setButtonMapper -> setMapping (((ScSetButton*)visualList[newButton]), newButton) ;
                if(!buttonElement.attribute("hotkey").isEmpty()) {
                    addHotkey(buttonElement.attribute("hotkey"),newButton,"setButton");
                }
            }
        }

        child = child.nextSiblingElement();
    }

}

void MainWindow::parseCLI(QDomNode cliNode) {

    QDomNode child = cliNode.firstChildElement();

    QRegExp rx("(\\[\\$[\\w\\.]+\\])");

    while (!child.isNull()) {
        if (child.toElement().tagName() == "cmd") {
            QString cmd = child.toElement().text();
            QString ignore = "";
            ignore = child.toElement().attribute("ignore");
            QStringList ignoreList = CSV::parseFromString(ignore)[0];
            int pos = 0;
            QStringList vars;
            while ((pos = rx.indexIn(cmd, pos)) != -1) {
                QString var = rx.cap(0).remove(0,2);
                var.chop(1);
                vars << var;
                pos += rx.matchedLength();
            }

            int cmdIndex = cmdList.length();

            cmdList.append(cmd);
            cmdIgnores.append(ignoreList);
            qDebug() << cmd + " @ " + QString::number(cmdList.length());

            QString var;
            foreach( var, vars ) {
                if(!cmdVars[var].contains(cmdIndex)) {
                    cmdVars[var].append(cmdIndex);
                }
                cmdOldValues[var] = "";
            }

            //cmdValues[var] = "";

        }
        child = child.nextSiblingElement();
    }

    QMapIterator<QString, QList<int> > i(cmdVars);
    while (i.hasNext()) {
        i.next();

        QString list = "";
        int cmd;
        foreach (cmd,i.value()) {
            list += QString::number(cmd) + "! ";
        }

        qDebug() << i.key() << ": " << list;
    }

}

void MainWindow::addLabel(QDomElement element, QWidget *parent) {

    QString newLabel = "label"+QString::number(layoutIterator);
    visualList[newLabel] = new QLabel(parent);
    visualList[newLabel]->setObjectName(newLabel);
    visualList[newLabel]->setGeometry(QRect(element.attribute("x").toInt(),
                                                          element.attribute("y").toInt(),
                                                          element.attribute("width").toInt(),
                                                          element.attribute("height").toInt()));
    ((QLabel*)visualList[newLabel])->setText(element.text());

    layoutIterator++;
}

void MainWindow::addTweetWidget(QDomElement element, QWidget *parent) {

    QString newTweet = element.attribute("id");
    widgetList[newTweet] = new twitterWidget(th,parent);
    widgetType[newTweet] = "tweet";
    widgetList[newTweet]->setObjectName(newTweet);
    ((twitterWidget*)widgetList[newTweet])->setGeometry(QRect(element.attribute("x").toInt(),
                                                          element.attribute("y").toInt(),
                                                          element.attribute("width").toInt(),
                                                          element.attribute("height").toInt()));

    QString picPath = "twitter"; // default path to twitter so the output directory isn't filled with pictures

    if(element.attribute("picPath") != "") {
        picPath = element.attribute("picPath");
    }

    QDir newPath(element.attribute(picPath));

    if (!newPath.isAbsolute()) {
        QString path = settings["outputPath"] + element.attribute("picPath");
        newPath.setPath(path);

    }

    if(!newPath.exists()) {
        newPath.mkpath(newPath.path()+"/media");
    }

    ((twitterWidget*)widgetList[newTweet])->setPath(newPath.path() + "/");

    layoutIterator++;
}

void MainWindow::addLine(QDomElement element, QWidget *parent) {

    QString newLine = "line"+QString::number(layoutIterator);
    visualList[newLine] = new QLabel(parent);
    visualList[newLine]->setObjectName(newLine);
    visualList[newLine]->setGeometry(QRect(element.attribute("x").toInt(),
                                                          element.attribute("y").toInt(),
                                                          element.attribute("width").toInt(),
                                                          element.attribute("height").toInt()));
    ((QFrame*)visualList[newLine])->setFrameShape(QFrame::HLine);
    ((QFrame*)visualList[newLine])->setFrameShadow(QFrame::Sunken);

    layoutIterator++;
}

void MainWindow::addCheckBox(QDomElement element, QWidget *parent) {

    QString newCheckBox = element.attribute("id");
    widgetType[newCheckBox] = "checkBox";
    widgetList[newCheckBox] = new QCheckBox(parent);
    widgetList[newCheckBox]->setObjectName(newCheckBox);
    ((QCheckBox*)widgetList[newCheckBox])->setGeometry(QRect(element.attribute("x").toInt(),
                                                          element.attribute("y").toInt(),
                                                          element.attribute("width").toInt(),
                                                          element.attribute("height").toInt()));
    ((QCheckBox*)widgetList[newCheckBox])->setText(element.text());
    if(!element.attribute("toggleHotkey").isEmpty()) {
        addHotkey(element.attribute("toggleHotkey"),newCheckBox,"Toggle");
    }
    if(!element.attribute("checkHotkey").isEmpty()) {
        addHotkey(element.attribute("checkHotkey"),newCheckBox,"Check");
    }
    if(!element.attribute("uncheckHotkey").isEmpty()) {
        addHotkey(element.attribute("uncheckHotkey"),newCheckBox,"Uncheck");
    }
}

void MainWindow::addComboBox(QDomElement element, QWidget *parent) {

    QString newComboBox = element.attribute("id");
    widgetType[newComboBox] = "comboBox";
    widgetList[newComboBox] = new QComboBox(parent);
    widgetList[newComboBox]->setObjectName(newComboBox);

    ((QComboBox*)widgetList[newComboBox])->setGeometry(QRect(element.attribute("x").toInt(),
                                                          element.attribute("y").toInt(),
                                                          element.attribute("width").toInt(),
                                                          element.attribute("height").toInt()));

    if (element.attribute("editable") == "true" || element.attribute("editable") == "1") {
        ((QComboBox*)widgetList[newComboBox])->setEditable(true);

    }

    QDomNode child = element.firstChildElement();
    int selectedIndex = 0;
    for (int itemIndex = 0; !child.isNull(); itemIndex++) {
        QDomElement itemElement = child.toElement();
        if (itemElement.tagName() == "comboItem") {

            QString text = itemElement.text();
            QString value = itemElement.attribute("value");
            if (value.isNull())
                value = text;

            ((QComboBox*)widgetList[newComboBox])->addItem(text, value);

            if (itemElement.attribute("selected") == "true" || itemElement.attribute("selected") == "1")
                selectedIndex = itemIndex;
        }

        child = child.nextSiblingElement();
    }

    ((QComboBox*)widgetList[newComboBox])->setCurrentIndex(selectedIndex);

}

void MainWindow::addRadioGroup(QDomElement element, QWidget *parent) {

    QString newRadioGroup = element.attribute("id");
    widgetType[newRadioGroup] = "radioGroup";
    widgetList[newRadioGroup] = new ScRadioGroup(parent);
    widgetList[newRadioGroup]->setObjectName(newRadioGroup);

    QDomNode child = element.firstChildElement();
    int radioIterator = 0;
    while (!child.isNull()) {
        if (child.toElement().tagName() == "radioButton"){

            QDomElement radioButton = child.toElement();
            int x = radioButton.attribute("x").toInt();
            int y = radioButton.attribute("y").toInt();
            int width = radioButton.attribute("width").toInt();
            int height = radioButton.attribute("height").toInt();
            QString text = radioButton.text();
            QString value = radioButton.attribute("value");
            QString radioName = "radio" + value;

            QRadioButton *newRadioButton = new QRadioButton(text,parent);
            newRadioButton->setGeometry(QRect(x,y,width,height));
            newRadioButton->setObjectName(radioName);
            if(radioIterator == 0)
                newRadioButton->setChecked(true);
            ((ScRadioGroup*)widgetList[newRadioGroup])->addButton(newRadioButton,radioIterator);
            ((ScRadioGroup*)widgetList[newRadioGroup])->setValue(radioIterator,value);
            radioIterator++;

        }

        child = child.nextSiblingElement();
    }

}

void MainWindow::addButton(QDomElement element, QWidget *parent) {
    if (element.attribute("type") == "reset") {
        QString newButton = element.attribute("id");

        QList<QString> resetL = CSV::parseFromString(element.attribute("reset"))[0];

        widgetType[newButton] = "resetButton";

        resetList.insert(newButton,resetL);

        visualList[newButton] = new QPushButton(parent);
        visualList[newButton]->setObjectName(newButton);
        visualList[newButton]->setGeometry(QRect(element.attribute("x").toInt(),
                                           element.attribute("y").toInt(),
                                           element.attribute("width").toInt(),
                                           element.attribute("height").toInt()));
        ((QPushButton*)visualList[newButton])->setText(element.text());
        if (!element.attribute("tooltip").isEmpty()) {
            ((QPushButton*)visualList[newButton])->setToolTip(element.attribute("tooltip"));
        }

        connect(((QPushButton*)visualList[newButton]), SIGNAL(clicked()), resetMapper, SLOT(map()));
        resetMapper -> setMapping (((QPushButton*)visualList[newButton]), newButton) ;
        if(!element.attribute("hotkey").isEmpty()) {
            addHotkey(element.attribute("hotkey"),newButton,"Reset");
        }

    } else if (element.attribute("type") == "swap") {

        QString newButton = element.attribute("id");

        QList<QString> swapl1 = CSV::parseFromString(element.attribute("swapSet1"))[0];
        QList<QString> swapl2 = CSV::parseFromString(element.attribute("swapSet2"))[0];

        QList<QString> swapset;

        swapset.insert(0,newButton + "1");
        swapset.insert(1,newButton + "2");

        swapSets.insert(newButton,swapset);

        swapList.insert(newButton + "1",swapl1);

        swapList.insert(newButton + "2",swapl2);

        widgetType[newButton] = "swapButton";

        visualList[newButton] = new QPushButton(parent);
        visualList[newButton]->setObjectName(newButton);
        visualList[newButton]->setGeometry(QRect(element.attribute("x").toInt(),
                                                 element.attribute("y").toInt(),
                                                 element.attribute("width").toInt(),
                                                 element.attribute("height").toInt()));
        ((QPushButton*)visualList[newButton])->setText(element.text());
        if (!element.attribute("tooltip").isEmpty()) {
            ((QPushButton*)visualList[newButton])->setToolTip(element.attribute("tooltip"));
        }

        connect(((QPushButton*)visualList[newButton]), SIGNAL(clicked()), swapMapper, SLOT(map()));
        swapMapper -> setMapping (((QPushButton*)visualList[newButton]), newButton) ;
        if(!element.attribute("hotkey").isEmpty()) {
            addHotkey(element.attribute("hotkey"),newButton,"Swap");
        }

    } else if (element.attribute("type") == "timestamp") {
        bool nSaveOnClick = false;

        if (element.attribute("saveonclick") == "true" || element.attribute("saveonclick") == "1") {
            nSaveOnClick = true;
        }
        QString newButton = element.attribute("id");
        widgetType[newButton] = "tsButton";

        widgetList[newButton] = new ScTSButton(nSaveOnClick,parent);
        widgetList[newButton]->setObjectName(newButton);
        ((ScTSButton*)widgetList[newButton])->setGeometry(QRect(element.attribute("x").toInt(),
                                           element.attribute("y").toInt(),
                                           element.attribute("width").toInt(),
                                           element.attribute("height").toInt()));

        ((ScTSButton*)widgetList[newButton])->setText(element.text());

        if (!element.attribute("tooltip").isEmpty()) {
            ((ScTSButton*)widgetList[newButton])->setToolTip(element.attribute("tooltip"));
        }

        connect(((ScTSButton*)widgetList[newButton]), SIGNAL(clicked()), tsMapper, SLOT(map()));
        tsMapper -> setMapping (((ScTSButton*)widgetList[newButton]), newButton) ;
        if(!element.attribute("hotkey").isEmpty()) {
            addHotkey(element.attribute("hotkey"),newButton,"Timestamp");
        }
    } else if (element.attribute("type") == "setButton") {
        bool nSaveOnClick = false;
        if (element.attribute("saveonclick") == "true" || element.attribute("saveonclick") == "1") {
            nSaveOnClick = true;
        }
        QString newButton = element.attribute("id");
        QString nWidget = element.attribute("widget");
        QString nValue = element.attribute("value");
        widgetType[newButton] = "setButton";

        visualList[newButton] = new ScSetButton(nSaveOnClick,nWidget,nValue,parent);
        visualList[newButton]->setObjectName(newButton);

        ((ScSetButton*)visualList[newButton])->setGeometry(QRect(element.attribute("x").toInt(),
                                           element.attribute("y").toInt(),
                                           element.attribute("width").toInt(),
                                           element.attribute("height").toInt()));

        ((ScSetButton*)visualList[newButton])->setText(element.text());

        if (!element.attribute("tooltip").isEmpty()) {
            ((ScSetButton*)visualList[newButton])->setToolTip(element.attribute("tooltip"));
        }

        connect(((ScSetButton*)visualList[newButton]), SIGNAL(clicked()), setButtonMapper, SLOT(map()));
        setButtonMapper -> setMapping (((ScSetButton*)visualList[newButton]), newButton) ;
        if(!element.attribute("hotkey").isEmpty()) {
            addHotkey(element.attribute("hotkey"),newButton,"setButton");
        }
    }
}

void MainWindow::tsClick(QString tsButton) {
    if (((ScTSButton*)widgetList[tsButton])->isActive()) {
        ((ScTSButton*)widgetList[tsButton])->setActive(false);
    } else {
        ((ScTSButton*)widgetList[tsButton])->setActive(true);
    }

    if (((ScTSButton*)widgetList[tsButton])->isSaveOnClick()) {
        saveData();
    }
}

void MainWindow::setButtonClick(QString setButton) {
    QString wType = "";
    QString widget = ((ScSetButton*)visualList[setButton])->getWidget();
    QString value = ((ScSetButton*)visualList[setButton])->getValue();
    wType = widgetType[widget];

    if (wType == "comboBox") {
        QVariant data(value);
        bool editable = ((QComboBox*)widgetList[widget])->isEditable();
        int valueIndex = ((QComboBox*)widgetList[widget])->findData(data);
        if (valueIndex == -1) {
            valueIndex = ((QComboBox*)widgetList[widget])->findText(value);
        }
        if (valueIndex != -1) {
            ((QComboBox*)widgetList[widget])->setCurrentIndex(valueIndex);
        } else if (editable) {
            ((QComboBox*)widgetList[widget])->setCurrentText(value);
        }
    }

    //ADD MORE WIDGETS TYPES

    if (((ScSetButton*)visualList[setButton])->isSaveOnClick()) {
        saveData();
    }
}

void MainWindow::addLineEdit(QDomElement element, QWidget *parent) {

    QString newLineEdit = element.attribute("id");

    widgetList[newLineEdit] = new ScLineEdit(parent);
    widgetList[newLineEdit]->setObjectName(newLineEdit);
    ((ScLineEdit*)widgetList[newLineEdit])->setGeometry(QRect(element.attribute("x").toInt(),
                                             element.attribute("y").toInt(),
                                             element.attribute("width").toInt(),
                                             element.attribute("height").toInt()));
    widgetType[newLineEdit] = "lineEdit";

    if (element.text() != "") {
        ((ScLineEdit*)widgetList[newLineEdit])->setPlaceholderText(element.text());
    }

    if(element.hasAttribute("dataSet")) {
        QString dataSetName = element.attribute("dataSet");
        if (dataSets[dataSetName].isEmpty()) {
            QString setPath = QFileInfo(settings["layoutPath"]).path() +"/"+ dataSetName;
            QList<QStringList> newDataSet = CSV::parseFromFile(setPath,"UTF-8");

            dataSets[dataSetName] = processDataSet(newDataSet);
            condensedDataSets[dataSetName] = condenseDataSet(dataSets[dataSetName]);
        }
        int dataField = 0;
        if (element.hasAttribute("dataField")) {
            dataField = element.attribute("dataField").toInt() - 1;

            for (int i = dataSets[dataSetName].length(); i <= dataField; i++) {
                QStringList newList;

                for (int i2 = 0; i2 < dataSets[dataSetName][0].length();i2++){
                    newList.insert(i2,"");
                }
                dataSets[dataSetName].insert(i,newList);
                condensedDataSets[dataSetName] = condenseDataSet(dataSets[dataSetName]);
            }

        }

        dataAssoc[newLineEdit] = dataField;
        completerList[newLineEdit] = new ScCompleter(parent);
        bool hasMaster;
        ((ScLineEdit*)widgetList[newLineEdit])->setName(newLineEdit,dataSetName);
        if (element.hasAttribute("master")) {
            QStringListModel *model = new QStringListModel(condensedDataSets[dataSetName][dataField]);
            completerList[newLineEdit]->setModel(model);
            dataMaster[element.attribute("master")].append(newLineEdit);
            ((ScLineEdit*)widgetList[newLineEdit])->setButtonVisible(false);
            hasMaster = true;
        } else {
            QStringListModel *model = new QStringListModel(dataSets[dataSetName][dataField]);
            completerList[newLineEdit]->setModel(model);
            connect(((ScLineEdit*)widgetList[newLineEdit]),SIGNAL(textChanged(QString)), this, SLOT(checkLineDataSet(QString)));
            connect(((ScLineEdit*)widgetList[newLineEdit]),SIGNAL(clearButtonClicked()), this, SLOT(removeFromDataSet()));
            hasMaster = false;

        }
        completerList[newLineEdit]->setName(newLineEdit,dataSetName,dataField,hasMaster);
        completerList[newLineEdit]->setCaseSensitivity(Qt::CaseInsensitive);
        completerList[newLineEdit]->setCompletionMode(QCompleter::PopupCompletion);

        ((ScLineEdit*)widgetList[newLineEdit])->setCompleter(completerList[newLineEdit]);

        connect(completerList[newLineEdit], SIGNAL(activated(QString)), this, SLOT(completerActivate(QString)));

    }
}

void MainWindow::completerActivate(QString item) {
    QString scSender = ((ScCompleter*)sender())->getName();
    QString scSetName = ((ScCompleter*)sender())->getSetName();
    int field = dataAssoc[scSender];
    int index = dataSets[scSetName][field].indexOf(item);

    for (int i = 0; i < dataMaster[scSender].length();i++) {
        QString id = dataMaster[scSender][i];
        int newField = dataAssoc[id];
        ((ScLineEdit*)widgetList[id])->setText(dataSets[scSetName][newField][index]);
    }
}

void MainWindow::removeFromDataSet() {
    QString scSender = ((ScLineEdit*)sender())->getName();
    QString scSetName = ((ScLineEdit*)sender())->getSetName();

    QString item = ((ScLineEdit*)sender())->text();

    int field = dataAssoc[scSender];
    int index = dataSets[scSetName][field].indexOf(item);

    int numSets = dataSets[scSetName].length();

    for (int i = 0; i < numSets; i++) {
        dataSets[scSetName][i].removeAt(index);
    }

    condensedDataSets[scSetName] = condenseDataSet(dataSets[scSetName]);

    int numSlave = dataMaster[scSender].length();
    for (int i = 0; i < numSlave; i++) {
        QString widget = dataMaster[scSender][i];
        ((ScLineEdit*)widgetList[widget])->clear();
    }

    removedSetQueue.append(scSetName);

    updateCompleters();

    ((ScLineEdit*)sender())->clear();


}

void MainWindow::updateCompleters() {
    QMapIterator<QString, ScCompleter *> i(completerList);
    while (i.hasNext()) {
        i.next();
        QString name = i.key();
        QString setName = completerList[name]->getSetName();
        int dataField = completerList[name]->getDataField();

        if (completerList[name]->hasMaster() == true) {
            QStringListModel *model = new QStringListModel(condensedDataSets[setName][dataField]);
            completerList[name]->setModel(model);
        } else {
            QStringListModel *model = new QStringListModel(dataSets[setName][dataField]);
            completerList[name]->setModel(model);
        }
    }
}

void MainWindow::checkLineDataSet(QString line) {
    QString scSender = ((ScLineEdit*)sender())->getName();
    QString scSetName = ((ScLineEdit*)sender())->getSetName();
    int field = dataAssoc[scSender];
    int index = dataSets[scSetName][field].indexOf(line);

    if(index != -1) {
        ((ScLineEdit*)widgetList[scSender])->setButtonVisible(true);
    } else {
        ((ScLineEdit*)widgetList[scSender])->setButtonVisible(false);
    }
}

QList<QStringList> MainWindow::processDataSet(QList<QStringList> oldSet) {
    int items = oldSet.length() - 1;
    int sets = oldSet[0].length() - 1;

    //this bit is confusing should probably fix it
    bool empty = true;
    for (int i = 0; i <= items; i++) {
        if (items > 0 || items == -1)
            empty = false;
        if (items == 0 && oldSet[0][i] != "")
            empty = false;
    }

    QList<QStringList> newSet;
    for (int i = 0; i <= sets; i++){
        QStringList newList;
        for (int i2 = 0; i2 <= items; i2++){
            if (!empty)
                newList.append(oldSet[i2][i]);
        }
        newSet.append(newList);
    }
    return newSet;
}

QList<QStringList> MainWindow::condenseDataSet(QList<QStringList> oldSet) {
    QList<QStringList> newSet;

    int sets = oldSet.length() - 1;
    for (int i = 0; i <= sets; i++){
        QStringList newList = oldSet[i].toSet().toList();
        newSet.append(newList);
    }

    return newSet;
}

void MainWindow::saveDataSets() {
    QMapIterator<QString, ScCompleter *> i(completerList);
    QStringList updatedSets;
    while (i.hasNext()) {
        i.next();
        QString name = i.key();
        QString setName = completerList[name]->getSetName();
        int dataField = completerList[name]->getDataField();

        if (completerList[name]->hasMaster() == false) { //if it's a master
            //check if current value in in dataSet
            QString currentVal = ((ScLineEdit*)widgetList[name])->text();
            int currIndex =  dataSets[setName][dataField].indexOf(currentVal);

            if (currIndex == -1 && currentVal != "") {
                //set all to blank first
                int numFields = dataSets[setName].length();
                for (int i = 0; i < numFields; i++) {
                    if (!checkDataSet1Blank(setName))
                        dataSets[setName][i].append("");
                }

                int newIndex = dataSets[setName][dataField].length() - 1;

                QString newValue = ((ScLineEdit*)widgetList[name])->text();
                dataSets[setName][dataField][newIndex] = newValue;

                //find each field that has name as master
                int slaveNum = dataMaster[name].length();
                for (int i = 0; i < slaveNum; i++) {
                    QString slaveName = dataMaster[name][i];
                    int field = dataAssoc[slaveName];
                    if (field != dataField) {
                        QString newValue = ((ScLineEdit*)widgetList[slaveName])->text();
                        dataSets[setName][field][newIndex] = newValue;
                    }
                }
                if (updatedSets.indexOf(setName) == -1)
                    updatedSets.append(setName);

                ((ScLineEdit*)widgetList[name])->setButtonVisible(true);

            } else if (currentVal != "") { //if has in set
                //find each field that has name as master
                int slaveNum = dataMaster[name].length();
                for (int i = 0; i < slaveNum; i++) {
                    QString slaveName = dataMaster[name][i];
                    int field = dataAssoc[slaveName];
                    if (field != dataField) {
                        QString newValue = ((ScLineEdit*)widgetList[slaveName])->text();
                        if (dataSets[setName][field][currIndex] != newValue){ // if not new value update
                            dataSets[setName][field][currIndex] = newValue;
                            if (updatedSets.indexOf(setName) == -1)
                                updatedSets.append(setName);
                        }//end update
                    }
                }


            }// end if has in set


        }// end if has master


    }

    if (!removedSetQueue.isEmpty()) {
        updatedSets.append(removedSetQueue);
        removedSetQueue.clear();
    }

    int numUpdate = updatedSets.length();

    for (int i = 0; i < numUpdate; i++) {
        condensedDataSets[updatedSets[i]] = condenseDataSet(dataSets[updatedSets[i]]);

        QList<QStringList> newList = processDataSet(dataSets[updatedSets[i]]);

        QString outputFile = QFileInfo(settings["layoutPath"]).path() +"/"+ updatedSets[i];
        CSV::write(newList,outputFile,"UTF-8");

    }

    updateCompleters();
}

void MainWindow::addSpinBox(QDomElement element, QWidget *parent) {

    QString newSpinBox = element.attribute("id");

    widgetList[newSpinBox] = new QSpinBox(parent);
    widgetList[newSpinBox]->setObjectName(newSpinBox);
    ((QSpinBox*)widgetList[newSpinBox])->setGeometry(QRect(element.attribute("x").toInt(),
                                              element.attribute("y").toInt(),
                                              element.attribute("width").toInt(),
                                              element.attribute("height").toInt()));
    if(!element.attribute("maximum").isEmpty()) {
        ((QSpinBox*)widgetList[newSpinBox])->setMaximum(element.attribute("maximum").toInt());
    }
    if(!element.attribute("increaseHotkey").isEmpty()) {
        addHotkey(element.attribute("increaseHotkey"),newSpinBox,"Increase");
    }
    if(!element.attribute("decreaseHotkey").isEmpty()) {
        addHotkey(element.attribute("decreaseHotkey"),newSpinBox,"Decrease");
    }
    widgetType[newSpinBox] = "spinBox";

}

QString MainWindow::addTabWidget(QDomElement element, QWidget *parent) {

    QString tabSet = "tabSet"+QString::number(layoutIterator);
    visualList[tabSet] = new QTabWidget(parent);
    visualList[tabSet]->setObjectName(tabSet);
    visualList[tabSet]->setGeometry(QRect(element.attribute("x").toInt(),
                                                          element.attribute("y").toInt(),
                                                          element.attribute("width").toInt(),
                                                          element.attribute("height").toInt()));

    layoutIterator++;
    return tabSet;
}

QDomDocument MainWindow::getDefaultLayout() {
    QString xmlcontent = "<!DOCTYPE StreamControlLayout>\r\n<layout width=\"400\" height=\"140\" tabbed=\"1\">\r\n <toolBar>\r\n  <label>Game</label>\r\n  <comboBox id=\"game\" editable=\"true\">\r\n   <comboItem>Super Street Fighter IV</comboItem>\r\n   <comboItem>Ultimate Marvel vs Capcom 3</comboItem>\r\n   <comboItem>Persona 4: Arena</comboItem>\r\n   <comboItem>Tekken Tag Tournament 2</comboItem>\r\n   <comboItem>King of Fighters XIII</comboItem>\r\n  </comboBox>\r\n </toolBar>\r\n <tab name=\"Match Info\">\r\n  <label x=\"10\" y=\"14\" width=\"46\" height=\"13\">Player 1</label>\r\n  <label x=\"10\" y=\"44\" width=\"46\" height=\"13\">Player 2</label>\r\n  <label x=\"300\" y=\"14\" width=\"46\" height=\"13\">Rounds</label>\r\n  <lineEdit id=\"pName1\" x=\"60\" y=\"10\" width=\"171\" height=\"20\" />\r\n  <lineEdit id=\"pName2\" x=\"60\" y=\"40\" width=\"171\" height=\"20\" />\r\n  <spinBox id=\"pScore1\" x=\"250\" y=\"10\" width=\"42\" height=\"22\" maximum=\"999\" />\r\n  <spinBox id=\"pScore2\" x=\"250\" y=\"40\" width=\"42\" height=\"22\" maximum=\"999\" />\r\n  <spinBox id=\"rounds\" x=\"340\" y=\"10\" width=\"42\" height=\"22\" maximum=\"999\" />\r\n  <button type=\"reset\" x=\"300\" y=\"40\" width=\"41\" height=\"23\" tooltip=\"Reset the Scores\" id=\"reset\" reset=\"pScore1,pScore2\">Reset</button>\r\n  <button type=\"swap\" x=\"340\" y=\"40\" width=\"41\" height=\"23\" tooltip=\"Swap the Scores\" id=\"swap1\" swapSet1=\"pName1,pScore1\" swapSet2=\"pName2,pScore2\">Swap</button>\r\n </tab>\r\n <tab name=\"Commentary\">\r\n  <label x=\"10\" y=\"14\" width=\"46\" height=\"13\">Title 1</label>\r\n  <label x=\"10\" y=\"44\" width=\"46\" height=\"13\">Title 2</label>\r\n  <lineEdit id=\"cTitle1\" x=\"60\" y=\"10\" width=\"321\" height=\"20\" />\r\n  <lineEdit id=\"cTitle2\" x=\"60\" y=\"40\" width=\"321\" height=\"20\" />\r\n </tab>\r\n <tab name=\"Misc 1\">\r\n  <label x=\"10\" y=\"14\" width=\"46\" height=\"13\">mText 1</label>\r\n  <label x=\"10\" y=\"44\" width=\"46\" height=\"13\">mText 2</label>\r\n  <lineEdit id=\"mText1\" x=\"60\" y=\"10\" width=\"321\" height=\"20\" />\r\n  <lineEdit id=\"mText2\" x=\"60\" y=\"40\" width=\"321\" height=\"20\" />\r\n </tab>\r\n <tab name=\"Misc 2\">\r\n  <label x=\"10\" y=\"14\" width=\"46\" height=\"13\">mText 3</label>\r\n  <label x=\"10\" y=\"44\" width=\"46\" height=\"13\">mText 4</label>\r\n  <lineEdit id=\"mText3\" x=\"60\" y=\"10\" width=\"321\" height=\"20\" />\r\n  <lineEdit id=\"mText4\" x=\"60\" y=\"40\" width=\"321\" height=\"20\" />\r\n </tab>\r\n</layout>\r\n";
    QDomDocument doc;
    doc.setContent(xmlcontent);
    return doc;
}

void MainWindow::clearMaps() {
    QList<QAction *> tbList = toolBar->actions();

    for (int tbi = 3; tbi < tbList.length();tbi++) {
        toolBar->removeAction(tbList.at(tbi));
    }

    widgetList.clear();
    visualList.clear();
    widgetType.clear();
    resetList.clear();
    swapSets.clear();
    swapList.clear();
    dataSets.clear();
    dataAssoc.clear();
    dataMaster.clear();
    resetMapper = new QSignalMapper (this) ;
    swapMapper = new QSignalMapper (this) ;
    tsMapper = new QSignalMapper (this) ;
    setButtonMapper = new QSignalMapper (this) ;

    cmdList.clear();
    cmdOldValues.clear();
    cmdVars.clear();
    cmdIgnores.clear();
}

bool MainWindow::checkDataSet1Blank(QString setName) {
    bool empty = true;
    int numFields = dataSets[setName].length();
    for (int i = 0; i < numFields; i++) {
        if (dataSets[setName][i].length() > 1 || dataSets[setName][i].length() == 0)
            empty = false;
        if (dataSets[setName][i].length() == 1 && dataSets[setName][i][0] != "")
            empty = false;

    }
    return empty;
}

void MainWindow::addHotkey(QString hotkey,QString widget, QString action) {
    QKeySequence qks(hotkey);
    int ks = qks[0];
    int modifiers = win_keyhelper::getModifiers(ks) | 0x4000; //add no-repeat modifier
    int key = win_keyhelper::getKey(ks);

    QStringList hotkeyItem;

    hotkey = qks.toString();

    hotkeyItem.append(hotkey);
    hotkeyItem.append(widget);
    hotkeyItem.append(action);

    hotkeys.append(hotkeyItem);
    hotkeysIndex.append(qks[0]);

    #ifdef Q_OS_WIN
    if (!altHotkeyHandling) {
        if (RegisterHotKey(
                (HWND(this->winId())),
                hotkeysIndex.count(),
                modifiers,
                key))
            {
                qDebug() << "registered hotkey: \"" + hotkey + "\" at " + QString::number(hotkeysIndex.count());
            }
    }
    #endif
}

void MainWindow::performHotkey(int hotkeyIndex) {
    QStringList hotkeyItem = hotkeys.at(hotkeyIndex);
    QString widget = hotkeyItem.at(1);
    QString action = hotkeyItem.at(2);

    if (widget == "Main") {
        if (action == "Save") {
            qDebug() << "Saving Data via Hotkey";
            saveData();
        }
    } else {
        //do widget actions and stuff
        QString wType = widgetType[widget];
        if (wType == "spinBox") {
            int step = ((QSpinBox*)widgetList[widget])->singleStep();
            int value = ((QSpinBox*)widgetList[widget])->value();
            if (action == "Increase") {
                ((QSpinBox*)widgetList[widget])->setValue(value + step);
                qDebug() << "increasing " + widget;
            } else if (action == "Decrease") {
                ((QSpinBox*)widgetList[widget])->setValue(value - step);
                qDebug() << "decreasing " + widget;
            }
        } else if (wType == "resetButton") {
            resetFields(widget);
        } else if (wType == "swapButton") {
            swapFields(widget);
        } else if (wType == "tsButton") {
            tsClick(widget);
        } else if (wType == "setButton") {
            setButtonClick(widget);
        } else if (wType == "checkBox") {
            if (action == "Toggle") {
                bool checked = ((QCheckBox*)widgetList[widget])->isChecked();
                if (checked){
                    ((QCheckBox*)widgetList[widget])->setChecked(false);
                } else {
                    ((QCheckBox*)widgetList[widget])->setChecked(true);
                }
            } else if (action == "Check") {
                ((QCheckBox*)widgetList[widget])->setChecked(true);
            } else if (action == "Uncheck") {
                ((QCheckBox*)widgetList[widget])->setChecked(false);
            }
        }
    }
}

void MainWindow::deleteHotkeys() {
    int num = hotkeysIndex.count();
    for (int i = 1; i <= num; i++) {
        #ifdef Q_OS_WIN
        if(UnregisterHotKey(HWND(this->winId()), i))
        {
            qDebug() << "UNREGISTED " + hotkeys.at(i-1).at(0);
        }
        #endif
    }

    hotkeys.clear();
    hotkeysIndex.clear();
}
