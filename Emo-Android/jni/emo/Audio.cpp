#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <squirrel.h>

#include "Constants.h"
#include "Engine.h"
#include "Runtime.h"
#include "Audio.h"
#include "VmFunc.h"

extern emo::Engine* engine;

void initAudioFunctions() {
    registerClass(engine->sqvm, EMO_AUDIO_CLASS);

    registerClassFunc(engine->sqvm, EMO_AUDIO_CLASS,    "constructor",    emoCreateAudioEngine);
    registerClassFunc(engine->sqvm, EMO_AUDIO_CLASS,    "load",           emoLoadAudio);
    registerClassFunc(engine->sqvm, EMO_AUDIO_CLASS,    "play",           emoPlayAudioChannel);
    registerClassFunc(engine->sqvm, EMO_AUDIO_CLASS,    "pause",          emoPauseAudioChannel);
    registerClassFunc(engine->sqvm, EMO_AUDIO_CLASS,    "stop",           emoStopAudioChannel);
    registerClassFunc(engine->sqvm, EMO_AUDIO_CLASS,    "seek",           emoSeekAudioChannel);
    registerClassFunc(engine->sqvm, EMO_AUDIO_CLASS,    "getChannelCount",emoGetAudioChannelCount);

    registerClassFunc(engine->sqvm, EMO_AUDIO_CLASS,    "getVolume",      emoGetAudioChannelVolume);
    registerClassFunc(engine->sqvm, EMO_AUDIO_CLASS,    "setVolume",      emoSetAudioChannelVolume);
    registerClassFunc(engine->sqvm, EMO_AUDIO_CLASS,    "getMaxVolume",   emoGetAudioChannelMaxVolume);
    registerClassFunc(engine->sqvm, EMO_AUDIO_CLASS,    "getMinVolume",   emoGetAudioChannelMinVolume);

    registerClassFunc(engine->sqvm, EMO_AUDIO_CLASS,    "setLoop",        emoSetAudioChannelLooping);
    registerClassFunc(engine->sqvm, EMO_AUDIO_CLASS,    "isLoop",         emoGetAudioChannelLooping);
    registerClassFunc(engine->sqvm, EMO_AUDIO_CLASS,    "getState",       emoGetAudioChannelState);

    registerClassFunc(engine->sqvm, EMO_AUDIO_CLASS,    "close",          emoCloseAudioChannel);
    registerClassFunc(engine->sqvm, EMO_AUDIO_CLASS,    "closeEngine",    emoCloseAudioEngine);

}

static SLresult checkOpenSLresult(const char* message, SLresult result) {
    if (SL_RESULT_SUCCESS != result) {
        LOGE(message);
    }
    return result;
}

namespace emo {

    Audio::Audio() {
        this->running = false;
        this->engineObject    = NULL;
        this->outputMixObject = NULL;
        this->engineEngine    = NULL;
    }

    Audio::~Audio() {

    }

    bool Audio::create(int channelCount) {
        if (this->running) {
            engine->setLastError(ERR_AUDIO_ENGINE_CREATED);
            LOGE("emo_audio: audio engine is already created.");
            return false;
        }

        this->channelCount = channelCount;

        SLresult result;
        this->channels = (Channel *)malloc(sizeof(Channel) * channelCount);

        for (int i = 0; i < channelCount; i++) {
            channels[i].loaded = SL_BOOLEAN_FALSE;
        }

        // create engine
        const SLInterfaceID engine_ids[] = {SL_IID_ENGINE};
        const SLboolean engine_req[] = {SL_BOOLEAN_TRUE};
        result = slCreateEngine(&this->engineObject, 0, NULL, 1, engine_ids, engine_req);
        if (SL_RESULT_SUCCESS != result) {
            engine->setLastError(ERR_AUDIO_ENGINE_INIT);
            LOGE("emo_audio: failed to create audio engine");
            return false;
        }

        // realize the engine
        result = (*this->engineObject)->Realize(this->engineObject, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) {
            engine->setLastError(ERR_AUDIO_ENGINE_INIT);
            LOGE("emo_audio: failed to realize audio engine");
            return false;
        }

        // get the engine interface, which is needed in order to create other objects
        result = (*this->engineObject)->GetInterface(this->engineObject, SL_IID_ENGINE, &this->engineEngine);
        if (SL_RESULT_SUCCESS != result) {
            engine->setLastError(ERR_AUDIO_ENGINE_INIT);
            LOGE("emo_audio: failed to get audio engine interface");
            return false;
        }

        // create output mix
        const SLInterfaceID mix_ids[1] = {SL_IID_VOLUME};
        const SLboolean mix_req[1] = {SL_BOOLEAN_TRUE};
        result = (*this->engineEngine)->CreateOutputMix(this->engineEngine, &this->outputMixObject, 0, mix_ids, mix_req);
        if (SL_RESULT_SUCCESS != result) {
            engine->setLastError(ERR_AUDIO_ENGINE_INIT);
            LOGE("emo_audio: failed to create output mix object");
            return false;
        }

        // realize the output mix
        result = (*this->outputMixObject)->Realize(this->outputMixObject, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) {
            engine->setLastError(ERR_AUDIO_ENGINE_INIT);
            LOGE("emo_audio: failed to realize output mix object");
            return false;
        }

        this->running = true;
        return true;
    }

