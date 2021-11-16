#ifndef WEBM_ENCODER_H_78978975981379873128C
#define WEBM_ENCODER_H_78978975981379873128C

#include <unistd.h>

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include "semaphore.h"
#include "log_util.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libswscale/swscale.h>
#include <libavutil/time.h>
}

typedef void (*CallbackMethod)(void *arg);

enum ProcessMode
{
  READ_EXIST_FILE,
  ASYNCHRONOUS_PROCESS
};

class WebmMediaEncoder
{
private:
  AVPixelFormat output_pix_fmt_ = AV_PIX_FMT_YUVA420P;
  AVPixelFormat input_pix_fmt_ = AV_PIX_FMT_ARGB;
  int input_fmt_bytes_ = 4;
  const int64_t MICROSECONDS_PER_MILLISECOND = 1000;
  int64_t log_interval_ = 500 * MICROSECONDS_PER_MILLISECOND;

  ProcessMode process_mode_;
  std::atomic_bool data_end_;
  Semaphore wait_for_data_;
  Semaphore wait_for_encoder_;
  CallbackMethod OnEncodeFinish = nullptr;
  void *arg_ = nullptr;

  std::string src_filename_, dst_filename_;

  int width_ = 720, height_ = 1280;
  int frame_rate_ = 30;

  FILE *input_fp_ = nullptr;
  uint8_t *raw_data_buff_;
  size_t raw_data_frame_size_;

  AVFrame *dst_frame_;
  struct SwsContext *sws_context_;
  size_t yuva_size_;
  size_t ya_size_;
  size_t uv_size_;

  AVPacket *dst_packet_;
  AVFormatContext *dst_fmt_ctx_;
  AVCodecContext *enc_ctx_;
  AVStream *dst_stream_;

  void InitRead();
  void InitConversion();
  void InitEncode();
  void Process();
  void Release();

public:
  WebmMediaEncoder() { process_mode_ = READ_EXIST_FILE; }

  //读取文件编码
  WebmMediaEncoder(std::string src_filename, std::string dst_filename, int width, int height);

  //异步接收数据编码
  WebmMediaEncoder(std::string dst_filename, int width, int height);

  //设定编码完成后回调函数
  void setOnEncodeFinish(CallbackMethod method, void *arg);

  //设定编码时打印日志时间间隔,单位毫秒
  void set_log_interval(int64_t interval);

  void set_log_switch(bool log_info_switch);

  //设定分辨率
  void set_resolution(int width, int height);

  //设定帧率
  void set_frame_rate(int frame_rate);

  //重新设定输入输出文件
  void set_src_file(std::string src_filename);
  void set_dst_file(std::string dst_filename);
  void set_src_dst_file(std::string src_filename, std::string dst_filename);

  //重新设定输入格式
  void set_input_pix_fmt(const std::string s);

  //启动编码器
  void Run();

  //以异步方式启动编码器
  void Start();
  void EncodeFrame(const uint8_t *data, const int size);
  void FinishEncoding();
};

#endif