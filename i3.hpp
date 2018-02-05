#pragma once

/* Qt */
#include <QObject>
#include <QDataStream>
#include <QtNetwork/QLocalSocket>
#include <QSocketNotifier>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <QProcessEnvironment>
#include <QSignalBlocker>
#include <QDebug>

/* stl */
#include <memory>

/* i3 */
#include <i3/ipc.h>

class i3 : public QObject
{
    Q_OBJECT
    /*
     * XXX: The official API doesn't define an invalid message type value.
     *      It defined the message type set by enumerating all _valid_ types.
     * 		The following defines an additional value for signaling invalid message type states.
     */
    const quint32 IPC_MESSAGE_TYPE_INVALID = 0xffffffff;

public:
    explicit i3(QObject *parent = nullptr) : QObject(parent)
    {
        _socket_path = QProcessEnvironment::systemEnvironment().value("I3_SOCKET_PATH", "/tmp/i3.sock");

        QObject::connect(&_socket, SIGNAL(connected()), this, SLOT(initVersionCache()));
        QObject::connect(&_socket, SIGNAL(disconnected()), this, SLOT(cleanup()));
        QObject::connect(&_socket, SIGNAL(readyRead()), this, SLOT(recvAsync()));
    }

    bool isConnected() const
    {
        const QMutexLocker locker(&_socket_mutex);
        return _socket.isOpen();
    }

    QString getVersionString() const
    {
        return _i3_version_string;
    }

    unsigned int getVersionNumber(int c = 0) const
    {
        if (c < 0 || c > 2) {
            throw std::out_of_range("version component index out-of-range");
        }
        return _i3_version[c];
    }

    QJsonObject getVersion()
    {
        return sendRecvMessage(I3_IPC_MESSAGE_TYPE_GET_VERSION, I3_IPC_REPLY_TYPE_VERSION).object();
    }

    QJsonArray runCommand(const QString &command)
    {
        return sendRecvMessage(I3_IPC_MESSAGE_TYPE_RUN_COMMAND, I3_IPC_REPLY_TYPE_COMMAND, command.toLocal8Bit()).array();
    }

    QJsonArray getWorkspaces()
    {
        return sendRecvMessage(I3_IPC_MESSAGE_TYPE_GET_WORKSPACES, I3_IPC_REPLY_TYPE_WORKSPACES).array();
    }

    QJsonObject subscribe(const QStringList &events)
    {
        const QJsonDocument payload_json(QJsonArray::fromStringList(events));
        return sendRecvMessage(I3_IPC_MESSAGE_TYPE_SUBSCRIBE, I3_IPC_REPLY_TYPE_SUBSCRIBE, payload_json.toJson()).object();
    }

    QJsonArray getOutputs()
    {
        return sendRecvMessage(I3_IPC_MESSAGE_TYPE_GET_OUTPUTS, I3_IPC_REPLY_TYPE_OUTPUTS).array();
    }

    QJsonObject getConfig()
    {
        return sendRecvMessage(I3_IPC_MESSAGE_TYPE_GET_CONFIG, I3_IPC_REPLY_TYPE_CONFIG).object();
    }

    QJsonObject getTree()
    {
        return sendRecvMessage(I3_IPC_MESSAGE_TYPE_GET_TREE, I3_IPC_REPLY_TYPE_TREE).object();
    }

    QJsonArray getMarks()
    {
        return sendRecvMessage(I3_IPC_MESSAGE_TYPE_GET_MARKS, I3_IPC_REPLY_TYPE_MARKS).array();
    }

    QJsonArray getBindingModes()
    {
        return sendRecvMessage(I3_IPC_MESSAGE_TYPE_GET_BINDING_MODES, I3_IPC_REPLY_TYPE_BINDING_MODES).array();
    }

    QJsonArray getBarConfig()
    {
        return sendRecvMessage(I3_IPC_MESSAGE_TYPE_GET_BAR_CONFIG, I3_IPC_REPLY_TYPE_BAR_CONFIG).array();
    }

    QJsonObject getBarConfig(const QString &bar_id)
    {
        return sendRecvMessage(I3_IPC_MESSAGE_TYPE_GET_BAR_CONFIG, I3_IPC_REPLY_TYPE_BAR_CONFIG, bar_id.toLocal8Bit()).object();
    }

signals:
    void event(quint32 type, const QJsonDocument& data);

public slots:
    void connect()
    {
        qDebug() << "Connecting to " << _socket_path;

        if (isConnected()) {
            return;
        }

        _socket.connectToServer(_socket_path);
    }

    void disconnect()
    {
        qDebug() << "Disconnecting.";

        if (isConnected()) {
            const QMutexLocker locker(&_socket_mutex);
            _socket.abort();
            _socket.disconnectFromServer();
        }
    }

private slots:
    void initVersionCache()
    {
        qDebug() << "queryVersion";

        const QJsonObject version_json = getVersion();

        _i3_version_string = version_json.value("human_readable").toString();
        _i3_version[0] = version_json.value("major").toInt();
        _i3_version[1] = version_json.value("minor").toInt();
        _i3_version[2] = version_json.value("patch").toInt();
    }

    void cleanup()
    {
        qDebug() << "Cleanup";
        _inbound_message_type = IPC_MESSAGE_TYPE_INVALID;
        _inbound_message_remaining = 0;
        _inbound_payload_buffer.clear();
    }

