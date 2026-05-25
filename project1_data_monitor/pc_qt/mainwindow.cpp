#include "mainwindow.h"

#include <QComboBox>
#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHostAddress>
#include <QHBoxLayout>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidgetItem>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      server_(new QTcpServer(this)),
      client_(nullptr)
{
    buildUi();

    connect(server_, &QTcpServer::newConnection, this, &MainWindow::acceptClient);
    setServerRunning(false);
    detachClient();
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *root = new QHBoxLayout(central);

    auto *left = new QVBoxLayout;
    auto *right = new QVBoxLayout;
    root->addLayout(left, 0);
    root->addLayout(right, 1);

    auto *connBox = new QGroupBox(tr("Connection"), central);
    auto *connLayout = new QFormLayout(connBox);
    hostEdit_ = new QLineEdit(QStringLiteral("0.0.0.0"), connBox);
    portSpin_ = new QSpinBox(connBox);
    portSpin_->setRange(1, 65535);
    portSpin_->setValue(9000);
    listenButton_ = new QPushButton(tr("Listen"), connBox);
    stopButton_ = new QPushButton(tr("Stop"), connBox);
    serverStateLabel_ = new QLabel(connBox);
    clientStateLabel_ = new QLabel(connBox);
    connLayout->addRow(tr("Host"), hostEdit_);
    connLayout->addRow(tr("Port"), portSpin_);
    connLayout->addRow(listenButton_, stopButton_);
    connLayout->addRow(tr("Server"), serverStateLabel_);
    connLayout->addRow(tr("Device"), clientStateLabel_);
    left->addWidget(connBox);

    auto *cmdBox = new QGroupBox(tr("Remote Control"), central);
    auto *cmdLayout = new QFormLayout(cmdBox);
    ledOnButton_ = new QPushButton(tr("LED On"), cmdBox);
    ledOffButton_ = new QPushButton(tr("LED Off"), cmdBox);
    beepOnButton_ = new QPushButton(tr("Beep On"), cmdBox);
    beepOffButton_ = new QPushButton(tr("Beep Off"), cmdBox);
    intervalSpin_ = new QSpinBox(cmdBox);
    intervalSpin_->setRange(100, 60000);
    intervalSpin_->setSuffix(tr(" ms"));
    intervalSpin_->setValue(1000);
    psThresholdSpin_ = new QSpinBox(cmdBox);
    psThresholdSpin_->setRange(0, 65535);
    psThresholdSpin_->setValue(1000);
    alsThresholdSpin_ = new QSpinBox(cmdBox);
    alsThresholdSpin_->setRange(0, 65535);
    alsThresholdSpin_->setValue(60000);
    modeCombo_ = new QComboBox(cmdBox);
    modeCombo_->addItem(tr("normal"), QStringLiteral("normal"));
    modeCombo_->addItem(tr("quiet"), QStringLiteral("quiet"));
    modeCombo_->addItem(tr("alarm_only"), QStringLiteral("alarm_only"));
    auto *intervalButton = new QPushButton(tr("Apply Interval"), cmdBox);
    auto *thresholdButton = new QPushButton(tr("Apply Threshold"), cmdBox);
    auto *modeButton = new QPushButton(tr("Apply Mode"), cmdBox);
    auto *shutdownButton = new QPushButton(tr("Shutdown Collector"), cmdBox);
    cmdLayout->addRow(ledOnButton_, ledOffButton_);
    cmdLayout->addRow(beepOnButton_, beepOffButton_);
    cmdLayout->addRow(tr("Interval"), intervalSpin_);
    cmdLayout->addRow(intervalButton);
    cmdLayout->addRow(tr("PS Threshold"), psThresholdSpin_);
    cmdLayout->addRow(tr("ALS Threshold"), alsThresholdSpin_);
    cmdLayout->addRow(thresholdButton);
    cmdLayout->addRow(tr("Mode"), modeCombo_);
    cmdLayout->addRow(modeButton);
    cmdLayout->addRow(shutdownButton);
    left->addWidget(cmdBox);
    left->addStretch(1);

    auto *stateBox = new QGroupBox(tr("Device State"), central);
    auto *stateLayout = new QFormLayout(stateBox);
    modeLabel_ = new QLabel(stateBox);
    lastUpdateLabel_ = new QLabel(stateBox);
    stateLayout->addRow(tr("Mode"), modeLabel_);
    stateLayout->addRow(tr("Last Update"), lastUpdateLabel_);
    right->addWidget(stateBox);

    metricTable_ = new QTableWidget(12, 2, central);
    metricTable_->setHorizontalHeaderLabels({tr("Metric"), tr("Value")});
    metricTable_->verticalHeader()->setVisible(false);
    metricTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    metricTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    const QStringList names = {
        tr("IR"), tr("ALS"), tr("PS"),
        tr("Accel X"), tr("Accel Y"), tr("Accel Z"),
        tr("Temperature"), tr("Gyro X"), tr("Gyro Y"), tr("Gyro Z"),
        tr("LED"), tr("Beep")
    };
    for (int row = 0; row < names.size(); ++row) {
        metricTable_->setItem(row, 0, new QTableWidgetItem(names.at(row)));
        metricTable_->setItem(row, 1, new QTableWidgetItem(QStringLiteral("-")));
    }
    right->addWidget(metricTable_, 2);

    eventLog_ = new QPlainTextEdit(central);
    eventLog_->setReadOnly(true);
    right->addWidget(eventLog_, 1);

    setCentralWidget(central);
    setWindowTitle(tr("Project1 i.MX6ULL Monitor"));

    connect(listenButton_, &QPushButton::clicked, this, &MainWindow::startServer);
    connect(stopButton_, &QPushButton::clicked, this, &MainWindow::stopServer);
    connect(ledOnButton_, &QPushButton::clicked, this, &MainWindow::sendLedCommand);
    connect(ledOffButton_, &QPushButton::clicked, this, &MainWindow::sendLedCommand);
    connect(beepOnButton_, &QPushButton::clicked, this, &MainWindow::sendBeepCommand);
    connect(beepOffButton_, &QPushButton::clicked, this, &MainWindow::sendBeepCommand);
    connect(intervalButton, &QPushButton::clicked, this, &MainWindow::sendIntervalCommand);
    connect(thresholdButton, &QPushButton::clicked, this, &MainWindow::sendThresholdCommand);
    connect(modeButton, &QPushButton::clicked, this, &MainWindow::sendModeCommand);
    connect(shutdownButton, &QPushButton::clicked, this, &MainWindow::sendShutdownCommand);
}

