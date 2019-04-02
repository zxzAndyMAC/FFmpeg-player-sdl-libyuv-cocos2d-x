
#ifndef FFMPEG_PQ_H
#define FFMPEG_PQ_H

#include "structs.h"
#include "MediaState.h"

namespace FFMPEG {
	class MediaState;
	class CPacketQueue
	{
	public:
		CPacketQueue(MediaState* ms);
		~CPacketQueue();

	public:
		int packet_queue_put(PacketQueue *q, AVPacket *pkt);//入队
		int packet_queue_put_nullpacket(PacketQueue *q, int stream_index);//入队空数据
		int packet_queue_init(PacketQueue *q);
		void packet_queue_flush(PacketQueue *q); //刷出包队列剩余的包
		void packet_queue_destroy(PacketQueue *q);
		void packet_mutex_destroy(PacketQueue *q);
		void packet_queue_abort(PacketQueue *q);
		void packet_queue_start(PacketQueue *q);
		int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial);//包数据出列

	private:
		int packet_queue_put_private(PacketQueue *q, AVPacket *pkt);

	private:
		MediaState* _mediaState;
	};
}

#endif
