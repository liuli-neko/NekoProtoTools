#include "test_rpc_ui.hpp"

#include <iostream>
#include <sstream>
#include <QApplication>

#include "../proto/cc_proto_json_serializer.hpp"
#include "../rpc/cc_rpc_base.hpp"
#include "../proto/cc_serializer_base.hpp"

#include "ilias_qt.hpp"

#include "ui_test_rpc_ui.h"

CS_RPC_USE_NAMESPACE
using namespace ILIAS_NAMESPACE;

class Message : public CS_PROTO_NAMESPACE::ProtoBase<Message, JsonSerializer> {
public:
    uint64_t timestamp;
    std::string msg;
    std::vector<int> numbers;

    CS_SERIALIZER(timestamp, msg, numbers)
};

std::string to_hex(const std::vector<char>& data) {
    std::stringstream ss;
    for (auto c : data) {
        ss << "\\" << std::hex << (uint32_t(c) & 0xFF);
    }
    return ss.str();
}

MainWidget::MainWidget(QIoContext* ctxt, std::shared_ptr<CS_PROTO_NAMESPACE::ProtoFactory> protoFactory, QWidget* parent) : 
    QMainWindow(parent), 
    ui(new Ui::MainWindow),
    mCtxt(ctxt),
    mChannelFactor(*mCtxt, protoFactory) {
    ui->setupUi(this);
    connect(ui->listening, &QPushButton::clicked, this, &MainWidget::startService);
    connect(ui->send, &QPushButton::clicked, this, &MainWidget::onSendMessage);
    connect(ui->close, &QPushButton::clicked, this, &MainWidget::closeService);
    connect(ui->makeMainChannel, &QPushButton::clicked, this, &MainWidget::onMakeMainChannel);
    connect(ui->channelList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* current, QListWidgetItem* previous){
        if (current != nullptr) {
            auto index = current->text().split(" ").last().toInt();
            this->selectedChannel(index);
        } else {
            this->selectedChannel(-1);
        }
    });
    ui->send->setDisabled(true);
    ui->makeSubChannel->setDisabled(true);
}

MainWidget::~MainWidget() {
    for (auto& t : mHandles) {
        t.cancel();
    }
    mExit = true;
    mChannelFactor.close();
    delete ui;
}

Task<void> MainWidget::clientLoop(std::weak_ptr<ChannelBase> channel) {
    auto ch = channel.lock();
    int id = -1;
    if (ch == nullptr) {
        CS_LOG_ERROR("channel expired");
        co_return Result<void>();
    } else {
        id = ch->channelId();
    }
    mExit = false;
    while (!mExit) {
        auto ret = co_await recvMessage(channel);
        if (!ret) {
            CS_LOG_ERROR("Client Error {}", ret.error().message());
            break;
        }
    }
    CS_LOG_INFO("Client {} exit", id);
    QList<QListWidgetItem*> items = ui->channelList->findItems(QString("channel %1").arg(id), Qt::MatchExactly);
    if (!items.isEmpty()) {
        QListWidgetItem* item = items.first();
        ui->channelList->takeItem(ui->channelList->row(item));
        delete item;
    }
    co_return Result<>();
}

ILIAS_NAMESPACE::Task<void> MainWidget::serverLoop() { 
    CS_LOG_INFO("serverLoop");
    if (!mExit) {
        co_return Unexpected<Error>(Error::InProgress);
    }
    ui->makeMainChannel->setDisabled(true);
    ui->listening->setDisabled(true);
    mExit = false;
    while (!mExit) {
        auto ret = co_await mChannelFactor.accept();
        if (!ret) {
            CS_LOG_ERROR("accept failed: {}", ret.error().message());
            break;
            CS_LOG_INFO("accept successed");
        }
        auto channel = ret.value();
        if (!channel.expired()) {
            auto id = channel.lock()->channelId();
            ui->channelList->addItem(QString("channel %1").arg(id));
            mClientLoopHandles.insert(std::make_pair(id, ilias_go clientLoop(channel)));
        }
    }
    ui->listening->setDisabled(false);
    co_return Result<>();
}