void MainWindow::startServer()
{
    const QHostAddress address(hostEdit_->text());
    if (!server_->listen(address, static_cast<quint16>(portSpin_->value()))) {
        appendLog(tr("listen failed: %1").arg(server_->errorString()));
        return;
    }

    setServerRunning(true);
    appendLog(tr("listening on %1:%2").arg(hostEdit_->text()).arg(portSpin_->value()));
}

void MainWindow::stopServer()
{
    detachClient();
    server_->close();
    setServerRunning(false);
    appendLog(tr("server stopped"));
}

void MainWindow::setServerRunning(bool running)
{
    listenButton_->setEnabled(!running);
    stopButton_->setEnabled(running);
    hostEdit_->setEnabled(!running);
    portSpin_->setEnabled(!running);
    serverStateLabel_->setText(running ? tr("listening") : tr("stopped"));
}

void MainWindow::acceptClient()
{
    while (server_->hasPendingConnections()) {
        attachClient(server_->nextPendingConnection());
    }
}

void MainWindow::attachClient(QTcpSocket *socket)
{
    detachClient();
    client_ = socket;
    rxBuffer_.clear();
    connect(client_, &QTcpSocket::readyRead, this, &MainWindow::readClientData);
    connect(client_, &QTcpSocket::disconnected, this, &MainWindow::clientDisconnected);
    clientStateLabel_->setText(QStringLiteral("%1:%2")
                                   .arg(client_->peerAddress().toString())
                                   .arg(client_->peerPort()));
    appendLog(tr("device connected: %1").arg(clientStateLabel_->text()));
}

void MainWindow::detachClient()
{
    if (client_) {
        client_->disconnect(this);
        client_->close();
        client_->deleteLater();
        client_ = nullptr;
    }
    rxBuffer_.clear();
    clientStateLabel_->setText(tr("not connected"));
}

void MainWindow::clientDisconnected()
{
    appendLog(tr("device disconnected"));
    detachClient();
}

void MainWindow::readClientData()
{
    rxBuffer_.append(client_->readAll());
    int pos = -1;
    while ((pos = rxBuffer_.indexOf('\n')) >= 0) {
        const QByteArray line = rxBuffer_.left(pos).trimmed();
        rxBuffer_.remove(0, pos + 1);
        if (!line.isEmpty()) {
            handleLine(line);
        }
    }
}

void MainWindow::handleLine(const QByteArray &line)
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        appendLog(tr("raw: %1").arg(QString::fromUtf8(line)));
        return;
    }

    const QJsonObject obj = doc.object();
    const QString type = obj.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("status")) {
        handleStatus(obj);
    } else if (type == QLatin1String("ack")) {
        appendLog(tr("ack cmd=%1 ok=%2 msg=%3")
                      .arg(obj.value(QStringLiteral("cmd")).toString())
                      .arg(obj.value(QStringLiteral("ok")).toInt())
                      .arg(obj.value(QStringLiteral("msg")).toString()));
    } else if (type == QLatin1String("register")) {
        appendLog(tr("registered %1 version %2")
                      .arg(obj.value(QStringLiteral("device")).toString())
                      .arg(obj.value(QStringLiteral("version")).toString()));
    } else {
        appendLog(QString::fromUtf8(line));
    }
}

