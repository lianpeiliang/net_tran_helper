#ifndef FRMSERIAL_H
#define FRMSERIAL_H

#include <QtWidgets>
#include <QtSerialPort/QSerialPort>

class frmSerial : public QWidget
{
    Q_OBJECT
public:
    explicit frmSerial(QWidget *parent = 0);
    ~frmSerial();

private slots:
    void refreshPorts();
    void togglePort();
    void sendData();
    void readData();
    void saveConfig();
    void changeTimer();
    void handleError(QSerialPort::SerialPortError error);
    void bytesWritten(qint64 bytes);
    void resetCounts();
    void saveLog();

private:
    QSerialPort *serial;
    QTimer *timer;
    QComboBox *port, *baud, *dataBits, *parity, *stopBits, *flow, *data;
    QCheckBox *hexSend, *hexReceive, *ascii, *autoSend, *pause;
    QSpinBox *interval;
    QPushButton *openButton, *refreshButton, *sendButton;
    QTextEdit *log;
    QLabel *hint, *counts;
    QList<QWidget *> connectionControls;
    quint64 receivedBytes, sentBytes;
    void closePort();
    void updateCounts();
    void append(bool sending, const QString &text);
};

#endif
