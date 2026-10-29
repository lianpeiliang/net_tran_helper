#include "frmudpclient.h"
#include "ui_frmudpclient.h"
#include "quiwidget.h"

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