    bool Audio::createChannelFromAsset(const char* fname, int index) {
        SLresult result;

        Channel* channel = &this->channels[index];

        if (channel->loaded == SL_BOOLEAN_TRUE) {
            this->closeChannel(index);
        }

        AAssetManager* mgr = engine->app->activity->assetManager;
        if (mgr == NULL) {
            engine->setLastError(ERR_ASSET_LOAD);
                LOGE("emo_audio: failed to load AAssetManager");
            return false;
        }

        AAsset* asset = AAssetManager_open(mgr, fname, AASSET_MODE_UNKNOWN);
        if (asset == NULL) {
            engine->setLastError(ERR_ASSET_OPEN);
            LOGE("emo_audio: failed to open an audio file");
            LOGE(fname);
            return false;
        }

        // open asset as file descriptor
        off_t start, length;
        int fd = AAsset_openFileDescriptor(asset, &start, &length);
        if (fd < 0) {
            engine->setLastError(ERR_ASSET_OPEN);
            LOGE("emo_audio: failed to open an audio file");
            LOGE(fname);
            return false;
        }
        AAsset_close(asset);

        // configure audio source
        SLDataLocator_AndroidFD loc_fd = {SL_DATALOCATOR_ANDROIDFD, fd, start, length};
        SLDataFormat_MIME format_mime = {SL_DATAFORMAT_MIME, NULL, SL_CONTAINERTYPE_UNSPECIFIED};
        SLDataSource audioSrc = {&loc_fd, &format_mime};

        // configure audio sink
        SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, this->outputMixObject};
        SLDataSink audioSnk = {&loc_outmix, NULL};

        // create audio player
        const SLInterfaceID player_ids[3] = {SL_IID_PLAY, SL_IID_VOLUME, SL_IID_SEEK};
        const SLboolean player_req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
        result = (*engineEngine)->CreateAudioPlayer(this->engineEngine, &channel->playerObject, &audioSrc, &audioSnk,
                3, player_ids, player_req);
        if (SL_RESULT_SUCCESS != result) {
            engine->setLastError(ERR_AUDIO_ASSET_INIT);
            LOGE("emo_audio: failed to create an audio player");
            return false;
        }

        // realize the player
        result = (*channel->playerObject)->Realize(channel->playerObject, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) {
            engine->setLastError(ERR_AUDIO_ASSET_INIT);
            LOGE("emo_audio: failed to realize an audio player");
            return false;
        }

        channel->loaded = SL_BOOLEAN_TRUE;

        // get the play interface
        result = (*channel->playerObject)->GetInterface(channel->playerObject, SL_IID_PLAY, &channel->playerPlay);
        if (SL_RESULT_SUCCESS != result) {
            engine->setLastError(ERR_AUDIO_ASSET_INIT);
            LOGE("emo_audio: failed to get an audio player interface");
            return false;
        }

