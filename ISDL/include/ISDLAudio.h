#ifndef SDL_SDK_H
#define SDL_SDK_H

#if defined(_WIN32)
#	if defined(ISDL_EXPORTS)
#		define ISDL_DLL __declspec(dllexport)
#	else
#		define ISDL_DLL __declspec(dllimport)
#	endif
#elif defined(__APPLE__)
#	define ISDL_DLL __attribute__((visibility("default")))
#else
#	define ISDL_DLL
#endif

#include <stdlib.h>
#include <string>
#include <functional>

namespace SDL {

	enum AUDIOSTATE 
	{
		PLAYING = 100,
		PAUSED,
		STOPPED,
		INITERROR
	};

	enum Format_InitFlags
	{
		Format_INIT_FLAC = 0x00000001,
		Format_INIT_MOD = 0x00000002,
		Format_INIT_MP3 = 0x00000008,
		Format_INIT_OGG = 0x00000010,
		Format_INIT_MID = 0x00000020
	};

	typedef void(*channleCallback)(int channel);

	class ISDL;

	class ISDL_DLL SimpleSDL
	{
	public:
		static ISDL* createInstance(int flags = Format_INIT_OGG | Format_INIT_MP3); //初始化想要播放的音频格式，传入Format_InitFlags
		static ISDL* getInstance();
		static void  release();
	};

	class ISDL
	{
	public:
		/*
		*如果使用了异步加载音频资源，不再需要的时候释放掉线程资源
		*/
		virtual void releaseThreadPool() = 0;

		/*
		*设置音效播放管道数，默认管道数量2
		*/
		virtual void allocateChannels(int nums) = 0;

		/*
		*加载音效，filePath，filename将会当做key值来管理音效
		*/
		virtual bool loadSound(const std::string &filePath) = 0;
		virtual bool loadSound(const std::string &filename, void *data, unsigned int size = 0) = 0;

		/*异步加载，非线程安全*/
		virtual void loadSoundAsynchronous(const std::string& filePath, std::function<void(bool)> callback) = 0;
		virtual void loadSoundAsynchronous(const std::string &filename, void *data, unsigned int size, std::function<void(bool)> callback) = 0;

		/*
		*播放音效
		*@param filename 要播放的文件，也会当做key
		*@param data 要播放的缓存，不为空的话优先通过缓存的方式加载音效
		*@param channel 通过哪个通道播放此音效，默认-1表示优先通过空闲通道进行播放
		*@param loop 是否循环播放
		*/
		virtual void playSound(const std::string &filename, void *data = nullptr, int channel = -1, bool loop = false) = 0;

		/*
		*获取状态
		*/
		virtual AUDIOSTATE getSoundState(int channel = -1) = 0;

		/*
		*暂停，恢复，停止播放
		*@param channel -1表示所有通道
		*/
		virtual void pauseSound(int channel = -1) = 0;
		virtual void resumeSound(int channel = -1) = 0;
		virtual void stopSound(int channel = -1) = 0;

		/*
		*释放音效资源
		*/
		virtual void unloadSound(const std::string &filename) = 0;
		virtual void unloadAllSound() = 0;

		/*
		*设置音量 0~128，可以根据channel或者filename单独设置某个音频或者某个通道的音量
		*@param channel -1表示所有通道
		*/
		virtual void setSoundVolume(int volume, int channel = -1) = 0;//0~128
		virtual void setSoundVolume(int volume, const std::string &filename) = 0;

		virtual int getSoundVolume() = 0;

		/*
		*音频播放结束回调,注意主动调用stop也会触发回调
		*/
		virtual void channelFinishedCallback(channleCallback cb) = 0;

		/********************************************************************/

		/*****************************music相关********************************/

		/*
		*加载音乐文件
		*/
		virtual bool loadMusic(const std::string &filePath) = 0;
		virtual bool loadMusic(const void *data, unsigned int size) = 0;

		/*异步加载，非线程安全*/
		virtual void loadMusicAsynchronous(const std::string& filePath, std::function<void(bool)> callback) = 0;
		virtual void loadMusicAsynchronous(const void *data, unsigned int size, std::function<void(bool)> callback) = 0;

		/*
		*播放音乐
		*@param times 播放次数
		*/
		virtual void playMusic(bool loop = true) = 0;
		virtual void playMusic(int times) = 0;

		/*
		*设置音乐音量 0~128
		*/
		virtual void setMusicVolume(int volume) = 0;

		virtual int getMusicVolume() = 0;

		/*
		*获取状态
		*/
		virtual AUDIOSTATE getMusicState() = 0;

		/*
		*重新播放，暂停，恢复，停止
		*/
		virtual void rewineMusic() = 0;
		virtual void pauseMusic() = 0;
		virtual void resumeMusic() = 0;
		virtual void stopMusic() = 0;

		/*
		*释放音乐资源
		*/
		virtual void unloadMusic() = 0;
	};
}

#endif
