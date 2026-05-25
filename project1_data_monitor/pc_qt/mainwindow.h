#ifndef PROJECT1_MAINWINDOW_H
#define PROJECT1_MAINWINDOW_H

#include <QJsonObject>
#include <QMainWindow>
#include <QTcpServer>
#include <QTcpSocket>

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QComboBox;
class QPlainTextEdit;
class QTableWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void startServer();
    void stopServer();
    void acceptClient();
    void readClientData();
    void clientDisconnected();
    void sendLedCommand();
    void sendBeepCommand();
    void sendIntervalCommand();
    void sendThresholdCommand();
    void sendModeCommand();
    void sendShutdownCommand();

private:
    void buildUi();
    void setServerRunning(bool running);
    void attachClient(QTcpSocket *socket);
    void detachClient();
    void handleLine(const QByteArray &line);
    void handleStatus(const QJsonObject &obj);
    void sendCommand(const QJsonObject &obj);
    void appendLog(const QString &text);
    void setMetric(int row, const QString &value, bool ok = true);
    QString statusSummary(const QJsonObject &obj) const;

    QTcpServer *server_;
    QTcpSocket *client_;
    QByteArray rxBuffer_;

    QLineEdit *hostEdit_;
    QSpinBox *portSpin_;
    QPushButton *listenButton_;
    QPushButton *stopButton_;
    QLabel *serverStateLabel_;
    QLabel *clientStateLabel_;
    QLabel *modeLabel_;
    QLabel *lastUpdateLabel_;
    QTableWidget *metricTable_;
    QPlainTextEdit *eventLog_;

    QPushButton *ledOnButton_;
    QPushButton *ledOffButton_;
    QPushButton *beepOnButton_;
    QPushButton *beepOffButton_;
    QSpinBox *intervalSpin_;
    QSpinBox *psThresholdSpin_;
    QSpinBox *alsThresholdSpin_;
    QComboBox *modeCombo_;
};

#endif