        // get the seek interface
        result = (*channel->playerObject)->GetInterface(channel->playerObject, SL_IID_SEEK, &channel->playerSeek);
        if (SL_RESULT_SUCCESS != result) {
            engine->setLastError(ERR_AUDIO_ASSET_INIT);
            LOGE("emo_audio: failed to get an audio seek interface");
            return false;
        }

        // the volume interface
        result = (*channel->playerObject)->GetInterface(channel->playerObject, SL_IID_VOLUME, &channel->playerVolume);
        if (SL_RESULT_SUCCESS != result) {
            engine->setLastError(ERR_AUDIO_ASSET_INIT);
            LOGE("emo_audio: failed to create an audio volume interface");
            return false;
        }

        return true;
    }

    bool Audio::setChannelState(int index, SLuint32 state) {
        Channel* channel = &this->channels[index];
        if (!channel->loaded) {
            engine->setLastError(ERR_AUDIO_CHANNEL_CLOSED);
            LOGE("emo_audio: audio channel is closed");
            return false;
        }
        SLresult result = (*channel->playerPlay)->SetPlayState(channel->playerPlay, state);
        checkOpenSLresult("emo_audio: failed to set play state", result);
        return (SL_RESULT_SUCCESS == result);
    }
    
    SLuint32 Audio::getChannelState(int index) {
        Channel* channel = &this->channels[index];
        if (!channel->loaded) {
            engine->setLastError(ERR_AUDIO_CHANNEL_CLOSED);
            LOGE("emo_audio: audio channel is closed");
            return SL_PLAYSTATE_STOPPED;
        }
        SLuint32 state;
        SLresult result = (*channel->playerPlay)->GetPlayState(channel->playerPlay, &state);
        checkOpenSLresult("emo_audio: failed to get play state", result);
        return state;
    }

    void Audio::closeChannel(int index) {
        Channel* channel = &this->channels[index];
        if (channel->loaded) {
            if (this->getChannelState(index) != SL_PLAYSTATE_PAUSED) {
                this->stopChannel(index);
            }
            (*channel->playerObject)->Destroy(channel->playerObject);
            channel->playerObject = NULL;
            channel->playerPlay   = NULL;
            channel->playerSeek   = NULL;
            channel->playerVolume = NULL;
            channel->loaded       = SL_BOOLEAN_FALSE;
        }
    }

    int Audio::getChannelCount() {
        return this->channelCount;
    }


    SLmillibel Audio::getChannelVolume(int index) {
        Channel* channel = &this->channels[index];
        if (!channel->loaded) {
            engine->setLastError(ERR_AUDIO_CHANNEL_CLOSED);
            LOGE("emo_audio: audio channel is closed");
            return 0;
        }
        SLmillibel volumeLevel;
        SLresult result = (*channel->playerVolume)->GetVolumeLevel(channel->playerVolume, &volumeLevel);
        checkOpenSLresult("emo_audio: failed to get audio channel volume", result);

        return volumeLevel;
    }

    SLmillibel Audio::setChannelVolume(int index, SLmillibel volumeLevel) {
        Channel* channel = &this->channels[index];
        if (!channel->loaded) {
            engine->setLastError(ERR_AUDIO_CHANNEL_CLOSED);
            LOGE("emo_audio: audio channel is closed");
            return 0;
        }
        SLresult result = (*channel->playerVolume)->SetVolumeLevel(channel->playerVolume, volumeLevel);
        checkOpenSLresult("emo_audio: failed to set audio channel volume", result);

        return this->getChannelVolume(index);
    }

    SLmillibel Audio::getChannelMaxVolume(int index) {
        Channel* channel = &this->channels[index];
        if (!channel->loaded) {
            engine->setLastError(ERR_AUDIO_CHANNEL_CLOSED);
            LOGE("emo_audio: audio channel is closed");
            return 0;
        }
        SLmillibel volumeLevel;
        SLresult result = (*channel->playerVolume)->GetMaxVolumeLevel(channel->playerVolume, &volumeLevel);
        checkOpenSLresult("emo_audio: failed to get audio channel max volume", result);

        return volumeLevel;
    }


    bool Audio::seekChannel(int index, int pos, SLuint32 seekMode) {
        Channel* channel = &this->channels[index];
        if (!channel->loaded) {
            engine->setLastError(ERR_AUDIO_CHANNEL_CLOSED);
            LOGE("emo_audio: audio channel is closed");
            return false;
        }
        SLresult result = (*channel->playerSeek)->SetPosition(channel->playerSeek, pos, seekMode);
        checkOpenSLresult("emo_audio: failed to seek audio channel", result);
        return (SL_RESULT_SUCCESS == result);
    }

    bool Audio::playChannel(int index) {
        Channel* channel = &this->channels[index];
        if (!channel->loaded) {
            engine->setLastError(ERR_AUDIO_CHANNEL_CLOSED);
            LOGE("emo_audio: audio channel is closed");
            return false;
        }
        if (this->getChannelState(index) != SL_PLAYSTATE_STOPPED) {
            this->seekChannel(index, 0, SL_SEEKMODE_FAST);
        }
        return this->setChannelState(index, SL_PLAYSTATE_PLAYING);
    }

    bool Audio::pauseChannel(int index) {
        Channel* channel = &this->channels[index];
        if (!channel->loaded) {
            engine->setLastError(ERR_AUDIO_CHANNEL_CLOSED);
            LOGE("emo_audio: audio channel is closed");
            return false;
        }
        return this->setChannelState(index, SL_PLAYSTATE_PAUSED);
    }

    bool Audio::stopChannel(int index) {
        Channel* channel = &this->channels[index];
        if (!channel->loaded) {
            engine->setLastError(ERR_AUDIO_CHANNEL_CLOSED);
            LOGE("emo_audio: audio channel is closed");
            return false;
        }
        return this->setChannelState(index, SL_PLAYSTATE_STOPPED);
    }

    void Audio::close() {
        if (running) {
            for (int i = 0; i < this->getChannelCount(); i++) {
                this->closeChannel(i);
            }
            free(channels);

            (*this->outputMixObject)->Destroy(this->outputMixObject);
            (*this->engineObject)->Destroy(this->engineObject);

            this->engineObject    = NULL;
            this->outputMixObject = NULL;
            this->engineEngine    = NULL;
        }

        this->running = false;
    }

    bool Audio::isRunning() {
        return this->running;
    }

    bool Audio::isLoaded(int index) {
       return this->channels[index].loaded;
    }

    bool Audio::getChannelLooping(int index) {
        Channel* channel = &this->channels[index];

        SLboolean enabled;
        SLmillisecond start = 0;
        SLmillisecond end = SL_TIME_UNKNOWN;
        SLresult result = (*channel->playerSeek)->GetLoop(channel->playerSeek, &enabled, &start, &end);

        if (SL_RESULT_SUCCESS == result && enabled) {
            return true;
        }

        if (SL_RESULT_SUCCESS != result) {
            LOGE("emo_audio: failed to get the channel loop status");
        }

        return false;
    }

    bool Audio::setChannelLooping(int index, SQBool enable) {
        Channel* channel = &this->channels[index];

        SLresult result;
        if (enable) {
            result = (*channel->playerSeek)->SetLoop(channel->playerSeek, SL_BOOLEAN_TRUE, 0, SL_TIME_UNKNOWN);
        } else {
            result = (*channel->playerSeek)->SetLoop(channel->playerSeek, SL_BOOLEAN_FALSE, 0, SL_TIME_UNKNOWN);
        }
        return (SL_RESULT_SUCCESS == result);
    }
}

