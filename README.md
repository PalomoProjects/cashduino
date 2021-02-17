# cashduino

## Documentation.

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

** TASK ENQUIRY BOARD **

MASTER TO SLAVE

|CMD|DAT1|DAT2|DAT3|DAT4|DAT5|DAT6|DAT7|DAT8|KEY1|KEY2|KEY3|KEY4|KEY5|
|---|----|----|----|----|----|----|----|----|----|----|----|----|----|
|C1 |00  |00  |00  |00  |00  |00  |00  |00  |33  |CC  |33  |CC  |33	 |

MASTER READ FROM SLAVE
|CMD|SUB |EVT |DAT1|DAT2|DAT3|DAT4|DAT5|DAT6|DAT7|DAT8|DAT9|DAT10|KEY1|KEY2|KEY3|KEY4|KEY5|
|---|----|----|----|----|----|----|----|----|----|----|----|----|-----|----|----|----|----|
|D1 |00  |00  |00  |00  |00  |00  |00  |00  |00  |00  |00  |00  |33   |CC  |33  |CC  |33  |

|CMD|DAT1|DAT2|DAT3|DAT4|DAT5|DAT6|DAT7|DAT8|KEY1|KEY2|KEY3|KEY4|KEY5|
|C1 |00  |00  |00  |00  |00  |00  |00  |00  |33  |CC  |33  |CC  |33	 |
