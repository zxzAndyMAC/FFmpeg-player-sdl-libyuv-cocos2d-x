#ifndef FFMPEG_SDK_H
#define FFMPEG_SDK_H

#if defined(_WIN32)
#	if defined(IFF_EXPORTS)
#		define IFF_DLL __declspec(dllexport)
#	else
#		define IFF_DLL __declspec(dllimport)
#	endif
#elif defined(__APPLE__)
#	define IFF_DLL __attribute__((visibility("default")))
#else
#	define IFF_DLL
#endif

namespace FFMPEG {

	typedef unsigned int FFHandle;

	typedef enum FFMPEG_EVENT
	{
		FF_ERROR = 100,//���󣬱��粻���ڵ�FFHandle
		FF_HOLD,//��δ����play
		FF_NONE,//���¼�
		FF_PAUSE,//��ͣ
		FF_MUTE,//����
		FF_OVER,//���Ž���
		FF_QUIT,//�˳�
		FF_PLAY
	}FFMPEG_EVENT;

	class IFFMpeg;
	class IFF_DLL SimpleFFmpeg
	{
	public:
		static IFFMpeg* createInstance();
		static IFFMpeg* getInstance();

		static FFHandle newMediaPlayer();

		static void  release();
	};

	class IFFMpeg
	{
	public:
		virtual ~IFFMpeg() {}

		virtual bool release(FFHandle id) = 0; //�ͷ�player

		virtual bool pause(FFHandle id) = 0; //��ͣ,��������ڵ�id����false
		virtual bool resume(FFHandle id) = 0; //�ָ�,��������ڵ�id����false
		virtual bool mute(FFHandle id) = 0; //����,��������ڵ�id����false
		virtual bool unmute(FFHandle id) = 0;//ȡ������,��������ڵ�id����false

		virtual bool play(FFHandle id, const char *filename) = 0; //ͨ���ļ�����,��������ڵ�id����false
		virtual bool play(FFHandle id, char* buffer, unsigned int size) = 0; //ͨ���ڴ沥��,��������ڵ�id����false

		virtual bool setSeek(FFHandle id, double seconds) = 0;//������ˣ���Ϊ��λ�����Ϊ����������Ϊ������secondsΪ��Ҫ�������˵�����

		virtual unsigned int getWidth(FFHandle id) = 0;//��������ڵ�id����-1
		virtual unsigned int getHeight(FFHandle id) = 0;//��������ڵ�id����-1

		virtual int getDuration(FFHandle id) = 0; //��ȡ��Ƶʱ��,��������ڵ�id����-1
		virtual double get_master_clock(FFHandle id) = 0;//��ȡ��ǰ���Ž���,��������ڵ�id����-1

		virtual FFMPEG_EVENT event_loop(FFHandle id) = 0; //�����¼�ѭ����ÿ֡����
		virtual uint8_t *getData(FFHandle id) = 0;//��ȡ��Ƶͼ��ÿ֡���ݣ������Ի棬��������ڷ���null
	};
}

#endif