void MainWindow::handleStatus(const QJsonObject &obj)
{
    modeLabel_->setText(obj.value(QStringLiteral("mode")).toString());
    lastUpdateLabel_->setText(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")));

    const bool envOk = obj.value(QStringLiteral("env_ok")).toInt() != 0;
    const bool imuOk = obj.value(QStringLiteral("imu_ok")).toInt() != 0;
    setMetric(0, QString::number(obj.value(QStringLiteral("ir")).toInt()), envOk);
    setMetric(1, QString::number(obj.value(QStringLiteral("als")).toInt()), envOk);
    setMetric(2, QString::number(obj.value(QStringLiteral("ps")).toInt()), envOk);
    setMetric(3, QString::number(obj.value(QStringLiteral("accel_x")).toInt()), imuOk);
    setMetric(4, QString::number(obj.value(QStringLiteral("accel_y")).toInt()), imuOk);
    setMetric(5, QString::number(obj.value(QStringLiteral("accel_z")).toInt()), imuOk);
    setMetric(6, QString::number(obj.value(QStringLiteral("temp")).toInt()), imuOk);
    setMetric(7, QString::number(obj.value(QStringLiteral("gyro_x")).toInt()), imuOk);
    setMetric(8, QString::number(obj.value(QStringLiteral("gyro_y")).toInt()), imuOk);
    setMetric(9, QString::number(obj.value(QStringLiteral("gyro_z")).toInt()), imuOk);
    setMetric(10, QString::number(obj.value(QStringLiteral("led")).toInt()));
    setMetric(11, QString::number(obj.value(QStringLiteral("beep")).toInt()));

    appendLog(statusSummary(obj));
}

void MainWindow::setMetric(int row, const QString &value, bool ok)
{
    auto *item = metricTable_->item(row, 1);
    if (!item) {
        item = new QTableWidgetItem;
        metricTable_->setItem(row, 1, item);
    }
    item->setText(ok ? value : tr("ERR"));
}

QString MainWindow::statusSummary(const QJsonObject &obj) const
{
    return tr("status mode=%1 ps=%2 als=%3 led=%4 beep=%5")
        .arg(obj.value(QStringLiteral("mode")).toString())
        .arg(obj.value(QStringLiteral("ps")).toInt())
        .arg(obj.value(QStringLiteral("als")).toInt())
        .arg(obj.value(QStringLiteral("led")).toInt())
        .arg(obj.value(QStringLiteral("beep")).toInt());
}

void MainWindow::sendCommand(const QJsonObject &obj)
{
    if (!client_ || client_->state() != QAbstractSocket::ConnectedState) {
        appendLog(tr("no connected device"));
        return;
    }

    const QByteArray line = QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n';
    client_->write(line);
    appendLog(tr("send %1").arg(QString::fromUtf8(line).trimmed()));
}

void MainWindow::sendLedCommand()
{
    const int value = sender() == ledOnButton_ ? 1 : 0;
    sendCommand({{QStringLiteral("type"), QStringLiteral("command")},
                 {QStringLiteral("cmd"), QStringLiteral("set_led")},
                 {QStringLiteral("value"), value}});
}

void MainWindow::sendBeepCommand()
{
    const int value = sender() == beepOnButton_ ? 1 : 0;
    sendCommand({{QStringLiteral("type"), QStringLiteral("command")},
                 {QStringLiteral("cmd"), QStringLiteral("set_beep")},
                 {QStringLiteral("value"), value}});
}

void MainWindow::sendIntervalCommand()
{
    sendCommand({{QStringLiteral("type"), QStringLiteral("command")},
                 {QStringLiteral("cmd"), QStringLiteral("set_interval")},
                 {QStringLiteral("value"), intervalSpin_->value()}});
}

void MainWindow::sendThresholdCommand()
{
    sendCommand({{QStringLiteral("type"), QStringLiteral("command")},
                 {QStringLiteral("cmd"), QStringLiteral("set_threshold")},
                 {QStringLiteral("ps"), psThresholdSpin_->value()},
                 {QStringLiteral("als"), alsThresholdSpin_->value()}});
}

void MainWindow::sendModeCommand()
{
    sendCommand({{QStringLiteral("type"), QStringLiteral("command")},
                 {QStringLiteral("cmd"), QStringLiteral("set_mode")},
                 {QStringLiteral("mode"), modeCombo_->currentData().toString()}});
}

void MainWindow::sendShutdownCommand()
{
    sendCommand({{QStringLiteral("type"), QStringLiteral("command")},
                 {QStringLiteral("cmd"), QStringLiteral("shutdown")}});
}

void MainWindow::appendLog(const QString &text)
{
    eventLog_->appendPlainText(QStringLiteral("[%1] %2")
                                   .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss")))
                                   .arg(text));
}
