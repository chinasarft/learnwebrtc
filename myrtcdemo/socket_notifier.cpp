
#include "socket_notifier.h"


AsyncTcpSocketDispatcher::AsyncTcpSocketDispatcher(int family) : rtc::SocketDispatcher(SocketNotifier::GetSocketNotifier()->GetSocketServer()) {
  SetEnabledEvents(rtc::DispatcherEvent::DE_READ | rtc::DispatcherEvent::DE_CONNECT | rtc::DispatcherEvent::DE_CLOSE);
  Create(family, SOCK_STREAM);
  SocketNotifier::GetSocketNotifier()->AddSyncSocket(this);
}

SocketNotifier* SocketNotifier::GetSocketNotifier() {
    static SocketNotifier socketNotifier;
    return &socketNotifier;
}


SocketNotifier::SocketNotifier() {

    ss_ = std::make_shared<rtc::PhysicalSocketServer>();

    thread_ = std::make_shared<rtc::Thread>(dynamic_cast<rtc::SocketServer*>(ss_.get()));

    thread_->SetName("socket_notifier", nullptr);
    thread_->Start();
}


void SocketNotifier::AddSyncSocket(rtc::Dispatcher* pDispatcher){
    ss_->Add(pDispatcher);
    ss_->WakeUp();
    return;
}

rtc::PhysicalSocketServer* SocketNotifier::GetSocketServer() {
    return ss_.get();
}
