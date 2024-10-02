#pragma once

#include <QMainWindow>

#include "nekoproto/communication/communication_base.hpp"
#include "nekoproto/proto/json_serializer.hpp"
#include "nekoproto/proto/serializer_base.hpp"
#include "nekoproto/proto/types/vector.hpp"
#include "ui_test_communication_ui.h"

#include <variant>

#include <ilias/platform/qt.hpp>

inline std::string to_hex(const std::vector<char>& data) {
    std::stringstream ss;
    for (auto c : data) {
        ss << "\\" << std::hex << (uint32_t(c) & 0xFF);
    }
    return ss.str();
}

struct Message {
    uint64_t timestamp;
    std::string msg;
    int id;
    std::vector<int> numbers;

    NEKO_SERIALIZER(timestamp, msg, id, numbers)
    NEKO_DECLARE_PROTOCOL(Message, NEKO_NAMESPACE::JsonSerializer)
};

class MainWidget : public QMainWindow {
    Q_OBJECT
public:
    MainWidget(ILIAS_NAMESPACE::QIoContext* ctxt, NEKO_NAMESPACE::ProtoFactory& protoFactory,
               QWidget* parent = nullptr);
    ~MainWidget();

    ILIAS_NAMESPACE::Task<void>
    clientLoop(std::map<int, NEKO_NAMESPACE::ProtoStreamClient<ILIAS_NAMESPACE::TcpClient>>::iterator client,
               QListWidgetItem* item);
    ILIAS_NAMESPACE::Task<void> serverLoop();
    ILIAS_NAMESPACE::Task<void> makeMainChannel();
    template <typename T>
    ILIAS_NAMESPACE::Task<void> sendMessage(NEKO_NAMESPACE::ProtoStreamClient<T>& client);
    template <typename T>
    ILIAS_NAMESPACE::Task<void> recvMessage(NEKO_NAMESPACE::ProtoStreamClient<T>& client);

protected Q_SLOTS:
    void startService();
    void onMakeMainChannel();
    void selectedChannel(int index);
    void onSendMessage();
    void closeService();

private:
    Ui::MainWindow* ui;
    ILIAS_NAMESPACE::QIoContext* mCtxt;
    NEKO_NAMESPACE::ProtoFactory& mProtoFactor;
    bool mExit = true;
    ILIAS_NAMESPACE::TcpListener mListener;
    std::vector<ILIAS_NAMESPACE::CancelHandle> mHandles;
    std::map<int, NEKO_NAMESPACE::ProtoStreamClient<ILIAS_NAMESPACE::TcpClient>> mClients;
    int mChannelCount = 0;
    int mCurrentIndex = 0;
};

template <typename T>
ILIAS_NAMESPACE::Task<void> MainWidget::sendMessage(NEKO_NAMESPACE::ProtoStreamClient<T>& client) {
    Message msg;
    msg.id        = mCurrentIndex;
    msg.msg       = std::string(ui->sendEdit->toPlainText().toUtf8());
    msg.timestamp = (time(NULL));
    msg.numbers   = std::vector<int>({1, 2, 3, 4, 5});
    auto ret1     = co_await client.send(msg.makeProto(), NEKO_NAMESPACE::ProtoStreamClient<T>::SerializerInThread);
    if (!ret1) {
        NEKO_LOG_ERROR("ui test", "send failed: {}",
                       QString::fromUtf8(ret1.error().message()).toLocal8Bit().constData());
        co_return ILIAS_NAMESPACE::Unexpected(ret1.error());
    }
    co_return ILIAS_NAMESPACE::Result<>();
}

template <typename T>
ILIAS_NAMESPACE::Task<void> MainWidget::recvMessage(NEKO_NAMESPACE::ProtoStreamClient<T>& client) {
    auto ret = co_await client.recv(NEKO_NAMESPACE::ProtoStreamClient<T>::SerializerInThread);
    if (!ret) {
        NEKO_LOG_ERROR("ui test", "recv failed: {}", ret.error().message());
        co_return ILIAS_NAMESPACE::Unexpected(ret.error());
    }
    auto retMsg = std::move(ret.value());

    auto msg = retMsg->cast<Message>();
    if (msg) {
        ui->recvEdit->append(
            QString("recv %1 [%2]:\n%3\n").arg(msg->id).arg(msg->timestamp).arg(QString::fromUtf8(msg->msg.c_str())));
    } else {
        NEKO_LOG_WARN("ui test", "recv: {}", to_hex(retMsg->toData()));
    }
    co_return ILIAS_NAMESPACE::Result<>();
}