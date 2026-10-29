#include "frmmain.h"
#include "ui_frmmain.h"
#include "quiwidget.h"

frmMain::frmMain(QWidget *parent) : QWidget(parent), ui(new Ui::frmMain)
{
    ui->setupUi(this);
    QSettings settings(App::ConfigFile, QSettings::IniFormat);
    const int themeIndex = settings.value("Appearance/Theme", "blue").toString() == "dark" ? 1 : 0;
    ui->cboxTheme->blockSignals(true);
    ui->cboxTheme->setCurrentIndex(themeIndex);
    ui->cboxTheme->blockSignals(false);
    on_cboxTheme_currentIndexChanged(themeIndex);
    QStringList ips;
    foreach (const QNetworkInterface &iface, QNetworkInterface::allInterfaces()) {
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
            const QString ip = entry.ip().toString();
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol &&
                    ip != "127.0.0.1" && !ips.contains(ip)) {
                ips.append(ip);
            }
        }
    }
    ips.append("127.0.0.1");
    QFont addressFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    addressFont.setPointSizeF(ui->labLocalIP->font().pointSizeF());
    ui->labLocalIP->setFont(addressFont);
    QStringList columns;
    foreach (const QString &ip, ips) {
        // IPv4 addresses use at most 15 characters; reserve three more for spacing.
        columns.append(ip.leftJustified(18, QLatin1Char(' ')));
    }
    ui->labLocalIP->setTextFormat(Qt::PlainText);
    ui->labLocalIP->setText(tr("本机 IP：%1").arg(columns.join("").trimmed()));
    // Stored indices retain the original TCP-first ordering for compatibility.
    const int savedIndex = App::CurrentIndex;
    ui->tabWidget->setCurrentIndex(savedIndex >= 0 && savedIndex < 4 ? (savedIndex + 2) % 4 : 0);
}

frmMain::~frmMain()
{
    delete ui;
}

void frmMain::on_cboxTheme_currentIndexChanged(int index)
{
    const QString name = index == 1 ? "dark" : "blue";
    QFile theme(QString(":/%1.qss").arg(name));
    if (theme.open(QFile::ReadOnly)) {
        qApp->setStyleSheet(QString::fromUtf8(theme.readAll()));
        QSettings settings(App::ConfigFile, QSettings::IniFormat);
        settings.setValue("Appearance/Theme", name);
    }
}

void frmMain::on_tabWidget_currentChanged(int index)
{
    App::CurrentIndex = (index + 2) % 4;
    App::writeConfig();
}