/*
 * SQInteger loadAudio(SQInteger audioIndex, SQChar* filename);
 */
SQInteger emoLoadAudio(HSQUIRRELVM v) {
    if (!engine->audio->isRunning()) {
        sq_pushinteger(v, ERR_AUDIO_ENGINE_CLOSED);
        return 1;
    }

    SQInteger channelIndex;
    const SQChar* filename;

    if (sq_gettype(v, 2) != OT_NULL && sq_gettype(v, 3) == OT_STRING) {
        sq_getinteger(v, 2, &channelIndex);
        sq_tostring(v, 3);
        sq_getstring(v, -1, &filename);
    } else {
        sq_pushinteger(v, ERR_INVALID_PARAM_TYPE);
        return 1;
    }

    if (channelIndex >= engine->audio->getChannelCount()) {
        sq_pushinteger(v, ERR_INVALID_PARAM);
        return 1;
    }

    if (!engine->audio->createChannelFromAsset(filename, channelIndex)) {
        sq_pushinteger(v, engine->getLastError());
        return 1;
    }

    sq_pushinteger(v, EMO_NO_ERROR);

    return 1;
}

SQInteger emoCreateAudioEngine(HSQUIRRELVM v) {
    if (engine->audio->isRunning()) {
        sq_pushinteger(v, ERR_AUDIO_ENGINE_CREATED);
        return 1;
    }

    SQInteger channelCount;

    if (sq_gettype(v, 2) != OT_NULL) {
        sq_getinteger(v, 2, &channelCount);
    } else {
        channelCount = DEFAULT_AUDIO_CHANNEL_COUNT;
    }

    if (!engine->audio->create(channelCount)) {
        sq_pushinteger(v, engine->getLastError());
        return 1;
    }

    sq_pushinteger(v, EMO_NO_ERROR);

    return 1;
}

