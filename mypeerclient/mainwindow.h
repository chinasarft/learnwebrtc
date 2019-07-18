#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "api/media_stream_interface.h"
#include "api/video/video_frame.h"
#include "peer_connection_client.h"
#include "media/base/media_channel.h"
#include "media/base/video_common.h"
#include "rtc_base/thread.h"
#if !defined(WEBRTC_WIN)
#include <arpa/inet.h>
#endif  // WEBRTC_WIN

#include <QMainWindow>
#include <QListWidgetItem>
#include <QImage>

#ifdef WEBRTC_MAC
//#include <AppKit/NSView.h>
#define HWND void*
#endif

namespace Ui {
class MainWindow;
}

class MainWndCallback {
 public:
  virtual void StartLogin(const std::string& server, int port) = 0;
  virtual void DisconnectFromServer() = 0;
  virtual void ConnectToPeer(int peer_id) = 0;
  virtual void DisconnectFromCurrentPeer() = 0;
  virtual void UIThreadCallback(int msg_id, void* data) = 0;
  virtual void Close() = 0;

 protected:
  virtual ~MainWndCallback() {}
};

// Pure virtual interface for the main window.
class VMainWindow {
 public:
  virtual ~VMainWindow() {}

  enum UI {
    CONNECT_TO_SERVER,
    LIST_PEERS,
    STREAMING,
  };

  virtual void RegisterObserver(MainWndCallback* callback) = 0;

  virtual void MessageBox(const char* caption,
                          const char* text,
                          bool is_error) = 0;

  virtual UI current_ui() = 0;

  virtual void SwitchToPeerList(const Peers& peers) = 0;
  virtual void SwitchToStreamingUI() = 0;

  virtual void StartLocalRenderer(webrtc::VideoTrackInterface* local_video) = 0;
  virtual void StopLocalRenderer() = 0;
  virtual void StartRemoteRenderer(
      webrtc::VideoTrackInterface* remote_video) = 0;
  virtual void StopRemoteRenderer() = 0;

  virtual void QueueUIThreadCallback(int msg_id, void* data) = 0;
};

class MainWindow : public QMainWindow, public VMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const char* server, int port, bool auto_connect, bool auto_call, QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void paintEvent(QPaintEvent *event);

signals:
    void uiCallbackSig(int msg_id, void* data);
    void getFrameSig(QImage img);


private slots:
    void uiCallbackSlot(int msg_id, void* data);
    void getFrameSlot(QImage img);

    void on_connectBtn_clicked();

    void on_listPeer_itemDoubleClicked(QListWidgetItem *item);

    void on_listPeer_currentRowChanged(int currentRow);

private:
    QImage image_;
    Ui::MainWindow *ui;

public:
    static const char kClassName[];

  bool Create();
  bool Destroy();

  virtual void RegisterObserver(MainWndCallback* callback);
  virtual void SwitchToPeerList(const Peers& peers);
  virtual void SwitchToStreamingUI();
  virtual void MessageBox(const char* caption, const char* text, bool is_error);
  virtual UI current_ui() { return ui_; }

  virtual void StartLocalRenderer(webrtc::VideoTrackInterface* local_video);
  virtual void StopLocalRenderer();
  virtual void StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video);
  virtual void StopRemoteRenderer();

  virtual void QueueUIThreadCallback(int msg_id, void* data);

  HWND localHandle() const;
  HWND remote1Handle() const;

  class VideoRenderer : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
   public:
    VideoRenderer(HWND wnd,
                  int width,
                  int height,
                  webrtc::VideoTrackInterface* track_to_render);
    virtual ~VideoRenderer();

    void Lock() { buffer_lock_.Enter(); }

    void Unlock() { buffer_lock_.Leave(); }

    // VideoSinkInterface implementation
    void OnFrame(const webrtc::VideoFrame& frame) override;
    const uint8_t* image() const { return image_.get(); }

   protected:
    void SetSize(int width, int height);

    enum {
      SET_SIZE,
      RENDER_FRAME,
    };

    HWND wnd_;
    int width_ = 0;
    int height_ = 0;
    std::unique_ptr<uint8_t[]> image_;
    rtc::CriticalSection buffer_lock_;
    rtc::scoped_refptr<webrtc::VideoTrackInterface> rendered_track_;
  };

  // A little helper class to make sure we always to proper locking and
  // unlocking when working with VideoRenderer buffers.
  template <typename T>
  class AutoLock {
   public:
    explicit AutoLock(T* obj) : obj_(obj) { obj_->Lock(); }
    ~AutoLock() { obj_->Unlock(); }

   protected:
    T* obj_;
  };

 protected:
  enum ChildWindowID {
    EDIT_ID = 1,
    BUTTON_ID,
    LABEL1_ID,
    LABEL2_ID,
    LISTBOX_ID,
  };

  void OnPaint();
  void OnDestroyed();

  void OnDefaultAction();

 private:
  std::unique_ptr<VideoRenderer> local_renderer_;
  std::unique_ptr<VideoRenderer> remote_renderer_;
  UI ui_;
  rtc::PlatformThreadId ui_thread_id_;
  bool destroyed_;
  void* nested_msg_;
  MainWndCallback* callback_;
  std::string server_;
  std::string port_;
  bool auto_connect_;
  bool auto_call_;
};

#endif // MAINWINDOW_H
