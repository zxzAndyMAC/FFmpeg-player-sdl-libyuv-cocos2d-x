#ifndef FFMPEG_MS_H
#define FFMPEG_MS_H

#include <queue>
#include "structs.h"
#include "PacketQueue.h"
#include "FrameQueue.h"
#include "IFFMpeg.h"

namespace FFMPEG {

	typedef struct FFEvent{
		int		type;
		void	*data;

		FFEvent(int t, void* d):type(t),data(d){}

		~FFEvent() {
			if (data)
			{
				delete data;
			}
		}
	}FFEvent;

	class CPacketQueue;
	class MediaState
	{
	public:
		MediaState( unsigned int id );
		~MediaState();
		
		void pause();
		void resume();
		void mute();
		void unmute();
		
		unsigned int getWidth();
		unsigned int getHeight();

		void create(const char *filename);
		void create(char* buffer, const int64_t size);
		void play( unsigned int loop_times );

		uint8_t *getThumbnail();

		int getDuration();
		double get_master_clock();//��ȡ��ʱ��

		FFMPEG_EVENT event_loop();
		uint8_t *getData();

		FFMPEG_EVENT getEvent() { return _event; }

		void setSeek(double seconds);

		void setFramedrop(int fd) { _framedrop = fd; }

		//��־�ص������������̰߳�ȫ��
		void ff_log_set_callback(void(*callback)(void*, int, const char*, va_list));

		//static void release();

	private:
		void pushEvent(int type, void* data);
		void do_exit();
		void init_vs();
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
		
		void decoder_start(Decoder *d);//���������߳�
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

		void refresh_loop_wait_event();
		void video_refresh(double *remaining_time);
		void video_display();

		VideoState *stream_open(const char *filename, AVInputFormat *iformat);
		VideoState *stream_open(char *filebuffer, int64_t filebuffersize);

		void video_image_display();

		bool init();

		void decoder_init(Decoder *d, AVCodecContext *avctx, PacketQueue *queue);//��������ʼ��
		int decoder_decode_frame(Decoder *d, AVFrame *frame, AVSubtitle *sub);//����������frame֡
		void decoder_destroy(Decoder *d);//���ٽ�����
		void decoder_abort(Decoder *d, FrameQueue *fq);//������ȡ������

		//��Ƶ���
		int audio_decode_frame();
		int synchronize_audio(int nb_samples);//ͬ����Ƶ

		//read thread
		
		int stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue);
		int is_realtime(AVFormatContext *s);

		bool init_media();
		void frame_loop();


		int set_texture_buf(AVFrame *frame, struct SwsContext **img_convert_ctx);

		static int read_buffer(void *opaque, uint8_t *buf, int buf_size);
		static int64_t seek_buffer(void *opaque, int64_t offset, int whence);
		static int decode_interrupt_cb(void *ctx);
		
		//����ѭ��
		void audio_loop();
		void video_loop();
		void subtitle_loop();

		//��Ƶ�ص�
		static void sdl_audio_callback(void *opaque, Uint8 *stream, int len);

	public:
		bool m_show_status; //��ӡ״̬

		const char		*m_wanted_stream_spec[AVMEDIA_TYPE_NB] = { 0 };
		AVPacket		m_flush_pkt, m_read_pkt;
		int				m_fast;
		int				m_lowres;
		AVDictionary	*m_codec_opts, *m_format_opts;
		int				m_find_stream_info;
		int				m_seek_by_bytes;
		int64_t			m_start_time;

		bool			m_audio_disable;
		bool			m_video_disable;
		bool			m_subtitle_disable;

		VideoState::ShowMode m_show_mode;

		int				m_infinite_buffer;// ���޻�����
		int				m_loop;//ѭ������
		int64_t			m_duration;

		int				m_startup_volume;
		int				m_av_sync_type;//����Ƶͬ����ʽ
	private:
		VideoState			*_vs;
		int					_decoder_reorder_pts;
		SDL_AudioDeviceID	_audio_dev;
		int					_framedrop;//����֡
		int64_t				_audio_callback_time;
		double				_remaining_time;

		CPacketQueue		*_packetQueue;
		CFrameQueue			*_frameQueue;
		double				_rdftspeed;
		AVRational			_tb;
		AVRational			_video_frame_rate;

		AVFrame				*_displayFrame;
		AVFrame				*_audio_frame;
		AVFrame				*_video_frame;
		Frame				*_audio_af;
		Frame				*_subtitle_sp;
		uint8_t				*_bgra;
		uint8_t				*_rgba;

		unsigned int		_uniqueID;
		FFMPEG_EVENT		_event;

		double				_duration;

		bool				_audio_loop_end, _video_loop_end, _subtitle_loop_end;

		bool				_play;
		std::queue<FFEvent*>_event_queue;
	};
}

#endif