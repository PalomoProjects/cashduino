#!/usr/bin/env python2.7
"""
|
|   Data structure from PC to Arduino
|   |STX |COMMAND|DAT1|DAT2|ETX |CHECKSUM|
|   |0x02| 0x0A  |0x00|0x00|0x03|0x04    |
|
|   Data structure from Arduino to PC
|   |STX |COMMAND|DAT1|DAT2|ETX |CHECKSUM|
|   |0xF2| 0x0A  |0x00|0x00|0xF3|0x04    |
|
|Command description::
|   CMD_COIN_OUT::
|       DAT1:: # of coins
|       DAT2:: channel
"""

import serial
import signal
import time
import thread
import traceback
import sys
from sys import platform


PLATFORM_LINUX="linux2"
PLATFORM_WIND="win32"

"""
| Settings for serial connection
"""
if (platform == PLATFORM_LINUX):
    SERIAL_PORT = "/dev/ttyACM1"
elif (platform == PLATFORM_WIND):
    SERIAL_PORT = "COM1"
else:
    print("Platform not supported : ( ... ")
    quit()

BAUD_RATE = "115200"

"""
| Definition of data structure for data
| exchange between PC and Arduino
"""
STR_STX='f2'
STR_ETX='f3'
CMD_STX='\x02'
CMD_ETX='\x03'
CMD_ENQUIRY         = '\x02\x0a\x00\x00\x03\x0d'
CMD_COIN_ENABLE     = '\x02\xa0\xff\xff\x03\xa1'
CMD_COIN_DISABLE    = '\x02\xa0\x00\x00\x03\xa3'
CMD_COIN_AUDIT      = '\x02\xa1\x00\x00\x03\xa4'
CMD_COIN_OUT        = '\x02\xa2\x01\x02\x03\xa8'

"""
| command list from Arduino
| hexadecimal data in string format
"""
ARDUINO_ENQUIRY         = '0a'
ARDUINO_COIN_ENABLE     = 'a0'
ARDUINO_COIN_READ       = 'a1'
ARDUINO_COIN_OUT        = 'a2'
ARDUINO_COIN_INSERTED   = 'b0'
ARDUINO_PULL_BUFFER     = 'ff'

"""
| data information
| CMD, DAT1, DAT2
"""
cashduino_payload=[0,0,0]
cashduino_total_coins=[0,0,0,0]
cashduino_total_cashbox=0

"""
| Interrup signal
"""
def handler(signum, frame):
    print('Signal handler called with signal', signum)
    signal.signal(signal.SIGABRT, handler)
    sys.exit("you killed me!")

"""
| This function read the serial data and
| define the Ardiono information.
"""
def cashduino_parsing_data(data):
    array=[]
    length=len(data)
    index=length/2
    checksum=0
    #print ("Serial Data:: " + data)#Debug
    #print ("Data Length:: " + str(length))#Debug
    for i in range(0, index):
        array.append(data[i*2] + data[(i*2)+1])

    if(index >= 6):
        checksum = (int(array[1],16) + int(array[2],16) + int(array[3],16) + int(array[4],16)) & 0xFF
        #print("checksum:: " + str(checksum)) #Debug
        if( (array[0]==STR_STX) and (array[4]==STR_ETX) and (int(array[5],16)==checksum) ):
            cashduino_payload[0] = int(array[1],16)
            cashduino_payload[1] = int(array[2],16)
            cashduino_payload[2] = int(array[3],16)
            print("CASHDUINO PAYLOAD:: CMD:: " + str(hex(cashduino_payload[0])) + \
            " DAT1:: " + str(hex(cashduino_payload[1])) + \
            " DAT2:: " + str(hex(cashduino_payload[2])))#Debug
            cashduino_process_data()
    else:
        cashduino_default_buffer()
        print("LENGTH ERROR TRY AGAIN")

"""
| This function set the buffer used to
| read the data from Arduino/CashDuino
"""
def cashduino_default_buffer():
    cashduino_payload[0] = 0xFF
    cashduino_payload[1] = 0x00
    cashduino_payload[2] = 0x00

"""
| This function set to default the array used to
| save the data values from coin device
"""
def cashduino_default_coins():
    cashduino_total_cashbox  = 0
    cashduino_total_coins[0] = 0x00
    cashduino_total_coins[1] = 0x00
    cashduino_total_coins[2] = 0x00
    cashduino_total_coins[3] = 0x00


"""
| This function read the Arduino Serial Port
"""
def cashduino_reception():
    while 1:
        try:
            RX1 = str(arduino_serial.read(12).encode('hex'))
            if(RX1!=''):
                cashduino_parsing_data(RX1)
                RX1=''
        except Exception:
            traceback.print_exc()

