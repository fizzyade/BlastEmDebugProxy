/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2022, Adrian Carpenter
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QProcess>
#include <QDebug>
#include "ConfigurationDialog.h"
#include "ROMServerEditDialog.h"
#include <QSettings>
#include <QJsonArray>
#include <QJsonObject>

MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent)
        , ui(new Ui::MainWindow) {

    QSettings settings;

    ui->setupUi(this);

    ui->serverWidget->clear();

    auto serverList = settings.value("romServers").toJsonArray();

    for (auto server : serverList) {
        auto serverObject = server.toObject();

        auto item = new QTreeWidgetItem;

        item->setText(0, QString("%1").arg(serverObject["port"].toInt()));
        item->setText(1, serverObject["rom"].toString());

        m_portMap[serverObject["port"].toInt()] = startServer(serverObject["port"].toInt(), serverObject["rom"].toString());

        ui->serverWidget->addTopLevelItem(item);
    }

    ui->serverWidget->setCurrentItem(ui->serverWidget->topLevelItem(0));

    updateButtons();

}

MainWindow::~MainWindow() {
    delete ui;
}

auto MainWindow::updateButtons() -> void {
    auto currentItem = ui->serverWidget->currentItem();

    ui->deleteButton->setEnabled(currentItem ? true : false);
    ui->editButton->setEnabled(currentItem ? true : false);
}

auto MainWindow::on_configureButton_clicked() -> void {
    ConfigurationDialog dialog;

    if (dialog.exec()) {
        QSettings settings;

        settings.setValue("blastEmBinary", dialog.fileName());
    }
}

auto MainWindow::on_addButton_clicked() -> void {
    ROMServerEditDialog dialog;

    if (dialog.exec()) {
        QSettings settings;

        auto serverList = settings.value("romServers").toJsonArray();

        QJsonObject newServer;

        newServer["port"] = dialog.serverPort();
        newServer["rom"] = dialog.binaryFileName();

        serverList.append(newServer);

        settings.setValue("romServers", serverList);

        auto item = new QTreeWidgetItem;

        item->setText(0, QString("%1").arg(dialog.serverPort()));
        item->setText(1, dialog.binaryFileName());

        m_portMap[dialog.serverPort()] = startServer(dialog.serverPort(), dialog.binaryFileName());

        ui->serverWidget->addTopLevelItem(item);

        updateButtons();
    }
}

auto MainWindow::on_editButton_clicked() -> void {
    QSettings settings;

    auto selectedItem = ui->serverWidget->currentItem();

    if (!selectedItem) {
        return;
    }

    ROMServerEditDialog dialog(selectedItem->text(1), selectedItem->text(0).toInt());

    if (dialog.exec()) {
        if (dialog.serverPort() != selectedItem->text(0).toInt()) {
            m_portMap[selectedItem->text(0).toInt()]->deleteLater();

            m_portMap[selectedItem->text(0).toInt()]->close();
        }

        selectedItem->setText(0, QString("%1").arg(dialog.serverPort()));
        selectedItem->setText(1, dialog.binaryFileName());

        m_portMap[dialog.serverPort()] = startServer(dialog.serverPort(), dialog.binaryFileName());

        QJsonArray serverList;

        for (int i = 0; i < ui->serverWidget->topLevelItemCount(); i++) {
            QJsonObject newServer;

            newServer["port"] = ui->serverWidget->topLevelItem(i)->text(0).toInt();
            newServer["rom"] = ui->serverWidget->topLevelItem(i)->text(1);

            serverList.append(newServer);
        }

        settings.setValue("romServers", serverList);
    }
}

auto MainWindow::on_deleteButton_clicked() -> void {
    QSettings settings;

    auto selectedItem = ui->serverWidget->currentItem();

    if (!selectedItem) {
        return;
    }

    auto port = selectedItem->text(0).toInt();

    auto serverList = settings.value("romServers").toJsonArray();

    QJsonArray newJsonArray;

    for (auto server : serverList) {
        auto serverObject = server.toObject();

        if (serverObject["port"] == port) {
            continue;
        }

        QJsonObject newServer;

        newServer["port"] = serverObject["port"];
        newServer["rom"] = serverObject["rom"];

        newJsonArray.append(newServer);
    }

    settings.setValue("romServers", newJsonArray);

    for (int i = ui->serverWidget->topLevelItemCount() - 1; i >= 0 ; i--) {
        auto item = ui->serverWidget->topLevelItem(i);

        if (item->text(0).toInt() == port) {
            delete item;
        }
    }

    m_portMap[port]->close();

    qDebug() << "server stopped on port" << port;

    updateButtons();
}

