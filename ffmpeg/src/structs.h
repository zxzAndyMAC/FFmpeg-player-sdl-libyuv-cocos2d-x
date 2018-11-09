
#ifndef FFMPEG_STRUCTS_H
#define FFMPEG_STRUCTS_H

extern "C" {
	#include "libavutil/avstring.h"
	#include "libavutil/eval.h"
	#include "libavutil/mathematics.h"
	#include "libavutil/pixdesc.h"
	#include "libavutil/imgutils.h"
	#include "libavutil/parseutils.h"
	#include "libavutil/samplefmt.h"
	#include "libavutil/avassert.h"
	#include "libavutil/time.h"
	#include "libavformat/avformat.h"
	#include "libavdevice/avdevice.h"
	#include "libswscale/swscale.h"
	#include "libavcodec/avfft.h"
	#include "libswresample/swresample.h"
}

#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>

#include <assert.h>

namespace FFMPEG {

#define FF_QUIT_EVENT    (SDL_USEREVENT + 2)
#define FF_PAUSE_EVENT    (SDL_USEREVENT + 1000)
#define FF_MUTE_EVENT    (SDL_USEREVENT + 2000)
#define FF_OVER_EVENT    (SDL_USEREVENT + 3000)
#define FF_SEEK_EVENT    (SDL_USEREVENT + 4000)

#define FF_QUIT_EVENT_ID(id) FF_QUIT_EVENT+id
#define FF_PAUSE_EVENT_ID(id) FF_PAUSE_EVENT+id
#define FF_MUTE_EVENT_ID(id) FF_MUTE_EVENT+id
#define FF_OVER_EVENT_ID(id) FF_OVER_EVENT+id
#define FF_SEEK_EVENT_ID(id) FF_SEEK_EVENT+id

#define MAX_QUEUE_SIZE (15 * 1024 * 1024)
#define MIN_FRAMES 25
#define EXTERNAL_CLOCK_MIN_FRAMES 2
#define EXTERNAL_CLOCK_MAX_FRAMES 10

	// 最小音频缓冲
#define SDL_AUDIO_MIN_BUFFER_SIZE 512
	// 计算实际音频缓冲大小，并不需要太频繁回调，这里设置的是最大音频回调次数是每秒30次
#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30

	// 音频控制 以db为单位的步进
#define SDL_VOLUME_STEP (0.75)

	// 最低同步阈值，如果低于该值，则不需要同步校正
#define AV_SYNC_THRESHOLD_MIN 0.04
	// 最大同步阈值，如果大于该值，则需要同步校正
#define AV_SYNC_THRESHOLD_MAX 0.1
	// 帧补偿同步阈值，如果帧持续时间比这更长，则不用来补偿同步
#define AV_SYNC_FRAMEDUP_THRESHOLD 0.1
	// 同步阈值。如果误差太大，则不进行校正
#define AV_NOSYNC_THRESHOLD 10.0

	// 正确同步的最大音频速度变化值(百分比)
#define SAMPLE_CORRECTION_PERCENT_MAX 10

	// 根据实时码流的缓冲区填充时间做外部时钟调整
#define EXTERNAL_CLOCK_SPEED_MIN  0.900
#define EXTERNAL_CLOCK_SPEED_MAX  1.010
#define EXTERNAL_CLOCK_SPEED_STEP 0.001

	// 使用差值来实现平均值
#define AUDIO_DIFF_AVG_NB   20

	// 刷新频率 应该小于 1/fps
#define REFRESH_RATE 0.01

	// 采样大小
#define SAMPLE_ARRAY_SIZE (8 * 65536)

#define CURSOR_HIDE_DELAY 1000000

#define USE_ONEPASS_SUBTITLE_RENDER 1


	// 包列表结构
	typedef struct MyAVPacketList {
		AVPacket pkt;
		struct MyAVPacketList *next;
		int serial;
	} MyAVPacketList;

	// 待解码包队列
	typedef struct PacketQueue {
		MyAVPacketList *first_pkt, *last_pkt;
		int nb_packets;
		int size;
		int64_t duration;
		int abort_request;
		int serial;
		SDL_mutex *mutex;
		SDL_cond *cond;
	} PacketQueue;

#define VIDEO_PICTURE_QUEUE_SIZE 3
#define SUBPICTURE_QUEUE_SIZE 16
#define SAMPLE_QUEUE_SIZE 9
#define FRAME_QUEUE_SIZE FFMAX(SAMPLE_QUEUE_SIZE, FFMAX(VIDEO_PICTURE_QUEUE_SIZE, SUBPICTURE_QUEUE_SIZE))

	// 音频参数
	typedef struct AudioParams {
		int freq;// 频率
		int channels;// 声道数
		int64_t channel_layout;// 声道设计，单声道，双声道还是立体声
		enum AVSampleFormat fmt;// 采样格式
		int frame_size;//  采样大小
		int bytes_per_sec;// 每秒多少字节
	} AudioParams;

	// 时钟
	typedef struct Clock {
		double pts;          // 时钟基准
		double pts_drift;     // 更新时钟的差值
		double last_updated;// 上一次更新的时间
		double speed;// 速度
		int serial;          // 时钟基于使用该序列的包
		bool paused;// 停止标志
		int *queue_serial;    // 指向当前数据包队列序列的指针，用于过时的时钟检测 
	} Clock;

	// 解码帧结构
	typedef struct Frame {
		AVFrame *frame; // 帧数据
		AVSubtitle sub;// 字幕
		int serial;// 序列
		double pts;           // 帧的显示时间戳 
		double duration;       // 帧显示时长
		int64_t pos;          // 文件中的位置 
		int width;// 帧的宽度
		int height;// 帧的高度
		int format;// 格式
		AVRational sar;// 额外参数
		int uploaded;// 上载
		int flip_v;// 翻转
	} Frame;

