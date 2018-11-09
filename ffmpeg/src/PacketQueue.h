
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
		int packet_queue_put(PacketQueue *q, AVPacket *pkt);//���
		int packet_queue_put_nullpacket(PacketQueue *q, int stream_index);//��ӿ�����
		int packet_queue_init(PacketQueue *q);
		void packet_queue_flush(PacketQueue *q); //ˢ��������ʣ��İ�
		void packet_queue_destroy(PacketQueue *q);
		void packet_queue_abort(PacketQueue *q);
		void packet_queue_start(PacketQueue *q);
		int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial);//�����ݳ���

	private:
		int packet_queue_put_private(PacketQueue *q, AVPacket *pkt);

	private:
		MediaState* _mediaState;
	};
}

#endif
