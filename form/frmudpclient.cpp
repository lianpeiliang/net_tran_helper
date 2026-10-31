#include "frmudpclient.h"
#include "ui_frmudpclient.h"
#include "quiwidget.h"
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QMessageBox>

frmUdpClient::frmUdpClient(QWidget *parent) : QWidget(parent), ui(new Ui::frmUdpClient)
{
    ui->setupUi(this);
    this->initForm();
    this->initConfig();
}

frmUdpClient::~frmUdpClient()
{
    delete ui;
}

void frmUdpClient::initForm()
{
    receivedBytes = 0;
    sentBytes = 0;
    updateByteCounts();
    ui->labSendHint->hide();

    udpSocket = new QUdpSocket(this);
    connect(udpSocket, SIGNAL(readyRead()), this, SLOT(readData()));

    ui->cboxLocalIP->blockSignals(true);
    ui->cboxLocalIP->addItem(tr("自动选择"), QString());
    QStringList addresses;
    foreach (const QNetworkInterface &iface, QNetworkInterface::allInterfaces()) {
        // Keep the address list consistent with the UDP server's listen addresses.
        const QString name = iface.humanReadableName().toLower();
        if (name.startsWith("vmware network adapter") || name.startsWith("npcap loopback adapter")) {
            continue;
        }
        const QNetworkInterface::InterfaceFlags required = QNetworkInterface::IsUp |
                QNetworkInterface::IsRunning | QNetworkInterface::CanBroadcast |
                QNetworkInterface::CanMulticast;
        if (iface.flags() != required) {
            continue;
        }
        foreach (const QNetworkAddressEntry &entry, iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol) {
                continue;
            }
            const QString address = entry.ip().toString();
            if (address != "127.0.0.1" && !addresses.contains(address)) {
                addresses.append(address);
                ui->cboxLocalIP->addItem(address, address);
            }
        }
    }
    ui->cboxLocalIP->addItem("127.0.0.1", "127.0.0.1");
    ui->cboxLocalIP->blockSignals(false);

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(on_btnSend_clicked()));

    ui->cboxInterval->addItems(App::Intervals);
    ui->cboxData->addItems(App::Datas);
}

void frmUdpClient::initConfig()
{
    ui->ckHexSend->setChecked(App::HexSendUdpClient);
    connect(ui->ckHexSend, SIGNAL(stateChanged(int)), this, SLOT(saveConfig()));

    ui->ckHexReceive->setChecked(App::HexReceiveUdpClient);
    connect(ui->ckHexReceive, SIGNAL(stateChanged(int)), this, SLOT(saveConfig()));

    ui->ckAscii->setChecked(App::AsciiUdpClient);
    connect(ui->ckAscii, SIGNAL(stateChanged(int)), this, SLOT(saveConfig()));

    ui->ckDebug->setChecked(App::DebugUdpClient);
    connect(ui->ckDebug, SIGNAL(stateChanged(int)), this, SLOT(saveConfig()));

    ui->ckAutoSend->setChecked(App::AutoSendUdpClient);
    connect(ui->ckAutoSend, SIGNAL(stateChanged(int)), this, SLOT(saveConfig()));

    ui->cboxInterval->setCurrentIndex(ui->cboxInterval->findText(QString::number(App::IntervalUdpClient)));
    connect(ui->cboxInterval, SIGNAL(currentIndexChanged(int)), this, SLOT(saveConfig()));

    ui->txtServerIP->setText(App::UdpServerIP);
    connect(ui->txtServerIP, SIGNAL(textChanged(QString)), this, SLOT(saveConfig()));

    ui->txtServerPort->setText(QString::number(App::UdpServerPort));
    connect(ui->txtServerPort, SIGNAL(textChanged(QString)), this, SLOT(saveConfig()));

    this->changeTimer();
}

void frmUdpClient::saveConfig()
{
    App::HexSendUdpClient = ui->ckHexSend->isChecked();
    App::HexReceiveUdpClient = ui->ckHexReceive->isChecked();
    App::AsciiUdpClient = ui->ckAscii->isChecked();
    App::DebugUdpClient = ui->ckDebug->isChecked();
    App::AutoSendUdpClient = ui->ckAutoSend->isChecked();
    App::IntervalUdpClient = ui->cboxInterval->currentText().toInt();
    App::UdpServerIP = ui->txtServerIP->text().trimmed();
    App::UdpServerPort = ui->txtServerPort->text().trimmed().toInt();
    App::writeConfig();

    this->changeTimer();
}

void frmUdpClient::changeTimer()
{
    timer->setInterval(App::IntervalUdpClient);
    if (App::AutoSendUdpClient) {
        if (!timer->isActive()) {
            timer->start();
        }
    } else {
        if (timer->isActive()) {
            timer->stop();
        }
    }
}

