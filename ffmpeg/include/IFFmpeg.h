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

		static void		release();
	};

	class IFFMpeg
	{
	public:
		virtual ~IFFMpeg(){}
		
		virtual FFHandle		create( const char *filename) = 0; //通过文件播放,如果不存在的id返回false

		virtual FFHandle		create( char* buffer, unsigned int size) = 0; //通过内存播放,如果不存在的id返回false

		/*音视频不同步问题
		具体原因是CPU在处理视频帧的时候处理得太慢，默认的音视频同步方案是视频同步到音频, 导致了音频播放过快，视频跟不上。
		framedrop 控制着允许丢帧的范围。可以通过修改 framedrop 的数值来解决不同步的问题，framedrop 是在视频帧处理不过来的时候丢弃一些帧达到同步的效果。
		*/
		virtual bool setFramedrop(FFHandle id, int fd) = 0;

		virtual bool release(FFHandle id) = 0; //释放player

		virtual bool pause(FFHandle id) = 0; //暂停,如果不存在的id返回false

		virtual bool resume(FFHandle id) = 0; //恢复,如果不存在的id返回false

		virtual bool mute(FFHandle id) = 0; //静音,如果不存在的id返回false

		virtual bool unmute(FFHandle id) = 0;//取消静音,如果不存在的id返回false

		virtual bool			play( FFHandle id, int loopCnt = 1 ) = 0;//播放视频

		virtual bool setSeek(FFHandle id, double seconds) = 0;//快进快退，秒为单位，快进为正数，快退为负数，seconds为需要快进或快退的秒数

		virtual unsigned int getWidth(FFHandle id) = 0;//如果不存在的id返回-1

		virtual unsigned int getHeight(FFHandle id) = 0;//如果不存在的id返回-1

		virtual int getDuration(FFHandle id) = 0; //获取视频时长,如果不存在的id返回-1

		virtual double get_master_clock(FFHandle id) = 0;//获取当前播放进度,如果不存在的id返回-1

		virtual void			event_loop() = 0; //播放事件循环，每帧调用
			
		virtual FFMPEG_EVENT	getEvent( FFHandle id ) = 0;
		
		virtual uint8_t*		getData( FFHandle id, unsigned int& width, unsigned int& height ) = 0;//获取视频图像每帧数据，用于自绘，如果不存在返回null

		virtual uint8_t*		getThumbnail( FFHandle id, unsigned int& width, unsigned int& height ) = 0; //获取缩略图
	};
}

#endif