SQInteger emoPlayAudioChannel(HSQUIRRELVM v) {
    if (!engine->audio->isRunning()) {
        sq_pushinteger(v, ERR_AUDIO_ENGINE_CLOSED);
        return 1;
    }

    SQInteger channelIndex;

    if (sq_gettype(v, 2) != OT_NULL) {
        sq_getinteger(v, 2, &channelIndex);
    } else {
        sq_pushinteger(v, ERR_INVALID_PARAM_TYPE);
        return 1;
    }

    if (channelIndex >= engine->audio->getChannelCount()) {
        sq_pushinteger(v, ERR_INVALID_PARAM);
        return 1;
    }

    if (!engine->audio->playChannel(channelIndex)) {
        sq_pushinteger(v, ERR_AUDIO_ENGINE_STATUS);
        return 1;
    }

    sq_pushinteger(v, EMO_NO_ERROR);

    return 1;
}

SQInteger emoPauseAudioChannel(HSQUIRRELVM v) {
    if (!engine->audio->isRunning()) {
        sq_pushinteger(v, ERR_AUDIO_ENGINE_CLOSED);
        return 1;
    }

    SQInteger channelIndex;

    if (sq_gettype(v, 2) != OT_NULL) {
        sq_getinteger(v, 2, &channelIndex);
    } else {
        sq_pushinteger(v, ERR_INVALID_PARAM_TYPE);
        return 1;
    }

    if (channelIndex >= engine->audio->getChannelCount()) {
        sq_pushinteger(v, ERR_INVALID_PARAM);
        return 1;
    }

    if (!engine->audio->pauseChannel(channelIndex)) {
        sq_pushinteger(v, ERR_AUDIO_ENGINE_STATUS);
        return 1;
    }

    sq_pushinteger(v, EMO_NO_ERROR);

    return 1;
}

SQInteger emoStopAudioChannel(HSQUIRRELVM v) {
    if (!engine->audio->isRunning()) {
        sq_pushinteger(v, ERR_AUDIO_ENGINE_CLOSED);
        return 1;
    }

    SQInteger channelIndex;

    if (sq_gettype(v, 2) != OT_NULL) {
        sq_getinteger(v, 2, &channelIndex);
    } else {
        sq_pushinteger(v, ERR_INVALID_PARAM_TYPE);
        return 1;
    }

    if (channelIndex >= engine->audio->getChannelCount()) {
        sq_pushinteger(v, ERR_INVALID_PARAM);
        return 1;
    }

    if (!engine->audio->stopChannel(channelIndex)) {
        sq_pushinteger(v, ERR_AUDIO_ENGINE_STATUS);
        return 1;
    }

    sq_pushinteger(v, EMO_NO_ERROR);

    return 1;
}


