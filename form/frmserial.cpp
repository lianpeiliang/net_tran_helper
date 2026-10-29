#include "frmserial.h"
#include "quiwidget.h"
#include <QtSerialPort/QSerialPortInfo>

frmSerial::frmSerial(QWidget *parent) : QWidget(parent), receivedBytes(0), sentBytes(0)
{
    serial = new QSerialPort(this);
    timer = new QTimer(this);
    QVBoxLayout *layout = new QVBoxLayout(this);
    QGridLayout *settings = new QGridLayout;
    layout->addLayout(settings);
    port = new QComboBox(this);
    port->setEditable(true);
    baud = new QComboBox(this);
    baud->setEditable(true);
    baud->addItems(QStringList() << "1200" << "2400" << "4800" << "9600" << "19200"
                   << "38400" << "57600" << "115200" << "230400" << "460800" << "921600");
    dataBits = new QComboBox(this);
    for (int i = 5; i <= 8; ++i) dataBits->addItem(QString::number(i), i);
    parity = new QComboBox(this);
    parity->addItem(tr("无"), QSerialPort::NoParity);
    parity->addItem(tr("奇校验"), QSerialPort::OddParity);
    parity->addItem(tr("偶校验"), QSerialPort::EvenParity);
    parity->addItem(tr("Mark"), QSerialPort::MarkParity);
    parity->addItem(tr("Space"), QSerialPort::SpaceParity);
    stopBits = new QComboBox(this);
    stopBits->addItem("1", QSerialPort::OneStop);
    stopBits->addItem("1.5", QSerialPort::OneAndHalfStop);
    stopBits->addItem("2", QSerialPort::TwoStop);
    flow = new QComboBox(this);
    flow->addItem(tr("无"), QSerialPort::NoFlowControl);
    flow->addItem("RTS/CTS", QSerialPort::HardwareControl);
    flow->addItem("XON/XOFF", QSerialPort::SoftwareControl);
    const QList<QComboBox *> boxes = QList<QComboBox *>() << port << baud << dataBits << parity << stopBits << flow;
    const QStringList names = QStringList() << "Port" << "Baud" << "DataBits" << "Parity" << "StopBits" << "Flow";
    const QStringList labels = QStringList() << tr("串口") << tr("波特率") << tr("数据位")
                                            << tr("校验位") << tr("停止位") << tr("流控");
    for (int i = 0; i < boxes.size(); ++i) {
        boxes[i]->setObjectName(names[i]);
        settings->addWidget(new QLabel(labels[i], this), i / 3, (i % 3) * 2);
        settings->addWidget(boxes[i], i / 3, (i % 3) * 2 + 1);
        connectionControls.append(boxes[i]);
    }
    refreshButton = new QPushButton(tr("刷新串口"), this);
    openButton = new QPushButton(tr("打开串口"), this);
    openButton->setObjectName("btnOpen");
    settings->addWidget(refreshButton, 0, 6);
    settings->addWidget(openButton, 1, 6);
    connectionControls.append(refreshButton);
    QLabel *note = new QLabel(tr("RS422/RS485 请使用对应适配器；RS485 收发方向由适配器/驱动自动控制。接收按字节流显示。"), this);
    note->setWordWrap(true);
    layout->addWidget(note);
    log = new QTextEdit(this);
    log->setReadOnly(true);
    log->setObjectName("txtMain");
    log->document()->setMaximumBlockCount(1000);
    layout->addWidget(log, 1);

    QHBoxLayout *options = new QHBoxLayout;
    hexSend = new QCheckBox(tr("十六进制发送"), this);
    hexReceive = new QCheckBox(tr("十六进制接收"), this);
    ascii = new QCheckBox(tr("ASCII模式"), this);
    autoSend = new QCheckBox(tr("自动发送"), this);
    pause = new QCheckBox(tr("暂停显示"), this);
    const QList<QCheckBox *> checks = QList<QCheckBox *>() << hexSend << hexReceive << ascii << autoSend << pause;
    const QStringList checkNames = QStringList() << "HexSend" << "HexReceive" << "Ascii" << "AutoSend" << "Pause";
    for (int i = 0; i < checks.size(); ++i) {
        checks[i]->setObjectName(checkNames[i]);
        options->addWidget(checks[i]);
    }
    interval = new QSpinBox(this);
    interval->setObjectName("Interval");
    interval->setRange(1, 3600000);
    interval->setSuffix(" ms");
    options->addWidget(interval);
    layout->addLayout(options);
    QHBoxLayout *sendRow = new QHBoxLayout;
    data = new QComboBox(this);
    data->setObjectName("cboxData");
    data->setEditable(true);
    data->addItems(App::Datas);
    sendButton = new QPushButton(tr("发送"), this);
    sendButton->setObjectName("btnSend");
    sendButton->setEnabled(false);
    sendRow->addWidget(data, 1);
    sendRow->addWidget(sendButton);
    layout->addLayout(sendRow);
    hint = new QLabel(this);
    hint->setObjectName("labHint");
    hint->setWordWrap(true);
    hint->setTextFormat(Qt::PlainText);
    layout->addWidget(hint);
    QHBoxLayout *footer = new QHBoxLayout;
    counts = new QLabel(this);
    counts->setObjectName("labCounts");
    QPushButton *reset = new QPushButton(tr("统计清零"), this);
    QPushButton *clear = new QPushButton(tr("清空日志"), this);
    QPushButton *save = new QPushButton(tr("保存日志"), this);
    footer->addWidget(counts, 1);
    footer->addWidget(reset);
    footer->addWidget(clear);
    footer->addWidget(save);
    layout->addLayout(footer);

    refreshPorts();
    QSettings config(App::ConfigFile, QSettings::IniFormat);
    config.beginGroup("SerialConfig");
    const QStringList defaults = QStringList() << port->currentText() << "115200" << "8" << tr("无") << "1" << tr("无");
    for (int i = 0; i < boxes.size(); ++i) {
        const QString value = config.value(names[i], defaults[i]).toString();
        const int index = boxes[i]->findText(value);
        if (index >= 0) boxes[i]->setCurrentIndex(index);
        else if (boxes[i]->isEditable()) boxes[i]->setEditText(value);
    }
    for (int i = 0; i < checks.size() - 1; ++i) {
        checks[i]->setChecked(config.value(checkNames[i], false).toBool());
    }
    interval->setValue(config.value("Interval", 1000).toInt());
    for (int i = 0; i < boxes.size(); ++i)
        connect(boxes[i], SIGNAL(currentTextChanged(QString)), this, SLOT(saveConfig()));
    for (int i = 0; i < checks.size() - 1; ++i)
        connect(checks[i], SIGNAL(toggled(bool)), this, SLOT(saveConfig()));
    connect(interval, SIGNAL(valueChanged(int)), this, SLOT(saveConfig()));
    connect(refreshButton, SIGNAL(clicked()), this, SLOT(refreshPorts()));
    connect(openButton, SIGNAL(clicked()), this, SLOT(togglePort()));
    connect(sendButton, SIGNAL(clicked()), this, SLOT(sendData()));
    connect(timer, SIGNAL(timeout()), this, SLOT(sendData()));
    connect(serial, SIGNAL(readyRead()), this, SLOT(readData()));
    connect(serial, SIGNAL(bytesWritten(qint64)), this, SLOT(bytesWritten(qint64)));
    connect(serial, SIGNAL(errorOccurred(QSerialPort::SerialPortError)), this, SLOT(handleError(QSerialPort::SerialPortError)));
    connect(reset, SIGNAL(clicked()), this, SLOT(resetCounts()));
    connect(clear, SIGNAL(clicked()), log, SLOT(clear()));
    connect(save, SIGNAL(clicked()), this, SLOT(saveLog()));
    updateCounts();
}

