# cashduino

	* Hardware compatible: Arduino R3.
	* I2C address 0x08.
	* Tasks: request from Master(Arduino) to slave(CashDuino).
	* Event: event reported from slave.
	* CMD: command.
	* SUB: Sub command.
	* EVT: Event counter.
	* DAT: Data

### Tasks
	* COIN ENABLE
	* COIN AUDIT
	* COIN OUT
	* BILL ENABLE
	* BILL ESCROW
	* BILL AUDIT
	* BILL OUT

### Events
	* COIN READ
	* BILL READ

**ENQUIRY BOARD**

MASTER TO SLAVE

|CMD|DAT1|DAT2|DAT3|DAT4|DAT5|DAT6|DAT7|DAT8|KEY1|KEY2|KEY3|KEY4|KEY5|
|---|----|----|----|----|----|----|----|----|----|----|----|----|----|
|C1 |00  |00  |00  |00  |00  |00  |00  |00  |33  |CC  |33  |CC  |33	 |

MASTER READ FROM SLAVE
|CMD|SUB |EVT |DAT1|DAT2|DAT3|DAT4|DAT5|DAT6|DAT7|DAT8|KEY1|KEY2|KEY3|KEY4|KEY5|
|---|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|
|D1 |00  |00  |00  |00  |00  |00  |00  |00  |00  |00  |33  |CC  |33  |CC  |33  |

**COIN ENABLE**

MASTER TO SLAVE

|CMD|DAT1|DAT2|DAT3|DAT4|DAT5|DAT6|DAT7|DAT8|KEY1|KEY2|KEY3|KEY4|KEY5|
|---|----|----|----|----|----|----|----|----|----|----|----|----|----|
|A1 |00  |00  |00  |00  |00  |00  |00  |00  |33  |CC  |33  |CC  |33	 |

MASTER READ FROM SLAVE
|CMD|SUB |EVT |DAT1|DAT2|DAT3|DAT4|DAT5|DAT6|DAT7|DAT8|KEY1|KEY2|KEY3|KEY4|KEY5|
|---|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|
|F1 |00  |00  |00  |00  |00  |00  |00  |00  |00  |00  |33  |CC  |33  |CC  |33  |

**COIN AUDIT**

MASTER TO SLAVE

|CMD|DAT1|DAT2|DAT3|DAT4|DAT5|DAT6|DAT7|DAT8|KEY1|KEY2|KEY3|KEY4|KEY5|
|---|----|----|----|----|----|----|----|----|----|----|----|----|----|
|A2 |00  |00  |00  |00  |00  |00  |00  |00  |33  |CC  |33  |CC  |33	 |

MASTER READ FROM SLAVE
|CMD|SUB |EVT |DAT1|DAT2|DAT3|DAT4|DAT5|DAT6|DAT7|DAT8|KEY1|KEY2|KEY3|KEY4|KEY5|
|---|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|
|F2 |00  |00  |00  |00  |00  |00  |00  |00  |00  |00  |33   |CC  |33 |CC  |33  |

Data description:
	* DAT1 = Knows tube Channel 1
	* DAT2 = Knows tube Channel 2
	* DAT3 = Knows tube Channel 3
	* DAT4 = Knows tube Channel 4
	* DAT5 = Knows tube Channel 5
	* DAT6 = Knows tube Channel 6
	* DAT7 = Knows tube Channel 7
	* DAT8 = Knows tube Channel 8

**COIN OUT**

MASTER TO SLAVE

|CMD|DAT1|DAT2|DAT3|DAT4|DAT5|DAT6|DAT7|DAT8|KEY1|KEY2|KEY3|KEY4|KEY5|
|---|----|----|----|----|----|----|----|----|----|----|----|----|----|
|A2 |00  |00  |00  |00  |00  |00  |00  |00  |33  |CC  |33  |CC  |33	 |

Data description:
	* DAT1 = VALUE
	* DAT2 = CHANNEL 1
	* DAT3 = VALUE
	* DAT4 = CHANNEL 2
	* DAT5 = VALUE
	* DAT6 = CHANNEL 3
	* DAT7 = VALUE
	* DAT8 = CHANNEL 4

MASTER READ FROM SLAVE
|CMD|SUB |EVT |DAT1|DAT2|DAT3|DAT4|DAT5|DAT6|DAT7|DAT8|KEY1|KEY2|KEY3|KEY4|KEY5|
|---|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|
|F2 |00  |00  |00  |00  |00  |00  |00  |00  |00  |00  |33   |CC  |33 |CC  |33  |

**COIN READ EVENT**

|CMD|SUB |EVT |DAT1|DAT2|DAT3|DAT4|DAT5|DAT6|DAT7|DAT8|KEY1|KEY2|KEY3|KEY4|KEY5|Event|
|---|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|-----|
|F4 |40  |00  |00  |00  |00  |00  |00  |00  |00  |00  |33  |CC  |33  |CC  |33  |CASHBOX|
|F4 |50  |00  |00  |00  |00  |00  |00  |00  |00  |00  |33  |CC  |33  |CC  |33  |TUBE|
|F4 |01  |00  |00  |00  |00  |00  |00  |00  |00  |00  |33  |CC  |33  |CC  |33  |REJECT BUTTON|
