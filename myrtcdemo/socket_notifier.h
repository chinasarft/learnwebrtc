#include "rtc_base/checks.h"
#include "rtc_base/thread.h"
#include "rtc_base/async_socket.h"
#include "rtc_base/physical_socket_server.h"
#include "rtc_base/socket_address.h"

class AsyncTcpSocketDispatcher : public rtc::SocketDispatcher {
public:
    explicit AsyncTcpSocketDispatcher(int family);
    int Connect(const rtc::SocketAddress& addr) override;
    
private:
    int family_;
};

class SocketNotifier {
public:
    static SocketNotifier* GetSocketNotifier();
    
    void AddSyncSocket(rtc::Dispatcher* pDispatcher);
    rtc::PhysicalSocketServer* GetSocketServer();
    rtc::Thread* GetThreadPtr() const;
	~SocketNotifier();
    
private:
    SocketNotifier();
    std::shared_ptr<rtc::PhysicalSocketServer> ss_;
    std::shared_ptr<rtc::Thread> thread_;
};


// signal
class SignalHandler {
public:
    static SignalHandler* GetSignalHandler();
    rtc::Thread* GetThreadPtr();
    
private:
    SignalHandler();
    rtc::Thread* thread_;
    rtc::SocketServer* ss_;
};

