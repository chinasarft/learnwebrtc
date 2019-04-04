#include <math.h>

#include "api/video/i420_buffer.h"
#include "examples/peerconnection/client/defaults.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "third_party/libyuv/include/libyuv/convert_argb.h"

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>


ATOM MainWindow::wnd_class_ = 0;
const char MainWindow::kClassName[] = "WebRTC_MainWnd";

MainWindow::MainWindow(const char* server, int port, bool auto_connect, bool auto_call, QWidget *parent) :
    ui(new Ui::MainWindow),
    ui_(CONNECT_TO_SERVER),
    destroyed_(false),
    nested_msg_(NULL),
    callback_(NULL),
    server_(server),
    auto_connect_(auto_connect),
    auto_call_(auto_call) {

    ui->setupUi(this);
    ui->textAddress->setText("localhost");

    char buffer[10];
    snprintf(buffer, sizeof(buffer), "%i", port);
    ui->textPort->setText(buffer);
    port_ = buffer;

    connect(this, &MainWindow::uiCallbackSig, this, &MainWindow::uiCallbackSlot, Qt::QueuedConnection);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_connectBtn_clicked()
{
    OnDefaultAction();
}

void MainWindow::on_listPeer_itemDoubleClicked(QListWidgetItem *item)
{
    if (ui_ == LIST_PEERS) {
        int peer_id = item->data(Qt::UserRole).toInt();
        if (peer_id != -1 && callback_) {
            callback_->ConnectToPeer(peer_id);
        }
    }
}

void MainWindow::on_listPeer_currentRowChanged(int currentRow)
{
}
void MainWindow::uiCallbackSlot(int msg_id, void* data) {
    callback_->UIThreadCallback(msg_id, data);
}

bool MainWindow::Create() {

    ui_thread_id_ = ::GetCurrentThreadId();

    return true;
}

void MainWindow::OnDefaultAction() {
    if (!callback_)
        return;
    if (ui_ == CONNECT_TO_SERVER) {
        std::string server = ui->textAddress->toPlainText().toStdString();
        std::string port_str = ui->textPort->toPlainText().toStdString();
        int port = port_str.length() ? atoi(port_str.c_str()) : 0;
        callback_->StartLogin(server, port);
    }
} 

HWND MainWindow::handle() const {
    return (HWND)(ui->video->winId());
}
void MainWindow::MessageBox(const char* caption, const char* text, bool is_error) {
    QMessageBox::about(NULL, caption, text);
}

void MainWindow::RegisterObserver(MainWndCallback* callback) {
    callback_ = callback;
}

void MainWindow::SwitchToPeerList(const Peers& peers) {
    ui->state->setText("connected");
    ui->listPeer->clear();
    int i = 0;
    ui_ = LIST_PEERS;
    for (auto iter = peers.begin(); iter != peers.end(); iter++) {
        ui->listPeer->addItem((*iter).second.c_str());
        ui->listPeer->item(i++)->setData(Qt::UserRole, (*iter).first);
    }
    return;
}

void MainWindow::SwitchToStreamingUI() {
    ui_ = STREAMING;
    return;
}

void MainWindow::StartLocalRenderer(webrtc::VideoTrackInterface* local_video) {
  local_renderer_.reset(new VideoRenderer(handle(), 1, 1, local_video));
}

void MainWindow::StopLocalRenderer() {
  local_renderer_.reset();
}

void MainWindow::StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video) {
  remote_renderer_.reset(new VideoRenderer(handle(), 1, 1, remote_video));
}

void MainWindow::StopRemoteRenderer() {
    remote_renderer_.reset();
}

void MainWindow::QueueUIThreadCallback(int msg_id, void* data) {
    this->uiCallbackSig(msg_id, data);
}




//
// MainWindow::VideoRenderer
//

MainWindow::VideoRenderer::VideoRenderer(
    HWND wnd,
    int width,
    int height,
    webrtc::VideoTrackInterface* track_to_render)
    : wnd_(wnd), rendered_track_(track_to_render) {
  ::InitializeCriticalSection(&buffer_lock_);
  ZeroMemory(&bmi_, sizeof(bmi_));
  bmi_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi_.bmiHeader.biPlanes = 1;
  bmi_.bmiHeader.biBitCount = 32;
  bmi_.bmiHeader.biCompression = BI_RGB;
  bmi_.bmiHeader.biWidth = width;
  bmi_.bmiHeader.biHeight = -height;
  bmi_.bmiHeader.biSizeImage =
      width * height * (bmi_.bmiHeader.biBitCount >> 3);
  rendered_track_->AddOrUpdateSink(this, rtc::VideoSinkWants());
}

MainWindow::VideoRenderer::~VideoRenderer() {
  rendered_track_->RemoveSink(this);
  ::DeleteCriticalSection(&buffer_lock_);
}

void MainWindow::VideoRenderer::SetSize(int width, int height) {
  AutoLock<VideoRenderer> lock(this);

  if (width == bmi_.bmiHeader.biWidth && height == bmi_.bmiHeader.biHeight) {
    return;
  }

  bmi_.bmiHeader.biWidth = width;
  bmi_.bmiHeader.biHeight = -height;
  bmi_.bmiHeader.biSizeImage =
      width * height * (bmi_.bmiHeader.biBitCount >> 3);
  image_.reset(new uint8_t[bmi_.bmiHeader.biSizeImage]);
}

void MainWindow::VideoRenderer::OnFrame(const webrtc::VideoFrame& video_frame) {
  {
    AutoLock<VideoRenderer> lock(this);

    rtc::scoped_refptr<webrtc::I420BufferInterface> buffer(
        video_frame.video_frame_buffer()->ToI420());
    if (video_frame.rotation() != webrtc::kVideoRotation_0) {
      buffer = webrtc::I420Buffer::Rotate(*buffer, video_frame.rotation());
    }

    SetSize(buffer->width(), buffer->height());

    RTC_DCHECK(image_.get() != NULL);
    libyuv::I420ToARGB(buffer->DataY(), buffer->StrideY(), buffer->DataU(),
                       buffer->StrideU(), buffer->DataV(), buffer->StrideV(),
                       image_.get(),
                       bmi_.bmiHeader.biWidth * bmi_.bmiHeader.biBitCount / 8,
                       buffer->width(), buffer->height());
  }
  InvalidateRect(wnd_, NULL, TRUE);
}