#include "rtc_base/checks.h"
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/net_helpers.h"
#include "rtc_base/win32_socket_init.h"
#include "rtc_base/win32_socket_server.h"

///--------------------for test start-----------------
#include <iostream>
#include "rtc_base/async_udp_socket.h"
#include "rtc_base/null_socket_server.h"
#include "rtc_base/socket_address.h"
using namespace rtc;
namespace lmktest {
class TestGenerator {
 public:
  TestGenerator() : last(0), count(0) {}

  int Next(int prev) {
    int result = prev + last;
    last = result;
    count += 1;
    return result;
  }

  int last;
  int count;
};

struct TestMessage : public MessageData {
  explicit TestMessage(int v) : value(v) {}

  int value;
};

// Receives on a socket and sends by posting messages.
class SocketClient : public TestGenerator, public sigslot::has_slots<> {
 public:
  SocketClient(AsyncSocket* socket,
               const SocketAddress& addr,
               Thread* post_thread,
               MessageHandler* phandler)
      : socket_(AsyncUDPSocket::Create(socket, addr)),
        post_thread_(post_thread),
        post_handler_(phandler) {
    socket_->SignalReadPacket.connect(this, &SocketClient::OnPacket);
  }

  ~SocketClient() override { delete socket_; }

  SocketAddress address() const { return socket_->GetLocalAddress(); }
  
//  sigslot::signal5<AsyncPacketSocket*, const char*, size_t, const SocketAddress&, const int64_t&>
  void OnPacket(AsyncPacketSocket* socket,
                const char* buf,
                size_t size,
                const SocketAddress& remote_addr,
                const int64_t& packet_time) {
    assert(size == sizeof(uint32_t));
    uint32_t prev = reinterpret_cast<const uint32_t*>(buf)[0];
    uint32_t result = Next(prev);

    post_thread_->PostDelayed(RTC_FROM_HERE, 200, post_handler_, 0,
                              new TestMessage(result));
  }

 private:
  AsyncUDPSocket* socket_;
  Thread* post_thread_;
  MessageHandler* post_handler_;
};
// Receives messages and sends on a socket.
class MessageClient : public MessageHandler, public TestGenerator {
 public:
  MessageClient(Thread* pth, Socket* socket) : socket_(socket) {}

  ~MessageClient() override { delete socket_; }

  void OnMessage(Message* pmsg) override {
    TestMessage* msg = static_cast<TestMessage*>(pmsg->pdata);
    int result = Next(msg->value);
    assert(socket_->Send(&result, sizeof(result)) > 0);
    delete msg;
  }

 private:
  Socket* socket_;
};

class CustomThread : public rtc::Thread {
 public:
  CustomThread()
      : Thread(std::unique_ptr<SocketServer>(new rtc::NullSocketServer())) {}
  ~CustomThread() override { Stop(); }
  bool Start() { return false; }

  bool WrapCurrent() { return Thread::WrapCurrent(); }
  void UnwrapCurrent() { Thread::UnwrapCurrent(); }
};

// A thread that does nothing when it runs and signals an event
// when it is destroyed.
class SignalWhenDestroyedThread : public Thread {
 public:
  SignalWhenDestroyedThread(Event* event)
      : Thread(std::unique_ptr<SocketServer>(new NullSocketServer())),
        event_(event) {}

  ~SignalWhenDestroyedThread() override {
    Stop();
    event_->Set();
  }

  void Run() override {
    // Do nothing.
  }

 private:
  Event* event_;
};

class AtomicBool {
 public:
  explicit AtomicBool(bool value = false) : flag_(value) {}
  AtomicBool& operator=(bool value) {
    CritScope scoped_lock(&cs_);
    flag_ = value;
    return *this;
  }
  bool get() const {
    CritScope scoped_lock(&cs_);
    return flag_;
  }

 private:
  CriticalSection cs_;
  bool flag_;
};

// Function objects to test Thread::Invoke.
struct FunctorA {
  int operator()() { return 42; }
};
class FunctorB {
 public:
  explicit FunctorB(AtomicBool* flag) : flag_(flag) {}
  void operator()() {
    if (flag_)
      *flag_ = true;
  }

 private:
  AtomicBool* flag_;
};
struct FunctorC {
  int operator()() {
    Thread::Current()->ProcessMessages(50);
    return 24;
  }
};
struct FunctorD {
 public:
  explicit FunctorD(AtomicBool* flag) : flag_(flag) {}
  FunctorD(FunctorD&&) = default;
  FunctorD& operator=(FunctorD&&) = default;
  void operator()() {
    if (flag_)
      *flag_ = true;
  }

 private:
  AtomicBool* flag_;
  RTC_DISALLOW_COPY_AND_ASSIGN(FunctorD);
};

}  // namespace lmktest

using namespace lmktest;
void runrtcbasetest_thread_wrap() {
  Thread* current_thread = Thread::Current();
  current_thread->UnwrapCurrent();
  CustomThread* cthread = new CustomThread();
  assert(cthread->WrapCurrent() == true);
  assert(cthread->RunningForTest() == true);
  assert(cthread->IsOwned() == false);
  cthread->UnwrapCurrent();
  assert(cthread->RunningForTest() == false);
  delete cthread;
  current_thread->WrapCurrent();
}

void webrtc_thread_invoke() {
  // Create and start the thread.
  auto thread = Thread::CreateWithSocketServer();
  thread->Start();
  // Try calling functors.
  assert(42 == thread->Invoke<int>(RTC_FROM_HERE, FunctorA()));
  AtomicBool called;
  FunctorB f2(&called);
  thread->Invoke<void>(RTC_FROM_HERE, f2);
  assert(true == called.get());
  // Try calling bare functions.
  struct LocalFuncs {
    static int Func1() { return 999; }
    static void Func2() {}
  };
  assert(999 == thread->Invoke<int>(RTC_FROM_HERE, &LocalFuncs::Func1));
  thread->Invoke<void>(RTC_FROM_HERE, &LocalFuncs::Func2);
}

