#include <QtTest>
#include "frmserial.h"
#include "app.h"

class SerialModuleTest : public QObject
{
    Q_OBJECT
private slots:
    void settingsSurviveConstruction()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        App::ConfigFile = dir.filePath("test.ini");
        {
            QSettings settings(App::ConfigFile, QSettings::IniFormat);
            settings.setValue("SerialConfig/Port", "COM_TEST");
            settings.setValue("SerialConfig/Baud", "38400");
            settings.setValue("SerialConfig/DataBits", "7");
            settings.setValue("SerialConfig/HexSend", true);
            settings.setValue("SerialConfig/HexReceive", true);
            settings.setValue("SerialConfig/AutoSend", true);
            settings.setValue("SerialConfig/Interval", 250);
        }
        {
            frmSerial widget;
            QCOMPARE(widget.findChild<QComboBox *>("Port")->currentText(), QString("COM_TEST"));
            QCOMPARE(widget.findChild<QComboBox *>("Baud")->currentText(), QString("38400"));
            QCOMPARE(widget.findChild<QComboBox *>("DataBits")->currentText(), QString("7"));
            QVERIFY(widget.findChild<QCheckBox *>("HexSend")->isChecked());
            QVERIFY(widget.findChild<QCheckBox *>("HexReceive")->isChecked());
            QVERIFY(widget.findChild<QCheckBox *>("AutoSend")->isChecked());
            QCOMPARE(widget.findChild<QSpinBox *>("Interval")->value(), 250);
            QVERIFY(!widget.findChild<QTimer *>()->isActive());
            QVERIFY(!widget.findChild<QSerialPort *>()->isOpen());
            widget.findChild<QComboBox *>("Baud")->setEditText("57600");
        }
        frmSerial restored;
        QCOMPARE(restored.findChild<QComboBox *>("Baud")->currentText(), QString("57600"));
        QVERIFY(restored.findChild<QCheckBox *>("HexReceive")->isChecked());
        QCOMPARE(restored.findChild<QSpinBox *>("Interval")->value(), 250);
    }

    void closedPortAndFailedOpen()
    {
        QTemporaryDir dir;
        App::ConfigFile = dir.filePath("test.ini");
        frmSerial widget;
        QLabel *hint = widget.findChild<QLabel *>("labHint");
        QSerialPort *serial = widget.findChild<QSerialPort *>();
        QPushButton *send = widget.findChild<QPushButton *>("btnSend");
        QVERIFY(!send->isEnabled());
        widget.findChild<QComboBox *>("cboxData")->setEditText("test");
        QVERIFY(QMetaObject::invokeMethod(&widget, "sendData"));
        QVERIFY(!hint->text().isEmpty());
        QVERIFY(widget.findChild<QTextEdit *>("txtMain")->toPlainText().isEmpty());
        QCOMPARE(serial->bytesToWrite(), qint64(0));

        widget.findChild<QComboBox *>("Port")->setEditText("INVALID_SERIAL_PORT_FOR_TEST");
        widget.findChild<QComboBox *>("Baud")->setEditText("invalid");
        QVERIFY(QMetaObject::invokeMethod(&widget, "togglePort"));
        const QString invalidHint = hint->text();
        QVERIFY(!serial->isOpen());
        widget.findChild<QComboBox *>("Baud")->setEditText("9600");
        widget.findChild<QCheckBox *>("AutoSend")->setChecked(true);
        QVERIFY(QMetaObject::invokeMethod(&widget, "togglePort"));
        QVERIFY(hint->text() != invalidHint);
        QVERIFY(!serial->isOpen());
        QVERIFY(!send->isEnabled());
        QVERIFY(widget.findChild<QComboBox *>("Port")->isEnabled());
        QVERIFY(!widget.findChild<QTimer *>()->isActive());
    }
};

QTEST_MAIN(SerialModuleTest)
#include "tst_serial_module.moc"