    /* handle asynchronous read */
    void recvAsync()
    {
        qDebug() << "recvAsync";

        quint32 message_type = IPC_MESSAGE_TYPE_INVALID;
        QJsonDocument payload_json;

        if (recvBuffered(message_type, payload_json)) {
            if (message_type & I3_IPC_EVENT_MASK) {
                qDebug() << "Emitting from recvAsync";
                emit event(message_type, payload_json);
            }
            else {
                qDebug() << "Dropping message of type=" << message_type;
            }
        }
    }

private:
    /* block (or timeout) until a message of the specified type is received */
    QJsonDocument recvMessage(quint32 message_type)
    {
        qDebug() << "recvMessage: type=" << message_type;

        QJsonDocument payload_json;
        quint32 received_type = IPC_MESSAGE_TYPE_INVALID;

        while ((received_type = recvMessage(payload_json)) != message_type) {

            if (received_type == IPC_MESSAGE_TYPE_INVALID) {
                throw std::runtime_error("IPC message receiving failed");
            }

            if (received_type & I3_IPC_EVENT_MASK) {
                qDebug() << "Emitting from recvMessage";
                emit event(received_type, payload_json);
            }
            else {
                qDebug() << "Non-event type received: t=" << received_type;
            }
        }

        return payload_json;
    }

    /* construct, send and receive a i3 IPC message */
    QJsonDocument sendRecvMessage(quint32 send_type, quint32 recv_type, const QByteArray &payload = QByteArray())
    {
        const QSignalBlocker blocker(&_socket);
        qDebug() << "sendRecvMessage: s=" << send_type << " r=" << recv_type;
        sendMessage(send_type, payload);
        return recvMessage(recv_type);
    }

    /* block (or timeout) until a message is received */
    quint32 recvMessage(QJsonDocument& payload_json)
    {
        qDebug() << "recvMessage: any";

        quint32 message_type = IPC_MESSAGE_TYPE_INVALID;

        for (;;) {
            if (_socket.bytesAvailable() == 0) {
                if (!_socket.waitForReadyRead()) {
                    break;
                }
            }
            if (recvBuffered(message_type, payload_json)) {
                break;
            }
        }

        return message_type;
    }

    /*
     * Receive as much data as possible from the socket without blocking.
     * Return true if after receiving of the data, there's a complete message available.
     * If true, the `type' and `payload_json' are initialized.
     * Otherwise their state is undefined and the function shall be called again when new data is available.
     * Throws in case an error is encountered (invalid IPC data, lost connection, etc).
     */
    bool recvBuffered(quint32& type, QJsonDocument& payload_json)
    {
        qDebug() << "recvBuffered";

        const QMutexLocker locker(&_socket_mutex);

        /* If no header was received... */
        if (_inbound_message_type == IPC_MESSAGE_TYPE_INVALID) {
            const qint64 header_size = strlen(I3_IPC_MAGIC) + 2*sizeof(quint32);

            if (_socket.bytesAvailable() >= header_size) {
                /*
                 * Read magic, type, size
                 *
                 * i3 IPC message header format:
                 *    u8  magic[6];
                 *    u32 size;
                 *    u32 type;
                 */
                if (QByteArray(I3_IPC_MAGIC) != _socket.read(strlen(I3_IPC_MAGIC))) {
                    throw std::runtime_error("IPC header: invalid magic bytes");
                }

                quint32 payload_size = 0;
                quint32 message_type = 0;

                if (_socket.read(reinterpret_cast<char*>(&payload_size), sizeof payload_size) != sizeof payload_size
                        || _socket.read(reinterpret_cast<char*>(&message_type), sizeof message_type) != sizeof message_type) {
                    throw std::runtime_error("IPC header: insufficient data available");
                }

                _inbound_message_type = message_type;
                _inbound_message_remaining = payload_size;
            }
            else {
                return false;
            }
        }

        /* If not all of the announced payload size was received... */
        if (_inbound_message_remaining > 0) {
            if (_socket.bytesAvailable() > 0) {
                const qint64 bytes_available = _socket.bytesAvailable();
                const quint64 bytes_to_read = std::min(quint64(_inbound_message_remaining), quint64(bytes_available));

                /* Read available to buffer */
                QByteArray bytes = _socket.read(bytes_to_read);

                _inbound_message_remaining -= bytes.size();
                _inbound_payload_buffer.append(bytes);
            }
            else {
                return false;
            }
        }

        /* Parse payload to JSON document */
        if (_inbound_payload_buffer.size() > 0) {
            payload_json = QJsonDocument::fromJson(_inbound_payload_buffer);
        }
        else {
            payload_json = QJsonDocument();
        }

        type = _inbound_message_type;

        /* Reset buffer state */
        _inbound_message_type = IPC_MESSAGE_TYPE_INVALID;
        _inbound_message_remaining = 0;
        _inbound_payload_buffer = QByteArray();

        return true;
    }

    /* construct and send an i3 IPC message */
    void sendMessage(int type, const QByteArray &payload)
    {
        qDebug() << "sendMessage: type=" << type;

        const QMutexLocker locker(&_socket_mutex);
        const qint32 message_type = type;
        const qint32 payload_size = payload.size();

        _socket.write(I3_IPC_MAGIC, strlen(I3_IPC_MAGIC));
        _socket.write(reinterpret_cast<const char*>(&payload_size), sizeof payload_size);
        _socket.write(reinterpret_cast<const char*>(&message_type), sizeof message_type);

        if (payload_size > 0) {
            _socket.write(payload.data(), payload_size);
        }
    }

    /*
     * Socket state
     */
    mutable QMutex _socket_mutex;
    QString _socket_path;
    QLocalSocket _socket;

    /*
     * Cached version information
     */
    QString _i3_version_string{"unknown"};
    int _i3_version[3]{0,0,0};

    /*
     * Inbound message state & queue
     */
    quint32 _inbound_message_type{IPC_MESSAGE_TYPE_INVALID};
    quint64 _inbound_message_remaining{0};
    QByteArray _inbound_payload_buffer;
};
