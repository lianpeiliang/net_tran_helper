#include <QtTest>
#include <QtWidgets>
#include "app.h"
#include "frmudpclient.h"
#include "frmudpserver.h"

class UdpByteCountsTest : public QObject
{
    Q_OBJECT

    static quint64 count(QWidget *widget, const char *name)
    {
        const QString text = widget->findChild<QLabel *>(name)->text();
        QRegExp digits("[0-9]+");
        return digits.indexIn(text) < 0 ? quint64(-1) : digits.cap().toULongLong();
    }

    static bool send(QWidget *widget, const QString &ip, int port, const QString &data)
    {
        return QMetaObject::invokeMethod(widget, "sendData", Qt::DirectConnection,
                                        Q_ARG(QString, ip), Q_ARG(int, port), Q_ARG(QString, data));
    }

private slots:
    void init()
    {
        App::Intervals = QStringList() << "1000";
        App::HexSendUdpClient = App::HexSendUdpServer = false;
        App::HexReceiveUdpClient = App::HexReceiveUdpServer = false;
        App::AsciiUdpClient = App::AsciiUdpServer = false;
        App::DebugUdpClient = App::DebugUdpServer = false;
        App::AutoSendUdpClient = App::AutoSendUdpServer = false;
        App::SelectAllUdpServer = false;
    }

    void counters_data()
    {
        QTest::addColumn<bool>("server");
        QTest::newRow("UDP client") << false;
        QTest::newRow("UDP server") << true;
    }

    void counters()
    {
        QFETCH(bool, server);
        QTemporaryDir settings;
        App::ConfigFile = settings.filePath("test.ini");
        QScopedPointer<QWidget> widget(server ? static_cast<QWidget *>(new frmUdpServer)
                                              : static_cast<QWidget *>(new frmUdpClient));
        QUdpSocket *socket = widget->findChild<QUdpSocket *>();
        QVERIFY(socket->bind(QHostAddress(QHostAddress::LocalHost), 0));
        QUdpSocket peer;
        QVERIFY(peer.bind(QHostAddress(QHostAddress::LocalHost), 0));
        QCOMPARE(count(widget.data(), "labReceivedBytes"), quint64(0));
        QCOMPARE(count(widget.data(), "labSentBytes"), quint64(0));

        // Hex text contains eight characters, but only three bytes are sent.
        widget->findChild<QCheckBox *>("ckHexSend")->setChecked(true);
        QVERIFY(send(widget.data(), "127.0.0.1", peer.localPort(), "00 01 FF"));
        QTRY_VERIFY(peer.hasPendingDatagrams());
        QByteArray payload(3, '\0');
        QCOMPARE(peer.readDatagram(payload.data(), payload.size()), qint64(3));
        QCOMPARE(payload, QByteArray::fromHex("0001ff"));
        QCOMPARE(count(widget.data(), "labSentBytes"), quint64(3));

        // Failed writes must not increase the total.
        QVERIFY(send(widget.data(), "invalid-address", peer.localPort(), "00 01 FF"));
        QCOMPARE(count(widget.data(), "labSentBytes"), quint64(3));

        const QString previousLog = widget->findChild<QTextEdit *>("txtMain")->toPlainText();
        widget->findChild<QCheckBox *>("ckShow")->setChecked(true);
        widget->findChild<QCheckBox *>("ckHexReceive")->setChecked(true);
        QCOMPARE(peer.writeDatagram(payload, QHostAddress::LocalHost, socket->localPort()), qint64(3));
        QTRY_COMPARE(count(widget.data(), "labReceivedBytes"), quint64(3));
        QCOMPARE(peer.writeDatagram(QByteArray("ab\r\n"), QHostAddress::LocalHost, socket->localPort()), qint64(4));
        QTRY_COMPARE(count(widget.data(), "labReceivedBytes"), quint64(7));
        QCOMPARE(widget->findChild<QTextEdit *>("txtMain")->toPlainText(), previousLog);

        widget->findChild<QCheckBox *>("ckHexReceive")->setChecked(false);
        widget->findChild<QCheckBox *>("ckAscii")->setChecked(true);
        widget->findChild<QPushButton *>("btnClear")->click();
        QCOMPARE(count(widget.data(), "labReceivedBytes"), quint64(7));
        QCOMPARE(count(widget.data(), "labSentBytes"), quint64(3));
        widget->findChild<QPushButton *>("btnResetCounts")->click();
        QCOMPARE(count(widget.data(), "labReceivedBytes"), quint64(0));
        QCOMPARE(count(widget.data(), "labSentBytes"), quint64(0));
        QCOMPARE(peer.writeDatagram(QByteArray("x"), QHostAddress::LocalHost, socket->localPort()), qint64(1));
        QTRY_COMPARE(count(widget.data(), "labReceivedBytes"), quint64(1));
    }

