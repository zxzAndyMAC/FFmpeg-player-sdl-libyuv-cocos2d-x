#ifndef FFMPEG_MS_H
#define FFMPEG_MS_H

#include "structs.h"
#include "PacketQueue.h"
#include "FrameQueue.h"
#include "IFFMpeg.h"

namespace FFMPEG {
	class CPacketQueue;
	class MediaState
	{
	public:
		MediaState(unsigned int id);
		~MediaState();
		
		void pause();
		void resume();
		void mute();
		void unmute();
		
		unsigned int getWidth();
		unsigned int getHeight();

		void play(const char *filename);
		void play(char* buffer, const unsigned int size);

		int getDuration();
		double get_master_clock();//��ȡ��ʱ��

		FFMPEG_EVENT event_loop();
		uint8_t *getData();

		FFMPEG_EVENT getEvent() { return _event; }

		void setSeek(double seconds);

		static void release();

	private:
		void do_exit();
		void stream_component_close(int stream_index);//�ر���
		void stream_close();//�ر�����
		double get_clock(Clock *c);//��ȡʱ��
		void set_clock(Clock *c, double pts, int serial);
		void set_clock_at(Clock *c, double pts, int serial, double time);
		void set_clock_speed(Clock *c, double speed);
		void init_clock(Clock *c, int *queue_serial);
		void sync_clock_to_slave(Clock *c, Clock *slave);//ͬ������ʱ��
		
		int get_master_sync_type();
		void check_external_clock_speed();//����ⲿʱ���ٶ�, �ⲿʱ��ͬ������Ƶ

		void stream_seek(int64_t pos, int64_t rel, int seek_by_bytes);//��������
		void stream_toggle_pause();//��ͣ/������Ƶ��

		void update_volume(int sign, double step);//��������
		void step_to_next_frame();//��֡����

		double compute_target_delay(double delay);//������ʱ
		double vp_duration(Frame *vp, Frame *nextvp);//������ʾʱ�� 
		void update_video_pts(double pts, int64_t pos, int serial);//������Ƶ��pts

		int queue_picture(AVFrame *src_frame, double pts, double duration, int64_t pos, int serial);//���Ѿ������֡ѹ���������Ƶ����
		int get_video_frame(AVFrame *frame);//��ȡ��Ƶ֡
		
		int decoder_start(Decoder *d, int(*fn)(void *), void *arg);//���������߳�
		int stream_component_open(int stream_index);//������

		int audio_open(void *opaque, int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, struct AudioParams *audio_hw_params);
	
		void toggle_pause();//��ͣ/������Ƶ��
		void toggle_mute();//����
		//////////////////////////////////
		AVDictionary *filter_codec_opts(AVDictionary *opts, enum AVCodecID codec_id,
			AVFormatContext *s, AVStream *st, AVCodec *codec);

		int check_stream_specifier(AVFormatContext *s, AVStream *st, const char *spec);

		AVDictionary **setup_find_stream_info_opts(AVFormatContext *s,
			AVDictionary *codec_opts);

		int refresh_loop_wait_event(SDL_Event *event);
		void video_refresh(double *remaining_time);
		void video_display();

		VideoState *stream_open(const char *filename, AVInputFormat *iformat);
		VideoState *stream_open(char *filebuffer, unsigned int filebuffersize);

		void video_image_display();

		bool init();

		void decoder_init(Decoder *d, AVCodecContext *avctx, PacketQueue *queue, SDL_cond *empty_queue_cond);//��������ʼ��
		int decoder_decode_frame(Decoder *d, AVFrame *frame, AVSubtitle *sub);//����������frame֡
		void decoder_destroy(Decoder *d);//���ٽ�����
		void decoder_abort(Decoder *d, FrameQueue *fq);//������ȡ������

		//��Ƶ���
		int audio_decode_frame();
		int synchronize_audio(int nb_samples);//ͬ����Ƶ

		//read thread
		
		int stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue);
		int is_realtime(AVFormatContext *s);
		static int read_thread(void *arg);


		int set_texture_buf(AVFrame *frame, struct SwsContext **img_convert_ctx);

		static int read_buffer(void *opaque, uint8_t *buf, int buf_size);
		static int decode_interrupt_cb(void *ctx);
		//�����߳�
		static int audio_thread(void *arg);
		static int video_thread(void *arg);
		static int subtitle_thread(void *arg);

		//��Ƶ�ص�
		static void sdl_audio_callback(void *opaque, Uint8 *stream, int len);

	public:
		bool show_status; //��ӡ״̬

		const char* wanted_stream_spec[AVMEDIA_TYPE_NB] = { 0 };
		AVPacket flush_pkt;
		int fast;
		int lowres;
		AVDictionary *codec_opts, *format_opts;
		int find_stream_info;
		int seek_by_bytes;
		int64_t start_time;

		bool audio_disable;
		bool video_disable;
		bool subtitle_disable;

		VideoState::ShowMode show_mode;

		int infinite_buffer;// ���޻�����
		int loop;//ѭ������
		int64_t duration;

		int startup_volume;
		int av_sync_type;//����Ƶͬ����ʽ
	private:
		VideoState *is;
		int decoder_reorder_pts;
		SDL_AudioDeviceID audio_dev;
		int framedrop;//����֡
		int64_t audio_callback_time;

		double remaining_time;

		CPacketQueue *_packetQueue;
		CFrameQueue *_frameQueue;
		double rdftspeed;

		AVFrame *displayFrame;
		uint8_t * _bgra;
		uint8_t * _rgba;
		SDL_mutex *vs_mutex;

		unsigned int _uniqueID;
		FFMPEG_EVENT _event;

		double _duration;
	};
}

#endif