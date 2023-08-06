/*
 * Description: sketch for exchange data between PC and Arduino
 * Author: Jesus Palomo (jesus@palomothings.com)
 * Project: arduino serial controller
 */

#include <Wire.h>
#include <avr/io.h>
#include <string.h>
#include <stdlib.h>

#define MAX_BUFF_CASHDUINO      16
#define MAX_BUFF_ARDUINO        14
#define MAX_BUFF_PC_PAYLOAD     4
#define CASH_DUINO_ADDR         8
#define KEY_1                   0x33
#define KEY_2                   0xCC
#define NONE                    0xFF
#define MAX_BUFF_SERIAL_MSG     32
#define TOTAL_BYTE_SER_MSG      6
#define STX_PC                  0x02
#define ETX_PC                  0x03
#define POLLING_TIME            8000

/* PIN arduino mega board led */
#define PIN_LED               13

/* ###################### USED FOR CASHDUINO COMMUNICATION ###################### */

/* Send command to CashDuino over TWI/I2C */
typedef enum{
    ARDUINO_CMD_ENQUIRY       = 0xC1,
    ARDUINO_CMD_COIN_ENABLE   = 0xA1,
    ARDUINO_CMD_COIN_AUDIT    = 0xA2,
    ARDUINO_CMD_COIN_OUT      = 0xA3
}cmd_arduino;

/* Read command from CashDuino over TWI/I2C */
typedef enum{
    CASHDUINO_CMD_ENQUIRY     = 0xD1,
    CASHDUINO_CMD_COIN_ENABLE = 0xF1,
    CASHDUINO_CMD_COIN_AUDIT  = 0xF2,
    CASHDUINO_CMD_COIN_OUT    = 0xF3,
    CASHDUINO_CMD_COIN_EVENT  = 0xF4
}cmd_cashduino;

/* ###################### USED FOR CASHDUINO COMMUNICATION ###################### */

/* ###################### USED FOR PC SERIAL COMMUNICATION ###################### */

/* states to asign when a PC command is received from serial port */
typedef enum{
    PC_ENQUIRY_BOARD   = 0x0A,
    PC_COIN_ENABLE     = 0xA0,
    PC_COIN_READ       = 0xA1,
    PC_COIN_OUT        = 0xA2,
    PC_PULL_BUFFER     = 0xFF,
    PC_INVALID_OPTION  = 0x00
}pc_receive;

/* Commands for send to PC over USB-Serial port (Arduino Mega)  */
typedef enum{
    ARDUINO_PC_ENQUIRY         = 0x0A,
    ARDUINO_PC_COIN_ENABLE     = 0xA0,
    ARDUINO_PC_COIN_READ       = 0xA1,
    ARDUINO_PC_COIN_OUT        = 0xA2,
    ARDUINO_PC_COIN_INSERTED   = 0xB0,
    ARDUINO_PC_PULL_BUFFER     = 0xFF,
    ARDUINO_PC_INVALID_OPTION  = 0x00
}pc_send;

/* ###################### USED FOR PC SERIAL COMMUNICATION ###################### */

/* data structure for data exchange  */
typedef struct{
    uint8_t arduino_task[MAX_BUFF_ARDUINO];
    uint8_t pc_payload[MAX_BUFF_PC_PAYLOAD];
    pc_receive pc_option;
}task;

/* global variables */
static uint8_t new_event  = NONE;
static uint8_t command    = 0x00;

/* This functios establish data communication between Arduino and Cashduino */
static bool cashduino_valid_package(uint8_t *rx_cashduino);
static bool cashduino_new_event(uint8_t *rx_cashduino);
static void cashduino_task(task *data, pc_receive pc_option);
static void cashduino_flush_task(uint8_t *cmd);
static void cashduino_read_buffer(uint8_t *rx_cashduino);
static uint8_t cashduino_parsing_buffer(uint8_t *rx_cashduino);

/* This functions establish data communication between Arduino and PC */
static pc_receive serial_data_receiver(task *data);
static void serial_data_transmitter(task *data, pc_send pc_option);
static void serial_data_processing(task *data);
static void serial_data_flush();

void setup() {
    /* join i2c bus (address optional for master) */
    Wire.begin();

    /* put your setup code here, to run once: */
    Serial.begin(115200);

    /* just for indicate polling process */
    pinMode(PIN_LED, OUTPUT);
}