void frmUdpClient::append(int type, const QString &data, bool clear)
{
    static int currentCount = 0;
    static int maxCount = 100;

    if (clear) {
        ui->txtMain->clear();
        currentCount = 0;
        return;
    }

    if (currentCount >= maxCount) {
        ui->txtMain->clear();
        currentCount = 0;
    }

    if (ui->ckShow->isChecked()) {
        return;
    }

    //过滤回车换行符
    QString strData = data;
    strData = strData.replace("\r", "");
    strData = strData.replace("\n", "");

    //不同类型不同颜色显示
    QString strType;
    if (type == 0) {
        strType = "发送";
        ui->txtMain->setTextColor(QColor("darkgreen"));
    } else {
        strType = "接收";
        ui->txtMain->setTextColor(QColor("red"));
    }

    strData = QString("时间[%1] %2: %3").arg(TIMEMS).arg(strType).arg(strData);
    ui->txtMain->append(strData);
    currentCount++;
}

void frmUdpClient::readData()
{
    QHostAddress host;
    quint16 port;
    QByteArray data;
    QString buffer;

    while (udpSocket->hasPendingDatagrams()) {
        data.resize(udpSocket->pendingDatagramSize());
        const qint64 bytesRead = udpSocket->readDatagram(data.data(), data.size(), &host, &port);
        if (bytesRead < 0) {
            break;
        }
        data.resize(static_cast<int>(bytesRead));
        receivedBytes += static_cast<quint64>(bytesRead);
        updateByteCounts();

        if (App::HexReceiveUdpClient) {
            buffer = QUIHelper::byteArrayToHexStr(data);
        } else if (App::AsciiUdpClient) {
            buffer = QUIHelper::byteArrayToAsciiStr(data);
        } else {
            buffer = QString(data);
        }

        QString ip = host.toString();
        ip = ip.replace("::ffff:", "");
        if (ip.isEmpty()) {
            continue;
        }

        QString str = QString("[%1:%2] %3").arg(ip).arg(port).arg(buffer);
        append(1, str);

        if (App::DebugUdpClient) {
            int count = App::Keys.count();
            for (int i = 0; i < count; i++) {
                if (App::Keys.at(i) == buffer) {
                    sendData(ip, port, App::Values.at(i));
                    break;
                }
            }
        }
    }
}

void frmUdpClient::sendData(const QString &ip, int port, const QString &data)
{
    QByteArray buffer;
    if (App::HexSendUdpClient) {
        QString error;
        if (!QUIHelper::parseHexData(data, buffer, error)) {
            ui->labSendHint->setText(error);
            ui->labSendHint->show();
            return;
        }
    } else if (App::AsciiUdpClient) {
        buffer = QUIHelper::asciiStrToByteArray(data);
    } else {
        buffer = data.toLatin1();
    }

    const QHostAddress destination(ip);
    if (!prepareLocalAddress(destination, port)) {
        return;
    }
    ui->labSendHint->hide();
    if (destination.isMulticast()) {
        if (destination.protocol() != QAbstractSocket::IPv4Protocol) {
            ui->labSendHint->setText(tr("当前组播发送仅支持 IPv4 地址。"));
            ui->labSendHint->show();
            return;
        }
        QNetworkInterface iface;
        foreach (const QNetworkInterface &candidate, QNetworkInterface::allInterfaces()) {
            foreach (const QNetworkAddressEntry &entry, candidate.addressEntries()) {
                if (entry.ip() == udpSocket->localAddress()) {
                    iface = candidate;
                }
            }
        }
        if (!iface.isValid()) {
            ui->labSendHint->setText(tr("无法确定组播出口网卡，请选择本地 IP。"));
            ui->labSendHint->show();
            return;
        }
        udpSocket->setMulticastInterface(iface);
        udpSocket->setSocketOption(QAbstractSocket::MulticastTtlOption, 1);
        udpSocket->setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1);
    }
    const qint64 bytesWritten = udpSocket->writeDatagram(buffer, destination, port);
    if (bytesWritten < 0) {
        ui->labSendHint->setText(tr("发送失败：%1").arg(udpSocket->errorString()));
        ui->labSendHint->show();
        return;
    }
    ui->labLocalEndpoint->setText(tr("本地地址：[%1]:%2")
                                 .arg(udpSocket->localAddress().toString()).arg(udpSocket->localPort()));
    sentBytes += static_cast<quint64>(bytesWritten);
    updateByteCounts();

    const QString sentData = App::HexSendUdpClient
            ? QUIHelper::byteArrayToHexStr(buffer) : data;
    QString str = QString("[%1:%2] %3").arg(ip).arg(port).arg(sentData);
    append(0, str);
}

