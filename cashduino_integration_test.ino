#include <Wire.h>

#define MAX_BUFF_CASHDUINO    16
#define MAX_BUFF_ARDUINO      14
#define CASH_DUINO_ADDR       8
#define KEY_1                 0x33
#define KEY_2                 0xCC
#define NONE                  0xFF
#define DISABLE_COIN_DEVICE   2
#define DISABLE_BILL_DEVICE   6
#define CASHDUINO_POLL_TIME   250

#define CASHDUINO_COIN_AUDIT  0xF2
#define CASHDUINO_COIN_EVENT  0xF4
#define CASHDUINO_BILL_EVENT  0xE5


uint8_t new_event  = 0xFF;
uint8_t command    = 0x00;

double  amount     = 0;
uint8_t cent       = 0;
uint8_t un_peso    = 0;
uint8_t dos_peso   = 0;
uint8_t cinco_peso = 0;
uint8_t diez_peso  = 0;

/*unsigned int CMD_BILL_AUDIT[]           = {0xB3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0xCC, 0x33, 0xCC, 0x33};*/
/*unsigned int CMD_BILL_OUT[]             = {0xB4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0xCC, 0x33, 0xCC, 0x33};*/

void setup() {
  // join i2c bus (address optional for master)
  Wire.begin();
  // put your setup code here, to run once:
  Serial.begin(9600);
}

void loop() {
 uint8_t MATRIX_CMD [][14] = {{0xC1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0xCC, 0x33, 0xCC, 0x33},
                             {0xA1, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x33, 0xCC, 0x33, 0xCC, 0x33},
                             {0xA1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0xCC, 0x33, 0xCC, 0x33},
                             {0xA2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0xCC, 0x33, 0xCC, 0x33},
                             {0xA3, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0xCC, 0x33, 0xCC, 0x33},
                             {0xB1, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x33, 0xCC, 0x33, 0xCC, 0x33},
                             {0xB1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0xCC, 0x33, 0xCC, 0x33},
                             {0xB2, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0xCC, 0x33, 0xCC, 0x33},
                             {0xB2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0xCC, 0x33, 0xCC, 0x33}
                            };
 /* type in the send box section the selecction that you wants */
 char MATRIX_MESSAGE[][38] = {"ARDUINO SEND ENQUIRY BOARD > ",        /* option 1 */
                              "ARDUINO SEND COIN ENABLE TASK > ",     /* option 2 */
                              "ARDUINO SEND COIN DISABLE TASK > ",    /* option 3 */
                              "ARDUINO SEND COIN AUDIT TASK > ",      /* option 4 */
                              "ARDUINO SEND COIN OUT TASK > ",        /* option 5 */
                              "ARDUINO SEND BILL ENABLE TASK > ",     /* option 6 */
                              "ARDUINO SEND BILL DISABLE TASK > ",    /* option 7 */
                              "ARDUINO SEND BILL TO STACKER TASK > ", /* option 8 */
                              "ARDUINO SEND BILL REJECT TASK >"       /* option 9 */
                             };
  uint8_t rx_cashduino[MAX_BUFF_CASHDUINO] = {0};
  uint8_t pc_option      = NONE;
  uint8_t bill_cmd       = NONE;
  uint8_t coin_cmd       = NONE;

  /* lisent the serial port and sends task */
  if ( ( pc_option = read_buffer_from_serial() ) != NONE ){
    if( (pc_option == DISABLE_COIN_DEVICE) || (pc_option == DISABLE_BILL_DEVICE) ) {
      amount = 0;
    }
    send_task_to_cashduino(MATRIX_MESSAGE[pc_option], MATRIX_CMD[pc_option]);
  }

  /* read the CashDuino buffer */
  read_buffer_from_cashduino(rx_cashduino);

  /* is a new event? */
  if ( looking_for_new_event(rx_cashduino) == true ){
    /* is a valid package? */
    if( is_a_valid_package(rx_cashduino) == true ){

      parsing_buffer_cashduino(rx_cashduino);
      coin_cmd = parsing_buffer_coin_acceptor(rx_cashduino, &amount, &cent, &un_peso, &dos_peso, &cinco_peso, &diez_peso);
      bill_cmd = parsing_buffer_bill_acceptor(rx_cashduino, &amount);

      if ( ( coin_cmd == CASHDUINO_COIN_EVENT) || ( bill_cmd == CASHDUINO_BILL_EVENT) ){
        Serial.print("Cash: $ " );
        Serial.println(amount, 2);
      }else if(coin_cmd == CASHDUINO_COIN_AUDIT){
        Serial.print(cent);Serial.print("($0.50 MXN)\n");
        Serial.print(un_peso);Serial.print("($1 MXN)\n");
        Serial.print(dos_peso);Serial.print("($2 MXN)\n");
        Serial.print(cinco_peso);Serial.print("($5 MXN)\n");
        Serial.print(diez_peso);Serial.print("($10 MXN)\n");
      }
    }
  }

  delay(CASHDUINO_POLL_TIME);

}

