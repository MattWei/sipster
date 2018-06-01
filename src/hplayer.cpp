#include "hplayer.hpp"

#include <iostream>
#include <pjsua-lib/pjsua_internal.h>

#include "common.h"

#include "util.hpp"

using namespace std;

#define THIS_FILE		"hplayer.cpp"

#if 1
#   define TRACE_(x)	PJ_LOG(4,x)
#else
#   define TRACE_(x)
#endif

#define BYTES_PER_SAMPLE    2

#if defined(PJ_IS_BIG_ENDIAN) && PJ_IS_BIG_ENDIAN!=0
    static void samples_to_host(pj_int16_t *samples, unsigned count)
    {
	unsigned i;
	for (i=0; i<count; ++i) {
	    samples[i] = pj_swap16(samples[i]);
	}
    }
#else
#   define samples_to_host(samples,count)
#endif

#define SIGNATURE	    PJMEDIA_SIG_PORT_WAV_PLAYLIST

#if 0
#define LOGI(...) PJ_LOG(4,(__VA_ARGS__))
#define LOGD(...)  PJ_LOG(3,(__VA_ARGS__))
#define LOGE(...)  PJ_LOG(3,(__VA_ARGS__))
#endif
#define LOGI(X, Y, ...) printf(Y, __VA_ARGS__)
#define LOGD(X, Y, ...)  printf(Y, __VA_ARGS__)
#define LOGE(X, Y, ...)  printf(Y, __VA_ARGS__)

Music::Music(std::string path, pj_oshandle_t fd, pj_off_t size, unsigned startPos, pj_off_t pos)
	: path(path)
	, fd(fd)
	, fSize(size)
	, startData(startPos)
	, fPos(pos)
{

}

void Music::reset()
{
	fPos = startData;
	pj_file_setpos(fd, fPos, PJ_SEEK_SET);
}

bool Music::isStartPos()
{
	if (fPos == startData) {
		return true;
    }

	return false;
}

void Music::setPos(pj_uint32_t bytes)
{
	if (bytes < startData) {
		bytes = startData;
	}

	if (bytes > fSize) {
		bytes = fSize;
	}

	fPos = bytes; 
    pj_file_setpos(fd, fPos, PJ_SEEK_SET);
}

pj_uint32_t Music::getPos()
{
	return fPos;
}

/////////////////HPlayer//////////
struct playlist_port *HPlayer::createFileListPort(pj_pool_t *pool,
						   const pj_str_t *name)
{
    struct playlist_port *port = new playlist_port();
    memset(port, 0x00, sizeof(struct playlist_port));
    /*
    port = PJ_POOL_ZALLOC_T(pool, struct playlist_port);
    if (!port)
	    return NULL;
    */
    /* Put in default values.
     * These will be overriden once the file is read.
     */
    pjmedia_port_info_init(&port->base.info, name, SIGNATURE,
			   44100, 1, 16, 80);

    port->base.get_frame = &fileListGetFrame;
    port->base.on_destroy = &fileListOnDestroy;
    port->music = NULL;
    return port;
}

pj_status_t HPlayer::lastFileCallback()
{
    pj_status_t status;

	/* All files have been played. Call callback, if any. */
	if (playerPort->cb)
	{
		LOGI(THIS_FILE, "File port %.*s EOF, calling callback",
			(int)playerPort->base.info.name.slen,
			playerPort->base.info.name.ptr);
		    
		playerPort->eof = PJ_TRUE;

		status = (*playerPort->cb)(&playerPort->base,
				playerPort->base.port_data.pdata);

		if (status != PJ_SUCCESS)
		{
			/* This will crash if file port is destroyed in the
			* callback, that's why we set the eof flag before
			* calling the callback:
			playerPort->eof = PJ_TRUE;
			*/
			return status;
		}

		playerPort->eof = PJ_FALSE;
	}


    return PJ_SUCCESS;	
}

/*
 * Fill buffer for file_list operations.
 */
