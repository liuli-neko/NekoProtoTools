#include "test_communication_ui.hpp"

#include <QApplication>
#include <iostream>
#include <sstream>

#pragma comment(linker, "/subsystem:console") // 设置连接器选项

NEKO_USE_NAMESPACE
using namespace ILIAS_NAMESPACE;

MainWidget::MainWidget(QIoContext* ctxt, NEKO_NAMESPACE::ProtoFactory& protoFactory, QWidget* parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), mCtxt(ctxt), mProtoFactor(protoFactory) {
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

Task<void>
MainWidget::clientLoop(std::map<int, NEKO_NAMESPACE::ProtoStreamClient<ILIAS_NAMESPACE::TcpClient>>::iterator client,
                       QListWidgetItem* item) {
    mExit = false;
    while (!mExit) {
        auto ret = co_await recvMessage(client->second);
        if (!ret) {
            NEKO_LOG_ERROR("ui test", "Client Error {}",
                           QString::fromUtf8(ret.error().message()).toLocal8Bit().constData());
            mClients.erase(client);
            auto it = ui->channelList->takeItem(ui->channelList->row(item));
            ui->channelList->update();
            delete it;
            item = nullptr;
            ui->channelList->update();
            break;
        }
    }
    co_return Result<>();
}

ILIAS_NAMESPACE::Task<void> MainWidget::serverLoop() {
    NEKO_LOG_INFO("ui test", "serverLoop");
    if (!mExit) {
        co_return Unexpected<Error>(Error::InProgress);
    }
    mListener = ILIAS_NAMESPACE::TcpListener(*mCtxt, AF_INET);
    ui->makeMainChannel->setDisabled(true);
    ui->listening->setDisabled(true);
    auto ret = mListener.bind(IPEndpoint(ui->serviceUrlEdit->text().toLocal8Bit().constData()));
    if (!ret) {
        NEKO_LOG_ERROR("ui test", "bind failed: {}",
                       QString::fromUtf8(ret.error().message()).toLocal8Bit().constData());
        co_return Unexpected(ret.error());
    }
    mExit = false;
    while (!mExit) {
        NEKO_LOG_INFO("ui test", "accepting");
        auto ret = co_await mListener.accept();
        if (!ret) {
            NEKO_LOG_ERROR("ui test", "accept failed: {}", ret.error().message());
            break;
        }
        NEKO_LOG_INFO("ui test", "accept successed");
        ui->channelList->addItem(QString("%1:%2 %3")
                                     .arg(ret.value().second.address().toString().c_str())
                                     .arg(ret.value().second.port())
                                     .arg(++mChannelCount));
        auto item   = ui->channelList->item(ui->channelList->count() - 1);
        auto client = mClients.emplace(mChannelCount, NEKO_NAMESPACE::ProtoStreamClient<TcpClient>(
                                                          mProtoFactor, *mCtxt, std::move(ret.value().first)));
        ilias_go clientLoop(client.first, item);
    }
    ui->listening->setDisabled(false);
    co_return Result<>();
}

ILIAS_NAMESPACE::Task<void> MainWidget::makeMainChannel() {
    TcpClient client(*mCtxt, AF_INET);
    auto ret = co_await client.connect(IPEndpoint(ui->serviceUrlEdit->text().toLocal8Bit().constData()));
    if (!ret) {
        NEKO_LOG_ERROR("ui test", "connect failed: {}",
                       QString::fromUtf8(ret.error().message().c_str()).toLocal8Bit().constData());
        co_return Unexpected(ret.error());
    }
    ui->channelList->addItem(
        QString("%1 %2").arg(ui->serviceUrlEdit->text().toLocal8Bit().constData()).arg(++mChannelCount));
    auto item       = ui->channelList->item(ui->channelList->count() - 1);
    auto clientiter = mClients.emplace(
        mChannelCount, NEKO_NAMESPACE::ProtoStreamClient<TcpClient>(mProtoFactor, *mCtxt, std::move(client)));
    ilias_go clientLoop(clientiter.first, item);

    co_return Result<>();
}

void MainWidget::onMakeMainChannel() { ilias_go makeMainChannel(); }

void MainWidget::selectedChannel(int index) {
    ui->send->setDisabled(index < 0);
    mCurrentIndex = index;
}

void MainWidget::onSendMessage() {
    auto iter = mClients.find(mCurrentIndex);
    if (iter != mClients.end()) {
        NEKO_LOG_INFO("ui test", "send message");
        ilias_go sendMessage(iter->second);
    } else {
        NEKO_LOG_ERROR("ui test", "channel not found");
    }
}

void MainWidget::closeService() {
    auto iter = mClients.find(mCurrentIndex);
    if (iter != mClients.end()) {
        NEKO_LOG_INFO("ui test", "close channel");
        ilias_go iter->second.close();
    }
}

void MainWidget::startService() { mHandles.push_back(ilias_go serverLoop()); }

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setDesktopSettingsAware(true);
    QIoContext ioContext;
    NEKO_NAMESPACE::ProtoFactory protoFactory;
    MainWidget mainWidget(&ioContext, protoFactory);
    mainWidget.show();

    return app.exec();
}