frmSerial::~frmSerial()
{
    timer->stop();
    serial->blockSignals(true);
    serial->close();
}

void frmSerial::refreshPorts()
{
    const QSignalBlocker blocker(port);
    const QString selected = port->currentText();
    port->clear();
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        port->addItem(info.portName());
        port->setItemData(port->count() - 1, info.description(), Qt::ToolTipRole);
    }
    if (!selected.isEmpty()) port->setEditText(selected);
}

void frmSerial::saveConfig()
{
    QSettings config(App::ConfigFile, QSettings::IniFormat);
    config.beginGroup("SerialConfig");
    foreach (QWidget *control, connectionControls) {
        QComboBox *box = qobject_cast<QComboBox *>(control);
        if (box && !box->currentText().isEmpty()) config.setValue(box->objectName(), box->currentText());
    }
    const QList<QCheckBox *> checks = QList<QCheckBox *>() << hexSend << hexReceive << ascii << autoSend;
    foreach (QCheckBox *check, checks) config.setValue(check->objectName(), check->isChecked());
    config.setValue("Interval", interval->value());
    changeTimer();
}

void frmSerial::togglePort()
{
    if (serial->isOpen()) {
        closePort();
        hint->setText(tr("串口已关闭。"));
        return;
    }
    bool valid = false;
    const int rate = baud->currentText().toInt(&valid);
    if (port->currentText().trimmed().isEmpty() || !valid || rate <= 0) {
        hint->setText(tr("请选择串口并输入有效的正整数波特率。"));
        return;
    }
    serial->setPortName(port->currentText().trimmed());
    if (!serial->setBaudRate(rate) ||
        !serial->setDataBits(static_cast<QSerialPort::DataBits>(dataBits->currentData().toInt())) ||
        !serial->setParity(static_cast<QSerialPort::Parity>(parity->currentData().toInt())) ||
        !serial->setStopBits(static_cast<QSerialPort::StopBits>(stopBits->currentData().toInt())) ||
        !serial->setFlowControl(static_cast<QSerialPort::FlowControl>(flow->currentData().toInt())) ||
        !serial->open(QIODevice::ReadWrite)) {
        const QString error = serial->errorString();
        closePort();
        hint->setText(tr("打开失败：%1").arg(error));
        return;
    }
    foreach (QWidget *control, connectionControls) control->setEnabled(false);
    openButton->setText(tr("关闭串口"));
    sendButton->setEnabled(true);
    hint->setText(tr("%1 已打开。").arg(serial->portName()));
    saveConfig();
}