void loop() {

    /* initialize local variables */
    uint8_t rx_cashduino[MAX_BUFF_CASHDUINO]  = {0};
    uint8_t cashduino_cmd                     = NONE;
    uint8_t polling                           = 0;
    task data;

    /* initialize variables */
    memset(data.arduino_task, 0x00, sizeof(data.arduino_task));
    memset(data.pc_payload, 0x00, sizeof(data.pc_payload));
    data.pc_option = PC_INVALID_OPTION;

    delay(5000); /*delay for CASHDUINO power up (not mandatory, just a recommendation)*/

    Serial.flush(); /*clean Serial Buffer*/
    serial_data_flush();

    while(1)
    {

        /* led to indicate polling time */
        polling ^= 1;
        digitalWrite(PIN_LED, polling ? HIGH : LOW);

        /* ###################### CASHDUINO ###################### */
        /* get cashduino buffer from I2C protocol */
        cashduino_read_buffer(rx_cashduino);

        /* verify if the data is new */
        if(cashduino_new_event(rx_cashduino) == true)
        {
            /* is a valid package? */
            if(cashduino_valid_package(rx_cashduino) == true)
            {
                /* parsing buffer to find data */
                cashduino_cmd = cashduino_parsing_buffer(rx_cashduino);

                switch(cashduino_cmd)
                {
                    case CASHDUINO_CMD_ENQUIRY:
                        serial_data_transmitter(&data, ARDUINO_PC_ENQUIRY);
                        break;
                    case CASHDUINO_CMD_COIN_ENABLE:
                        serial_data_transmitter(&data, ARDUINO_PC_COIN_ENABLE);
                        break;
                    case CASHDUINO_CMD_COIN_AUDIT:
                        data.pc_payload[0]=rx_cashduino[data.pc_payload[0]];
                        data.pc_payload[1]=rx_cashduino[data.pc_payload[1]];
                        serial_data_transmitter(&data, ARDUINO_PC_COIN_READ);
                        break;
                    case CASHDUINO_CMD_COIN_OUT:
                        serial_data_transmitter(&data, ARDUINO_PC_COIN_OUT);
                        break;
                    case CASHDUINO_CMD_COIN_EVENT:
                        data.pc_payload[0]=rx_cashduino[1];
                        data.pc_payload[1]=rx_cashduino[2];
                        serial_data_transmitter(&data, ARDUINO_PC_COIN_INSERTED);
                        break;
                }
            }
        }
    /* ###################### CASHDUINO ###################### */
        /* processing PC request */
        serial_data_processing(&data);
    }
}

/* this function identify a pc command and forward to cashduino */
static void serial_data_processing(task *data)
{
    unsigned int i = 0;

    pc_receive pc_option = PC_INVALID_OPTION;

    /* time to attend another process */
    for(i = 0; i < POLLING_TIME; i ++)
    {
        pc_option = serial_data_receiver(data);

        switch(pc_option)
        {
            case PC_ENQUIRY_BOARD:
                cashduino_task(data, PC_ENQUIRY_BOARD);
                break;
            case PC_COIN_ENABLE:
                cashduino_task(data, PC_COIN_ENABLE);
                break;
            case PC_COIN_READ:
                cashduino_task(data, PC_COIN_READ);
                break;
            case PC_COIN_OUT:
                cashduino_task(data, PC_COIN_OUT);
                break;
            case PC_PULL_BUFFER:
                serial_data_transmitter(data, ARDUINO_PC_PULL_BUFFER);
                break;
            case PC_INVALID_OPTION:
                /* do nothing */
                break;
        }

    }

}

/* parsing a PC data frame identify the command type */
static pc_receive serial_data_receiver(task *data)
{
    uint8_t checksum = 0;
    uint8_t serial_data[MAX_BUFF_SERIAL_MSG] = {0};
    uint8_t serial_index = 0;
    data->pc_option = PC_INVALID_OPTION;

    if(Serial.available() == TOTAL_BYTE_SER_MSG)
    {
        while(Serial.available())
        {
            serial_data[serial_index] = Serial.read();
            serial_index++;
            /* safety function to prevent overflow */
            if(serial_index >= MAX_BUFF_SERIAL_MSG)
            {
                serial_index = 0;
            }
        }
        Serial.flush();
        serial_data_flush();
    }

    if ( serial_index == TOTAL_BYTE_SER_MSG )
    {
        if ( ( serial_data[0] == STX_PC) && ( serial_data[4] == ETX_PC ) )
        {
            checksum = ( serial_data[1] + serial_data[2] + serial_data[3] + serial_data[4] ) & 0xFF;

            if( serial_data[5] == checksum )
            {
                data->pc_option = (pc_receive) serial_data[1];
                data->pc_payload[0] = serial_data[2];
                data->pc_payload[1] = serial_data[3];
                serial_index = 0;
            }
            else
            {
                data->pc_option = PC_PULL_BUFFER;
                serial_index = 0;
            }
        }
        else
        {
            data->pc_option = PC_PULL_BUFFER;
            serial_index = 0;
        }
    }

    return data->pc_option;

}

