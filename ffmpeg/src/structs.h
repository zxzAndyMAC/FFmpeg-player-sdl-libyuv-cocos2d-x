
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

	// ��С��Ƶ����
#define SDL_AUDIO_MIN_BUFFER_SIZE 512
	// ����ʵ����Ƶ�����С��������Ҫ̫Ƶ���ص����������õ��������Ƶ�ص�������ÿ��30��
#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30

	// ��Ƶ���� ��dbΪ��λ�Ĳ���
#define SDL_VOLUME_STEP (0.75)

	// ���ͬ����ֵ��������ڸ�ֵ������Ҫͬ��У��
#define AV_SYNC_THRESHOLD_MIN 0.04
	// ���ͬ����ֵ��������ڸ�ֵ������Ҫͬ��У��
#define AV_SYNC_THRESHOLD_MAX 0.1
	// ֡����ͬ����ֵ�����֡����ʱ��������������������ͬ��
#define AV_SYNC_FRAMEDUP_THRESHOLD 0.1
	// ͬ����ֵ��������̫���򲻽���У��
#define AV_NOSYNC_THRESHOLD 10.0

	// ��ȷͬ���������Ƶ�ٶȱ仯ֵ(�ٷֱ�)
#define SAMPLE_CORRECTION_PERCENT_MAX 10

	// ����ʵʱ�����Ļ��������ʱ�����ⲿʱ�ӵ���
#define EXTERNAL_CLOCK_SPEED_MIN  0.900
#define EXTERNAL_CLOCK_SPEED_MAX  1.010
#define EXTERNAL_CLOCK_SPEED_STEP 0.001

	// ʹ�ò�ֵ��ʵ��ƽ��ֵ
#define AUDIO_DIFF_AVG_NB   20

	// ˢ��Ƶ�� Ӧ��С�� 1/fps
#define REFRESH_RATE 0.01

	// ������С
#define SAMPLE_ARRAY_SIZE (8 * 65536)

#define CURSOR_HIDE_DELAY 1000000