ILIAS_NAMESPACE::Task<void> MainWidget::makeMainChannel() { 
    auto mainChannel = co_await mChannelFactor.connect(ui->serviceUrlEdit->text().toLocal8Bit().constData(), 0);
    if (!mainChannel) {
        CS_LOG_ERROR("connect failed: {}", mainChannel.error().message());
        co_return Unexpected<Error>(mainChannel.error());
    }
    auto channel = mainChannel.value();
    if (!channel.expired()) {
        auto id = channel.lock()->channelId();
        ui->channelList->addItem(QString("channel %1").arg(id));
        mClientLoopHandles.insert(std::make_pair(id, ilias_go clientLoop(channel)));
    }
    
    co_return Result<>();
}

ILIAS_NAMESPACE::Task<void> MainWidget::sendMessage(std::weak_ptr<cs_ccproto::ChannelBase> channel) {
    if (auto cl = channel.lock(); cl != nullptr) {
        // CS_LOG_INFO("send message to channel {}", cl->channelId());
        auto msg = std::make_unique<Message>();
        msg->msg = std::string(ui->sendEdit->toPlainText().toUtf8());
        msg->timestamp = (time(NULL));
        auto ids = mChannelFactor.getChannelIds();
        msg->numbers = std::vector<int>(ids.begin(), ids.end());
        auto ret1 = co_await cl->send(std::move(msg));
        if (!ret1) {
            CS_LOG_ERROR("send failed: {}", ret1.error().message());
            co_return Unexpected(ret1.error());
        }
    } else {
        CS_LOG_ERROR("channel expired");
        co_return Result<void>();
    }
    co_return Result<>();
}

ILIAS_NAMESPACE::Task<void> MainWidget::recvMessage(std::weak_ptr<cs_ccproto::ChannelBase> channel) {
    if (auto cl = channel.lock(); cl != nullptr) {
        auto ret = co_await cl->recv();
        if (!ret) {
            CS_LOG_ERROR("recv failed: {}", ret.error().message());
            co_return Unexpected(ret.error());
        }
        auto retMsg = std::shared_ptr<IProto>(ret.value().release());

        auto msg = std::dynamic_pointer_cast<Message>(retMsg);
        if (msg) {
            ui->recvEdit->append(QString("from %1 [%2]:\n%3\n")
                    .arg(cl->channelId())
                    .arg(msg->timestamp)
                    .arg(QString::fromUtf8(msg->msg.c_str())));
            std::cout << "current ids : ";
            for (auto n : msg->numbers) {
                std::cout << n << " ";
            }
            std::cout << std::endl;
        } else {
            std::cout << "recv: " << to_hex(retMsg->toData()) << std::endl;
        }
    } else {
        CS_LOG_ERROR("channel expired");
        co_return Result<>();
    }
    co_return Result<>();
}

void MainWidget::closeChannel(std::weak_ptr<cs_ccproto::ChannelBase> channel) {
    int id = -1;
    if (auto cl = channel.lock(); cl != nullptr) {
        id = cl->channelId();
        cl->close();
        mChannelFactor.destroyChannel(cl->channelId());
    }

    QList<QListWidgetItem*> items = ui->channelList->findItems(QString("channel %1").arg(id), Qt::MatchExactly);
    if (!items.isEmpty()) {
        QListWidgetItem* item = items.first();
        ui->channelList->takeItem(ui->channelList->row(item));
        delete item;
    }
}

void MainWidget::onMakeMainChannel() {
    ilias_go makeMainChannel();
}

void MainWidget::selectedChannel(int index) {
    auto findChannel = mChannelFactor.getChannel(index);
    if (!findChannel.expired()) {
        mCurrentChannel = findChannel;
    }
    ui->send->setDisabled(mCurrentChannel.expired());
}

void MainWidget::onSendMessage() {
    ilias_go sendMessage(mCurrentChannel);
}

void MainWidget::closeService() {
    mChannelFactor.close();
    for (auto& [id, h] : mClientLoopHandles) {
        h.cancel();
        h.join();
    }
    mClientLoopHandles.clear();
}

void MainWidget::startService() {
    auto ret = mChannelFactor.listen(ui->serviceUrlEdit->text().toLocal8Bit().constData());
    if (!ret) {
        CS_LOG_ERROR("listen failed: {}", ret.error().message());
        return;
    } else {
        mHandles.push_back(ilias_go serverLoop());
    }
}


int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setDesktopSettingsAware(true);
    spdlog::set_level(spdlog::level::debug);
    QIoContext ioContext;
    std::shared_ptr<CS_PROTO_NAMESPACE::ProtoFactory> protoFactory(new CS_PROTO_NAMESPACE::ProtoFactory());
    MainWidget mainWidget(&ioContext, protoFactory);
    mainWidget.show();

    return app.exec();
}
