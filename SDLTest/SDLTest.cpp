// SDLTest.cpp : 定义控制台应用程序的入口点。
//
// #define _CRTDBG_MAP_ALLOC  
// #include <stdlib.h>  
// #include <crtdbg.h>  
/*#include "vld.h"*/
#include <stdio.h>
#include <mutex>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif
#include <iostream>

#include "ISDLAudio.h"
#include "IFFmpeg.h"

#pragma comment(lib, "ISDL.lib")
#pragma comment(lib, "ffmpeg.lib")

class Uncopyable {
protected:
	Uncopyable() {};
	virtual ~Uncopyable() {};

private:
	Uncopyable(const Uncopyable&);
	Uncopyable& operator=(const Uncopyable&);
};

class Lock :private Uncopyable {
public:
	explicit Lock(std::mutex* pm) :_mutexptr(pm)
	{
		_mutexptr->lock();
	}
	~Lock()
	{
		_mutexptr->unlock();
	}

private:
	std::mutex* _mutexptr;
};

class TTimer :private Uncopyable {
public:
	explicit TTimer(const std::string title)
	{
		struct timeval current;
		gettimeofday(&current, NULL);
		_cur_time = (long long)current.tv_sec * 1000 + current.tv_usec / 1000;;;
		_title = title;
	}
	~TTimer()
	{
		
	}

	double getTime()
	{
		struct timeval current;
		gettimeofday(&current, NULL);
		double _time_used = (long long)current.tv_sec * 1000 + current.tv_usec / 1000 - _cur_time;
		std::cout << _title;
		std::cout << " : ";
		std::cout << _time_used;
		std::cout << "ms" << std::endl;
		return _time_used;
	}

#ifdef _WIN32
	int gettimeofday(struct timeval * val, struct timezone *)
	{
		if (val)
		{
			LARGE_INTEGER liTime, liFreq;
			QueryPerformanceFrequency(&liFreq);
			QueryPerformanceCounter(&liTime);
			val->tv_sec = (long)(liTime.QuadPart / liFreq.QuadPart);
			val->tv_usec = (long)(liTime.QuadPart * 1000000.0 / liFreq.QuadPart - val->tv_sec * 1000000.0);
		}
		return 0;
	}
#endif
private:
	long long _cur_time;
	std::string _title;
};

/////////////////////////////////////////////////////////////////
static void channelcallback(int channel)
{
	printf("channel: %d finished", channel);
}

int music_test()
{
	FILE *fp = fopen("4.1.ogg", "rb");
	struct stat statBuf;
	auto descriptor = _fileno(fp);

	if (fstat(descriptor, &statBuf) == -1) {
		fclose(fp);
		return 0;
	}
	const size_t size = statBuf.st_size;
	char *buffer = new char[size];
	size_t readsize = fread(buffer, 1, size, fp);
	fclose(fp);

	SDL::ISDL* sdl = SDL::SimpleSDL::createInstance();
	sdl->setSoundVolume(128);
	sdl->setMusicVolume(128);
	//sdl->loadSound("4.1.ogg", buffer, readsize);
	//sdl->playSound("4.1.ogg");

	//Sleep(5000);
	//sdl->playSound("4.1.ogg");
	// 	sdl->loadMusic(buffer, readsize);
	// 	sdl->playMusic();
	//Sleep(6000);

	sdl->channelFinishedCallback(channelcallback);

	sdl->loadSoundAsynchronous("4.1.ogg", buffer, readsize, [sdl](bool success) {
		if (success)
		{
			sdl->playSound("4.1.ogg");
			Sleep(1000);
			sdl->pauseSound();
			Sleep(2000);
			sdl->resumeSound();
			Sleep(2000);
			sdl->stopSound();
		}
	});

	getchar();

	SDL::SimpleSDL::release();

	delete buffer;

	return 0;
}

char * loadbuffer(const char* filename, size_t *readsize)
{
	std::string tag(filename); 
	tag.append(" IO");
	TTimer t(tag);
	FILE *fp = fopen(filename, "rb");
	struct stat statBuf;
	auto descriptor = _fileno(fp);

	if (fstat(descriptor, &statBuf) == -1) {
		fclose(fp);
		return 0;
	}
	const size_t size = statBuf.st_size;
	char *buffer = new char[size];
	*readsize = fread(buffer, 1, size, fp);
	fclose(fp);
	t.getTime();
	return buffer;
}

double testLoad(const char* filename)
{
	double time;
	size_t readsize;
	char* buffer = loadbuffer(filename, &readsize);
	SDL::ISDL* sdl = SDL::SimpleSDL::getInstance();

	TTimer t(filename);
	sdl->loadSound(filename, buffer, readsize);
	time = t.getTime();
	//sdl->playSound(filename);
	delete buffer;

	return time;
}

int main()
{
dotest:
	SDL::ISDL* sdl = SDL::SimpleSDL::createInstance();
	sdl->setSoundVolume(128);
	sdl->setMusicVolume(128);
	double max = 0.0, min = 0.0;
	double total = 0;
	
	for (int i=1;i<=34;i++)
	{
		char filename[20];
		sprintf(filename, "%d.ogg", i);
		double time = testLoad(filename);
		if (time>max)
		{
			max = time;
		}
		if (time<min)
		{
			min = time;
		}
		if (min<=0)
		{
			min = time;
		}
		total += time;
		//break;
	}

	std::cout << "最大耗时";
	std::cout << " : ";
	std::cout << max;
	std::cout << "ms" << std::endl;

	std::cout << "最小耗时";
	std::cout << " : ";
	std::cout << min;
	std::cout << "ms" << std::endl;

	double pingjun = total / 34;
	std::cout << "平均耗时";
	std::cout << " : ";
	std::cout << pingjun;
	std::cout << "ms" << std::endl;

	getchar();
	goto dotest;
	return 0;
}

int main111()
{
	using namespace FFMPEG;

	//music_test();
	//getchar();
	//SimpleFFmpeg::createInstance();
	//SDL::ISDL* sdl = SDL::SimpleSDL::createInstance();
	//sdl->setSoundVolume(128);
	//sdl->setMusicVolume(128);
	//sdl->loadMusic("7.1.ogg");

	IFFMpeg* ff = SimpleFFmpeg::createInstance();
	FFMPEG::FFHandle id_1 = SimpleFFmpeg::newMediaPlayer();
	FFMPEG::FFHandle id_2 = SimpleFFmpeg::newMediaPlayer();
	ff->play(id_1, "WAIT.MP4");
	//ff->play(id_2, "14.mp4");

	bool pause = false;
	FFMPEG_EVENT ee = FF_NONE, ee2 = FF_NONE;
 	while (1)
 	{
		double time = ff->get_master_clock(id_1);
		printf("time:%d\n", (int)time);
		if (!pause)
		{
			Sleep(2000);
			ff->setSeek(id_1, 60.0);
			pause = true;
		}
		
		//SimpleFFmpeg::release();
		//break;
		//if (ee != FF_OVER)
			ee = ff->event_loop(id_1);
		
		
		//if (ee2 != FF_OVER)
		//	ee2 = ff->event_loop(id_2);
// 		if (!pause)
// 		{
// 			pause = true;
// 			Sleep(2000);
// 			ff->pause();
// 		}
		if (ee == FF_OVER)// && ee2 == FF_OVER)
		{
			Sleep(2000);
			ff->setSeek(id_1, -50.0);
		}
// 		uint8_t *data = ff->getData(); 
 	}
	printf("===========over=============");
	getchar();
//	_CrtDumpMemoryLeaks();
	 
	SimpleFFmpeg::release();
    return 0;

}