SQInteger emoSeekAudioChannel(HSQUIRRELVM v) {
    if (!engine->audio->isRunning()) {
        sq_pushinteger(v, ERR_AUDIO_ENGINE_CLOSED);
        return 1;
    }

    SQInteger channelIndex;
    SQInteger seekPosition;

    if (sq_gettype(v, 2) != OT_NULL && sq_gettype(v, 3) != OT_NULL) {
        sq_getinteger(v, 2, &channelIndex);
        sq_getinteger(v, 3, &seekPosition);
    } else {
        sq_pushinteger(v, ERR_INVALID_PARAM_TYPE);
        return 1;
    }

    if (channelIndex >= engine->audio->getChannelCount()) {
        sq_pushinteger(v, ERR_INVALID_PARAM);
        return 1;
    }

    if (!engine->audio->seekChannel(channelIndex, seekPosition, SL_SEEKMODE_ACCURATE)) {
        sq_pushinteger(v, ERR_AUDIO_ENGINE_STATUS);
        return 1;
    }

    sq_pushinteger(v, EMO_NO_ERROR);

    return 1;
}

SQInteger emoGetAudioChannelVolume(HSQUIRRELVM v) {
    if (!engine->audio->isRunning()) {
        LOGE("emoGetAudioChannelVolume: audio engine is closed");
        sq_pushinteger(v, 0);
        return 1;
    }

    SQInteger channelIndex;

    if (sq_gettype(v, 2) != OT_NULL) {
        sq_getinteger(v, 2, &channelIndex);
    } else {
        sq_pushinteger(v, 0);
        LOGE("emoGetAudioChannelVolume: invalid parameter type");
        return 1;
    }

    if (channelIndex >= engine->audio->getChannelCount()) {
        LOGE("emoGetAudioChannelVolume: invalid channel index");
        sq_pushinteger(v, 0);
        return 1;
    }

    sq_pushinteger(v, engine->audio->getChannelVolume(channelIndex));

    return 1;
}

SQInteger emoSetAudioChannelVolume(HSQUIRRELVM v) {
    if (!engine->audio->isRunning()) {
        sq_pushinteger(v, ERR_AUDIO_ENGINE_CLOSED);
        return 1;
    }

    SQInteger channelIndex;
    SQInteger channelVolume;

    if (sq_gettype(v, 2) != OT_NULL && sq_gettype(v, 3) != OT_NULL) {
        sq_getinteger(v, 2, &channelIndex);
        sq_getinteger(v, 3, &channelVolume);
    } else {
        sq_pushinteger(v, ERR_INVALID_PARAM_TYPE);
        return 1;
    }

    if (channelIndex >= engine->audio->getChannelCount()) {
        sq_pushinteger(v, ERR_INVALID_PARAM);
        return 1;
    }

    if (!engine->audio->setChannelVolume(channelIndex, channelVolume)) {
        sq_pushinteger(v, ERR_AUDIO_ENGINE_STATUS);
        return 1;
    }

    sq_pushinteger(v, EMO_NO_ERROR);

    return 1;
}

SQInteger emoGetAudioChannelMaxVolume(HSQUIRRELVM v) {
    if (!engine->audio->isRunning()) {
        LOGE("emoGetAudioChannelVolume: audio engine is closed");
        sq_pushinteger(v, 0);
        return 1;
    }

    SQInteger channelIndex;

    if (sq_gettype(v, 2) != OT_NULL) {
        sq_getinteger(v, 2, &channelIndex);
    } else {
        sq_pushinteger(v, 0);
        LOGE("emoGetAudioChannelVolume: invalid parameter type");
        return 1;
    }

    if (channelIndex >= engine->audio->getChannelCount()) {
        LOGE("emoGetAudioChannelVolume: invalid channel index");
        sq_pushinteger(v, 0);
        return 1;
    }

    sq_pushinteger(v, engine->audio->getChannelMaxVolume(channelIndex));

    return 1;
}

SQInteger emoGetAudioChannelCount(HSQUIRRELVM v) {
    sq_pushinteger(v, engine->audio->getChannelCount());
    return 1;

}

SQInteger emoGetAudioChannelMinVolume(HSQUIRRELVM v) {
    sq_pushinteger(v, SL_MILLIBEL_MIN);
    return 1;
}