#define USE_ONEPASS_SUBTITLE_RENDER 1


	// ���б�ṹ
	typedef struct MyAVPacketList {
		AVPacket pkt;
		struct MyAVPacketList *next;
		int serial;
	} MyAVPacketList;

	// �����������
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

	// ��Ƶ����
	typedef struct AudioParams {
		int freq;// Ƶ��
		int channels;// ������
		int64_t channel_layout;// ������ƣ���������˫��������������
		enum AVSampleFormat fmt;// ������ʽ
		int frame_size;//  ������С
		int bytes_per_sec;// ÿ������ֽ�
	} AudioParams;

	// ʱ��
	typedef struct Clock {
		double pts;          // ʱ�ӻ�׼
		double pts_drift;     // ����ʱ�ӵĲ�ֵ
		double last_updated;// ��һ�θ��µ�ʱ��
		double speed;// �ٶ�
		int serial;          // ʱ�ӻ���ʹ�ø����еİ�
		bool paused;// ֹͣ��־
		int *queue_serial;    // ָ��ǰ���ݰ��������е�ָ�룬���ڹ�ʱ��ʱ�Ӽ�� 
	} Clock;

	// ����֡�ṹ
	typedef struct Frame {
		AVFrame *frame; // ֡����
		AVSubtitle sub;// ��Ļ
		int serial;// ����
		double pts;           // ֡����ʾʱ��� 
		double duration;       // ֡��ʾʱ��
		int64_t pos;          // �ļ��е�λ�� 
		int width;// ֡�Ŀ��
		int height;// ֡�ĸ߶�
		int format;// ��ʽ
		AVRational sar;// �������
		int uploaded;// ����
		int flip_v;// ��ת
	} Frame;

	// ������֡����
	typedef struct FrameQueue {
		Frame queue[FRAME_QUEUE_SIZE];// ��������
		int rindex;// ������
		int windex;// д����
		int size;// ��С
		int max_size;// ����С
		int keep_last;// ������һ��
		int rindex_shown; // ����ʾ
		SDL_mutex *mutex;
		SDL_cond *cond;
		PacketQueue *pktq;
	} FrameQueue;

	// ����Ƶͬ������
	enum {
		AV_SYNC_AUDIO_MASTER, //��Ƶͬ����Ƶ  һ��ѡ�����
		AV_SYNC_VIDEO_MASTER, //��Ƶͬ����Ƶ
		AV_SYNC_EXTERNAL_CLOCK, //ͨ���ⲿʱ����ͬ��
	};

	// �������ṹ
	typedef struct Decoder {
		AVPacket pkt; // ��
		PacketQueue *queue;// ������
		AVCodecContext *avctx;// ����������
		int pkt_serial;// ������
		int finished;// �Ƿ��Ѿ�����
		int packet_pending;// �Ƿ��а��ڵȴ�
		SDL_cond *empty_queue_cond;// �ն�����������
		int64_t start_pts;// ��ʼ��ʱ���
		AVRational start_pts_tb;// ��ʼ�Ķ������
		int64_t next_pts;// ��һ֡ʱ���
		AVRational next_pts_tb;// ��һ֡�Ķ������
		SDL_Thread *decoder_tid;// �����߳�
	} Decoder;

	// ��Ƶ״̬�ṹ
	typedef struct VideoState {
		SDL_Thread *read_tid; // ��ȡ�߳�
		AVInputFormat *iformat;// �����ʽ
		int abort_request;// ����ȡ��
		int force_refresh;// ǿ��ˢ��
		bool paused;
		bool last_paused;// ���ֹͣ
		int queue_attachments_req; // ���и�������
		int seek_req;// ��������
		int seek_flags;// ���ұ�־
		int64_t seek_pos;// ����λ��
		int64_t seek_rel;
		int read_pause_return;// ��ֹͣ����
		AVFormatContext *ic;// �����ʽ������
		int realtime;// �Ƿ�ʵʱ����

		Clock audclk;// ��Ƶʱ��
		Clock vidclk;// ��Ƶʱ��
		Clock extclk;// �ⲿʱ��

		FrameQueue pictq;// ��Ƶ����
		FrameQueue subpq;// ��Ļ����
		FrameQueue sampq;// ��Ƶ����

		Decoder auddec;// ��Ƶ������
		Decoder viddec;// ��Ƶ������
		Decoder subdec;// ��Ļ������

		int audio_stream;// ��Ƶ����Id

		int av_sync_type; // ͬ������

		double audio_clock;// ��Ƶʱ��
		int audio_clock_serial;// ��Ƶʱ������
		double audio_diff_cum; // ������Ƶ��ּ��� 
		double audio_diff_avg_coef;
		double audio_diff_threshold;// ��Ƶ�����ֵ
		int audio_diff_avg_count;// ƽ���������
		AVStream *audio_st;// ��Ƶ����
		PacketQueue audioq;// ��Ƶ������
		int audio_hw_buf_size; // Ӳ�������С
		uint8_t *audio_buf;// ��Ƶ������
		uint8_t *audio_buf1;// ��Ƶ������1
		unsigned int audio_buf_size;  // ��Ƶ�����С  �ֽ�
		unsigned int audio_buf1_size; // ��Ƶ�����С1
		int audio_buf_index; // ��Ƶ�������� �ֽ�
		int audio_write_buf_size;// ��Ƶд�뻺���С
		int audio_volume;// ����
		bool muted; // �Ƿ���
		struct AudioParams audio_src; // ��Ƶ����
		struct AudioParams audio_tgt;
		struct SwrContext *swr_ctx;// ��Ƶת��������
		int frame_drops_early;
		int frame_drops_late;

		enum ShowMode {//��ʾ����
			SHOW_MODE_NONE = -1, 
			SHOW_MODE_VIDEO = 0,  // ��ʾ��Ƶ
			SHOW_MODE_WAVES,  // ��ʾ���ˣ���Ƶ
			SHOW_MODE_RDFT, // ����Ӧ�˲���
			SHOW_MODE_NB
		} show_mode;
		int16_t sample_array[SAMPLE_ARRAY_SIZE];// ��������
		int sample_array_index;// ��������
		int last_i_start;// ��һ��ʼ
		RDFTContext *rdft;// ����Ӧ�˲���������
		int rdft_bits;// ��ʹ�ñ�����
		FFTSample *rdft_data; // ���ٸ���Ҷ����
		int xpos;
		double last_vis_time;

//��ʹ��SDL_Texture
// 		SDL_Texture *sub_texture;
// 		SDL_Texture *vid_texture;

		int subtitle_stream;// ��Ļ����Id
		AVStream *subtitle_st;// ��Ļ����
		PacketQueue subtitleq;// ��Ļ������

		double frame_timer;// ֡��ʱ��
		double frame_last_returned_time;// ��һ�η���ʱ��
		double frame_last_filter_delay;// ��һ����������ʱ
		int video_stream; // ��Ƶ����Id
		AVStream *video_st;// ��Ƶ����
		PacketQueue videoq;// ��Ƶ������
		double max_frame_duration;      // ���֡��ʾʱ��
		struct SwsContext *img_convert_ctx; // ��Ƶת��������
		struct SwsContext *sub_convert_ctx; // ��Ļת��������
		int eof; // ������־

		char *filename;// �ļ���
		char *filebuffer;
		unsigned int filebuffersize;
		int width, height, xleft, ytop; // ��ߣ���ʵ����
		int step;// ����

		int last_video_stream, last_audio_stream, last_subtitle_stream; // ��һ����Ƶ����Id����һ����Ƶ����Id����һ����Ļ����Id

		SDL_cond *continue_read_thread;// �������߳�
	} VideoState;

	void print_error(const char *filename, int err);
}

#endif