"""
| This function read the possible command from
| cashduino
"""
def cashduino_process_data():
    if (cashduino_payload[0] == 0x0a):
        print("READ ENQUIRY")
    if (cashduino_payload[0] == 0xa0):
        print("READ COIN ENABLE")
    if (cashduino_payload[0] == 0xa1):
        print("READ COIN AUDIT")
    if (cashduino_payload[0] == 0xa2):
        print("READ COIN OUT")
    if (cashduino_payload[0] == 0xb0):
        print("READ COIN EVENT")
        cashduino_process_coin()

"""
| This function read the payload and assign the currency
| value regarding each channel
"""
def cashduino_process_coin():
    if(cashduino_payload[1]==0x01):
        print("PULL LEVER")
    if(cashduino_payload[1]==0x40):
        print("$0.50 INSERTED - CASHBOX")
    if(cashduino_payload[1]==0x41):
        print("$0.50 INSERTED - CASHBOX")
    if(cashduino_payload[1]==0x42):
        print("$1 INSERTED - CASHBOX")
    if(cashduino_payload[1]==0x43):
        print("$2 INSERTED - CASHBOX")
    if(cashduino_payload[1]==0x44):
        print("$5 INSERTED - CASHBOX")
    if(cashduino_payload[1]==0x45):
        print("$10 INSERTED - CASHBOX")
    if(cashduino_payload[1]==0x52):
        print("$1 INSERTED - TUBE")
    if(cashduino_payload[1]==0x53):
        print("$2 INSERTED - TUBE")
    if(cashduino_payload[1]==0x54):
        print("$5 INSERTED - TUBE")
    if(cashduino_payload[1]==0x55):
        print("$10 INSERTED - TUBE")
    if(cashduino_payload[1]==0x92):
        print("$1 DELIVERED")
    if(cashduino_payload[1]==0x93):
        print("$2 DELIVERED")
    if(cashduino_payload[1]==0x94):
        print("$5 DELIVERED")
    if(cashduino_payload[1]==0x95):
        print("$10 DELIVERED")

"""
| This funtion deliver coins to the user
"""
def cashduino_coins_out(amount):
    """
    | Build this data frame for coin out
    |   0x02 0xa2 DAT1 DAT2 x03 CHECKSUM
    |
    |Command description::
    |   CMD_COIN_OUT::
    |       DAT1:: # of coins
    |       DAT2:: channel
    | channel = 02 -> $1
    | channel = 03 -> $2
    | channel = 04 -> $5
    | channel = 05 -> $10
    """
    channel_id=[5,4,3,2]
    coin_out  =[0,0,0,0]
    divisa    =[10,5,2,1]
    var_loop=True
    var_counter=0
    var_return=False
    audit_flag=False
    audit_flag = cashduino_coins_audit()
    if(audit_flag == True):
        print ("COIN AUID WAS SUCCESS ... : )")
        for index in range(4):
            if(cashduino_total_coins[index] > 0):
                coin_out[index] = int(amount) / int(divisa[index]);
                if(coin_out[index] > cashduino_total_coins[index]):
                    coin_out[index] = int(cashduino_total_coins[index]);
                amount = int(amount) - int(( divisa[index] * coin_out[index]))
            #Only for Debug
            #print(str(index) + " " + str(coin_out[index]) + " " \
            #+  str(amount) + " " + str(cashduino_total_coins[index]) + " " \
            #+ str(divisa[index]))
        for index in range(4):
            if(coin_out[index]>0):
                dat1 = str(coin_out[index])
                if (len(dat1)==1):
                    dat1 = '0' + str(coin_out[index])
                dat2 = str(channel_id[index])
                if (len(dat2)==1):
                    dat2 = '0' + str(channel_id[index])
                checksum = (int('0xa2',16) + \
                int(str(dat1),16) + \
                int(str(dat2),16) + \
                int('0x03',16)) & int('0xff',16)
                checksum = hex(checksum).split('x')[1]
                cmd_aux = '02a2' + dat1 + dat2 + '03' + checksum
                #print(cmd_aux)#Debug
                var_loop=True
                var_return=False
                var_counter=0
                while(var_loop==True):
                    cashduino_default_buffer()
                    var_counter=var_counter+1
                    #print("SEND:: " + cmd_aux)#Debug
                    arduino_serial.write(cmd_aux.decode('hex'))
                    time.sleep(1)
                    if(cashduino_payload[0]!=0xFF):
                        var_loop=False
                        var_return=True
                    if (var_counter==5):
                        var_loop=False
                        var_return=False
                        return var_return
    else:
        print ("COIN AUID WAS FAILED ...  : (")
    return var_return
