
#include "socket_notifier.h"
#include "rtc_base/null_socket_server.h"


AsyncTcpSocketDispatcher::AsyncTcpSocketDispatcher(int family) : family_(family), rtc::SocketDispatcher(SocketNotifier::GetSocketNotifier()->GetSocketServer()) {
  
}

int AsyncTcpSocketDispatcher::Connect(const  rtc::SocketAddress& addr) {
    Create(family_, SOCK_STREAM);
    SetEnabledEvents(rtc::DispatcherEvent::DE_READ | rtc::DispatcherEvent::DE_CONNECT | rtc::DispatcherEvent::DE_CLOSE);
    SocketNotifier::GetSocketNotifier()->AddSyncSocket(this);
    return this->SocketDispatcher::Connect(addr);
}

SocketNotifier* SocketNotifier::GetSocketNotifier() {
    static SocketNotifier socketNotifier;
    return &socketNotifier;
}


SocketNotifier::SocketNotifier() {

    ss_ = std::make_shared<rtc::PhysicalSocketServer>();

    thread_ = std::make_shared<rtc::Thread>(dynamic_cast<rtc::SocketServer*>(ss_.get()));

    thread_->SetName("my_socket_thread", nullptr);
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

rtc::Thread* SocketNotifier::GetThreadPtr() const {
    return thread_.get();
}


SignalHandler* SignalHandler::GetSignalHandler() {
    static SignalHandler sigHandler;
    return &sigHandler;
}

SignalHandler::SignalHandler() {
    ss_.reset(new rtc::NullSocketServer());
    thread_ = std::make_shared<rtc::Thread>(ss_.get());
    thread_->SetName("my_signal_thread", nullptr);
    thread_->Start();
}

rtc::Thread* SignalHandler::GetThreadPtr() {
	return thread_.get();
}

