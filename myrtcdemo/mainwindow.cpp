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

void draw(QImage&& img, bool isLocal) {
    emit mainWindow->getFrameSig(std::forward<QImage>(img), isLocal);
};

MainWindow::MainWindow(QWidget *parent) :
    ui(new Ui::MainWindow),
    ui_(CONNECT_TO_SERVER),
    destroyed_(false),
    nested_msg_(NULL) {

    ui->setupUi(this);
    ui->textAddress->setText("localhost");
    //ui->textAddress->setText("127.0.0.1");
    ui->textAddress->setText("10.93.245.95");

    port_ = "8888";
    ui->textPort->setText(port_.c_str());
    mainWindow = this;
    connect(this, &MainWindow::uiCallbackSig, this, &MainWindow::uiCallbackSlot, Qt::QueuedConnection);
    connect(this, &MainWindow::getFrameSig, this, &MainWindow::getFrameSlot, Qt::QueuedConnection);
}

void MainWindow::getFrameSlot(QImage img, bool isLocal) {
    if (isLocal)
        image_ = img;
    else
        remoteImage_ = img;
    update();
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    /*
    实际上peerconnection_client这里是通过local_render_和remote_render_获取到frame和，在这里进行合帧的
    然后渲染的(还是gdi来渲染的), 所以为了demo演示，这里也使用效率差的pixmap来做
    */
    if (image_.size().width() <= 0) return;
    QPainter painter(&remoteImage_);
    painter.drawImage(0, 0, image_);
    if (remoteImage_.isNull())
      ui->video->setPixmap(QPixmap::fromImage(image_));
    else
      ui->video->setPixmap(QPixmap::fromImage(remoteImage_));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_connectBtn_clicked()
{
    if (!callback_)
        return;
    if (ui_ == CONNECT_TO_SERVER) {
        std::string server = std::string((const char*)ui->textAddress->toPlainText().toLocal8Bit().constData());
        std::string port_str = std::string((const char*)ui->textPort->toPlainText().toLocal8Bit().constData());
        int port = port_str.length() ? atoi(port_str.c_str()) : 0;
        callback_->StartLogin(server, port);
    }
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

HWND MainWindow::localHandle() const {
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
    //ui_ = STREAMING;
    return;
}

void MainWindow::StartLocalRenderer(webrtc::VideoTrackInterface* local_video) {
  local_renderer_.reset(new VideoRenderer(true, local_video));
}

void MainWindow::StopLocalRenderer() {
  local_renderer_.reset();
}

void MainWindow::StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video) {
  remote_renderer_.reset(new VideoRenderer(false, remote_video));
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
    bool isLocal,
    webrtc::VideoTrackInterface* track_to_render)
    : isLocal_(isLocal), rendered_track_(track_to_render) {
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
  if (biSizeImage > imageBuf_.size())
    imageBuf_.resize(biSizeImage);
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
                       imageBuf_.data(),
                       width_ * 4,
                       buffer->width(), buffer->height());
    QImage tmpImg((uchar *)(imageBuf_.data()), buffer->width(), buffer->height(), QImage::Format_ARGB32);
    // TODO 这里一次copy
    if (isLocal_) {
        tmpImg = tmpImg.scaled(320, 180);
    }
    draw(std::move(tmpImg), isLocal_);
}