pj_status_t HPlayer::fileFillBuffer()
{
    pj_uint32_t size_left = playerPort->bufsize;
    pj_uint32_t size_to_read;
    pj_ssize_t size;
    pj_status_t status;

    PJSUA_LOCK();

    /* Can't read file if EOF and loop flag is disabled */
    if (playerPort->eof) {
		PJSUA_UNLOCK();

		return PJ_EEOF;
	}


    while (size_left > 0)
    {
		Music *currentMusic = playerPort->music;
        if (currentMusic == NULL) {
            if (size_left > 0) {
                pj_bzero(&playerPort->buf[playerPort->bufsize-size_left],
                         size_left);
            }
            PJSUA_UNLOCK();
            return PJ_SUCCESS;
        }

		if (currentMusic->isStartPos()) {
			int timeOnSeconds = currentMusic->fSize * 8 / afd->avg_bps;
			LOGI(THIS_FILE, "File %s start play length:%d", currentMusic->path.c_str(), timeOnSeconds);
			pushMessage(currentMusic->path, MessageType::START, timeOnSeconds);
		}
		            
	    /* Calculate how many bytes to read in this run. */
	    size = size_to_read = size_left;
	    status = pj_file_read(currentMusic->fd,
			      &playerPort->buf[playerPort->bufsize-size_left],
			      &size);
	    if (status != PJ_SUCCESS) {
			PJSUA_UNLOCK();
			return status;
		}
	        
	
	    if (size < 0)
	    {
	        /* Should return more appropriate error code here.. */
		    PJSUA_UNLOCK();
	        return PJ_ECANCELLED;
	    }
	
	    size_left -= (pj_uint32_t)size;
	    currentMusic->fPos += size;
	
	    /* If size is less than size_to_read, it indicates that we've
	    * encountered EOF. Rewind the file.
	    */
	    if (size < (pj_ssize_t)size_to_read)
	    {
	        /* Rewind the file for the next iteration */
			//currentMusic->reset();
		    /* Clear the remaining part of the buffer first, to prevent
		    * old samples from being played. If the playback restarts,
		    * this will be overwritten by new reading.
		    */
		    if (size_left > 0) {
		        pj_bzero(&playerPort->buf[playerPort->bufsize-size_left], 
			        size_left);
		    }

            /* Call back on one file end */
            LOGI(THIS_FILE, "File %s play to end..", currentMusic->path.c_str());
            pushMessage(currentMusic->path, MessageType::END, 0);

            delete playerPort->music;
            currentMusic = playerPort->music = NULL;
            size_left = 0;
	    } /* size < size_to_read */
        else {
            /* Call back on one file playing update */
            //LOGI(THIS_FILE, "File index(%d) play to %d", current_file, currentMusic->fPos);
			//if (currentMusic->fPos % (currentMusic->fSize / 1000) == 0) {
			pushMessage(currentMusic->path, MessageType::PLAYING, currentMusic->fPos * 1000.0 / currentMusic->fSize);
			//}
        }

    } /* while () */
    
	PJSUA_UNLOCK();

    /* Convert samples to host rep */
    samples_to_host((pj_int16_t*)playerPort->buf, playerPort->bufsize/BYTES_PER_SAMPLE);
    
    return PJ_SUCCESS;
}


/*
 * Get frame from file for file_list operation
 */
//static pj_status_t file_list_get_frame(pjmedia_port *this_port,
pj_status_t HPlayer::fileListGetFrame(pjmedia_port *this_port,
				       pjmedia_frame *frame)
{
    struct playlist_port *fport = (struct playlist_port*)this_port;
    HPlayer *player = (HPlayer*)this_port->port_data.pdata;

    pj_size_t frame_size;
    pj_status_t status;

    pj_assert(fport->base.info.signature == SIGNATURE);

    frame_size = frame->size;

    /* Copy frame from buffer. */
    frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
    frame->size = frame_size;
    frame->timestamp.u64 = 0;

    //循环缓冲区剩余数据大于一帧数据
    if (fport->readpos + frame_size <= fport->buf + fport->bufsize) {

	    /* Read contiguous buffer. */
	    pj_memcpy(frame->buf, fport->readpos, frame_size);

	    /* Fill up the buffer if all has been read. */
	    fport->readpos += frame_size;
	    if (fport->readpos == fport->buf + fport->bufsize) {
	        fport->readpos = fport->buf;

	        status = player->fileFillBuffer();
	        if (status != PJ_SUCCESS) {
		        frame->type = PJMEDIA_FRAME_TYPE_NONE;
		        frame->size = 0;
		        return status;
	        }
	    }
    } else {//循环缓冲区,先读后面未读的，再回缓冲区开始读。
        
	    unsigned endread;

	    /* Split read.
	    * First stage: read until end of buffer.
	    */
	    endread = (unsigned)((fport->buf+fport->bufsize) - fport->readpos);
	    pj_memcpy(frame->buf, fport->readpos, endread);

	    /* Second stage: fill up buffer, and read from the start of buffer. */
	    status = player->fileFillBuffer();
	    if (status != PJ_SUCCESS) {
	        pj_bzero(((char*)frame->buf)+endread, frame_size-endread);
	        return status;
	    }

	    pj_memcpy(((char*)frame->buf)+endread, fport->buf, frame_size-endread);
	    fport->readpos = fport->buf + (frame_size - endread);
    }

    return PJ_SUCCESS;
}