void frmUdpClient::on_cboxLocalIP_currentIndexChanged(int index)
{
    Q_UNUSED(index);
    udpSocket->abort();
    ui->labLocalEndpoint->setText(tr("本地地址：待发送（下次发送时重新绑定）"));
    ui->labSendHint->hide();
}

void frmUdpClient::on_spinLocalPort_valueChanged(int port)
{
    Q_UNUSED(port);
    on_cboxLocalIP_currentIndexChanged(ui->cboxLocalIP->currentIndex());
}

bool frmUdpClient::prepareLocalAddress(const QHostAddress &destination, int port)
{
    QString error;
    QHostAddress local;
    const quint16 localPort = static_cast<quint16>(ui->spinLocalPort->value());
    const QString selected = ui->cboxLocalIP->itemData(ui->cboxLocalIP->currentIndex()).toString();
    if (destination.isNull() || port <= 0 || port > 65535) {
        error = tr("请输入有效的远程 IP 和端口（1-65535）。");
    } else if (selected.isEmpty()) {
        // UDP connect selects a route without transmitting a datagram.
        // Bind the sending socket to that source so its displayed address is exact.
        QUdpSocket route;
        route.connectToHost(destination, port);
        if (route.state() != QAbstractSocket::ConnectedState && !route.waitForConnected(1000)) {
            error = tr("无法自动选择本地地址：%1").arg(route.errorString());
        } else {
            local = route.localAddress();
            if (local.isNull() || local == QHostAddress::Any ||
                    local == QHostAddress::AnyIPv4 || local == QHostAddress::AnyIPv6) {
                error = tr("无法确定本地地址，请手动选择本地 IP。");
            }
        }
    } else {
        local = QHostAddress(selected);
        if (local.protocol() != destination.protocol()) {
            error = tr("本地 IP 与远程 IP 必须使用相同的 IPv4/IPv6 协议。");
        }
    }
    if (error.isEmpty() && (udpSocket->state() != QAbstractSocket::BoundState ||
                           udpSocket->localAddress() != local ||
                           (localPort != 0 && udpSocket->localPort() != localPort))) {
        udpSocket->abort();
        ui->labLocalEndpoint->setText(tr("本地地址：尚未绑定"));
        if (!udpSocket->bind(local, localPort, QAbstractSocket::DontShareAddress)) {
            error = tr("无法绑定本地地址 [%1]:%2：%3").arg(local.toString())
                    .arg(localPort).arg(udpSocket->errorString());
        }
    }
    if (!error.isEmpty()) {
        ui->labSendHint->setText(error);
        ui->labSendHint->show();
        return false;
    }
    return true;
}

void frmUdpClient::updateByteCounts()
{
    ui->labReceivedBytes->setText(tr("接收：%1 字节").arg(receivedBytes));
    ui->labSentBytes->setText(tr("发送：%1 字节").arg(sentBytes));
}

void frmUdpClient::on_btnResetCounts_clicked()
{
    receivedBytes = 0;
    sentBytes = 0;
    updateByteCounts();
}

bool frmUdpClient::saveConfiguration(const QString &fileName, QString &error) const
{
    error.clear();
    bool validPort = false;
    const int remotePort = ui->txtServerPort->text().trimmed().toInt(&validPort);
    const QString remoteAddress = ui->txtServerIP->text().trimmed();
    if (!validPort || remotePort < 1 || remotePort > 65535 || QHostAddress(remoteAddress).isNull()) {
        error = tr("请输入有效的远程 IP 和端口（1-65535）后再保存配置。");
        return false;
    }

    QJsonObject config;
    config.insert("type", "udp-client");
    config.insert("version", 1);
    config.insert("data", ui->cboxData->currentText());
    config.insert("localIP", ui->cboxLocalIP->currentData().toString());
    config.insert("localPort", ui->spinLocalPort->value());
    config.insert("remoteAddress", remoteAddress);
    config.insert("remotePort", remotePort);
    config.insert("hexSend", ui->ckHexSend->isChecked());
    config.insert("ascii", ui->ckAscii->isChecked());

    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        error = tr("无法保存配置：%1").arg(file.errorString());
        return false;
    }
    const QByteArray json = QJsonDocument(config).toJson(QJsonDocument::Indented);
    if (file.write(json) != json.size() || !file.commit()) {
        error = tr("无法保存配置：%1").arg(file.errorString());
        return false;
    }
    return true;
}

