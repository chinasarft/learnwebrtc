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
#include <QPainter>
#include <QPoint>

const char MainWindow::kClassName[] = "WebRTC_MainWnd";
MainWindow *mainWindow;

void draw(QImage img) {
    emit mainWindow->getFrameSig(img);
};

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
    ui->textAddress->setText("100.100.62.17");

    char buffer[10];
    snprintf(buffer, sizeof(buffer), "%i", port);
    ui->textPort->setText(buffer);
    port_ = buffer;
    mainWindow = this;
    connect(this, &MainWindow::uiCallbackSig, this, &MainWindow::uiCallbackSlot, Qt::QueuedConnection);
    connect(this, &MainWindow::getFrameSig, this, &MainWindow::getFrameSlot, Qt::QueuedConnection);
}

void MainWindow::getFrameSlot(QImage img) {
    image_ = img;
    update();
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    /*
    实际上peerconnection_client这里是通过local_render_和remote_render_获取到frame和，在这里进行合帧的
    然后渲染的(还是gdi来渲染的), 所以为了demo演示，这里也使用效率差的pixmap来做
    */
    if (image_.size().width() <= 0) return;
    ui->video->setPixmap(QPixmap::fromImage(image_));
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

    ui_thread_id_ = rtc::CurrentThreadId();

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
  rendered_track_->AddOrUpdateSink(this, rtc::VideoSinkWants());
}

MainWindow::VideoRenderer::~VideoRenderer() {
  rendered_track_->RemoveSink(this);
}

void MainWindow::VideoRenderer::SetSize(int width, int height) {
  AutoLock<VideoRenderer> lock(this);
  if (width == width_ && height == height_) {
    return;
  }
  width_ = width;
  height_ = height;
  int biSizeImage = width * height * 4;
  image_.reset(new uint8_t[biSizeImage]);
}

void MainWindow::VideoRenderer::OnFrame(const webrtc::VideoFrame& video_frame) {
    AutoLock<VideoRenderer> lock(this);

    rtc::scoped_refptr<webrtc::I420BufferInterface> buffer(
        video_frame.video_frame_buffer()->ToI420());
    if (video_frame.rotation() != webrtc::kVideoRotation_0) {
      buffer = webrtc::I420Buffer::Rotate(*buffer, video_frame.rotation());
    }

    SetSize(buffer->width(), buffer->height());

    //RTC_DCHECK(image_.get() != NULL);
    libyuv::I420ToARGB(buffer->DataY(), buffer->StrideY(), buffer->DataU(),
                       buffer->StrideU(), buffer->DataV(), buffer->StrideV(),
                       image_.get(),
                       width_ * 4,
                       buffer->width(), buffer->height());
    QImage tmpImg((uchar *)(image_.get()), buffer->width(), buffer->height(), QImage::Format_ARGB32);
    // TODO 这里一次copy
    draw(tmpImg.copy());
}
