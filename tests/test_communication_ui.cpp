#include "test_communication_ui.hpp"

#include <QApplication>
#include <iostream>
#include <sstream>

#include "../communication/communication_base.hpp"
#include "../core/json_serializer.hpp"
#include "../core/serializer_base.hpp"
#include "../core/types/vector.hpp"

#include "ilias_qt.hpp"

#include "ui_test_communication_ui.h"

NEKO_USE_NAMESPACE
using namespace ILIAS_NAMESPACE;

struct Message {
    uint64_t timestamp;
    std::string msg;
    std::vector<int> numbers;

    NEKO_SERIALIZER(timestamp, msg, numbers)
    NEKO_DECLARE_PROTOCOL(Message, JsonSerializer)
};

std::string to_hex(const std::vector<char>& data) {
    std::stringstream ss;
    for (auto c : data) {
        ss << "\\" << std::hex << (uint32_t(c) & 0xFF);
    }
    return ss.str();
}

MainWidget::MainWidget(QIoContext* ctxt, std::shared_ptr<NEKO_NAMESPACE::ProtoFactory> protoFactory, QWidget* parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), mCtxt(ctxt), mChannelFactor(*mCtxt, protoFactory) {
    ui->setupUi(this);
    connect(ui->listening, &QPushButton::clicked, this, &MainWidget::startService);
    connect(ui->send, &QPushButton::clicked, this, &MainWidget::onSendMessage);
    connect(ui->close, &QPushButton::clicked, this, &MainWidget::closeService);
    connect(ui->makeMainChannel, &QPushButton::clicked, this, &MainWidget::onMakeMainChannel);
    connect(ui->channelList, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem* current, QListWidgetItem* previous) {
                if (current != nullptr) {
                    auto index = current->text().split(" ").last().toInt();
                    this->selectedChannel(index);
                } else {
                    this->selectedChannel(-1);
                }
            });
    ui->send->setDisabled(true);
}

MainWidget::~MainWidget() {
    closeService();
    for (auto& t : mHandles) {
        t.cancel();
    }
    mExit = true;
    delete ui;
}

Task<void> MainWidget::clientLoop(std::weak_ptr<ChannelBase> channel) {
    auto ch = channel.lock();
    int id  = -1;
    if (ch == nullptr) {
        NEKO_LOG_ERROR("channel expired");
        co_return Result<void>();
    } else {
        id = ch->channelId();
    }
    mExit = false;
    while (!mExit) {
        auto ret = co_await recvMessage(channel);
        if (!ret) {
            NEKO_LOG_ERROR("Client Error {}", ret.error().message());
            break;
        }
    }
    NEKO_LOG_INFO("Client {} exit", id);
    QList<QListWidgetItem*> items = ui->channelList->findItems(QString("channel %1").arg(id), Qt::MatchExactly);
    if (!items.isEmpty()) {
        QListWidgetItem* item = items.first();
        ui->channelList->takeItem(ui->channelList->row(item));
        delete item;
    }
    co_return Result<>();
}

ILIAS_NAMESPACE::Task<void> MainWidget::serverLoop() {
    NEKO_LOG_INFO("serverLoop");
    if (!mExit) {
        co_return Unexpected<Error>(Error::InProgress);
    }
    ui->makeMainChannel->setDisabled(true);
    ui->listening->setDisabled(true);
    mExit = false;
    while (!mExit) {
        auto ret = co_await mChannelFactor.accept();
        if (!ret) {
            NEKO_LOG_ERROR("accept failed: {}", ret.error().message());
            break;
            NEKO_LOG_INFO("accept successed");
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
        NEKO_LOG_ERROR("connect failed: {}", mainChannel.error().message());
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

ILIAS_NAMESPACE::Task<void> MainWidget::sendMessage(std::weak_ptr<NEKO_NAMESPACE::ChannelBase> channel) {
    if (auto cl = channel.lock(); cl != nullptr) {
        // CS_LOG_INFO("send message to channel {}", cl->channelId());
        auto msg          = NEKO_MAKE_UNIQUE(Message::ProtoType, Message{});
        (*msg)->msg       = std::string(ui->sendEdit->toPlainText().toUtf8());
        (*msg)->timestamp = (time(NULL));
        auto ids          = mChannelFactor.getChannelIds();
        (*msg)->numbers   = std::vector<int>(ids.begin(), ids.end());
        auto ret1         = co_await cl->send(std::move(msg));
        if (!ret1) {
            NEKO_LOG_ERROR("send failed: {}", ret1.error().message());
            co_return Unexpected(ret1.error());
        }
    } else {
        NEKO_LOG_ERROR("channel expired");
        co_return Result<void>();
    }
    co_return Result<>();
}

ILIAS_NAMESPACE::Task<void> MainWidget::recvMessage(std::weak_ptr<NEKO_NAMESPACE::ChannelBase> channel) {
    if (auto cl = channel.lock(); cl != nullptr) {
        auto ret = co_await cl->recv();
        if (!ret) {
            NEKO_LOG_ERROR("recv failed: {}", ret.error().message());
            co_return Unexpected(ret.error());
        }
        auto retMsg = std::move(ret.value());

        auto msg = retMsg->cast<Message>();
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
        NEKO_LOG_ERROR("channel expired");
        co_return Result<>();
    }
    co_return Result<>();
}

void MainWidget::closeChannel(std::weak_ptr<NEKO_NAMESPACE::ChannelBase> channel) {
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

void MainWidget::onMakeMainChannel() { ilias_go makeMainChannel(); }

void MainWidget::selectedChannel(int index) {
    auto findChannel = mChannelFactor.getChannel(index);
    if (!findChannel.expired()) {
        mCurrentChannel = findChannel;
    }
    ui->send->setDisabled(mCurrentChannel.expired());
}

void MainWidget::onSendMessage() { ilias_go sendMessage(mCurrentChannel); }

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
        NEKO_LOG_ERROR("listen failed: {}", ret.error().message());
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
    std::shared_ptr<NEKO_NAMESPACE::ProtoFactory> protoFactory(new NEKO_NAMESPACE::ProtoFactory());
    MainWidget mainWidget(&ioContext, protoFactory);
    mainWidget.show();

    return app.exec();
}