void HPlayer::resetPortMusic(playlist_port *fport)
{
    if (fport->music != NULL) {
        pj_file_close(fport->music->fd);
        delete fport->music;
        fport->music = NULL;
    }
}

/*
 * Destroy port.
 */
pj_status_t HPlayer::fileListOnDestroy(pjmedia_port *this_port)
{
    struct playlist_port *fport = (struct playlist_port*) this_port;
    int index;

    pj_assert(this_port->info.signature == SIGNATURE);

    if (fport->music != NULL) {
        pj_file_close(fport->music->fd);
        delete fport->music;
        fport->music = NULL;
    }
    return PJ_SUCCESS;
}



HPlayer::HPlayer()
    : playerId(PJSUA_INVALID_ID)
	, isRunning(false)
	//, messageHandleThread(NULL)
{

}

HPlayer::~HPlayer()
{
    PJSUA_LOCK();
	isRunning = false;
	pushMessage("System", MessageType::EXIT, 0);

	if (playerPort) {
		resetPortMusic(playerPort);
		free(playerPort);
		playerPort = NULL;
	}
	

	//if (messageHandleThread != NULL) {
	//	pj_thread_join(messageHandleThread);
	//}

    if (playerId != PJSUA_INVALID_ID) {
	    unregisterMediaPort();
	    pjsua_player_destroy(playerId);
    }
    PJSUA_UNLOCK();
}


bool HPlayer::checkAllFileExists(const StringVector &file_names)
{
	int index;
	const char *filename;	/* filename for open operations.    */

    /* Be sure all files exist	*/
    for (index = 0; index < file_names.size(); index++) {
    	/* Check the file really exists. */
        filename = file_names[index].c_str();
    	if (!pj_file_exists(filename)) {
	        LOGE(THIS_FILE,
		      "WAV playlist error: file '%s' not found",
	      	      filename);
			return false;
    	}
    }

	return true;
}