bool frmUdpClient::loadConfiguration(const QString &fileName, QString &error)
{
    error.clear();
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        error = tr("无法读取配置：%1").arg(file.errorString());
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = tr("配置文件不是有效的 JSON 对象：%1").arg(parseError.errorString());
        return false;
    }
    const QJsonObject config = document.object();
    const QJsonValue localPort = config.value("localPort");
    const QJsonValue remotePort = config.value("remotePort");
    const QString localIP = config.value("localIP").toString();
    const QString remoteAddress = config.value("remoteAddress").toString();
    if (config.value("type").toString() != "udp-client" ||
            config.value("version").toDouble(-1) != 1 ||
            !config.value("data").isString() || !config.value("localIP").isString() ||
            !config.value("remoteAddress").isString() ||
            !localPort.isDouble() || localPort.toInt(-1) < 0 || localPort.toInt(-1) > 65535 ||
            localPort.toDouble() != localPort.toInt(-1) ||
            !remotePort.isDouble() || remotePort.toInt(-1) < 1 || remotePort.toInt(-1) > 65535 ||
            remotePort.toDouble() != remotePort.toInt(-1) ||
            (!localIP.isEmpty() && QHostAddress(localIP).isNull()) ||
            QHostAddress(remoteAddress).isNull() ||
            !config.value("hexSend").isBool() || !config.value("ascii").isBool()) {
        error = tr("配置格式不正确或版本不支持，请选择由 UDP 客户端导出的配置文件。");
        return false;
    }

    // Validate everything before changing widgets or their persisted settings.
    {
        const QSignalBlocker localBlocker(ui->cboxLocalIP);
        const QSignalBlocker portBlocker(ui->spinLocalPort);
        const QSignalBlocker remoteBlocker(ui->txtServerIP);
        const QSignalBlocker remotePortBlocker(ui->txtServerPort);
        const QSignalBlocker hexBlocker(ui->ckHexSend);
        const QSignalBlocker asciiBlocker(ui->ckAscii);
        int index = ui->cboxLocalIP->findData(localIP);
        if (index < 0) {
            // Preserve a saved address even when its network adapter is currently unavailable.
            ui->cboxLocalIP->addItem(localIP, localIP);
            index = ui->cboxLocalIP->count() - 1;
        }
        ui->cboxLocalIP->setCurrentIndex(index);
        ui->spinLocalPort->setValue(localPort.toInt());
        ui->txtServerIP->setText(remoteAddress);
        ui->txtServerPort->setText(QString::number(remotePort.toInt()));
        ui->cboxData->setEditText(config.value("data").toString());
        ui->ckHexSend->setChecked(config.value("hexSend").toBool());
        ui->ckAscii->setChecked(config.value("ascii").toBool());
    }
    on_cboxLocalIP_currentIndexChanged(ui->cboxLocalIP->currentIndex());
    saveConfig();
    return true;
}

void frmUdpClient::on_btnSaveConfig_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("保存 UDP 客户端配置"),
                                                   "udp_client.json", tr("JSON 配置 (*.json)"));
    if (fileName.isEmpty()) {
        return;
    }
    if (!fileName.endsWith(".json", Qt::CaseInsensitive)) {
        fileName += ".json";
        if (QFile::exists(fileName) && QMessageBox::question(this, tr("覆盖配置"),
                tr("文件已存在，是否覆盖？"), QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No) != QMessageBox::Yes) {
            return;
        }
    }
    QString error;
    if (!saveConfiguration(fileName, error)) {
        QMessageBox::warning(this, tr("保存配置失败"), error);
    }
}

void frmUdpClient::on_btnImportConfig_clicked()
{
    const QString fileName = QFileDialog::getOpenFileName(this, tr("导入 UDP 客户端配置"),
                                                        QString(), tr("JSON 配置 (*.json)"));
    if (fileName.isEmpty()) {
        return;
    }
    QString error;
    if (!loadConfiguration(fileName, error)) {
        QMessageBox::warning(this, tr("导入配置失败"), error);
    }
}

void frmUdpClient::on_btnSave_clicked()
{
    QString data = ui->txtMain->toPlainText();
    if (data.length() <= 0) {
        return;
    }

    QString fileName = QString("%1/%2.txt").arg(QUIHelper::appPath()).arg(STRDATETIME);
    QFile file(fileName);
    if (file.open(QFile::WriteOnly | QFile::Text)) {
        file.write(data.toUtf8());
        file.close();
    }

    on_btnClear_clicked();
}

void frmUdpClient::on_btnClear_clicked()
{
    append(0, "", true);
}

void frmUdpClient::on_btnSend_clicked()
{
    QString data = ui->cboxData->currentText();
    if (data.length() <= 0) {
        return;
    }

    sendData(App::UdpServerIP, App::UdpServerPort, data);
}
