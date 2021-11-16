#include "webm_encoder.h"

#include "time.h"
#include "stdio.h"

extern std::atomic_bool log_switch;

void WebmMediaEncoder::set_log_switch(bool log_info_switch)
{
    log_switch.store(log_info_switch);
}

void WebmMediaEncoder::set_log_interval(int64_t interval)
{
    log_interval_ = interval * MICROSECONDS_PER_MILLISECOND;
}

void WebmMediaEncoder::set_resolution(int width, int height)
{
    width_ = width;
    height_ = height;
}

void WebmMediaEncoder::set_frame_rate(int frame_rate)
{
    frame_rate_ = frame_rate;
}

void WebmMediaEncoder::set_src_file(std::string src_filename)
{
    src_filename_ = src_filename;
}

void WebmMediaEncoder::set_dst_file(std::string dst_filename)
{
    dst_filename_ = dst_filename;
}

void WebmMediaEncoder::set_src_dst_file(std::string src_filename, std::string dst_filename)
{
    set_src_file(src_filename);
    set_dst_file(dst_filename);
}

void WebmMediaEncoder::setOnEncodeFinish(CallbackMethod method, void *arg)
{
    OnEncodeFinish = method;
    arg_ = arg;
}

WebmMediaEncoder::WebmMediaEncoder(std::string src_filename, std::string dst_filename, int width, int height)
{
    src_filename_ = src_filename;
    dst_filename_ = dst_filename;
    width_ = width;
    height_ = height;
    process_mode_ = READ_EXIST_FILE;
}

WebmMediaEncoder::WebmMediaEncoder(std::string dst_filename, int width, int height)
{
    dst_filename_ = dst_filename;
    width_ = width;
    height_ = height;
    process_mode_ = ASYNCHRONOUS_PROCESS;
}

void WebmMediaEncoder::InitRead()
{
    ecd_info("Start InitRead");
    raw_data_frame_size_ = input_fmt_bytes_ * width_ * height_;
    raw_data_buff_ = static_cast<uint8_t *>(av_malloc(raw_data_frame_size_));
    if (!raw_data_buff_)
    {
        ecd_error("Failed to allocate frame");
        return;
    }

    memset(raw_data_buff_, 0x00, raw_data_frame_size_);

    input_fp_ = fopen(src_filename_.c_str(), "rb");
    if (!input_fp_)
    {
        ecd_error("Failed to open input file");
        return;
    }
    ecd_info("Finished InitRead");
}

void WebmMediaEncoder::InitConversion()
{
    ecd_info("Start InitConversion");
    dst_frame_ = av_frame_alloc();
    if (!dst_frame_)
    {
        ecd_error("Failed to allocate frame");
        return;
    }

    dst_frame_->format = output_pix_fmt_;
    dst_frame_->width = width_;
    dst_frame_->height = height_;

    ya_size_ = width_ * height_ * sizeof(uint8_t);
    uv_size_ = ya_size_ / 4;
    yuva_size_ = (ya_size_ + uv_size_) * 2;

    //init sws context
    if (av_image_alloc(dst_frame_->data, dst_frame_->linesize, width_, height_,
                       output_pix_fmt_, 16) < 0)
    {
        ecd_error("Failed to allocate frame");
        return;
    }

    sws_context_ = sws_getContext(width_, height_, input_pix_fmt_, width_, height_, output_pix_fmt_,
                                  SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws_context_)
    {
        ecd_error("Failed to init sws context");
        return;
    }
    ecd_info("Finished InitConversion");
}