pj_status_t HPlayer::addFileToPlaylist(pj_pool_t *pool, const std::string &filename, bool needLock)
{

	pj_status_t status;

	/* Normalize ptime */
    if (ptime == 0)
	    ptime = 20;

    /* ok run this for all files to be sure all are good for playback. 
        check is wav 16bit*/
    //for (index = file_names.size() - 1; index >= 0; index--) {
	    pjmedia_wave_hdr wavehdr;
	    pj_ssize_t size_to_read, size_read;

	    /* we end with the last one so we are good to go if still in function*/
	    //pj_memcpy(filename, file_list[index].ptr, file_list[index].slen);
	    //filename[file_list[index].slen] = '\0';

	    /* Get the file size. */
	    //playerPort->current_file = index;
	    //playerPort->fsize_list[index] = pj_file_size(filename);
        //filename = file_names[index];
		int fSize = pj_file_size(filename.c_str());


	    /* Size must be more than WAVE header size */
	    if (fSize <= sizeof(pjmedia_wave_hdr)) {
	        status = PJMEDIA_ENOTVALIDWAVE;
			return status;
	        //goto on_error;
	    }
	
		pj_oshandle_t fd;
	    /* Open file. */
	    status = pj_file_open( pool, filename.c_str(), PJ_O_RDONLY, &fd);
	    if (status != PJ_SUCCESS) {
			return status;
			//goto on_error;
		}

	
	    /* Read the file header plus fmt header only. */
	    size_read = size_to_read = sizeof(wavehdr) - 8;
	    status = pj_file_read(fd, &wavehdr, &size_read);
	    if (status != PJ_SUCCESS) {
			return status;
	        //goto on_error;
	    }

	    if (size_read != size_to_read) {
	        status = PJMEDIA_ENOTVALIDWAVE;

			return status;
	        //goto on_error;
	    }
	
	    /* Normalize WAVE header fields values from little-endian to host
	    * byte order.
	    */
	    pjmedia_wave_hdr_file_to_host(&wavehdr);
	
	    /* Validate WAVE file. */
	    if (wavehdr.riff_hdr.riff != PJMEDIA_RIFF_TAG ||
	        wavehdr.riff_hdr.wave != PJMEDIA_WAVE_TAG ||
	        wavehdr.fmt_hdr.fmt != PJMEDIA_FMT_TAG)
	    {
	        TRACE_((THIS_FILE,
		        "actual value|expected riff=%x|%x, wave=%x|%x fmt=%x|%x",
		    wavehdr.riff_hdr.riff, PJMEDIA_RIFF_TAG,
		    wavehdr.riff_hdr.wave, PJMEDIA_WAVE_TAG,
		    wavehdr.fmt_hdr.fmt, PJMEDIA_FMT_TAG));
	        status = PJMEDIA_ENOTVALIDWAVE;

			return status;
	        //goto on_error;
	    }
	
	    /* Must be PCM with 16bits per sample */
	    if (wavehdr.fmt_hdr.fmt_tag != 1 ||
	        wavehdr.fmt_hdr.bits_per_sample != 16)
	    {
	        status = PJMEDIA_EWAVEUNSUPP;

			return status;
	        //goto on_error;
	    }
	
	    /* Block align must be 2*nchannels */
	    if (wavehdr.fmt_hdr.block_align != 
		    wavehdr.fmt_hdr.nchan * BYTES_PER_SAMPLE)
	    {
	        status = PJMEDIA_EWAVEUNSUPP;

			return status;
	        //goto on_error;
	    }
	
	    /* If length of fmt_header is greater than 16, skip the remaining
	    * fmt header data.
	    */
	    if (wavehdr.fmt_hdr.len > 16) {
	        size_to_read = wavehdr.fmt_hdr.len - 16;
	        status = pj_file_setpos(fd, size_to_read, 
				    PJ_SEEK_CUR);
	        if (status != PJ_SUCCESS) {
				return status;
		        //goto on_error;
	        }
	    }
	
	    /* Repeat reading the WAVE file until we have 'data' chunk */
	    for (;;) {
	        pjmedia_wave_subchunk subchunk;
	        size_read = 8;
	        status = pj_file_read(fd, &subchunk, &size_read);
	        if (status != PJ_SUCCESS || size_read != 8) {
		        status = PJMEDIA_EWAVETOOSHORT;

				return status;
		        //goto on_error;
	        }
	    
	        /* Normalize endianness */
	        PJMEDIA_WAVE_NORMALIZE_SUBCHUNK(&subchunk);
	    
	        /* Break if this is "data" chunk */
	        if (subchunk.id == PJMEDIA_DATA_TAG) {
		        wavehdr.data_hdr.data = PJMEDIA_DATA_TAG;
		        wavehdr.data_hdr.len = subchunk.len;
		        break;
	        }
	    
	        /* Otherwise skip the chunk contents */
	        size_to_read = subchunk.len;
	        status = pj_file_setpos(fd, size_to_read, PJ_SEEK_CUR);
	        if (status != PJ_SUCCESS) {
				return status;
		        //goto on_error;
	        }
	    }
	
	    /* Current file position now points to start of data */
		pj_off_t startPos = 0;
	    status = pj_file_getpos(fd, &startPos);
	    //playerPort->start_data_list[index] = (unsigned)pos;
	
	    /* Validate length. */
	    if (wavehdr.data_hdr.len != fSize - startPos) 
	    {
	        status = PJMEDIA_EWAVEUNSUPP;
			return status;
	        //goto on_error;
	    }
	    if (wavehdr.data_hdr.len < 400) {
	        status = PJMEDIA_EWAVETOOSHORT;
			return status;
	        //goto on_error;
	    }
	
	    /* It seems like we have a valid WAVE file. */
	
	    /* Update port info if we don't have one, otherwise check
	    * that the WAV file has the same attributes as previous files. 
	    */
	    if (!has_wave_info) {
	        afd->channel_count = wavehdr.fmt_hdr.nchan;
	        afd->clock_rate = wavehdr.fmt_hdr.sample_rate;
	        afd->bits_per_sample = wavehdr.fmt_hdr.bits_per_sample;
	        afd->frame_time_usec = ptime * 1000;
	        afd->avg_bps = afd->max_bps = afd->clock_rate *
					  afd->channel_count *
					  afd->bits_per_sample;

	        has_wave_info = PJ_TRUE;

            LOGI(THIS_FILE,
                 "WAV playlist afd: samp.rate=%d, ch=%d, bits_per_sample=%d",
                 afd->clock_rate,
                 afd->channel_count,
                 afd->bits_per_sample);

	    } else {

	        /* Check that this file has the same characteristics as the other
	        * files.
	        */
	        if (wavehdr.fmt_hdr.nchan != afd->channel_count ||
		        wavehdr.fmt_hdr.sample_rate != afd->clock_rate ||
		        wavehdr.fmt_hdr.bits_per_sample != afd->bits_per_sample)
	        {
		        /* This file has different characteristics than the other 
		        * files. 
		        */
		        LOGE(THIS_FILE,
		            "WAV playlist error: file '%s' has differrent number"
			        " of channels, sample rate, or bits per sample",
	      		    filename.c_str());
		        status = PJMEDIA_EWAVEUNSUPP;

				return status;
		        //goto on_error;
	        }
	    }
	
	
	    /* Set initial position of the file. */
	    int fPos = startPos;
		if (needLock) {
			PJSUA_LOCK();
		}

        resetPortMusic(playerPort);
		playerPort->music = new Music(filename, fd, fSize, startPos, fPos);
		if (needLock) {
			PJSUA_UNLOCK();
		}

    //} //Loop all file

	return PJ_SUCCESS;
}

