#include "TcpServerManager.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

TcpServerManager::TcpServerManager(QObject *parent)
    : QObject(parent),
      m_tcpServer(nullptr),
      m_port(0)
{
    m_tcpServer = new QTcpServer(this);
    
    // 连接信号和槽
    connect(m_tcpServer, &QTcpServer::newConnection, this, &TcpServerManager::onNewConnection);
    connect(m_tcpServer, &QTcpServer::acceptError, this, [this](QAbstractSocket::SocketError error) {
        onServerError(error);
    });
}

TcpServerManager::~TcpServerManager()
{
    stopServer();
    delete m_tcpServer;
}

bool TcpServerManager::startServer(int port)
{
    if (m_tcpServer->isListening()) {
        qDebug() << "TCP服务器已经在端口" << m_port << "上运行";
        return true;
    }
    
    if (!m_tcpServer->listen(QHostAddress::Any, port)) {
        QString errorMsg = QString("启动TCP服务器失败: %1").arg(m_tcpServer->errorString());
        qDebug() << errorMsg;
        emit errorOccurred(errorMsg);
        return false;
    }
    
    m_port = port;
    qDebug() << "TCP服务器成功启动，端口:" << port;
    emit serverStarted(port);
    return true;
}

void TcpServerManager::stopServer()
{
    if (!m_tcpServer->isListening()) {
        return;
    }
    
    // 断开所有客户端连接
    for (QTcpSocket *client : m_clientConnections) {
        cleanupClient(client);
    }
    m_clientConnections.clear();
    
    // 停止服务器监听
    m_tcpServer->close();
    m_port = 0;
    
    qDebug() << "TCP服务器已停止";
    emit serverStopped();
}

bool TcpServerManager::sendParams(const QMap<QString, QVariant> &params)
{
    if (m_clientConnections.isEmpty()) {
        qDebug() << "没有客户端连接，无法发送参数";
        return false;
    }
    
    // 格式化参数
    QByteArray data = formatParams(params);
    if (data.isEmpty()) {
        qDebug() << "格式化参数失败";
        return false;
    }
    
    // 发送给所有客户端
    bool allSent = true;
    for (QTcpSocket *client : m_clientConnections) {
        if (!sendParamsToClient(client, params)) {
            allSent = false;
        }
    }
    
    return allSent;
}

bool TcpServerManager::sendParamsToClient(QTcpSocket *client, const QMap<QString, QVariant> &params)
{
    if (!client || client->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "无效的客户端或客户端未连接";
        return false;
    }
    
    // 格式化参数
    QByteArray data = formatParams(params);
    if (data.isEmpty()) {
        qDebug() << "格式化参数失败";
        return false;
    }
    
    // 发送数据（使用Qt的异步发送机制，不阻塞线程）
    qint64 bytesWritten = client->write(data);
    if (bytesWritten == -1) {
        QString errorMsg = QString("向客户端发送数据失败: %1").arg(client->errorString());
        qDebug() << errorMsg;
        emit errorOccurred(errorMsg);
        return false;
    }
    
    // 调用flush确保数据被立即发送到网络缓冲区
    client->flush();
    
    qDebug() << "向客户端" << client->peerAddress().toString() << ":" << client->peerPort() << "发送了" << bytesWritten << "字节数据";
    return true;
}

int TcpServerManager::getClientCount() const
{
    return m_clientConnections.count();
}

bool TcpServerManager::sendRawData(const QByteArray &data)
{
    if (m_clientConnections.isEmpty()) {
        return false;
    }
    
    bool allSent = true;
    for (QTcpSocket *client : m_clientConnections) {
        if (!client || client->state() != QAbstractSocket::ConnectedState) {
            allSent = false;
            continue;
        }
        
        // 发送数据
        qint64 bytesWritten = client->write(data);
        if (bytesWritten == -1) {
            allSent = false;
            continue;
        }
        client->flush();
    }
    
    return allSent;
}

bool TcpServerManager::isRunning() const
{
    return m_tcpServer->isListening();
}

void TcpServerManager::onNewConnection()
{
    while (m_tcpServer->hasPendingConnections()) {
        QTcpSocket *client = m_tcpServer->nextPendingConnection();
        if (!client) {
            continue;
        }
        
        // 连接客户端信号和槽
        connect(client, &QTcpSocket::disconnected, this, &TcpServerManager::onClientDisconnected);
        connect(client, &QTcpSocket::readyRead, this, &TcpServerManager::onReadyRead);
        connect(client, &QTcpSocket::errorOccurred, this, &TcpServerManager::onClientError);
        
        // 添加到客户端列表
        m_clientConnections.append(client);
        
        qDebug() << "新客户端连接:" << client->peerAddress().toString() << ":" << client->peerPort();
        emit clientConnected(client);
    }
}

void TcpServerManager::onClientDisconnected()
{
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender());
    if (!client) {
        return;
    }
    
    qDebug() << "客户端断开连接:" << client->peerAddress().toString() << ":" << client->peerPort();
    emit clientDisconnected(client);
    
    // 清理客户端连接
    cleanupClient(client);
    m_clientConnections.removeAll(client);
}

void TcpServerManager::onReadyRead()
{
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender());
    if (!client) {
        return;
    }
    
    QByteArray data = client->readAll();
    qDebug() << "从客户端" << client->peerAddress().toString() << ":" << client->peerPort() << "接收了" << data.size() << "字节数据";
    
    // 发送数据接收信号
    emit dataReceived(client, data);
}

void TcpServerManager::onServerError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error); // 标记未使用的参数
    QString errorMsg = QString("TCP服务器错误: %1").arg(m_tcpServer->errorString());
    qDebug() << errorMsg;
    emit errorOccurred(errorMsg);
}

void TcpServerManager::onClientError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error); // 标记未使用的参数
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender());
    if (!client) {
        return;
    }
    
    QString errorMsg = QString("客户端错误 (%1:%2): %3").arg(client->peerAddress().toString())
                                                      .arg(client->peerPort())
                                                      .arg(client->errorString());
    qDebug() << errorMsg;
    emit errorOccurred(errorMsg);
}

QByteArray TcpServerManager::formatParams(const QMap<QString, QVariant> &params)
{
    // 创建JSON对象
    QJsonObject jsonObj;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        jsonObj.insert(it.key(), QJsonValue::fromVariant(it.value()));
    }
    
    // 添加时间戳
    jsonObj.insert("timestamp", QJsonValue::fromVariant(QDateTime::currentDateTime().toString(Qt::ISODate)));
    
    // 转换为JSON字符串
    QJsonDocument jsonDoc(jsonObj);
    QByteArray jsonData = jsonDoc.toJson(QJsonDocument::Compact);
    
    // 添加换行符作为消息结束符
    jsonData.append('\n');
    
    return jsonData;
}

void TcpServerManager::cleanupClient(QTcpSocket *client)
{
    if (!client) {
        return;
    }
    
    // 断开所有信号连接
    client->disconnect(this);
    
    // 关闭连接
    if (client->state() == QAbstractSocket::ConnectedState) {
        client->disconnectFromHost();
        client->waitForDisconnected(1000);
    }
    
    // 释放资源
    client->deleteLater();
}
