#ifndef H265_H
#define H265_H

/**
 A simple H265 MP4 example using FFMPEG to write a
 moving red square on a black background to out.mp4.

 Based upon:
   - https://github.com/shi-yan/videosamples
   - https://www.ffmpeg.org/doxygen/0.7/encoding-example_8c-source.html
*/


#include <stdexcept>
#include <cstdint>
#include <vector>

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/avutil.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/time.h>
    #include <libavutil/opt.h>
    #include <libswscale/swscale.h>
}

struct H265Encoder
{

    H265Encoder
    (
        const char * filename,
        uint16_t width = 1920,
        uint16_t height = 1080,
        uint16_t fps = 60
    )
    :
      filename(filename),
      width(width),
      height(height),
      fps(fps)
    {
        oformat = av_guess_format(nullptr, filename, nullptr);
        if (!oformat)
        {
            throw std::runtime_error("Can't create output format");
        }
        oformat->video_codec = AV_CODEC_ID_H265;

        if (avformat_alloc_output_context2(&ofctx, oformat, nullptr, filename))
        {
            throw std::runtime_error("Can't create output context");
        }

        codec = avcodec_find_encoder(oformat->video_codec);
        if (!codec)
        {
            throw std::runtime_error("Can't create codec");
        }

        AVStream* stream = avformat_new_stream(ofctx, codec);

        if (!stream)
        {
            throw std::runtime_error("Can't find format");
        }

        cctx = avcodec_alloc_context3(codec);

        if (!cctx)
        {
            throw std::runtime_error("Can't create codec context");
        }

        stream->codecpar->codec_id = oformat->video_codec;
        stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        stream->codecpar->width = width;
        stream->codecpar->height = height;
        stream->codecpar->format = AV_PIX_FMT_YUV420P;
        stream->codecpar->bit_rate = bitrate;
        avcodec_parameters_to_context(cctx, stream->codecpar);
        cctx->time_base = (AVRational){ 1, fps };
        cctx->max_b_frames = 2;
        cctx->gop_size = 12;
        cctx->framerate = (AVRational){ fps, 1 };

        av_opt_set(cctx, "preset", "ultrafast", 0);
        avcodec_parameters_from_context(stream->codecpar, cctx);
    }

    ~H265Encoder()
    {
        if (videoFrame)
        {
            av_frame_free(&videoFrame);
        }
        if (cctx)
        {
            avcodec_free_context(&cctx);
        }
        if (ofctx)
        {
            avformat_free_context(ofctx);
        }
        if (swsCtx)
        {
            sws_freeContext(swsCtx);
        }
    }

    void open(bool info = false)
    {
        if ((avcodec_open2(cctx, codec, NULL)) < 0)
        {
            throw std::runtime_error("Failed to open codec");
        }

        if (!(oformat->flags & AVFMT_NOFILE))
        {
            if ((avio_open(&ofctx->pb, filename, AVIO_FLAG_WRITE)) < 0)
            {
                throw std::runtime_error("Failed to open file");
            }
        }

        if ((avformat_write_header(ofctx, NULL)) < 0)
        {
            throw std::runtime_error("Failed to write header");
        }

        if (info) { av_dump_format(ofctx, 0, filename, 1); }
    }

    void write(uint8_t * frame)
    {
        if (!videoFrame)
        {
            videoFrame = av_frame_alloc();
            videoFrame->format = AV_PIX_FMT_YUV420P;
            videoFrame->width = cctx->width;
            videoFrame->height = cctx->height;
            if ((av_frame_get_buffer(videoFrame, 0)) < 0)
            {
                throw std::runtime_error("Failed to allocate picture");
            }
        }
        if (!swsCtx)
        {
            swsCtx = sws_getContext
            (
                cctx->width,
                cctx->height,
                AV_PIX_FMT_RGBA,
                cctx->width,
                cctx->height,
                AV_PIX_FMT_YUV420P,
                SWS_BICUBIC,
                0,
                0,
                0
            );
        }

        // Fixed, stride should be 4x for RBGA data.
        int stride[1] = {4 * cctx->width};

        sws_scale
        (
            swsCtx,
            (const uint8_t* const*)&frame,
            stride,
            0,
            cctx->height,
            videoFrame->data,
            videoFrame->linesize
        );

        videoFrame->pts = (1.0 / float(fps)) * 90000 * (frameCounter++);
        if ((avcodec_send_frame(cctx, videoFrame)) < 0)
        {
            throw std::runtime_error("Failed to send frame");
        }

        AVPacket * pkt = av_packet_alloc();
        pkt->data = NULL;
        pkt->size = 0;
        pkt->flags |= AV_PKT_FLAG_KEY;
        if (avcodec_receive_packet(cctx, pkt) == 0)
        {
            uint8_t* size = ((uint8_t*)pkt->data);
            av_interleaved_write_frame(ofctx, pkt);
            av_packet_unref(pkt);
        }
    }

    void finish()
    {
        AVPacket * pkt = av_packet_alloc();
        pkt->data = NULL;
        pkt->size = 0;

        while(true)
        {
            avcodec_send_frame(cctx, NULL);
            if (avcodec_receive_packet(cctx, pkt) == 0)
            {
                av_interleaved_write_frame(ofctx, pkt);
                av_packet_unref(pkt);
            }
            else
            {
                break;
            }
        }

        av_write_trailer(ofctx);
        if (!(oformat->flags & AVFMT_NOFILE))
        {
            if (avio_close(ofctx->pb) < 0)
            {
                throw std::runtime_error("Failed to close file");
            }
        }
    }

    AVFrame* videoFrame = nullptr;
    AVCodec* codec = nullptr;
    AVStream* stream = nullptr;
    AVCodecContext* cctx = nullptr;
    SwsContext* swsCtx = nullptr;
    AVFormatContext* ofctx = nullptr;
    AVOutputFormat* oformat = nullptr;

    uint16_t fps = 60;
    uint16_t width = 1920;
    uint16_t height = 1080;
    uint32_t bitrate = 2000000;
    uint16_t frameCounter = 0;

    const char * filename;
};

#endif /* H265_H */