/*
 * Create wave list player.
 */

PJ_DEF(pj_status_t) HPlayer::wavPlaylistCreate(pj_pool_t *pool,
						                        const pj_str_t *port_label,
                                                const std::string &fileName,
//unsigned options,
						                        pj_ssize_t buff_size,
						                        pjmedia_port **p_port)
{
	pj_status_t status;

    //pj_off_t pos;
    int index;

    pj_str_t tmp_port_label;
    //char filename[PJ_MAXPATH];	/* filename for open operations.    */

    /* Check arguments. */
    //PJ_ASSERT_RETURN(pool && file_names.size() && p_port, PJ_EINVAL);

    /* Normalize port_label */
    if (port_label == NULL || port_label->slen == 0) {
	    tmp_port_label = pj_str("WAV playlist");
	    port_label = &tmp_port_label;
    }

    /* Create fport instance. */
    playerPort = createFileListPort(pool, port_label);
    if (!playerPort) {
	    return PJ_ENOMEM;
    }

    afd = pjmedia_format_get_audio_format_detail(&playerPort->base.info.fmt, 1);
    has_wave_info = PJ_FALSE;
    /* start with the first file. */

    /* Create file buffer once for this operation.
     */
    if (buff_size < 1) 
		buff_size = PJMEDIA_FILE_PORT_BUFSIZE;
    
	playerPort->bufsize = (pj_uint32_t)buff_size;

    /* Create buffer. */
    playerPort->buf = (char*) pj_pool_alloc(pool, playerPort->bufsize);
    if (!playerPort->buf) {
	    return PJ_ENOMEM;
    }

    /* Initialize port */
    //playerPort->options = options;
    playerPort->readpos = playerPort->buf;

	status = addFileToPlaylist(pool, fileName);
    if (status != PJ_SUCCESS) {
	    goto on_error;
    }

    /* Fill up the buffer. */
    status = fileFillBuffer();
    if (status != PJ_SUCCESS) {
	    goto on_error;
    }
    
    /* Done. */
    
    *p_port = &playerPort->base;
    
    LOGI(THIS_FILE,
	     "WAV playlist '%.*s' created: samp.rate=%d, ch=%d, bufsize=%uKB",
	     (int)port_label->slen,
	     port_label->ptr,
	     afd->clock_rate,
	     afd->channel_count,
	     playerPort->bufsize / 1000);
    
    return PJ_SUCCESS;

on_error:
    resetPortMusic(playerPort);
    return status;
}


/*
 * Create a file playlist media port, and automatically add the port
 * to the conference bridge.
 */