void frmSerial::closePort()
{
    timer->stop();
    // Do not let close-time errors re-enter this function.
    const QSignalBlocker blocker(serial);
    serial->close();
    foreach (QWidget *control, connectionControls) control->setEnabled(true);
    openButton->setText(tr("打开串口"));
    sendButton->setEnabled(false);
}

void frmSerial::changeTimer()
{
    timer->stop();
    if (serial->isOpen() && autoSend->isChecked()) timer->start(interval->value());
}

void frmSerial::sendData()
{
    if (!serial->isOpen()) {
        hint->setText(tr("请先打开串口。"));
        return;
    }
    const QString text = data->currentText();
    if (text.isEmpty()) return;
    QByteArray buffer;
    QString error;
    if (hexSend->isChecked()) {
        if (!QUIHelper::parseHexData(text, buffer, error)) {
            hint->setText(error);
            timer->stop();
            return;
        }
    } else if (ascii->isChecked()) buffer = QUIHelper::asciiStrToByteArray(text);
    else buffer = text.toLatin1();
    // Bound the asynchronous output queue when auto-send is faster than the wire.
    if (serial->bytesToWrite() + buffer.size() > 1024 * 1024) {
        hint->setText(tr("待发送数据过多，已停止自动发送，请等待发送完成。"));
        timer->stop();
        return;
    }
    const qint64 accepted = serial->write(buffer);
    if (accepted < 0) {
        hint->setText(tr("发送失败：%1").arg(serial->errorString()));
        timer->stop();
        return;
    }
    hint->setText(accepted == buffer.size() ? tr("数据已提交发送。") : tr("仅提交 %1 字节，请检查串口状态。").arg(accepted));
    if (accepted != buffer.size()) timer->stop();
    append(true, hexSend->isChecked() ? QUIHelper::byteArrayToHexStr(buffer.left(int(accepted)))
                                   : (ascii->isChecked() ? QUIHelper::byteArrayToAsciiStr(buffer.left(int(accepted)))
                                                        : QString::fromLatin1(buffer.left(int(accepted)))));
}

void frmSerial::readData()
{
    const QByteArray buffer = serial->readAll();
    if (buffer.isEmpty()) return;
    receivedBytes += quint64(buffer.size());
    updateCounts();
    append(false, hexReceive->isChecked() ? QUIHelper::byteArrayToHexStr(buffer)
                 : (ascii->isChecked() ? QUIHelper::byteArrayToAsciiStr(buffer) : QString::fromLatin1(buffer)));
}

void frmSerial::bytesWritten(qint64 bytes)
{
    if (bytes > 0) sentBytes += quint64(bytes);
    updateCounts();
}

void frmSerial::handleError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) return;
    const QString message = serial->errorString();
    if (serial->isOpen() && (error == QSerialPort::ResourceError || error == QSerialPort::ReadError ||
                            error == QSerialPort::WriteError || error == QSerialPort::PermissionError)) closePort();
    hint->setText(tr("串口错误：%1").arg(message));
}

void frmSerial::append(bool sending, const QString &text)
{
    if (pause->isChecked()) return;
    log->setTextColor(QColor(sending ? "darkgreen" : "red"));
    // Insert plain text so received bytes cannot be interpreted as HTML.
    log->moveCursor(QTextCursor::End);
    log->insertPlainText(tr("时间[%1] %2: %3\n").arg(QTime::currentTime().toString("HH:mm:ss.zzz"))
                         .arg(sending ? tr("发送提交") : tr("接收")).arg(text));
    log->ensureCursorVisible();
}

void frmSerial::updateCounts()
{
    counts->setText(tr("接收：%1 字节    发送：%2 字节").arg(receivedBytes).arg(sentBytes));
}

void frmSerial::resetCounts()
{
    receivedBytes = sentBytes = 0;
    updateCounts();
}

void frmSerial::saveLog()
{
    const QString name = QFileDialog::getSaveFileName(this, tr("保存串口日志"),
                         "serial_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".txt", tr("文本文件 (*.txt)"));
    if (name.isEmpty()) return;
    QFile file(name);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text) || file.write(log->toPlainText().toUtf8()) < 0) {
        hint->setText(tr("保存失败：%1").arg(file.errorString()));
        return;
    }
    hint->setText(tr("日志已保存：%1").arg(name));
}