auto MainWindow::startServer(int port, QString rom) -> QTcpServer * {
    QTcpServer *server = new QTcpServer();

    if (!server) {
        qDebug() << "unable to start server on port" << port;

        return nullptr;
    }

    qDebug() << "server started on port" << port << "with rom" << rom;

    m_portMap[port] = server;

    server->listen(QHostAddress::Any, port);

    connect(server, &QTcpServer::newConnection, this, [this, server, port, rom]() {
        QSettings settings;

        QTcpSocket *clientSocket = server->nextPendingConnection();

        qDebug() << "gdb connected on port" << port << "...";

        auto blastEmProcess = new QProcess(this);

        m_processes.insert(blastEmProcess);
        m_sockets.insert(clientSocket);

        connect(blastEmProcess, &QObject::destroyed, this, [=]() {
            qDebug() << "congratulations, you didn't leak the blastem process object";
        });

        connect(clientSocket, &QObject::destroyed, this, [=]() {
            qDebug() << "congratulations, you didn't leak the gdb socket object";
        });

#if !TARGET_OS_WINDOWS
        QTcpSocket *blastEmSocket = nullptr;

        connect(blastEmProcess, &QProcess::readyRead, this, [clientSocket, blastEmProcess]() {
            auto data = blastEmProcess->readAll();

            clientSocket->write(data);

            qDebug() << "blastem wrote something..." << data;
        });
#else
        auto blastEmSocket = new QTcpSocket;
#endif
        connect(blastEmProcess, &QProcess::started, this, [blastEmSocket, clientSocket, blastEmProcess]() {
            if (blastEmSocket != nullptr) {
                /**
                 * The BlastEm debugger under Windows uses a socket bound on port 1234, but you have to manually start it,
                 * BlastProxy should alleviate this by spawning on demand.
                 */
                connect(blastEmSocket, &QTcpSocket::readyRead, [blastEmSocket, clientSocket, blastEmProcess]() {
                    auto data = blastEmSocket->readAll();

                    clientSocket->write(data);

                    qDebug() << "blastem wrote something..." << data;
                });

                blastEmSocket->connectToHost(QHostAddress::Any, 1234);
            }

            qDebug() << "blastem was started...";
        });

        blastEmProcess->start(settings.value("blastEmBinary").toString(), QStringList() << rom << "-D");

        connect(clientSocket, &QTcpSocket::readyRead, this, [blastEmSocket, clientSocket, blastEmProcess]() {
            auto data = clientSocket->readAll();

            if (QString::fromLatin1(data) == "$D#44") {
                 clientSocket->close();

                 return;
            }

            if (blastEmSocket) {
                blastEmSocket->write(data);
            } else {
                blastEmProcess->write(data);
            }

            qDebug() << "gdb sent something to blastem..." << QString::fromLatin1(data) << data;
        });

        connect(blastEmProcess, &QProcess::finished, this, [this, clientSocket, blastEmProcess]() {
            m_processes.remove(blastEmProcess);

            blastEmProcess->deleteLater();

            if (m_sockets.contains(clientSocket)) {
                clientSocket->close();
            }

            qDebug() << "blastem was closed...";
        });

        connect(clientSocket, &QTcpSocket::disconnected, this, [this, clientSocket, blastEmProcess, port]() {
            m_sockets.remove(clientSocket);

            clientSocket->deleteLater();

            if (m_processes.contains(blastEmProcess)) {
                blastEmProcess->kill();
            }

            m_portMap.remove(port);

            qDebug() << "gdb disconnected...";
        });
    });

    return server;
}

auto MainWindow::on_serverWidget_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous) -> void {
    Q_UNUSED(current)
    Q_UNUSED(previous)

    updateButtons();
}
