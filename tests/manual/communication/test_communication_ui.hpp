#pragma once

#include <QMainWindow>

#include "communication/communication_base.hpp"
#include "proto/json_serializer.hpp"
#include "proto/serializer_base.hpp"
#include <variant>

#include "ilias/qt.hpp"
namespace Ui {
class MainWindow;
} // namespace Ui

class MainWidget : public QMainWindow {
    Q_OBJECT
public:
    MainWidget(ILIAS_NAMESPACE::QIoContext* ctxt, std::shared_ptr<NEKO_NAMESPACE::ProtoFactory> protoFactory,
               QWidget* parent = nullptr);
    ~MainWidget();

    ILIAS_NAMESPACE::Task<void> clientLoop(std::weak_ptr<NEKO_NAMESPACE::ChannelBase> channel);
    ILIAS_NAMESPACE::Task<void> serverLoop();
    ILIAS_NAMESPACE::Task<void> makeMainChannel();
    ILIAS_NAMESPACE::Task<void> sendMessage(std::weak_ptr<NEKO_NAMESPACE::ChannelBase> channel);
    ILIAS_NAMESPACE::Task<void> recvMessage(std::weak_ptr<NEKO_NAMESPACE::ChannelBase> channel);
    void closeChannel(std::weak_ptr<NEKO_NAMESPACE::ChannelBase> channel);

protected Q_SLOTS:
    void startService();
    void onMakeMainChannel();
    void selectedChannel(int index);
    void onSendMessage();
    void closeService();

private:
    Ui::MainWindow* ui;
    ILIAS_NAMESPACE::QIoContext* mCtxt;
    NEKO_NAMESPACE::ChannelFactory mChannelFactor;
    bool mExit = true;
    std::weak_ptr<NEKO_NAMESPACE::ChannelBase> mCurrentChannel;
    std::vector<ILIAS_NAMESPACE::CancelHandle> mHandles;
    std::map<int, ILIAS_NAMESPACE::JoinHandle<void>> mClientLoopHandles;
};