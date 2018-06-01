#ifndef __H_PLAYER_H__
#define __H_PLAYER_H__

#include "pj/config_site.h"

#include <string>
#include <time.h>

#include "SIPSTERMedia.h"

/**
 * @file pjsua2/media.hpp
 * @brief PJSUA2 media operations
 */


#include <vector>

#include <condition_variable>

//#include "log.h"
//#include "message_manager.h"

#include <pjsua2.hpp>

using namespace pj;
using std::vector;

#define PJMEDIA_FILE_RANDOM 0x02

struct PlayOption
{
    unsigned int options;
    unsigned int songsInterval;
    unsigned int listInterval;
};

/*
class PlayStatusCallBack
{
public:
    virtual void init() = 0;
    virtual void onStatusChanged(status_package_t status) = 0;
};
*/

class Music 
{
public:
    pj_off_t fSize;
    unsigned startData;
    pj_off_t fPos;
    pj_oshandle_t fd;
    std::string path;

    Music(std::string path, pj_oshandle_t fd, pj_off_t size, unsigned startPos, pj_off_t pos);
    void reset();
    bool isStartPos();
    void setPos(pj_uint32_t samples);
    pj_uint32_t getPos();
};

struct playlist_port
{
    pjmedia_port     base;
    unsigned	     options;
    pj_bool_t	     eof;
    pj_uint32_t	     bufsize;
    char	    *buf;
    char	    *readpos;
    Music *music;

    pj_status_t	   (*cb)(pjmedia_port*, void*);
};


class HPlayer : public AudioMedia
{
public:
    /** 
     * Constructor.
     */
    HPlayer(); //PlayStatusCallBack *callBack

    /**
     * Create a file playlist media port, and automatically add the port
     * to the conference bridge.
     *
     * @param file_names  Array of file names to be added to the play list.
     *			  Note that the files must have the same clock rate,
     *			  number of channels, and number of bits per sample.
     * @param label	  Optional label to be set for the media port.
     * @param options	  Optional option flag. Application may specify
     *			  PJMEDIA_FILE_NO_LOOP to prevent looping.
     */
	pj_status_t createPlaylist(std::string fileName,
			const string &label); //, const StringVector &file_names,
			//struct PlayOption options) throw(Error);

    /**
     * Get additional info about the player. This operation is only valid
     * for player. For playlist, Error will be thrown.
     *
     * @return		the info.
     */
    AudioMediaPlayerInfo getInfo() const throw(Error);

    /**
     * Get current playback position in samples. This operation is not valid
     * for playlist.
     *
     * @return		   Current playback position, in samples.
     */
    pj_uint32_t getPos() const throw(Error);

    /**
     * Set playback position in samples
     *
     * @param samples	   The desired playback position, in samples.
     */
    pj_status_t setPos(pj_uint32_t samples);

    /**
     * Typecast from base class AudioMedia. This is useful for application
     * written in language that does not support downcasting such as Python.
     *
     * @param media		The object to be downcasted
     *
     * @return			The object as AudioMediaPlayer instance
     */
    static HPlayer* typecastFromAudioMedia(AudioMedia *media);

    /**
     * Destructor.
     */
    virtual ~HPlayer();

    void play(std::string songPath);
    pj_status_t addMusic(const StringVector &file_names);

    /**
     * Delete index music
     * Need to call stopTransmit() before if only one music on playlist and you want to delete it
     */
    pj_status_t deleteMusic(int index);

    pj_status_t changedPosition(int from, int to);

    void setRepeat(unsigned int type);

    SIPSTERMedia* media;
public:
    /*
     * Callbacks
     */

    /**
     * Register a callback to be called when the file player reading has
     * reached the end of file, or when the file reading has reached the
     * end of file of the last file for a playlist. If the file or playlist
     * is set to play repeatedly, then the callback will be called multiple
     * times.
     *
     * @return			If the callback returns false, the playback
     * 				will stop. Note that if application destroys
     * 				the player in the callback, it must return
     * 				false here.
     */
    virtual bool onEof()
    { return true; }


private:
    enum MessageType { EXIT, START, PLAYING, END };

    unsigned int ptime;

    //pj_thread_t *messageHandleThread;
    bool isRunning;
    
    //MessageManager mMessageManager;

    //PlayStatusCallBack *mPlayStatusCallBack;
    /**
     * Player Id.
     */
    int	playerId;
    //time_t intervalStartTime;
    int prePlayingStep;

    //struct PlayOption playOption;

    struct playlist_port *playerPort;
    pjmedia_audio_format_detail *afd;
    pj_bool_t has_wave_info;

    //pj_status_t initMsgQueue(pj_pool_t *pool);
    bool pushMessage(std::string songPath, MessageType type, int param);
    //void handleMessage();

    pj_status_t createListPlayer(const std::string &fileName,
					   const pj_str_t *label,
					   //unsigned options,
					   pjsua_player_id *p_id);

    pj_status_t wavPlaylistCreate(pj_pool_t *pool,
                                  const pj_str_t *port_label,
                                  const std::string &fileName,
						//unsigned options,
						pj_ssize_t buff_size,
						pjmedia_port **p_port);

    struct playlist_port *createFileListPort(pj_pool_t *pool,
						   const pj_str_t *name);

    bool checkAllFileExists(const StringVector &file_names);

    pj_status_t fileFillBuffer();
    pj_status_t lastFileCallback();
    pj_status_t addFileToPlaylist(pj_pool_t *pool, const std::string &file_name, bool needLock = false);

    static pj_status_t fileListGetFrame(pjmedia_port *this_port, pjmedia_frame *frame);
    static pj_status_t fileListOnDestroy(pjmedia_port *this_port);
    //static void *messageThread(void *param);

    /**
     *  Low level PJMEDIA callback
     */
    static pj_status_t eof_cb(pjmedia_port *port,
                              void *usr_data);

    void resetPortMusic(playlist_port *fport);
};

#endif