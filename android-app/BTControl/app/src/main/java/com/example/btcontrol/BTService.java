package com.example.btcontrol;

import android.app.Service;
import android.content.Intent;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;

public class BTService extends Service{
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        // Called when the service is started
        Handler mainHandler=new Handler(Looper.getMainLooper());
        new Thread(()->
        {
            mainHandler.post(()->{});
        }).start();
        return START_STICKY;
    }
}