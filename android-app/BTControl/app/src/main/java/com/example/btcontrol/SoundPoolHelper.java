package com.example.btcontrol;

import android.media.AudioAttributes;
import android.media.SoundPool;

public class SoundPoolHelper
{
    private static SoundPool soundPool;
    static public synchronized SoundPool getSoundPool()
    {
        if(soundPool==null) {
            AudioAttributes audioAttributes = new AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_MEDIA)
                    .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                    .build();

            soundPool = new SoundPool.Builder()
                    .setMaxStreams(1)
                    .setAudioAttributes(audioAttributes)
                    .build();
        }
        return soundPool;
    }
    static public synchronized void release() {
        if(soundPool != null) {
            soundPool.release();
            soundPool = null;
        }
    }
}