void WebmMediaEncoder::InitEncode()
{
    ecd_info("Start InitEncode");
    AVCodec *encoder;
    AVDictionary *param = 0;
    //av_dict_set(&param, "quality", "realtime", 0);
    //av_dict_set(&param, "speed", "15", 0);
    av_dict_set(&param, "threads", "2", 0);

    dst_fmt_ctx_ = avformat_alloc_context();
    if (!dst_fmt_ctx_)
    {
        ecd_error("avformat_alloc_context failed");
        return;
    }
    if (avformat_alloc_output_context2(&dst_fmt_ctx_, NULL, "webm", dst_filename_.c_str()) < 0)
    {
        ecd_error("avformat_alloc_output_context2 failed");
        return;
    }

    dst_stream_ = avformat_new_stream(dst_fmt_ctx_, NULL);
    if (!dst_stream_)
    {
        ecd_error("Failed allocating output stream");
        return;
    }

    encoder = const_cast<AVCodec *>(avcodec_find_encoder(AV_CODEC_ID_VP9));
    if (!encoder)
    {
        ecd_error("Necessary encoder not found");
        return;
    }

    enc_ctx_ = avcodec_alloc_context3(encoder);
    if (!enc_ctx_)
    {
        ecd_error("Failed to allocate the encoder context");
        return;
    }

    enc_ctx_->height = height_;
    enc_ctx_->width = width_;
    enc_ctx_->pix_fmt = output_pix_fmt_;
    enc_ctx_->time_base = (AVRational){1, frame_rate_};

    if (dst_fmt_ctx_->oformat->flags & AVFMT_GLOBALHEADER)
        enc_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(enc_ctx_, encoder, &param) < 0)
    {
        ecd_error("Cannot open video encoder");
        return;
    }

    if (avcodec_parameters_from_context(dst_stream_->codecpar, enc_ctx_) < 0)
    {
        ecd_error("Failed to copy encoder parameters to output");
        return;
    }

    dst_stream_->time_base = enc_ctx_->time_base;

    if (!(dst_fmt_ctx_->oformat->flags & AVFMT_NOFILE))
    {
        if (avio_open(&dst_fmt_ctx_->pb, dst_filename_.c_str(), AVIO_FLAG_WRITE) < 0)
        {
            ecd_error("Could not open output file : %s", dst_filename_.c_str());
            return;
        }
    }

    /* init muxer, write output file header */
    if (avformat_write_header(dst_fmt_ctx_, NULL) < 0)
    {
        ecd_error("Error occurred when opening output file");
        return;
    }

    dst_packet_ = av_packet_alloc();
    if (!dst_packet_)
    {
        ecd_error("Failed to allocate packet");
        return;
    }
    ecd_info("Finish InitEncode");
}

void WebmMediaEncoder::EncodeFrame(const uint8_t *data, const int size)
{
    wait_for_encoder_.Wait();
    memcpy(raw_data_buff_, data, size < raw_data_frame_size_ ? size : raw_data_frame_size_);
    wait_for_data_.Signal();
}

void WebmMediaEncoder::FinishEncoding()
{
    ecd_info("Trigger FinishEncoding");
    wait_for_encoder_.Wait();
    data_end_.store(true);
    wait_for_data_.Signal();
    ecd_info("FinishEncoding OK!");
}

