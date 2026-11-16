package com.example.btcontrol;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class ThreadPoolHelper {
    static final int NUM_OF_THREADS=3;
    private final static ExecutorService threadPoolExecutor=Executors.newFixedThreadPool(NUM_OF_THREADS);
    public static ExecutorService getThreadPoolExecutor()
    {
        return threadPoolExecutor;
    }
}