PJ_DEF(pj_status_t) HPlayer::createListPlayer(const std::string &fileName,
					   const pj_str_t *label,
					   //unsigned options,
					   pjsua_player_id *p_id)
{
    unsigned slot, file_id;
    pj_pool_t *pool = NULL;
    pjmedia_port *port;
    pj_status_t status = PJ_SUCCESS;

    if (pjsua_var.player_cnt >= PJ_ARRAY_SIZE(pjsua_var.player))
		return PJ_ETOOMANY;

    //LOGI(THIS_FILE, "Creating playlist with %d file(s)..", file_names.size());
    pj_log_push_indent();

    PJSUA_LOCK();

    //Get Free player Id;
    for (file_id=0; file_id<PJ_ARRAY_SIZE(pjsua_var.player); ++file_id) {
	    if (pjsua_var.player[file_id].port == NULL)
	        break;
    }

    if (file_id == PJ_ARRAY_SIZE(pjsua_var.player)) {
	    /* This is unexpected */
	    pj_assert(0);
	    status = PJ_EBUG;
	    goto on_error;
    }

    LOGI(THIS_FILE, "file_id:%d, samples_per_frame:%d, clock_rate:%d",
            file_id, pjsua_var.mconf_cfg.samples_per_frame, pjsua_var.media_cfg.clock_rate);

    if (pjsua_var.media_cfg.clock_rate != 0) {
        ptime = pjsua_var.mconf_cfg.samples_per_frame * 1000 /
                pjsua_var.media_cfg.clock_rate;
    }

    if (ptime == 0) {
        ptime = 20;
    }

    //mMessageManager.setPlayingPacketInterval(ptime, 5);

    pool = pjsua_pool_create("playlist", 1000, 1000);
    if (!pool) {
	    status = PJ_ENOMEM;
	    goto on_error;
    }
/*
	status = initMsgQueue(pool);
	if (status != PJ_SUCCESS) {
		goto on_error;;
	}
*/
    status = wavPlaylistCreate(pool, label, fileName,
					 0, &port); //file_names, options,
    if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create playlist", status);
	    goto on_error;
    }

    //Get the bridge id?
    status = pjmedia_conf_add_port(pjsua_var.mconf, pool,
				   port, &port->info.name, &slot);
    if (status != PJ_SUCCESS) {
	    pjmedia_port_destroy(port);
	    pjsua_perror(THIS_FILE, "Unable to add port", status);
	    goto on_error;
    }

    pjsua_var.player[file_id].type = 1;
    pjsua_var.player[file_id].pool = pool;
    pjsua_var.player[file_id].port = port;
    pjsua_var.player[file_id].slot = slot;

    if (p_id) *p_id = file_id;

    ++pjsua_var.player_cnt;

    PJSUA_UNLOCK();

    PJ_LOG(4,(THIS_FILE, "Playlist created, id=%d, slot=%d", file_id, slot));

    pj_log_pop_indent();

    return PJ_SUCCESS;

on_error:
    PJSUA_UNLOCK();
    if (pool) pj_pool_release(pool);
    pj_log_pop_indent();

    return status;
}

#if 0
void *HPlayer::messageThread(void *param)
{
	pj_thread_desc desc;
	pj_thread_t *thisThread;
	pj_status_t status;

	status = pj_thread_register("HPlayerHandleMessageThread", desc, &thisThread);
	if (status != PJ_SUCCESS) {
		PJ_LOG(3, (THIS_FILE, "Create Handle message fail %d", status));
	} else {
		HPlayer *hPlayer = (HPlayer *)param;
		hPlayer->handleMessage();
	}

	PJ_LOG(3, (THIS_FILE, "Message handle exit"));

    return (void *)(0);
}

pj_status_t HPlayer::initMsgQueue(pj_pool_t *pool)
{
	//pj_status_t rc = pj_thread_create(pool, "message_thread", (pj_thread_proc*)&messageThread, this, 
	//								PJ_THREAD_DEFAULT_STACK_SIZE, 0, &messageHandleThread);
	//return rc;
}
#endif

bool HPlayer::pushMessage(std::string songPath, MessageType type, int param)
{

	//status_package_t packet;
	//memset(&packet, 0x00, sizeof(status_package_t));
	//packet.songPath = songPath;
	//packet.type = type;
	//packet.param = param;

    //mMessageManager.sendMessage(packet);
    //std::cout << "HPlayer::pushMessage, song:" << songPath << ", type:" << type  << ", param:" << param << std::endl;
	if (type == MessageType::START) {
		prePlayingStep = 0;
	} else if (type == MessageType::PLAYING) {
		if (prePlayingStep == param) {
			return true;
		} 

		prePlayingStep = param;
	} else if (type == MessageType::END) {
		prePlayingStep = 1000;
	}

    SETUP_EVENT(PLAYERSTATUS);

	ev.media = media;
    args->songPath = songPath;
    args->type = type;
	args->param = param;

    ENQUEUE_EVENT(ev);

	return true;
}