SQInteger emoSetAudioChannelLooping(HSQUIRRELVM v) {
    if (!engine->audio->isRunning()) {
        sq_pushinteger(v, ERR_AUDIO_ENGINE_CLOSED);
        return 1;
    }

    SQInteger channelIndex;
    SQBool useLoop;

    if (sq_gettype(v, 2) != OT_INTEGER && getBool(v, 3, &useLoop)) {
        sq_getinteger(v, 2, &channelIndex);
    } else {
        sq_pushinteger(v, ERR_INVALID_PARAM_TYPE);
        return 1;
    }

    if (channelIndex >= engine->audio->getChannelCount()) {
        sq_pushinteger(v, ERR_INVALID_PARAM);
        return 1;
    }

    if (!engine->audio->setChannelLooping(channelIndex, useLoop)) {
        sq_pushinteger(v, ERR_AUDIO_ENGINE_STATUS);
        return 1;
    }

    sq_pushinteger(v, EMO_NO_ERROR);

    return 1;
}

SQInteger emoGetAudioChannelLooping(HSQUIRRELVM v) {
    if (!engine->audio->isRunning()) {
        LOGE("emoGetAudioChannelLooping: audio engine is closed");
        sq_pushbool(v, false);
        return 1;
    }

    SQInteger channelIndex;

    if (sq_gettype(v, 2) != OT_NULL) {
        sq_getinteger(v, 2, &channelIndex);
    } else {
        sq_pushbool(v, false);
        LOGE("emoGetAudioChannelLooping: invalid parameter type");
        return 1;
    }

    if (channelIndex >= engine->audio->getChannelCount()) {
        LOGE("emoGetAudioChannelLooping: invalid channel index");
        sq_pushbool(v, false);
        return 1;
    }

    if (engine->audio->getChannelLooping(channelIndex)) {
        sq_pushbool(v, true);
    } else {
        sq_pushbool(v, false);
    }

    return 1;

}

SQInteger emoGetAudioChannelState(HSQUIRRELVM v) {
    if (!engine->audio->isRunning()) {
        LOGE("emoGetAudioChannelState: audio engine is closed");
        sq_pushinteger(v, AUDIO_CHANNEL_STOPPED);
        return 1;
    }

    SQInteger channelIndex;

    if (sq_gettype(v, 2) != OT_NULL) {
        sq_getinteger(v, 2, &channelIndex);
    } else {
        sq_pushinteger(v, AUDIO_CHANNEL_STOPPED);
        LOGE("emoGetAudioChannelState: invalid parameter type");
        return 1;
    }

    if (channelIndex >= engine->audio->getChannelCount()) {
        LOGE("emoGetAudioChannelState: invalid channel index");
        sq_pushinteger(v, AUDIO_CHANNEL_STOPPED);
        return 1;
    }

    SLuint32 state = engine->audio->getChannelState(channelIndex);

    switch(state) {
    case SL_PLAYSTATE_STOPPED:
        sq_pushinteger(v, AUDIO_CHANNEL_STOPPED);
        break;
    case SL_PLAYSTATE_PAUSED:
        sq_pushinteger(v, AUDIO_CHANNEL_PAUSED);
        break;
    case SL_PLAYSTATE_PLAYING:
        sq_pushinteger(v, AUDIO_CHANNEL_PLAYING);
        break;
    }

    return 1;

}


SQInteger emoCloseAudioChannel(HSQUIRRELVM v) {
    if (!engine->audio->isRunning()) {
        sq_pushinteger(v, ERR_AUDIO_ENGINE_CLOSED);
        return 1;
    }

    SQInteger channelIndex;

    if (sq_gettype(v, 2) != OT_NULL) {
        sq_getinteger(v, 2, &channelIndex);
    } else {
        sq_pushinteger(v, ERR_INVALID_PARAM_TYPE);
        return 1;
    }

    if (channelIndex >= engine->audio->getChannelCount()) {
        sq_pushinteger(v, ERR_INVALID_PARAM);
        return 1;
    }

    engine->audio->closeChannel(channelIndex);

    sq_pushinteger(v, EMO_NO_ERROR);

    return 1;
}

SQInteger emoCloseAudioEngine(HSQUIRRELVM v) {
    if (!engine->audio->isRunning()) {
        sq_pushinteger(v, ERR_AUDIO_ENGINE_CLOSED);
        return 1;
    }

    engine->audio->close();

    sq_pushinteger(v, EMO_NO_ERROR);

    return 1;
}
