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
		FF_ERROR = 100,//错误，比如不存在的FFHandle
		FF_HOLD,//还未调用play
		FF_NONE,//无事件
		FF_PAUSE,//暂停
		FF_MUTE,//静音
		FF_OVER,//播放结束
		FF_QUIT,//退出
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

		virtual bool release(FFHandle id) = 0; //释放player

		virtual bool pause(FFHandle id) = 0; //暂停,如果不存在的id返回false
		virtual bool resume(FFHandle id) = 0; //恢复,如果不存在的id返回false
		virtual bool mute(FFHandle id) = 0; //静音,如果不存在的id返回false
		virtual bool unmute(FFHandle id) = 0;//取消静音,如果不存在的id返回false

		virtual bool play(FFHandle id, const char *filename) = 0; //通过文件播放,如果不存在的id返回false
		virtual bool play(FFHandle id, char* buffer, unsigned int size) = 0; //通过内存播放,如果不存在的id返回false

		virtual bool setSeek(FFHandle id, double seconds) = 0;//快进快退，秒为单位，快进为正数，快退为负数，seconds为需要快进或快退的秒数

		virtual unsigned int getWidth(FFHandle id) = 0;//如果不存在的id返回-1
		virtual unsigned int getHeight(FFHandle id) = 0;//如果不存在的id返回-1

		virtual int getDuration(FFHandle id) = 0; //获取视频时长,如果不存在的id返回-1
		virtual double get_master_clock(FFHandle id) = 0;//获取当前播放进度,如果不存在的id返回-1

		virtual FFMPEG_EVENT event_loop(FFHandle id) = 0; //播放事件循环，每帧调用
		virtual uint8_t *getData(FFHandle id) = 0;//获取视频图像每帧数据，用于自绘，如果不存在返回null
	};
}

#endif