void WebmMediaEncoder::Process()
{
    ecd_info("Start Process");
    int ret, frame_cnt = 0;
    int line_size[1] = {input_fmt_bytes_ * width_};
    int64_t pre_check_point_time = av_gettime(), end_frame_time, time_interval = 5000 * 1000;
    int64_t start_encode = av_gettime(), pre_log_time = 0;
    wait_for_encoder_.Signal();
    while (true)
    {
        if (process_mode_ == READ_EXIST_FILE)
        {
            size_t read_size = fread(raw_data_buff_, 1, raw_data_frame_size_, input_fp_);
            if (read_size <= 0)
            {
                break;
            }
        }
        else
        {
            wait_for_data_.Wait();
            bool data_end = data_end_.load();
            if (data_end == true)
                break;
        }

        ret = sws_scale(sws_context_, &raw_data_buff_, line_size, 0, height_, dst_frame_->data, dst_frame_->linesize);
        if (ret < 0)
            ecd_error("Failed to scale rgb to yuv");

        if (process_mode_ == ASYNCHRONOUS_PROCESS)
            wait_for_encoder_.Signal();

        dst_frame_->pts = frame_cnt * av_q2d(enc_ctx_->time_base) * 1000;
        ret = avcodec_send_frame(enc_ctx_, dst_frame_);
        if (ret < 0)
            ecd_error("Error when sending a frame for encoding");

        while (ret >= 0)
        {
            ret = avcodec_receive_packet(enc_ctx_, dst_packet_);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                av_packet_unref(dst_packet_);
                break;
            }
            else if (ret < 0)
                ecd_error("Error during encoding");

            dst_packet_->stream_index = 0;
            av_interleaved_write_frame(dst_fmt_ctx_, dst_packet_);
            av_packet_unref(dst_packet_);
        }

        end_frame_time = av_gettime();
        frame_cnt++;

        if (end_frame_time - pre_log_time >= log_interval_)
        {
            ecd_info("frame: %d, average speed: %.2f ms/frame", frame_cnt, (end_frame_time - start_encode) / 1000.0 / frame_cnt);
            pre_log_time = end_frame_time;
        }
    }
    ecd_info("average speed: %.2f ms/frame", (av_gettime() - start_encode) / 1000.0 / frame_cnt);

    ecd_info("flush...");
    ret = avcodec_send_frame(enc_ctx_, nullptr);
    if (ret < 0)
        ecd_error("Error when sending a frame for encoding");

    while (ret >= 0)
    {
        ret = avcodec_receive_packet(enc_ctx_, dst_packet_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            av_packet_unref(dst_packet_);
            break;
        }

        else if (ret < 0)
            ecd_error("Error during encoding");

        dst_packet_->stream_index = 0;
        av_interleaved_write_frame(dst_fmt_ctx_, dst_packet_);
        av_packet_unref(dst_packet_);
    }
    av_write_trailer(dst_fmt_ctx_);
    ecd_info("End Process");
}

void WebmMediaEncoder::Release()
{
    ecd_info("Start Release");
    if (input_fp_ && process_mode_ == READ_EXIST_FILE)
        fclose(input_fp_);

    av_free(raw_data_buff_);
    av_frame_free(&dst_frame_);
    sws_freeContext(sws_context_);
    av_packet_free(&dst_packet_);
    avformat_free_context(dst_fmt_ctx_);
    avcodec_close(enc_ctx_);
    avcodec_free_context(&enc_ctx_);
    ecd_info("End Release");
}

void WebmMediaEncoder::Start()
{
    ecd_info("WebmMediaEncoder Start");
    process_mode_ = ASYNCHRONOUS_PROCESS;
    data_end_.store(false);

    raw_data_frame_size_ = input_fmt_bytes_ * width_ * height_;
    raw_data_buff_ = static_cast<uint8_t *>(av_malloc(raw_data_frame_size_));
    if (!raw_data_buff_)
    {
        ecd_error("Failed to allocate frame");
        return;
    }

    InitConversion();
    InitEncode();

    auto process_thread = [&]()
    {
        Process();
        Release();
        if (OnEncodeFinish != nullptr)
            OnEncodeFinish(arg_);
    };

    std::thread work_thread(process_thread);
    work_thread.detach();
    ecd_info("WebmMediaEncoder End");
}

void WebmMediaEncoder::Run()
{

    InitRead();
    InitConversion();
    InitEncode();
    Process();
    Release();

    ecd_info("encode finished");
}

void WebmMediaEncoder::set_input_pix_fmt(const std::string str)
{
    if (str == "argb")
    {
        input_pix_fmt_ = AV_PIX_FMT_ARGB;
        input_fmt_bytes_ = 4;
    }
    else if (str == "rgba")
    {
        input_pix_fmt_ = AV_PIX_FMT_RGBA;
        input_fmt_bytes_ = 4;
    }
    else if (str == "rgb" || str == "rgb24")
    {
        input_pix_fmt_ = AV_PIX_FMT_RGB24;
        input_fmt_bytes_ = 3;
    }
    else
    {
        ecd_error("No such pix_fmt");
    }
}
