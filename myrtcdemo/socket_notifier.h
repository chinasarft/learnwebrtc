#include "rtc_base/checks.h"
#include "rtc_base/thread.h"
#include "rtc_base/async_socket.h"
#include "rtc_base/physical_socket_server.h"

class AsyncTcpSocketDispatcher : public rtc::SocketDispatcher {
public:
	explicit AsyncTcpSocketDispatcher(int family);
};

class SocketNotifier {
public:
    static SocketNotifier* GetSocketNotifier();
    
    void AddSyncSocket(rtc::Dispatcher* pDispatcher);
    rtc::PhysicalSocketServer* GetSocketServer();

private:
    SocketNotifier();
    std::shared_ptr<rtc::PhysicalSocketServer> ss_;
    std::shared_ptr<rtc::Thread> thread_;
};