void output1_int64(const char *fmt, int64_t n) {
  char buf[1024] = {0};
  snprintf(buf, sizeof(buf), fmt, n);
  OutputDebugStringA(buf);
}
void output1_char(const char *fmt, const char *msg) {
  char buf[1024] = {0};
  snprintf(buf, sizeof(buf), fmt, msg);
  OutputDebugStringA(buf);
}
//message queue test
class MsgData : public MessageData {
 public:
  MsgData() {}
  ~MsgData() {}
  void setStr(std::string str) { unique_str_ = str; }
  std::string& getStr() { return unique_str_; }

 private:
  std::string unique_str_;
};

class MsgHandler : public MessageHandler {
 public:
  virtual void OnMessage(Message* msg) {
    switch (msg->message_id) {
      case 0: {
        MsgData* data = (MsgData*)msg->pdata;
        output1_char("data:%s\n", data->getStr().c_str());
      } break;
      default: {
        output1_int64("unknown id:%d\n", msg->message_id);
      } break;
    }
  }

 private:
};

class mysocket : public SocketServer {
 public:
  mysocket() {
    event_ = new Event(false, false);
    event_->Reset();
  }
  ~mysocket() { delete (event_); }
  virtual bool Wait(int cms, bool process_io) {
    output1_int64("mysocket wait:%d\n", (int64_t)cms);
    // return event_->Wait(event_->kForever);
    return event_->Wait(cms);
  }
  virtual void WakeUp() {
    OutputDebugStringA("mysocket wakeup\n");
    event_->Set();
  }
  virtual Socket* CreateSocket(int type) { return nullptr; }
  virtual Socket* CreateSocket(int family, int type) { return nullptr; }
  virtual AsyncSocket* CreateAsyncSocket(int type) { return nullptr; }
  virtual AsyncSocket* CreateAsyncSocket(int family, int type) {
    return nullptr;
  }

 private:
  Event* event_;
};
int webrtc_messagequeue_test() {
  MsgData data;
  data.setStr("sliver");

  MsgHandler handler;

  Message* msg = new Message();
  msg->message_id = 0;
  msg->pdata = &data;
  msg->phandler = &handler;

  mysocket* m_socket = new mysocket();
  MessageQueue queue(m_socket, true);

  Location locate;
  output1_int64("post msg:%lld\n", TimeMillis());
  queue.Post(locate, &handler);
  queue.Get(msg);
  output1_int64("get msg:%lld\n", TimeMillis());

  output1_int64("post delay msg:%lld\n", TimeMillis());
  queue.PostDelayed(locate, 20000, &handler);
  queue.Get(msg);
  queue.GetDelay();
  output1_int64("get delay msg:%lld\n", TimeMillis());

  return 0;
}



class HelpData :public MessageData {
public:
 std::string info_;
};

class Police : public MessageHandler {
public:
 enum {
   MSG_HELP,
  };
 void Help(const std::string& info) {
   HelpData* data = new HelpData;
   data->info_ = info;
   Thread::Current()->Post(RTC_FROM_HERE, this, MSG_HELP,dynamic_cast<MessageData*>(data));
  }
 virtual void OnMessage(Message* msg) {
   switch (msg->message_id) {
   case MSG_HELP:
     HelpData* data = (HelpData*)msg->pdata;
     output1_char("MSG_HELP:%s\n", data->info_.c_str());
     break;
    }
  }
};

void webrtc_test_thread_run() {
 output1_char("Test Thread is started", "");
 Police p;
 p.Help("Please help me!");
 Thread::Current()->Run();
 output1_char("Test Thread is completed", "");
 return;
}

class TestAsyncResolver : public sigslot::has_slots<> {
 public:
  TestAsyncResolver() {
    server_address_.SetIP("www.baidu.com");//localhost ");
    server_address_.SetPort(80);
  }
  rtc::AsyncResolver* resolver_;
  rtc::SocketAddress server_address_;
  void OnResolveResult(AsyncResolverInterface* resolver);
};

void TestAsyncResolver::OnResolveResult(AsyncResolverInterface* resolver) {
  if (resolver_->GetError() != 0) {
    output1_char("resolve fail\n", "");
    resolver_->Destroy(false);
    resolver_ = NULL;
  } else {
    output1_char("resolve ok\n", "");
    server_address_ = resolver_->address();
  }
}


void webrtc_test_asyncresolver() {
  WinsockInitializer w32sock;
  TestAsyncResolver tar;

  AsyncResolver * r{nullptr};
  AsyncResolverInterface* ri = static_cast<AsyncResolverInterface *>(r);
  //AsyncResolverInterface* ri = dynamic_cast<AsyncResolverInterface *>(r);
  char addr[10] = {0};
  sprintf(addr, "%lld %lld",reinterpret_cast<int64_t>(ri), sizeof(AsyncResolver));
  output1_char(addr, "");


  tar.resolver_ = new rtc::AsyncResolver();
  tar.resolver_->SignalDone.connect(&tar, &TestAsyncResolver::OnResolveResult);
  tar.resolver_->Start(tar.server_address_);
  Thread::Current()->ProcessMessages(10000);
  //Thread::Current()->SleepMs(10000000);
}

//--------------------for test end---------------------

int main() {
  webrtc_thread_invoke();
  webrtc_messagequeue_test();
  webrtc_test_thread_run();
  webrtc_test_asyncresolver();
  return 0;
}
