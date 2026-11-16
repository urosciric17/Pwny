package com.example.btcontrol;

import android.app.AlertDialog;
import android.content.ContentValues;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.os.Build;
import android.os.Bundle;
import android.Manifest;
import androidx.activity.EdgeToEdge;
import androidx.annotation.RequiresPermission;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.res.ResourcesCompat;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.ProgressBar;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Arrays;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Semaphore;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

public class MainActivity extends AppCompatActivity {
    private static final int REQUEST_CODE = 1;
    CountDownLatch barrier=new CountDownLatch(1);
    Semaphore saveSemaphore = new Semaphore(0);
    Semaphore cancelSemaphore = new Semaphore(0);
    boolean permissionsAlreadyGranted=false;
    Spinner signalTypeSpinner;
    Button capButton;
    Button sendButton;
    //Button addButton;
    Button searchButton;
    TextView statusField;
    TextView batteryLevelField;
    EditText nameField;
    ListView listView;
    BluetoothSocket btSocket;
    UUID uuid;
    BluetoothDevice device;
    int previousSpinnerPosition=-1;
    private volatile ArrayList<String> signalNames=new ArrayList<>();
    private final Object lock = new Object();
    private final Object BTlock = new Object();
    private final Object DBlock = new Object();
    private ArrayAdapter<String> listAdapter;
    private int soundId;
    private SQLiteDatabase db;
    private SignalDBHelper dbHelper;
    //private final int MAX_MSG_LEN=40;
    private boolean shouldSave=false;
    private String signalNameToAdd;
    private volatile String signalTypeName="IR";
    private final String[] signalTypeNames = {"IR","NFC","GHZ"};
    private volatile SignalType selectedSignalType;
    final int UID_LEN = 4;
    private final String[] logMessages = {"Unknown IR code","IR capture start!","IR capture success!",
    "IR capture cancelled!","IR send failed!","IR send success!","NFC invalid UID!","NFC UID read start!",
    "NFC UID read success!","NFC UID read cancelled!","Not MIFARE CLASSIC!","NFC target not found",
    "NFC authentication failed!","NFC read failed!", "NFC writing started!",
    "NFC writing failed!","NFC writing success!","NFC writing cancelled!","SUB-GHZ capture start!",
    "SUB-GHZ capture cancelled!", "SUB-GHZ capture success", "SUB-GHZ sending failed",
    "SUB-GHZ sending success!","Error: Unrecognized command!"};
    private final int nfcLogCodeBeginIndex=11;
    private final int nfcLogCodeEndIndex=17;
    private final int irCapStartLogCode=1;
    private final int nfcUidReadStartLogCode=7;
    private final int subghzCapStartLogCode=18;
    private final int nfcWriteStartLogCode=14;
    final int[] cancelLogCodes = {3,9,17,19};
    /*private enum SignalType{
        IR,
        NFC,
        SUB_GHZ
    }*/
    private enum DialogType{
        CAPTURING_DIALOG,
        WRITING_DIALOG
    }
    private volatile Signal signalToSend;
    private ExecutorService executor;
    private AtomicBoolean BTConnected=new AtomicBoolean(false);
    private AlertDialog currentDialog;
    private final int BT_MSG_MAX_LEN=100;
    private SignalType getSignalType(int id)
    {
        return SignalType.values()[id];
    }
    private void requestBTPermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            // Android 12+
            if (checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {

                requestPermissions(new String[]{
                        Manifest.permission.BLUETOOTH_CONNECT,
                }, REQUEST_CODE);
            }
        } else {
            // Android 11 and below
            if (checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
                requestPermissions(new String[]{
                        Manifest.permission.ACCESS_FINE_LOCATION
                }, REQUEST_CODE);
            }
        }
    }

    private void log(int index)
    {
        runOnUiThread(()->Toast.makeText(this, logMessages[index], Toast.LENGTH_SHORT).show());
    }

    private boolean checkBTPermissions()
    {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            return checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED;
        } else {
            // Must check Location on Android 11 and below to access Bluetooth
            return checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED;
        }
    }

    private Cursor selectAll(String tableName)
    {
            return db.query(
                    tableName,              // table name
                    null, // columns to return
                    null,           // WHERE clause (without 'WHERE')
                    null, // arguments for ?
                    null,                 // groupBy
                    null,                 // having
                    "name ASC"                 // orderBy
            );
    }
    private void loadSignalNames(String tableName)
    {
        executor.execute(()->
        {
            synchronized (DBlock) {
                Cursor cursor=selectAll(tableName);
                synchronized (lock) {
                    signalNames.clear();
                    if (cursor.moveToFirst()) {
                        do {
                            signalNames.add(cursor.getString(cursor.getColumnIndexOrThrow("name")));
                        } while (cursor.moveToNext());
                    }
                    runOnUiThread(() -> listAdapter.notifyDataSetChanged());
                }
                cursor.close();
            }
        });
    }
    private void emptyInStream(InputStream in) throws IOException
    {
        while(in.available()>0)
            in.read();
    }

    boolean arrayContains(int[] haystack,int needle)
    {
        boolean ret=false;
        for (int el:haystack) {
            if (el==needle) {
                ret = true;
                break;
            }
        }
        return ret;
    }
    private boolean readBTSocket(InputStream in,byte[] inBuffer,int initialBytesRemaining) throws IOException {
        int bytesRemaining = initialBytesRemaining;
        boolean ret = true;
        while (bytesRemaining > 0) {
            int bytesRead = in.read(inBuffer, initialBytesRemaining - bytesRemaining, bytesRemaining);
            if (bytesRead == -1) {
                ret = false;
                break;
            }
            bytesRemaining -= bytesRead;
        }
        return ret;
    }

    //@RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    //@Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        boolean permissionsGranted=true;
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_CODE) {
            for (int result : grantResults) {
                if (result != PackageManager.PERMISSION_GRANTED) {
                    permissionsGranted = false;
                    break;
                }
            }
            if (permissionsGranted)
            // Permission granted - you can now use Bluetooth
            {
                Toast.makeText(this, "Bluetooth permissions granted!", Toast.LENGTH_SHORT).show();
            } else {
                // Permission denied - inform the user
                Toast.makeText(this, "All bluetooth permissions required", Toast.LENGTH_SHORT).show();
            }
            barrier.countDown();
        }
    }

    private Cursor searchSignalByName(String tableName,String signalName)
    {
        final String selection = "name LIKE ? COLLATE NOCASE";
        final String[] selectionArgs=new String[]{signalName+"%"};
            return db.query(tableName,
                    null,
                    selection,
                    selectionArgs,
                    null,
                    null,
                    "name ASC");
    }
    private boolean signalExists(String tableName,String signalName)
    {
        Cursor cursor=searchSignalByName(tableName,signalName);
        try{
           return cursor.getCount()>0;
        }
        finally {
            cursor.close();
        }
    }

    private void searchHandler()
    {
        SoundPoolHelper.getSoundPool().play(soundId, 1.0f, 1.0f, 1, 0, 1.0f);
        String signalName=nameField.getText().toString().trim();
        final String tableNameToSearch=signalTypeName;
        executor.execute(()->
        {
            synchronized (DBlock) {
                Cursor cursor = searchSignalByName(tableNameToSearch, signalName);
                synchronized (lock) {
                    signalNames.clear();
                    if (cursor.moveToFirst()) {
                        do {
                            signalNames.add(cursor.getString(cursor.getColumnIndexOrThrow("name")));
                        } while (cursor.moveToNext());
                    }
                    runOnUiThread(() -> listAdapter.notifyDataSetChanged());
                }
                cursor.close();
            }
        });
    }

    void spawnProgBar(DialogType dialogType)
    {
        runOnUiThread(() -> {
            String title="";
            LinearLayout layout2 = new LinearLayout(this);
            layout2.setOrientation(LinearLayout.VERTICAL);
            layout2.setPadding(50, 40, 50, 10);
            ProgressBar progressBar = new ProgressBar(MainActivity.this);
            layout2.addView(progressBar);
            if(dialogType==DialogType.CAPTURING_DIALOG)
                title="Capturing...";
            else
                title="Writing...";
            currentDialog=new AlertDialog.Builder(MainActivity.this).setTitle(title).
                    setNegativeButton("Cancel", (dialog2, which2) -> {
                        if(!BTConnected.get()) {
                            Toast.makeText(MainActivity.this, "Please first establish Bluetooth connection!", Toast.LENGTH_SHORT).show();
                        }
                        else executor.execute(()-> {
                            synchronized (BTlock)
                            {
                                OutputStream out= null;
                                try {
                                    out = BTSocket.getSocket(device,uuid).getOutputStream();
                                    out.write("c".getBytes(StandardCharsets.UTF_8));
                                    out.flush();
                                    //cancelSemaphore.acquireUninterruptibly();
                                } catch (IOException e) {
                                    runOnUiThread(()->Toast.makeText(MainActivity.this, "Error: Failed sending the cancel command!", Toast.LENGTH_SHORT).show());
                                }
                            }
                        });
                    }).setCancelable(false).setView(layout2)
                    .show();
        });
    }

    private boolean sendCapMsg(String signalTypeName,String selectedCapFreq)
    {
        if(!BTConnected.get()) {
            runOnUiThread(()->
            {
                Toast.makeText(MainActivity.this, "Please first establish Bluetooth connection!", Toast.LENGTH_SHORT).show();
            });
            return false;
        }
        try {
            btSocket = BTSocket.getSocket(device, uuid);
            synchronized (BTlock)
            {
                OutputStream out = btSocket.getOutputStream();
                StringBuilder signalStringBuilder=new StringBuilder(signalTypeName.toLowerCase());
                signalStringBuilder.append(":cap");
                if(selectedCapFreq!=null) {
                    signalStringBuilder.append(":");
                    signalStringBuilder.append(selectedCapFreq);
                }
                signalStringBuilder.append('\n');
                out.write(signalStringBuilder.toString().getBytes(StandardCharsets.UTF_8));
                out.flush();
            }
        }
        catch (IOException e) {
            Log.e("Urke","Bluetooth error: " + e.getMessage());
        }
        return true;
    }

    private void captureHandler()
    {
        final String[] subGhzFreqs={"433.92","315.00","868.35","915.00"};
        SoundPoolHelper.getSoundPool().play(soundId, 1.0f, 1.0f, 1, 0, 1.0f);
        final String signalTypeNameSnapshot=signalTypeName;
        if(signalTypeNameSnapshot.equals("GHZ"))
        {
            LinearLayout layout = new LinearLayout(this);
            layout.setOrientation(LinearLayout.VERTICAL);
            layout.setPadding(50, 40, 50, 10);
            Spinner freqSpinner=new Spinner(this);
            ArrayAdapter<String> adapter = new ArrayAdapter<>(this,
                    android.R.layout.simple_spinner_item,
                    subGhzFreqs);
            adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
            freqSpinner.setAdapter(adapter);
            layout.addView(freqSpinner);
            new AlertDialog.Builder(this)
                    .setTitle("Select capturing frequency")
                    .setView(layout) // This injects your programmatic layout into the dialog
                    .setPositiveButton("Capture", (dialog, which) -> {
                        final String selectedCapFreq=freqSpinner.getSelectedItem().toString();
                        executor.execute(()->
                        {
                            sendCapMsg(signalTypeName,selectedCapFreq);
                                /*spawnProgBar(DialogType.CAPTURING_DIALOG)*/
                        });
                    })
                    .setNegativeButton("Cancel", (dialog, which)->{
                        dialog.dismiss();
                    })
                    .show();
        }
        else {
            executor.execute(() ->
            {
                sendCapMsg(signalTypeName,null);
                    /*spawnProgBar(DialogType.CAPTURING_DIALOG)*/
            });
        }
    }
    String signalTypeToString(SignalType signalType)
    {
       return signalTypeNames[signalType.ordinal()];
    }

    int hexCharToDec(char hexChar) {
        final byte decDigitAsciiOffset = 48;
        final byte hexDigitAsciiOffset = 55;
        int ret = -1;  // indicates that hex char wasnt an appropriate value
        if (hexChar <= '9' && hexChar >= '0')
            ret = hexChar - decDigitAsciiOffset;
        else if (hexChar <= 'F' && hexChar >= 'A')
            ret = hexChar - hexDigitAsciiOffset;
        return ret;
    }


    byte[] hexStrToUid(String str) {
        byte[] uid = new byte[UID_LEN];
        byte j = 0;
        int strLen = str.length();
        //if (strLen != 8)
        //    return false;
        for (byte i = 0; i < strLen; i += 2) {
            int firstChar = hexCharToDec(str.charAt(i));
            int secondChar = hexCharToDec(str.charAt(i + 1));
            //if (firstChar == -1 || secondChar == -1)
            //    return false;
            uid[j++] = (byte)((firstChar << 4) | secondChar);
        }
        return uid;
    }
    char intToHexChar(int val)
    {
        char ret=0;
        if(val>=0 && val<=9)
            ret=(char)('0'+val);
        else if(val>9 && val<16)
            ret=(char)(val-10+'A');
        return ret;
    }

    String byteToHexStr(byte b)
    {
        StringBuilder sb=new StringBuilder();
        sb.append(intToHexChar((b&0xF0)>>4));
        sb.append(intToHexChar((b&0x0F)));
        return sb.toString();
    }

    String UidToHexStr(byte[] UID)
    {
        StringBuilder hexSB=new StringBuilder();
        for(int i=0;i<UID_LEN;i++)
        {
            hexSB.append(byteToHexStr(UID[i]));
        }
        return hexSB.toString();
    }
    private String buildMsgToSend(Signal signalToSend)
    {
        StringBuilder sb=null;
        Cursor c;
        switch(signalToSend.type)
        {
            case IR:
            {
                //IRSignal helpSignal = getSignalByName(signalNameToQuery);
                sb = new StringBuilder("ir:rep:");
                //IRSignal helpSignal = getSignalByName(signalToSend.name,signalTypeToString(signalToSend.type));
                String protocol = "";
                String code = "";
                String nbits = "";
                synchronized (DBlock) {
                    c = searchSignalByName("IR", signalToSend.name);
                    Log.d("Urke", signalToSend.name);
                    if (c.moveToFirst()) {
                        protocol = c.getString(c.getColumnIndexOrThrow("protocol"));
                        code = c.getString(c.getColumnIndexOrThrow("code"));
                        nbits = c.getString(c.getColumnIndexOrThrow("nbits"));
                    }
                    c.close();
                }
                sb.append(protocol + ":");
                sb.append(code + ":");
                sb.append(nbits);
                sb.append("\n");
                break;
            }
            case NFC: {
                //IRSignal helpSignal = getSignalByName(signalNameToQuery);
                sb = new StringBuilder("nfc:rep:");
                //Signal helpSignal = getSignalByName(signalToSend.name,signalTypeToString(signalToSend.type));
                byte[] UID = new byte[UID_LEN];
                boolean foundUID = false;
                synchronized (DBlock) {
                    c = searchSignalByName("NFC", signalToSend.name);
                    if (c.moveToFirst()) {
                        UID = c.getBlob(c.getColumnIndexOrThrow("uid"));
                        //for(int i=0;i<4;i++)
                        //   UID[i]=(byte)0xAA;
                        foundUID = true;
                    }
                    c.close();
                }
                if (foundUID) {
                    sb.append(UidToHexStr(UID));
                    sb.append("\n");
                }
                break;
            }
            case SUB_GHZ: {
                //IRSignal helpSignal = getSignalByName(signalNameToQuery);
                sb = new StringBuilder("ghz:rep:");
                //Signal helpSignal = getSignalByName(signalToSend.name,signalTypeToString(signalToSend.type));
                String frequency = null;
                String code = null;
                synchronized (DBlock) {
                    c = searchSignalByName("GHZ", signalToSend.name);
                    if (c.moveToFirst()) {
                        frequency = c.getString(c.getColumnIndexOrThrow("frequency"));
                        code = c.getString(c.getColumnIndexOrThrow("code"));
                    }
                    c.close();
                }
                sb.append(frequency);
                sb.append(":");
                sb.append(code);
                sb.append("\n");
                break;
            }
        }
        return sb.toString();
    }
    private void sendHandler()
    {
        SoundPoolHelper.getSoundPool().play(soundId, 1.0f, 1.0f, 1, 0, 1.0f);
        //SignalType signalType=getSignalType(signalTypeSpinner.getSelectedItemPosition());
        if(!BTConnected.get()) {
            Toast.makeText(MainActivity.this, "Please first establish Bluetooth connection!", Toast.LENGTH_SHORT).show();
            return;
        }
        if(!signalToSend.valid.get())
        {
            Toast.makeText(MainActivity.this, "Please select a signal!", Toast.LENGTH_SHORT).show();
            return;
        }
        final Signal signalToSendSnapshot=signalToSend;
            //final IRSignal localSignalToSend=(IRSignal) signalToSend;
            executor.execute(() ->
            {
                try {
                    String msgToSend=buildMsgToSend(signalToSendSnapshot);
                    synchronized (BTlock) {
                        btSocket = BTSocket.getSocket(device, uuid);
                        OutputStream out = btSocket.getOutputStream();
                        out.write(msgToSend.getBytes(StandardCharsets.UTF_8));
                        out.flush();
                    }
                }
                catch (IOException e)
                {
                    Log.e("Urke", "Bluetooth error: " + e.getMessage());
                }
                //if(signalToSendSnapshot.type==SignalType.NFC)
                //    spawnProgBar(DialogType.WRITING_DIALOG);
            });
    }

    private void updateBatteryLevel(String batLevel) {
        runOnUiThread(() -> {
            if(batLevel!=null) {
                batteryLevelField.setText("Battery: " + batLevel + "%");
                if (Integer.parseInt(batLevel) <= 15)
                    batteryLevelField.setTextColor(ResourcesCompat.getColor(getResources(), R.color.dark_red,null));
                else
                    batteryLevelField.setTextColor(ResourcesCompat.getColor(getResources(), R.color.dark_green,null));
            }
            else {
                batteryLevelField.setText("Battery: --");
                batteryLevelField.setTextColor(ResourcesCompat.getColor(getResources(), R.color.dark_green,null));
            }
    });
}

    private void updateStatus(final boolean connected) {
            runOnUiThread(() -> {
                if(connected) {
                    statusField.setText("Status: Connected");
                    statusField.setTextColor(0xFF00FF00);
                }
                else
                {
                    statusField.setText("Status: Disconnected");
                    statusField.setTextColor(0xFFFF0000);
                    updateBatteryLevel(null);
                }
            });
    }
    private String getSignalTypeName(int position)
    {
        return signalTypeNames[position];
    }
    /*private Signal getSignalByName(String signalName,String selectedTableName)
    {
        Signal ret=null;
        Cursor cursor = db.query(selectedTableName,
                null,
                "name=?",
                new String[]{signalName},
                null,
                null,
                "name ASC,id ASC");
        if(cursor.moveToFirst())
        {
            if(selectedTableName.equals("IR"))
                ret=new IRSignal(cursor.getString(cursor.getColumnIndexOrThrow("protocol")),
                    cursor.getString(cursor.getColumnIndexOrThrow("code")),
                    cursor.getString(cursor.getColumnIndexOrThrow("nbits")));
            else if(selectedTableName.equals("NFC"))
                ret=new NFCSignal();
            else if(selectedTableName.equals("SUB_GHZ"))
                ret=new SUB_GHZSignal();
        }
        cursor.close();
        return ret;
    }*/
    private void assignWidgets()
    {
        listView=findViewById(R.id.ListView);
        listView.setChoiceMode(ListView.CHOICE_MODE_SINGLE);
        listAdapter = new ArrayAdapter<>(
                this,                 // Context
                android.R.layout.simple_list_item_1,  // Layout for each item
                this.signalNames       // Data
        );
        listView.setAdapter(listAdapter);
        listView.setOnItemClickListener((parent, view, position, id)->{
            listView.setItemChecked(position,true);
            Toast.makeText(MainActivity.this, "chosen:"+signalNames.get(position), Toast.LENGTH_SHORT).show();
            //listView.setSelection(position); // Scroll to it if needed
            //listAdapter.notifyDataSetChanged(); // Refresh the list to show highlight
            synchronized (lock) {
                signalToSend.name = signalNames.get(position);
            }
            signalToSend.valid.set(true);
            signalToSend.type=selectedSignalType;
            //final String tableToQuery=signalTypeName;
            /*executor.execute(()->
            {
                synchronized (DBlock) {
                    synchronized (lock) {
                        //IRSignal helpSignal = getSignalByName(signalNameToQuery);
                        if (signalType == SignalType.IR) {
                            IRSignal helpSignal = getSignalByName(signalNameToQuery,tableToQuery);
                            signalToSend = new IRSignal(helpSignal.getProtocol(), helpSignal.getCode(), helpSignal.getNbits());
                            //signalToSend.set(helpSignal.getProtocol(), helpSignal.getCode(), helpSignal.getNbits());
                        }
                    }
                }
            });*/
        });
        listView.setOnItemLongClickListener((parent, view, position, id) -> {
            String selectedSignalName = (String) parent.getItemAtPosition(position);
            new AlertDialog.Builder(MainActivity.this)
                    .setTitle("Manage signal")
                    .setMessage("What would you like to do with " + selectedSignalName + " ?")
                    .setNegativeButton("Delete", (dialog, which)->{
                        String signalTypeNameSnapshot=signalTypeName;
                        executor.execute(()->{
                            int rowsDeleted;
                            synchronized (DBlock) {
                                rowsDeleted = db.delete(
                                        signalTypeNameSnapshot,          // table name
                                        "name = ?",        // WHERE clause
                                        new String[]{selectedSignalName} // arguments for '?'
                                );
                            }
                                if(rowsDeleted>0)
                                    synchronized (lock) {
                                        while (signalNames.remove(selectedSignalName));
                                        runOnUiThread(()->listAdapter.notifyDataSetChanged());
                                    }
                        });
                    })
                    .setNeutralButton("Edit", (dialog, which)->{
                        LinearLayout layout = new LinearLayout(this);
                        layout.setOrientation(LinearLayout.VERTICAL);
                        layout.setPadding(50, 40, 50, 10);
                        EditText input = new EditText(this);
                        input.setText(selectedSignalName); // prefill with current name
                        input.setSelection(selectedSignalName.length()); // put cursor at the end
                        layout.addView(input);
                        new AlertDialog.Builder(MainActivity.this)
                                .setTitle("Edit signal")
                                .setMessage("Enter the new name for "+selectedSignalName+":")
                                .setView(layout)
                                .setNegativeButton("Save", (dialog2, which2) -> {
                                    final String signalTypeNameSnapshot = signalTypeName;
                                    executor.execute(()->
                                    {
                                        String newSignalName = input.getText().toString().trim();
                                        if(!newSignalName.isBlank() && !signalExists(signalTypeNameSnapshot,newSignalName)) {
                                            ContentValues values = new ContentValues();
                                            values.put("name", newSignalName);
                                            int rowsUpdated;
                                            synchronized (DBlock) {
                                                rowsUpdated = db.update(
                                                        signalTypeNameSnapshot,          // table name
                                                        values,            // values to update
                                                        "name = ?",        // WHERE clause
                                                        new String[]{selectedSignalName} // arguments for ?
                                                );
                                            }
                                                if (rowsUpdated > 0) {
                                                    synchronized (lock)
                                                    {
                                                        int signalIndex = signalNames.indexOf(selectedSignalName);
                                                        signalNames.set(signalIndex, newSignalName);
                                                        signalNames.sort((s1, s2) -> s1.compareTo(s2));
                                                        runOnUiThread(() -> listAdapter.notifyDataSetChanged());
                                                    }
                                                }
                                        }
                                        else runOnUiThread(()->Toast.makeText(MainActivity.this,"Error: Unacceptable signal name!", Toast.LENGTH_SHORT).show());
                                    });
                                })
                                .setPositiveButton("Cancel", (dialog2, which2) -> {
                                    dialog2.dismiss();
                                })
                                .show();
                    })
                    .setPositiveButton("Cancel", (dialog, which) -> {
                        dialog.dismiss();
                    })
                    .show();
            return true; // return true if you handled the long click
        });
        signalTypeSpinner=findViewById(R.id.SignalTypeSpinner);
        ArrayAdapter<String> adapter = new ArrayAdapter<>(this,
                android.R.layout.simple_spinner_item,
                signalTypeNames);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        signalTypeSpinner.setAdapter(adapter);
        signalTypeSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                if(previousSpinnerPosition!=position) {
                    synchronized (lock) {
                        signalTypeName = getSignalTypeName(position);
                    }
                    selectedSignalType=getSignalType(signalTypeSpinner.getSelectedItemPosition());
                    if(selectedSignalType==SignalType.NFC)
                    {
                        capButton.setText("Capture UID");
                        sendButton.setText("Write UID");
                        searchButton.setText("Search UID");
                    }
                    else {
                        capButton.setText("Capture signal");
                        sendButton.setText("Send signal");
                        searchButton.setText("Search signal");
                    }
                    signalToSend.valid.set(false);
                    previousSpinnerPosition=position;
                    loadSignalNames(signalTypeName);
                }
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
            }
        });
        sendButton=findViewById(R.id.SendButton);
        sendButton.setOnClickListener((v)->sendHandler());
        //addButton=findViewById(R.id.AddButton);
        capButton=findViewById(R.id.CapButton);
        capButton.setOnClickListener((v)->captureHandler());
        searchButton=findViewById(R.id.SearchButton);
        searchButton.setOnClickListener((v)->searchHandler());
        //nfcButton=findViewById(R.id.IRToNFCButton);
        //subGhzButton=findViewById(R.id.IRToSubGhzButton);
        nameField=findViewById(R.id.NameEdit);
        statusField=findViewById(R.id.BTStatusText);
        batteryLevelField=findViewById(R.id.BatteryLevelText);
    }

    private int getSizeToRead(InputStream in) throws IOException
    {
        int sizeToReadInBytes = in.read();
        sizeToReadInBytes = Math.min(sizeToReadInBytes,BT_MSG_MAX_LEN);
        return sizeToReadInBytes;
    }

    private ContentValues getValuesToInsert(String name,String[] tokens,String typeName){
        ContentValues values=new ContentValues();
        values.put("name", name);
        switch(typeName) {
            case "ir":
                values.put("protocol", tokens[1]);
                values.put("code", tokens[2]);
                values.put("nbits", tokens[3]);
                break;
            case "nfc":
                values.put("uid",hexStrToUid(tokens[1]));
                break;
            case "ghz":
                values.put("frequency",tokens[1]);
                values.put("code",tokens[2]);
                break;
        }
        return values;
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        setContentView(R.layout.ir_activity);
        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.Signals), (v, insets) -> {
            Insets systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom);
            return insets;
        });
        signalToSend=new Signal(SignalType.IR);
        executor = ThreadPoolHelper.getThreadPoolExecutor();
        soundId = SoundPoolHelper.getSoundPool().load(this, R.raw.click, 1);
        dbHelper = SignalDBHelper.getInstance(this);
        db = dbHelper.getWritableDatabase();
        assignWidgets();
        loadSignalNames("IR");
        if (!checkBTPermissions())
            requestBTPermissions();
        else
            permissionsAlreadyGranted=true;
        executor.execute(()->
        {
            if(!permissionsAlreadyGranted) {
                try {
                    barrier.await();
                } catch (InterruptedException e) {
                    //throw new RuntimeException(e);
                }
            }
            if(checkBTPermissions())
            {
                Set<BluetoothDevice> pairedDevices = null;
                BluetoothAdapter btAdapter = BluetoothAdapter.getDefaultAdapter();
                pairedDevices = btAdapter.getBondedDevices();
                for (BluetoothDevice device : pairedDevices) {
                    Log.d("Urke",device.getName());
                    if (device.getName().equals("Pwny")) {
                        uuid = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");
                        while(true) {
                            try {
                                synchronized (BTlock) {
                                    btSocket = BTSocket.getSocket(device, uuid);
                                }
                                if(!BTConnected.get()) {
                                    updateStatus(true);
                                    BTConnected.set(true);
                                    synchronized (BTlock) {
                                        OutputStream out = btSocket.getOutputStream();
                                        out.write("bat\n".getBytes(StandardCharsets.UTF_8));
                                        out.flush();
                                    }
                                }
                                InputStream in = btSocket.getInputStream();
                                while (true) {
                                    int toRead = in.read();
                                    if(toRead>BT_MSG_MAX_LEN) {
                                        emptyInStream(in);
                                        continue;
                                    }
                                    if (toRead != -1) {
                                        byte[] recvBuffer = new byte[toRead];
                                        boolean readSuccess = readBTSocket(in, recvBuffer, toRead);
                                        String received = new String(recvBuffer);
                                        Log.d("Urke", received);
                                        if (readSuccess) {
                                            String[] tokens = received.split(":");
                                            String msgType = tokens[0];
                                            if ((msgType.equals("ir") && tokens.length == 4)
                                            || (msgType.equals("nfc") && tokens.length==2)
                                            ||(msgType.equals("ghz") && tokens.length==3)) {
                                                runOnUiThread(() -> {
                                                    if(currentDialog!=null)
                                                        currentDialog.dismiss();
                                                    LinearLayout layout = new LinearLayout(this);
                                                    layout.setOrientation(LinearLayout.VERTICAL);
                                                    layout.setPadding(50, 40, 50, 10);
                                                    EditText nameInput = new EditText(this);
                                                    layout.addView(nameInput);
                                                    new AlertDialog.Builder(MainActivity.this).setTitle("Save signal").
                                                            setMessage("Input the signal name please")
                                                            .setPositiveButton("Save", (dialog, which) -> {
                                                                signalNameToAdd = nameInput.getText().toString();
                                                                shouldSave = true;
                                                                saveSemaphore.release();
                                                                dialog.dismiss();
                                                            }).
                                                            setNegativeButton("Cancel", (dialog, which) -> {
                                                                shouldSave = false;
                                                                saveSemaphore.release();
                                                                dialog.dismiss();
                                                            }).setCancelable(false).setView(layout)
                                                            .show();
                                                });
                                                executor.execute(()->
                                                {
                                                    saveSemaphore.acquireUninterruptibly();
                                                    if (shouldSave) {
                                                        boolean signalExists;
                                                        String chosenSignalTypeName=msgType.toUpperCase();
                                                        synchronized (DBlock) {
                                                            signalExists = signalExists(chosenSignalTypeName, signalNameToAdd);
                                                        }
                                                        if (!signalExists && !signalNameToAdd.isBlank()) {
                                                            ContentValues values = getValuesToInsert(signalNameToAdd, tokens, msgType);
                                                            long inserted;
                                                            synchronized (DBlock) {
                                                                inserted = db.insert(chosenSignalTypeName, null, values);
                                                            }
                                                            if(inserted==-1)
                                                                runOnUiThread(() -> Toast.makeText(MainActivity.this, "Error: Could not insert entry into the database", Toast.LENGTH_SHORT).show());
                                                            else {
                                                                synchronized (lock) {
                                                                    if (signalTypeName.equals(chosenSignalTypeName)) {
                                                                        signalNames.add(signalNameToAdd);
                                                                        signalNames.sort((s1, s2) -> s1.compareTo(s2));
                                                                        runOnUiThread(() -> listAdapter.notifyDataSetChanged());
                                                                    }
                                                                }
                                                                Log.d("Urke", "Inserted " + inserted + " item(s)");
                                                            }
                                                        } else
                                                            runOnUiThread(() -> Toast.makeText(MainActivity.this, "Error: Could not save signal under a blank or already existent name!", Toast.LENGTH_SHORT).show());
                                                    }
                                                });
                                            }
                                            else if(msgType.equals("log") && tokens.length==2)
                                            {
                                                int logCode=Integer.parseInt(tokens[1]);
                                                //if(arrayContains(cancelLogCodes,logCode))
                                                    //cancelSemaphore.release();
                                                if(arrayContains(cancelLogCodes,logCode) || (logCode>=nfcLogCodeBeginIndex && logCode<=nfcLogCodeEndIndex)) {
                                                    runOnUiThread(() -> {
                                                        currentDialog.dismiss();
                                                    });
                                                    if(logCode==nfcWriteStartLogCode)
                                                        spawnProgBar(DialogType.WRITING_DIALOG);
                                                }
                                                else if (logCode==irCapStartLogCode || logCode==nfcUidReadStartLogCode || logCode==subghzCapStartLogCode)
                                                    spawnProgBar(DialogType.CAPTURING_DIALOG);
                                                
                                                this.log(logCode);
                                            }
                                            else if(msgType.equals("bat") && tokens.length==2)
                                            {
                                                String batLvl=tokens[1];
                                                updateBatteryLevel(batLvl);
                                            }
                                            Log.d("Urke", "Read everything!");
                                        } else {
                                            Log.d("Urke", "Message was too big!");
                                            runOnUiThread(() -> {
                                                Toast.makeText(MainActivity.this, "Message was too big!", Toast.LENGTH_SHORT).show();
                                            });
                                            throw new IOException();
                                        }
                                    }
                                    else {
                                        throw new IOException();
                                    }
                                }
                            } catch (IOException e) {
                                Log.e("Urke", "Bluetooth error: " + e.getMessage());
                                if(BTConnected.get()) {
                                    updateStatus(false);
                                    BTConnected.set(false);
                                }
                                synchronized (BTlock) {
                                    try {
                                        BTSocket.disconnect();
                                    } catch (IOException ex) {
                                        Log.e("Urke", "Bluetooth error: " + ex.getMessage());
                                    }
                                }
                                try {
                                    Thread.sleep(1000);
                                } catch (InterruptedException ex1) {
                                    Log.e("Urke",ex1.getMessage());

                                }
                            }
                        }
                    }
                }
            }
        });
    }
}