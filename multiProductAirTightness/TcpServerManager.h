#ifndef TCPSERVERMANAGER_H
#define TCPSERVERMANAGER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QList>
#include <QJsonObject>
#include <QJsonDocument>

class TcpServerManager : public QObject
{
    Q_OBJECT

public:
    explicit TcpServerManager(QObject *parent = nullptr);
    ~TcpServerManager();

    // 启动TCP服务器
    bool startServer(int port = 8088);
    
    // 停止TCP服务器
    void stopServer();
    
    // 发送参数给所有连接的客户端
    bool sendParams(const QMap<QString, QVariant> &params);
    
    // 发送参数给指定客户端
    bool sendParamsToClient(QTcpSocket *client, const QMap<QString, QVariant> &params);
    
    // 直接发送原始数据给所有客户端（不做JSON包装）
    bool sendRawData(const QByteArray &data);
    
    // 获取当前连接的客户端数量
    int getClientCount() const;
    
    // 检查服务器是否正在运行
    bool isRunning() const;

signals:
    // 客户端连接状态变化信号
    void clientConnected(QTcpSocket *client);
    void clientDisconnected(QTcpSocket *client);
    
    // 服务器状态变化信号
    void serverStarted(int port);
    void serverStopped();
    
    // 数据接收信号
    void dataReceived(QTcpSocket *client, const QByteArray &data);
    
    // 错误信号
    void errorOccurred(const QString &errorMsg);

private slots:
    // 处理新的客户端连接
    void onNewConnection();
    
    // 处理客户端断开连接
    void onClientDisconnected();
    
    // 处理客户端数据接收
    void onReadyRead();
    
    // 处理服务器错误
    void onServerError(QAbstractSocket::SocketError error);
    
    // 处理客户端错误
    void onClientError(QAbstractSocket::SocketError error);

private:
    QTcpServer *m_tcpServer;                // TCP服务器实例
    QList<QTcpSocket *> m_clientConnections; // 客户端连接列表
    int m_port;                              // 服务器端口
    
    // 格式化参数为JSON字符串
    QByteArray formatParams(const QMap<QString, QVariant> &params);
    
    // 清理客户端连接
    void cleanupClient(QTcpSocket *client);
};

#endif // TCPSERVERMANAGER_H