/* function to validate the data structure */
bool is_a_valid_package(uint8_t *rx_cashduino){
  if( (rx_cashduino[11] == KEY_1) && (rx_cashduino[12] == KEY_2) &&
      (rx_cashduino[13] == KEY_1) && (rx_cashduino[14] == KEY_2) &&
      (rx_cashduino[15] == KEY_1) ){
    return true;
  }
  return false;
}

/* function to know if the board is working */
void parsing_buffer_cashduino(uint8_t *rx_cashduino){
  /* type of command */
  switch(rx_cashduino[0]){
    case 0xD1:
      /*Serial.print("CashDuino is a live :) \n" );//debug*/
      break;
  }
}

/* function to parsing the cashduino buffer and get information */
uint8_t parsing_buffer_coin_acceptor(uint8_t *rx_cashduino, double *amount,
                                            uint8_t *channel0, uint8_t *channel1,
                                            uint8_t *channel2, uint8_t *channel3,
                                            uint8_t *channel4){

  uint8_t ret_code = NONE;
  /* type of command */
  switch(rx_cashduino[0]){
    case 0xF1:
      /*Serial.print("Coin charger configured \n" ); //debug*/
      break;
    case CASHDUINO_COIN_AUDIT:
      ret_code = CASHDUINO_COIN_AUDIT;
      /*Serial.print("Event Counter: ");Serial.print(rx_cashduino[2]);Serial.print("\n");
      Serial.print("Channel 1: ");Serial.print(rx_cashduino[3]);Serial.print("($0.50 MXN)\n");
      Serial.print("Channel 2: ");Serial.print(rx_cashduino[4]);Serial.print("($1 MXN)\n");
      Serial.print("Channel 3: ");Serial.print(rx_cashduino[5]);Serial.print("($2 MXN)\n");
      Serial.print("Channel 4: ");Serial.print(rx_cashduino[6]);Serial.print("($5 MXN)\n");
      Serial.print("Channel 5: ");Serial.print(rx_cashduino[7]);Serial.print("($10 MXN)\n");
      Serial.print("Channel 6: ");Serial.print(rx_cashduino[8]);Serial.print("\n");
      Serial.print("Channel 7: ");Serial.print(rx_cashduino[9]);Serial.print("\n");//debug*/
      *(channel0) = rx_cashduino[3];
      *(channel1) = rx_cashduino[4];
      *(channel2) = rx_cashduino[5];
      *(channel3) = rx_cashduino[6];
      *(channel4) = rx_cashduino[7];
      break;
    case 0xF3:
      /*Serial.print("Coin changer dispense coin \n" );*/
      break;
     case CASHDUINO_COIN_EVENT:
      /*Serial.print("Event Counter: ");Serial.print(rx_cashduino[2]);Serial.print("\n"); //debug*/
      if( (rx_cashduino[1] >= 0x40) && (rx_cashduino[1] <= 0x4F)){
        ret_code = CASHDUINO_COIN_EVENT;
        /*Serial.print("Coin in CashBox =) -> " ); //debug*/
        switch (rx_cashduino[1]){
          case 0x41:
            /*Serial.print("($0.50 MXN) Inserted\n"); //debug*/
            *(amount) += 0.5;
            break;
          case 0x42:
            /*Serial.print("($1 MXN) Inserted\n"); //debug*/
            *(amount) += 1;
            break;
          case 0x43:
            /*Serial.print("($2 MXN) Inserted\n"); //debug*/
            *(amount) += 2;
            break;
          case 0x44:
            /*Serial.print("($5 MXN) Inserted\n"); //debug*/
            *(amount) += 5;
            break;
          case 0x45:
            /*Serial.print("($10 MXN) Inserted\n"); //debug*/
            *(amount) += 10;
            break;
        }
      }
      if( (rx_cashduino[1] >= 0x50) && (rx_cashduino[1] <= 0x5F)){
        ret_code = CASHDUINO_COIN_EVENT;
        /*Serial.print("Coin in Tube =) -> " ); //debug*/
        switch (rx_cashduino[1]){
          case 0x51:
            /*Serial.print("$0.50 MXN\n"); //debug*/
            *(amount) += 0.5;
            break;
          case 0x52:
            /*Serial.print("$1 MXN\n"); //debug*/
            *(amount) += 1;
            break;
          case 0x53:
            /*Serial.print("$2 MXN\n"); //debug*/
            *(amount) += 2;
            break;
          case 0x54:
            /*Serial.print("$5 MXN\n"); //debug*/
            *(amount) += 5;
            break;
          case 0x55:
            /*Serial.print("$10 MXN\n");//debug*/
            *(amount) += 10;
            break;
          break;
        }
      }
      if(rx_cashduino[1] == 0x01){
        /*Serial.print("Push Reject =(\n" );*/
      }
      break;
  }
  return ret_code;
}

