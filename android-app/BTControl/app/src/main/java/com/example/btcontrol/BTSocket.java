package com.example.btcontrol;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.util.Log;

import java.io.IOException;
import java.util.UUID;
public class BTSocket {
    static private BluetoothSocket BTsock;
    static public BluetoothSocket getSocket(BluetoothDevice device, UUID uuid) throws IOException
    {
            if (BTsock == null) {
                BTsock = device.createRfcommSocketToServiceRecord(uuid);
                BTsock.connect();
            }
            return BTsock;
    }
    static public void disconnect() throws IOException
    {
        if(BTsock!=null && BTsock.isConnected()) {
            BTsock.close();
        }
        BTsock=null;
    }
}
