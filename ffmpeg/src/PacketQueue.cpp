
#if defined(_FF_DEBUG_)
#include "vld.h"
#endif
#include "PacketQueue.h"

namespace FFMPEG {

	CPacketQueue::CPacketQueue(MediaState* ms)
	{
		_mediaState = ms;
	}

	CPacketQueue::~CPacketQueue()
	{}

	int CPacketQueue::packet_queue_put_private(PacketQueue *q, AVPacket *pkt)
	{
		MyAVPacketList *pkt1;

		if (q->abort_request)
			return -1;

		pkt1 = (MyAVPacketList *)av_malloc(sizeof(MyAVPacketList));
		if (!pkt1)
			return -1;
		pkt1->pkt = *pkt;
		pkt1->next = NULL;
		if (pkt == &_mediaState->m_flush_pkt)
			q->serial++;
		pkt1->serial = q->serial;

		if (!q->last_pkt)
			q->first_pkt = pkt1;
		else
			q->last_pkt->next = pkt1;
		q->last_pkt = pkt1;
		q->nb_packets++;
		q->size += pkt1->pkt.size + sizeof(*pkt1);
		q->duration += pkt1->pkt.duration;
		
		SDL_CondSignal(q->cond);
		return 0;
	}

	int CPacketQueue::packet_queue_put(PacketQueue *q, AVPacket *pkt)
	{
		int ret;

		SDL_LockMutex(q->mutex);
		ret = packet_queue_put_private(q, pkt);
		SDL_UnlockMutex(q->mutex);

		if (pkt != &_mediaState->m_flush_pkt && ret < 0)
			av_packet_unref(pkt);

		return ret;
	}

	int CPacketQueue::packet_queue_put_nullpacket(PacketQueue *q, int stream_index)
	{
		AVPacket pkt1, *pkt = &pkt1;
		av_init_packet(pkt);
		pkt->data = NULL;
		pkt->size = 0;
		pkt->stream_index = stream_index;
		return packet_queue_put(q, pkt);
	}

	int CPacketQueue::packet_queue_init(PacketQueue *q)
	{
		memset(q, 0, sizeof(PacketQueue));
		q->mutex = SDL_CreateMutex();
		if (!q->mutex) {
			av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
			return AVERROR(ENOMEM);
		}
		q->cond = SDL_CreateCond();
		if (!q->cond) {
			av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
			return AVERROR(ENOMEM);
		}
		q->abort_request = 1;
		return 0;
	}

	void CPacketQueue::packet_queue_flush(PacketQueue *q)
	{
		MyAVPacketList *pkt, *pkt1;

		SDL_LockMutex(q->mutex);
		for (pkt = q->first_pkt; pkt; pkt = pkt1) {
			pkt1 = pkt->next;
			av_packet_unref(&pkt->pkt);
			av_freep(pkt);
		}
		q->last_pkt = NULL;
		q->first_pkt = NULL;
		q->nb_packets = 0;
		q->size = 0;
		q->duration = 0;
		SDL_UnlockMutex(q->mutex);
	}

	void CPacketQueue::packet_queue_destroy(PacketQueue *q)
	{
		packet_queue_flush(q);
		
	}

	void CPacketQueue::packet_mutex_destroy(PacketQueue *q)
	{
		SDL_DestroyMutex(q->mutex);
		SDL_DestroyCond(q->cond);
	}

	void CPacketQueue::packet_queue_abort(PacketQueue *q)
	{
		SDL_LockMutex(q->mutex);

		q->abort_request = 1;

		SDL_CondSignal(q->cond);

		SDL_UnlockMutex(q->mutex);
	}

	void CPacketQueue::packet_queue_start(PacketQueue *q)
	{
		SDL_LockMutex(q->mutex);
		q->abort_request = 0;
		packet_queue_put_private(q, &_mediaState->m_flush_pkt);
		SDL_UnlockMutex(q->mutex);
	}

	int CPacketQueue::packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial)
	{
		MyAVPacketList *pkt1;
		SDL_LockMutex(q->mutex);

		int ret = -1;

		if (q->abort_request) {
			ret = -1;
			goto end;
		}

		pkt1 = q->first_pkt;
		if (pkt1) {
			q->first_pkt = pkt1->next;
			if (!q->first_pkt)
				q->last_pkt = NULL;
			q->nb_packets--;
			q->size -= pkt1->pkt.size + sizeof(*pkt1);
			q->duration -= pkt1->pkt.duration;
			*pkt = pkt1->pkt;
			if (serial)
				*serial = pkt1->serial;
			av_free(pkt1);
			ret = 1;
			goto end;
		}
		else if (!block) {
			ret = 0;
			goto end;
		}
		else {
			ret = -2;
			goto end;
		}

	end:
		SDL_UnlockMutex(q->mutex);
		return ret;
	}
}