/* send data to PC using a custom data frame */
static void serial_data_transmitter(task *data, pc_send pc_option)
{
    uint8_t arduino_pc[TOTAL_BYTE_SER_MSG] = {0};
    static uint8_t arduino_pc_aux[TOTAL_BYTE_SER_MSG] = {0};
    unsigned int checksum = 0;

    arduino_pc[1] = pc_option;

    arduino_pc[0] = 0xF2;
    arduino_pc[2] = data->pc_payload[0];
    arduino_pc[3] = data->pc_payload[1];
    arduino_pc[4] = 0xF3;

    checksum = arduino_pc[1];
    checksum += arduino_pc[2];
    checksum += arduino_pc[3];
    checksum += arduino_pc[4];

    arduino_pc[5] = checksum & 0xFF;
    Serial.write(arduino_pc, sizeof(arduino_pc));
    memcpy(arduino_pc_aux, arduino_pc, TOTAL_BYTE_SER_MSG);
    Serial.flush();
    serial_data_flush();
    memset(data->pc_payload, 0, sizeof(data->pc_payload));
}

static void serial_data_flush()
{
    while (Serial.read() >= 0)
    ; /* do nothing */
}

/* FUNCTIONS TO CONTROL COMMUNICATION BETWEEN ARDUINO AND CASHDUINO  */

/* function to validate the data structure */
static bool cashduino_valid_package(uint8_t *rx_cashduino)
{
    if( (rx_cashduino[11] == KEY_1) && (rx_cashduino[12] == KEY_2) &&
      (rx_cashduino[13] == KEY_1) && (rx_cashduino[14] == KEY_2) &&
      (rx_cashduino[15] == KEY_1) )
    {
        return true;
    }
    return false;
}

/* function to update the event counter */
static bool cashduino_new_event(uint8_t *rx_cashduino)
{
    if( (command != rx_cashduino[0]) || ( new_event != rx_cashduino[2] ) )
    {
        command = rx_cashduino[0];
        new_event = rx_cashduino[2];
        return true;
    }
    return false;
}

/* function to push data over TWI */
static void cashduino_task(task *data, pc_receive pc_option)
{

    /* copy key */
    data->arduino_task[9] = KEY_1;
    data->arduino_task[10] = KEY_2;
    data->arduino_task[11] = KEY_1;
    data->arduino_task[12] = KEY_2;
    data->arduino_task[13] = KEY_1;

    switch(pc_option)
    {
        case PC_ENQUIRY_BOARD:
            data->arduino_task[0] = ARDUINO_CMD_ENQUIRY;
            break;
        case PC_COIN_ENABLE:
            data->arduino_task[0] = ARDUINO_CMD_COIN_ENABLE;
            data->arduino_task[2] = data->pc_payload[0];
            data->arduino_task[4] = data->pc_payload[1];
            break;
        case PC_COIN_READ:
            data->arduino_task[0] = ARDUINO_CMD_COIN_AUDIT;
            break;
        case PC_COIN_OUT:
            data->arduino_task[0] = ARDUINO_CMD_COIN_OUT;
            data->arduino_task[1] = data->pc_payload[0];
            data->arduino_task[2] = data->pc_payload[1];
            break;
        case PC_PULL_BUFFER:
            data->arduino_task[0] = ARDUINO_CMD_ENQUIRY;
            break;
        case PC_INVALID_OPTION:
            break;
    }

    /* send data to CashDuino */
    cashduino_flush_task(data->arduino_task);

    /* clean up */
    memset(data->arduino_task, 0, sizeof(data->arduino_task));

    }

/* flushing data over I2C buffer */
static void cashduino_flush_task(uint8_t *cmd)
{
    uint8_t i = 0;
    Wire.beginTransmission(CASH_DUINO_ADDR);  /* transmit to CashDuino */
    for(i = 0; i<MAX_BUFF_ARDUINO; i++)
    {
        Wire.write(cmd[i]);                   /* send byte */
    }
    Wire.endTransmission();                   /* stop transmitting */
}

/* function to pull the buffer from the cashduino */
static void cashduino_read_buffer(uint8_t *rx_cashduino)
{
    Wire.requestFrom(CASH_DUINO_ADDR, MAX_BUFF_CASHDUINO);      /* request 6 bytes from slave CashDuino */
    while (Wire.available())
    {
        *(rx_cashduino++) = Wire.read();                       /* receive a byte as character */
    }
}

/* function to parsing the cashduino buffer and get information */
static uint8_t cashduino_parsing_buffer(uint8_t *rx_cashduino)
{
    uint8_t ret_code = NONE;

    if(rx_cashduino[0] != NONE)
    {
        ret_code = rx_cashduino[0];
    }

    return ret_code;
}