/* function to parsing the cashduino buffer and get information */
uint8_t parsing_buffer_bill_acceptor(uint8_t *rx_cashduino, double *amount){

  uint8_t ret_code = NONE;
  /* type of command */
  switch(rx_cashduino[0]){
    case 0xE1:
      /*Serial.print("Bill acceptor configured :)\n" );//debug*/
      break;
    case 0xE2:
      /*Serial.print("Bill Escrow\n" );//debug*/
      break;
    case 0xE3:
      /*Serial.print("Bill Audit\n" );//debug*/
      break;
    case 0xE4:
      /*Serial.print("Bill Out\n" );//debug*/
      break;
    case 0xE5:
      /*Serial.print("Event Counter: ");Serial.print(rx_cashduino[2]);Serial.print("\n");*/
      if( (rx_cashduino[1] >= 0x90) && (rx_cashduino[1] <= 0x9F)){
        /*Serial.print("Bill in pre stacker =) -> " );*/
        switch (rx_cashduino[1]){
          case 0x90:
            /*Serial.print("$20 MXN\n");//debug*/
            break;
          case 0x91:
            /*Serial.print("$50 MXN\n");//debug*/
            break;
          case 0x92:
            /*Serial.print("$100 MXN\n");//debug*/
            break;
          case 0x93:
            /*Serial.print("$200 MXN\n");//debug*/
            break;
          case 0x94:
            /*Serial.print("$500 MXN\n");//debug*/
            break;
          break;
        }
      }
      if( (rx_cashduino[1] >= 0x80) && (rx_cashduino[1] <= 0x8F)){
        ret_code = CASHDUINO_BILL_EVENT;
        /*Serial.print("Bill in cashbox =) -> " );*/
        switch (rx_cashduino[1]){
          case 0x80:
            /*Serial.print("$20 MXN\n");//debug*/
            *(amount) += 20;
            break;
          case 0x81:
            /*Serial.print("$50 MXN\n");//debug*/
            *(amount) += 50;
            break;
          case 0x82:
            /*Serial.print("$100 MXN\n");//debug*/
            *(amount) += 100;
            break;
          case 0x83:
            /*Serial.print("$200 MXN\n");//debug*/
            *(amount) += 200;
            break;
          case 0x84:
            /*Serial.print("$500 MXN\n");//debug*/
            *(amount) += 500;
            break;
          break;
        }
      }
      if( (rx_cashduino[1] >= 0xA0) && (rx_cashduino[1] <= 0xAF)){
        /*Serial.print("Bill rejected =) -> " );*/
        switch (rx_cashduino[1]){
          case 0xA0:
            /*Serial.print("$20 MXN\n");//debug*/
            break;
          case 0xA1:
            /*Serial.print("$50 MXN\n");//debug*/
            break;
          case 0xA2:
            /*Serial.print("$100 MXN\n");//debug*/
            break;
          case 0xA3:
            /*Serial.print("$200 MXN\n");//debug*/
            break;
          case 0xA4:
            /*Serial.print("$500 MXN\n");//debug*/
            break;
          break;
        }
      }
      break;
  }
  return ret_code;
}

/* function to pull the buffer from the cashduino */
bool looking_for_new_event(uint8_t *rx_cashduino){
  uint8_t i = 0;
  if( (command != rx_cashduino[0]) || ( new_event != rx_cashduino[2] ) ){
      command = rx_cashduino[0];
      new_event = rx_cashduino[2];
      Serial.print("CASHDUINO ANSWER < " );                 //Debug
      for(i = 0; i<MAX_BUFF_CASHDUINO; i++){
        Serial.print(rx_cashduino[i], HEX);                 //Debug
      }
      Serial.print("\n" );                                  //Debug
      return true;
  }
  return false;
}

void read_buffer_from_cashduino(uint8_t *rx_cashduino){
  Wire.requestFrom(CASH_DUINO_ADDR, MAX_BUFF_CASHDUINO);    // request 6 bytes from slave CashDuino
  while (Wire.available()) {                                // slave may send less than requested
    *(rx_cashduino++) = Wire.read();                        // receive a byte as character
    /*Serial.println(rx_cashduino[cashduino_id], HEX);*/    // Debug print the character
  }
}

/* function to push data over TWI */
void send_task_to_cashduino(const char *message, uint8_t *cmd){
  Serial.print(message);                      //Debug
  uint8_t i = 0;
  Wire.beginTransmission(CASH_DUINO_ADDR);    // transmit to CashDuino
  for(i = 0; i<MAX_BUFF_ARDUINO; i++){
    Wire.write(cmd[i]);                       // sends one byte
    Serial.print(cmd[i], HEX);                // Debug
  }
  Wire.endTransmission();                     // stop transmitting
  Serial.print("\n" );                        //Debug
}

/* function to sends task */
uint8_t read_buffer_from_serial(void){
  uint8_t serial_option  = 0;
  if (Serial.available() > 0)
  {
    serial_option = Serial.read();
    if( ( serial_option >= '1' ) && (serial_option <= '9') ){
      serial_option -= '0';
      return serial_option - 1;
    }
  }
  return NONE;
}