#if 0
 void HPlayer::handleMessage()
{
	PJ_LOG(3, (THIS_FILE, "Message handler start!"));
	isRunning = true;
    mPlayStatusCallBack->init();

	while (isRunning) {
        status_package_t packet = mMessageManager.receiveMessage();
        if (packet.type == MessageType::EXIT) {
            isRunning = false;
            mMessageManager.quit();
            break;
        }

        mPlayStatusCallBack->onStatusChanged(packet);
	}
}
#endif

pj_status_t HPlayer::createPlaylist(std::string fileName,
				      const string &label)
{
    //playOption = options;

    if (playerId != PJSUA_INVALID_ID) {
		//PJSUA2_RAISE_ERROR(PJ_EEXISTS);

        return PJ_FALSE;
    }

    pj_str_t pj_lbl = str2Pj(label);
    pj_status_t status;

    if (createListPlayer(fileName,
					     &pj_lbl,
					     //playOption.options,
					     &playerId) != PJ_SUCCESS) {
        return PJ_FALSE;
    }

    /* Register EOF callback */
    pjmedia_port *port;
    status = pjsua_player_get_port(playerId, &port);
    if (status != PJ_SUCCESS) {
	    pjsua_player_destroy(playerId);
		//PJSUA2_RAISE_ERROR2(status, "HPlayer::createPlaylist()");

        return PJ_FALSE;
    }
    status = pjmedia_wav_playlist_set_eof_cb(port, this, &eof_cb);
    if (status != PJ_SUCCESS) {
	    pjsua_player_destroy(playerId);
	    //PJSUA2_RAISE_ERROR2(status, "HPlayer::createPlaylist()");
        return PJ_FALSE;
    }

    /* Get media port id. */
    id = pjsua_player_get_conf_port(playerId);

    registerMediaPort(NULL);

    return PJ_SUCCESS;
}

AudioMediaPlayerInfo HPlayer::getInfo() const throw(Error)
{
    AudioMediaPlayerInfo info;
    pjmedia_wav_player_info pj_info;

    pj_bzero(&info, sizeof(info));
    if (pjsua_player_get_info(playerId, &pj_info) != PJ_SUCCESS) {
        return info;
    }

    info.formatId 		= pj_info.fmt_id;
    info.payloadBitsPerSample	= pj_info.payload_bits_per_sample;
    info.sizeBytes		= pj_info.size_bytes;
    info.sizeSamples		= pj_info.size_samples;

    return info;
}

pj_uint32_t HPlayer::getPos() const throw(Error)
{
    if (playerPort->music != NULL) {
        return playerPort->music->getPos();
    }

	return (pj_uint32_t)0;
}

pj_status_t HPlayer::setPos(pj_uint32_t samples)
{
    if (playerPort->music == NULL) {
        return PJ_EEOF;
    }

    //PJSUA2_CHECK_EXPR( pjsua_player_set_pos(playerId, samples) );
	Music *currentMusic = playerPort->music;
	currentMusic->setPos(samples);

	pj_status_t status;
    playerPort->eof = PJ_FALSE;
    status = fileFillBuffer();
    if (status != PJ_SUCCESS)
		return status;

    playerPort->readpos = playerPort->buf;

    return PJ_SUCCESS;
}

HPlayer* HPlayer::typecastFromAudioMedia(
						AudioMedia *media)
{
    return static_cast<HPlayer*>(media);
}

pj_status_t HPlayer::eof_cb(pjmedia_port *port,
                                     void *usr_data)
{
    PJ_UNUSED_ARG(port);
    HPlayer *player = (HPlayer*)usr_data;
    return player->onEof() ? PJ_SUCCESS : PJ_EEOF;
}

void HPlayer::play(std::string songPath)
{
	std::cout << "HPlayer play: " << songPath << std::endl;

	LOGI(THIS_FILE, "Play song %s\n", songPath.c_str());
	if (songPath.size() <= 0 || songPath == "") {
		return;
	}

    if (playerId == PJSUA_INVALID_ID) {
        createPlaylist(songPath, "");
    } else {
        PJSUA_LOCK();
        addFileToPlaylist(pjsua_var.player[playerId].pool, songPath, false);
        PJSUA_UNLOCK();
    }
}
