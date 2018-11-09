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
		static ISDL* createInstance(int flags = Format_INIT_OGG | Format_INIT_MP3); //��ʼ����Ҫ���ŵ���Ƶ��ʽ������Format_InitFlags
		static ISDL* getInstance();
		static void  release();
	};

	class ISDL
	{
	public:
		/*
		*���ʹ�����첽������Ƶ��Դ��������Ҫ��ʱ���ͷŵ��߳���Դ
		*/
		virtual void releaseThreadPool() = 0;

		/*
		*������Ч���Źܵ�����Ĭ�Ϲܵ�����2
		*/
		virtual void allocateChannels(int nums) = 0;

		/*
		*������Ч��filePath��filename���ᵱ��keyֵ��������Ч
		*/
		virtual bool loadSound(const std::string &filePath) = 0;
		virtual bool loadSound(const std::string &filename, void *data, unsigned int size = 0) = 0;

		/*�첽���أ����̰߳�ȫ*/
		virtual void loadSoundAsynchronous(const std::string& filePath, std::function<void(bool)> callback) = 0;
		virtual void loadSoundAsynchronous(const std::string &filename, void *data, unsigned int size, std::function<void(bool)> callback) = 0;

		/*
		*������Ч
		*@param filename Ҫ���ŵ��ļ���Ҳ�ᵱ��key
		*@param data Ҫ���ŵĻ��棬��Ϊ�յĻ�����ͨ������ķ�ʽ������Ч
		*@param channel ͨ���ĸ�ͨ�����Ŵ���Ч��Ĭ��-1��ʾ����ͨ������ͨ�����в���
		*@param loop �Ƿ�ѭ������
		*/
		virtual void playSound(const std::string &filename, void *data = nullptr, int channel = -1, bool loop = false) = 0;

		/*
		*��ȡ״̬
		*/
		virtual AUDIOSTATE getSoundState(int channel = -1) = 0;

		/*
		*��ͣ���ָ���ֹͣ����
		*@param channel -1��ʾ����ͨ��
		*/
		virtual void pauseSound(int channel = -1) = 0;
		virtual void resumeSound(int channel = -1) = 0;
		virtual void stopSound(int channel = -1) = 0;

		/*
		*�ͷ���Ч��Դ
		*/
		virtual void unloadSound(const std::string &filename) = 0;
		virtual void unloadAllSound() = 0;

		/*
		*�������� 0~128�����Ը���channel����filename��������ĳ����Ƶ����ĳ��ͨ��������
		*@param channel -1��ʾ����ͨ��
		*/
		virtual void setSoundVolume(int volume, int channel = -1) = 0;//0~128
		virtual void setSoundVolume(int volume, const std::string &filename) = 0;

		virtual int getSoundVolume() = 0;

		/*
		*��Ƶ���Ž����ص�,ע����������stopҲ�ᴥ���ص�
		*/
		virtual void channelFinishedCallback(channleCallback cb) = 0;

		/********************************************************************/

		/*****************************music���********************************/

		/*
		*���������ļ�
		*/
		virtual bool loadMusic(const std::string &filePath) = 0;
		virtual bool loadMusic(const void *data, unsigned int size) = 0;

		/*�첽���أ����̰߳�ȫ*/
		virtual void loadMusicAsynchronous(const std::string& filePath, std::function<void(bool)> callback) = 0;
		virtual void loadMusicAsynchronous(const void *data, unsigned int size, std::function<void(bool)> callback) = 0;

		/*
		*��������
		*@param times ���Ŵ���
		*/
		virtual void playMusic(bool loop = true) = 0;
		virtual void playMusic(int times) = 0;

		/*
		*������������ 0~128
		*/
		virtual void setMusicVolume(int volume) = 0;

		virtual int getMusicVolume() = 0;

		/*
		*��ȡ״̬
		*/
		virtual AUDIOSTATE getMusicState() = 0;

		/*
		*���²��ţ���ͣ���ָ���ֹͣ
		*/
		virtual void rewineMusic() = 0;
		virtual void pauseMusic() = 0;
		virtual void resumeMusic() = 0;
		virtual void stopMusic() = 0;

		/*
		*�ͷ�������Դ
		*/
		virtual void unloadMusic() = 0;
	};
}

#endif