    void serverMissingTarget()
    {
        QTemporaryDir settings;
        App::ConfigFile = settings.filePath("test.ini");
        frmUdpServer server;
        QLabel *hint = server.findChild<QLabel *>("labSendHint");
        QPushButton *button = server.findChild<QPushButton *>("btnSend");
        QListWidget *clients = server.findChild<QListWidget *>("listWidget");
        QCheckBox *all = server.findChild<QCheckBox *>("ckSelectAll");
        server.findChild<QComboBox *>("cboxData")->setEditText("abc");
        QVERIFY(hint->isHidden());
        button->click();
        QVERIFY(hint->text().contains(QString::fromUtf8("暂无客户端")));
        QVERIFY(!hint->isHidden());
        all->setChecked(true);
        button->click();
        QVERIFY(hint->text().contains(QString::fromUtf8("暂无客户端")));
        all->setChecked(false);

        QUdpSocket peer;
        QVERIFY(peer.bind(QHostAddress(QHostAddress::LocalHost), 0));
        clients->addItem(QString("127.0.0.1:%1").arg(peer.localPort()));
        clients->setCurrentRow(0);
        clients->clearSelection(); // A current row is not necessarily selected.
        button->click();
        QVERIFY(hint->text().contains(QString::fromUtf8("请选择")));
        QCOMPARE(count(&server, "labSentBytes"), quint64(0));
        QVERIFY(!peer.hasPendingDatagrams());
        // Repeated timer callbacks update one inline hint without dialogs.
        for (int i = 0; i < 3; ++i) {
            QVERIFY(QMetaObject::invokeMethod(&server, "on_btnSend_clicked"));
        }
        QCOMPARE(count(&server, "labSentBytes"), quint64(0));
        clients->item(0)->setSelected(true);
        button->click();
        QTRY_VERIFY(peer.hasPendingDatagrams());
        QCOMPARE(count(&server, "labSentBytes"), quint64(3));
        QVERIFY(hint->isHidden());
    }

    void serverGroupSend()
    {
        QTemporaryDir settings;
        App::ConfigFile = settings.filePath("test.ini");
        frmUdpServer server;
        QUdpSocket first, second;
        QVERIFY(first.bind(QHostAddress(QHostAddress::LocalHost), 0));
        QVERIFY(second.bind(QHostAddress(QHostAddress::LocalHost), 0));
        QListWidget *clients = server.findChild<QListWidget *>("listWidget");
        clients->addItem(QString("127.0.0.1:%1").arg(first.localPort()));
        clients->addItem(QString("127.0.0.1:%1").arg(second.localPort()));
        server.findChild<QCheckBox *>("ckSelectAll")->setChecked(true);
        server.findChild<QComboBox *>("cboxData")->setEditText("abc");
        server.findChild<QPushButton *>("btnSend")->click();
        QTRY_VERIFY(first.hasPendingDatagrams());
        QTRY_VERIFY(second.hasPendingDatagrams());
        QCOMPARE(count(&server, "labSentBytes"), quint64(6));
    }
};

QTEST_MAIN(UdpByteCountsTest)
#include "tst_udp_byte_counts.moc"
