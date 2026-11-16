package com.example.btcontrol;

import android.content.Context;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
public class SignalDBHelper extends SQLiteOpenHelper {

    private static final String DB_NAME = "signalLibrary.db";
    private static final int DB_VERSION = 1; // initial version
    private static final String createIR="CREATE TABLE IR (id INTEGER PRIMARY KEY, name TEXT, protocol INTEGER, code TEXT, nbits INTEGER);";
    private static final String createNFC="CREATE TABLE NFC (id INTEGER PRIMARY KEY, name TEXT, uid BLOB);";
    private static final String createSUBGHZ="CREATE TABLE GHZ (id INTEGER PRIMARY KEY, name TEXT, frequency INTEGER, code TEXT);";
    private static SignalDBHelper SignalDBHelperInstance=null;
    private SignalDBHelper(Context context) {
        super(context, DB_NAME, null, DB_VERSION);
    }
    static public synchronized SignalDBHelper getInstance(Context context)
    {
        if(SignalDBHelperInstance==null)
            SignalDBHelperInstance=new SignalDBHelper(context);
        return SignalDBHelperInstance;
    }
    @Override
    public void onCreate(SQLiteDatabase db) {

        db.execSQL(createIR);
        db.execSQL(createNFC);
        db.execSQL(createSUBGHZ);
    }
    @Override
    public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
        // This runs only if newVersion > oldVersion
    }
}