	// 解码后的帧队列
	typedef struct FrameQueue {
		Frame queue[FRAME_QUEUE_SIZE];// 队列数组
		int rindex;// 读索引
		int windex;// 写索引
		int size;// 大小
		int max_size;// 最大大小
		int keep_last;// 保持上一个
		int rindex_shown; // 读显示
		SDL_mutex *mutex;
		SDL_cond *cond;
		PacketQueue *pktq;
	} FrameQueue;

	// 视音频同步类型
	enum {
		AV_SYNC_AUDIO_MASTER, //视频同步音频  一般选择这个
		AV_SYNC_VIDEO_MASTER, //音频同步视频
		AV_SYNC_EXTERNAL_CLOCK, //通过外部时钟来同步
	};

	// 解码器结构
	typedef struct Decoder {
		AVPacket pkt; // 包
		PacketQueue *queue;// 包队列
		AVCodecContext *avctx;// 解码上下文
		int pkt_serial;// 包序列
		int finished;// 是否已经结束
		int packet_pending;// 是否有包在等待
		SDL_cond *empty_queue_cond;// 空队列条件变量
		int64_t start_pts;// 开始的时间戳
		AVRational start_pts_tb;// 开始的额外参数
		int64_t next_pts;// 下一帧时间戳
		AVRational next_pts_tb;// 下一帧的额外参数
		SDL_Thread *decoder_tid;// 解码线程
	} Decoder;

	// 视频状态结构
	typedef struct VideoState {
		SDL_Thread *read_tid; // 读取线程
		AVInputFormat *iformat;// 输入格式
		int abort_request;// 请求取消
		int force_refresh;// 强制刷新
		bool paused;
		bool last_paused;// 最后停止
		int queue_attachments_req; // 队列附件请求
		int seek_req;// 查找请求
		int seek_flags;// 查找标志
		int64_t seek_pos;// 查找位置
		int64_t seek_rel;
		int read_pause_return;// 读停止返回
		AVFormatContext *ic;// 解码格式上下文
		int realtime;// 是否实时码流

		Clock audclk;// 音频时钟
		Clock vidclk;// 视频时钟
		Clock extclk;// 外部时钟

		FrameQueue pictq;// 视频队列
		FrameQueue subpq;// 字幕队列
		FrameQueue sampq;// 音频队列

		Decoder auddec;// 音频解码器
		Decoder viddec;// 视频解码器
		Decoder subdec;// 字幕解码器

		int audio_stream;// 音频码流Id

		int av_sync_type; // 同步类型

		double audio_clock;// 音频时钟
		int audio_clock_serial;// 音频时钟序列
		double audio_diff_cum; // 用于音频差分计算 
		double audio_diff_avg_coef;
		double audio_diff_threshold;// 音频差分阈值
		int audio_diff_avg_count;// 平均差分数量
		AVStream *audio_st;// 音频码流
		PacketQueue audioq;// 音频包队列
		int audio_hw_buf_size; // 硬件缓冲大小
		uint8_t *audio_buf;// 音频缓冲区
		uint8_t *audio_buf1;// 音频缓冲区1
		unsigned int audio_buf_size;  // 音频缓冲大小  字节
		unsigned int audio_buf1_size; // 音频缓冲大小1
		int audio_buf_index; // 音频缓冲索引 字节
		int audio_write_buf_size;// 音频写入缓冲大小
		int audio_volume;// 音量
		bool muted; // 是否静音
		struct AudioParams audio_src; // 音频参数
		struct AudioParams audio_tgt;
		struct SwrContext *swr_ctx;// 音频转码上下文
		int frame_drops_early;
		int frame_drops_late;

		enum ShowMode {//显示类型
			SHOW_MODE_NONE = -1, 
			SHOW_MODE_VIDEO = 0,  // 显示视频
			SHOW_MODE_WAVES,  // 显示波浪，音频
			SHOW_MODE_RDFT, // 自适应滤波器
			SHOW_MODE_NB
		} show_mode;
		int16_t sample_array[SAMPLE_ARRAY_SIZE];// 采样数组
		int sample_array_index;// 采样索引
		int last_i_start;// 上一开始
		RDFTContext *rdft;// 自适应滤波器上下文
		int rdft_bits;// 自使用比特率
		FFTSample *rdft_data; // 快速傅里叶采样
		int xpos;
		double last_vis_time;

//不使用SDL_Texture
// 		SDL_Texture *sub_texture;
// 		SDL_Texture *vid_texture;

		int subtitle_stream;// 字幕码流Id
		AVStream *subtitle_st;// 字幕码流
		PacketQueue subtitleq;// 字幕包队列

		double frame_timer;// 帧计时器
		double frame_last_returned_time;// 上一次返回时间
		double frame_last_filter_delay;// 上一个过滤器延时
		int video_stream; // 视频码流Id
		AVStream *video_st;// 视频码流
		PacketQueue videoq;// 视频包队列
		double max_frame_duration;      // 最大帧显示时间
		struct SwsContext *img_convert_ctx; // 视频转码上下文
		struct SwsContext *sub_convert_ctx; // 字幕转码上下文
		int eof; // 结束标志

		char *filename;// 文件名
		char *filebuffer;
		unsigned int filebuffersize;
		int width, height, xleft, ytop; // 宽高，其实坐标
		int step;// 步进

		int last_video_stream, last_audio_stream, last_subtitle_stream; // 上一个视频码流Id、上一个音频码流Id、上一个字幕码流Id

		SDL_cond *continue_read_thread;// 连续读线程
	} VideoState;

	void print_error(const char *filename, int err);
}

#endif
