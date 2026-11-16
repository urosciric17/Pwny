package com.example.btcontrol;

import java.util.concurrent.atomic.AtomicBoolean;

public class Signal {
    public String name;
    protected SignalType type;
    public AtomicBoolean valid=new AtomicBoolean(false);
    public Signal(SignalType type)
    {
        this.type=type;
    }
}
