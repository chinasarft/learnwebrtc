#ifdef WIN32
#include "rtc_base/win32_socket_init.h"
#include "rtc_base/win32_socket_server.h"
#endif
#include "rtc_base/ssl_adapter.h"
#include "conductor.h"

#include "mainwindow.h"
#include <QApplication>

#ifndef WIN32
#include "socket_notifier.h"
#endif


int main(int argc, char **argv)
{
#ifdef WIN32
    rtc::WinsockInitializer winsock_init;
    rtc::Win32SocketServer w32_ss;
    rtc::Win32Thread w32_thread(&w32_ss);
    rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);
#else
    SocketNotifier::GetSocketNotifier();
    SignalHandler::GetSignalHandler();
    fprintf(stderr, "main thread id:%p\n", pthread_self());
#endif

    QApplication a(argc, argv);
    MainWindow wnd;
    wnd.show();

    rtc::InitializeSSL();
    PeerConnectionClient client;
    rtc::scoped_refptr<Conductor> conductor(
        new rtc::RefCountedObject<Conductor>(&client, &wnd));


    int ret = a.exec();

    rtc::CleanupSSL();

    return ret;
}
