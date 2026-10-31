#ifndef FRMUDPCLIENT_H
#define FRMUDPCLIENT_H

#include <QWidget>
#include <QtNetwork>

namespace Ui {
class frmUdpClient;
}

class frmUdpClient : public QWidget
{
    Q_OBJECT

public:
    explicit frmUdpClient(QWidget *parent = 0);
    ~frmUdpClient();

    bool saveConfiguration(const QString &fileName, QString &error) const;
    bool loadConfiguration(const QString &fileName, QString &error);

private:
    Ui::frmUdpClient *ui;

    QUdpSocket *udpSocket;
    QTimer *timer;
    quint64 receivedBytes;
    quint64 sentBytes;

    void updateByteCounts();
    bool prepareLocalAddress(const QHostAddress &destination, int port);

private slots:
    void initForm();
    void initConfig();
    void saveConfig();
    void changeTimer();
    void append(int type, const QString &data, bool clear = false);

    void readData();
    void sendData(const QString &ip, int port, const QString &data);

private slots:
    void on_btnSaveConfig_clicked();
    void on_btnImportConfig_clicked();
    void on_btnSave_clicked();
    void on_btnClear_clicked();
    void on_btnSend_clicked();
    void on_btnResetCounts_clicked();
    void on_cboxLocalIP_currentIndexChanged(int index);
    void on_spinLocalPort_valueChanged(int port);
};

#endif // FRMUDPCLIENT_H