"""
| This function audit the coin changer and read read
| the amount of coin in each tube
"""
def cashduino_coins_audit():
    """
    | The id belos is the index array in
    | the arduino buffer, this buffer came
    | from cashduino
    | id = 06 -> $1
    | id = 07 -> $2
    | id = 08 -> $5
    | id = 09 -> $10
    """
    channel_id=[3,4,5,6,7,8,9,10]
    """
    | Build this data frame for coin audit
    |   0x02 0xa1 DAT1 DAT2 x03 CHECKSUM
    |
    |Command description::
    |   CMD_COIN_AUDIT::
    |       DAT1:: id
    |       DAT2:: id
    """
    var_loop=True
    var_counter=0
    var_return=False
    cashduino_default_coins()
    for index in range(4):
        dat1 = str(channel_id[index*2])
        if (len(dat1)==1):
            dat1 = '0' + str(channel_id[index*2])
        dat2 = str(channel_id[(index*2)+1])
        if (len(dat2)==1):
            dat2 = '0' + str(channel_id[(index*2)+1])
        checksum = (int('0xa1',16) + \
        int(str(dat1),16) + \
        int(str(dat2),16) + \
        int('0x03',16)) & int('0xff',16)
        checksum = hex(checksum).split('x')[1]
        cmd_aux = '02a1' + dat1 + dat2 + '03' + checksum
        #print(cmd_aux)#Debug
        var_loop=True
        var_return=False
        var_counter=0
        while(var_loop==True):
            cashduino_default_buffer()
            var_counter=var_counter+1
            arduino_serial.write(cmd_aux.decode('hex'))
            time.sleep(1)
            if(cashduino_payload[0]!=0xFF):
                if( int(str(dat2),16) == channel_id[3]):
                    cashduino_total_coins[3]=cashduino_payload[2]
                if( int(str(dat1),16) == channel_id[4]):
                    cashduino_total_coins[2]=cashduino_payload[1]
                if( int(str(dat2),16) == channel_id[5]):
                    cashduino_total_coins[1]=cashduino_payload[2]
                if( int(str(dat1),16) == channel_id[6]):
                    cashduino_total_coins[0]=cashduino_payload[1]
                var_loop=False
                var_return=True
            if (var_counter==5):
                var_loop=False
                var_return=False
                return var_return
    if(var_return == True):
        print("$10:: " + str(cashduino_total_coins[0]) + \
        " $5:: " + str(cashduino_total_coins[1]) + \
        " $2:: " + str(cashduino_total_coins[2]) + \
        " $1:: " + str(cashduino_total_coins[3]))
        cashduino_total_cashbox = cashduino_total_coins[0] * 10 + \
        cashduino_total_coins[1] * 5 + \
        cashduino_total_coins[2] * 2 + \
        cashduino_total_coins[3] * 1
        print("TOTAL IN CASHBOX:: $" + str(cashduino_total_cashbox))
    return var_return

"""
| This function read the user input
"""
def reading_keyboard_input():
    while (1):
        try:
            option = int(input("Type an option: "))
            #print("option: {0}\n".format(option))
            if option == 1:
                print("SEND:: ENQUIRY")
                arduino_serial.write(CMD_ENQUIRY)
            elif option == 2:
                print("SEND:: COIN ENABLE")
                arduino_serial.write(CMD_COIN_ENABLE)
            elif option == 3:
                print("SEND:: COIN DISABLE")
                arduino_serial.write(CMD_COIN_DISABLE)
            elif option == 4:
                print("SEND:: COINI AUDIT")
                if(cashduino_coins_audit() == True):
                    print ("COIN AUID WAS SUCCESS ... : )")
                else:
                    print ("COIN AUID WAS FAILED ...  : (")
            elif option == 5:
                amount_to_out = int(input("Type the amount to deliver: "))
                print("SEND:: COINI OUT - AMOUNT:: $" + str(amount_to_out))
                cashduino_coins_out(amount_to_out)
            else:
                print("TYPE A VALID SELECTION FROM 1 TO 5")
            time.sleep(1)
        except Exception:
            traceback.print_exc()

"""
| Start point fot this script
"""
if __name__ == "__main__":

    """
    | platform = linux2
    | platform = wind32
    """

    """
    | setting serial communication and open port
    """
    try:
        arduino_serial = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout = 0.5)
        arduino_serial.flush()
        RX1= ''
        print ("Serial Port for Arduino is:: " + str(arduino_serial.isOpen()))
    except Exception as e:
        traceback.print_exc()
        quit()

    """
    | catch interrupt signal from keyboard
    """
    if (platform == PLATFORM_LINUX):
        signal.signal(signal.SIGABRT, handler)
        signal.signal(signal.SIGHUP, handler)
        signal.signal(signal.SIGINT, handler)
        signal.signal(signal.SIGQUIT, handler)
    elif (platform == PLATFORM_WIND):
        SERIAL_PORT = "COM7"
    else:
        print("Platform not supported : ( ... ")
        quit()

    """
    | start thread for capture keyboard input from user
    """
    thread.start_new_thread(reading_keyboard_input,())

    """
    | start thread for capture Arduino data
    """
    thread.start_new_thread(cashduino_reception,())

    while(1):
        PASS=1
