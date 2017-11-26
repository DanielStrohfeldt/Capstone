import RPi.GPIO as GPIO
import signal
import sys
import time

#### PIN SETUP ####
CLK = (3)
MOSI = (5)
MISO = (7)
CS_0 = (11)
CS_1 = (13)

#### CONSTANTS ####
READ_CH0_CMD = 0xD000
READ_CH1_CMD = 0xF000 

READ_CMDS = {0 : READ_CH0_CMD, 1 : READ_CH1_CMD}
ADC_SELECTS = {0 : CS_0, 1 : CS_1}

def clean_up(sig_num, frame):
##### Clean Up After Program Execution Or CTRL-C #####
    GPIO.output(CS_0, GPIO.HIGH)
    GPIO.output(CS_1, GPIO.HIGH)
    GPIO.cleanup()
    sys.exit(1)

def print_board_info():
##### Check the Board Revision #####
    print GPIO.RPI_INFO
    time.sleep(5)

def setup_spi_interface():
##### Set Up #####
    GPIO.setmode(GPIO.BOARD)
    GPIO.setup(CLK, GPIO.OUT)
    GPIO.setup(MOSI, GPIO.OUT)
    GPIO.setup(MISO, GPIO.IN)
    GPIO.setup(CS_0, GPIO.OUT)
    GPIO.setup(CS_1, GPIO.OUT)

def read_adc(adc_select, adc_channel):
#### Read An ADC ####
    print "ADC SELECT : {}".format(adc_select)
    print "ADC CHANNEL : {}".format(adc_channel)
    if adc_select != 0 and adc_select != 1:
       print "INVALID ADC SELECT"
       return -1

    if adc_channel != 0 and adc_channel != 1:
       print "INVALID ADC CHANNEL"
       return -1

    GPIO.output(ADC_SELECTS[adc_select], GPIO.LOW)
    GPIO.output(CLK, GPIO.HIGH)

    send_bits(READ_CMDS[adc_channel], 4) 
    adc_value = receive_bits(13)

    GPIO.output(ADC_SELECTS[adc_select], GPIO.HIGH)

    return adc_value

def send_bits(data, num_bits):
#### Send Data To SPI Device ####
    data = convert_to_binary(data) 
    
    for bit in range(num_bits):
        if data[bit] == '1':
           GPIO.output(MOSI, GPIO.HIGH)
        else:
           GPIO.output(MOSI, GPIO.LOW)

        GPIO.output(CLK, GPIO.HIGH)
        GPIO.output(CLK, GPIO.LOW)
    

def convert_to_binary(data):
#### CONVERT DATA TO BINARY STRING AND STRIP '0b' From Front ####
    data = bin(data)
    return data[2:]

def receive_bits(num_bits):
#### Receive Data From SPI Device ####
    received_bits = []
    adc_value = 0

    for bit in range(num_bits):
    #### Generate Clock #### 
        GPIO.output(CLK, GPIO.HIGH)
        GPIO.output(CLK, GPIO.LOW)
    
        received_bits.append(GPIO.input(MISO))

    received_bits.pop()
    adc_value = convert_to_adc_counts(received_bits)
    return adc_value

def convert_to_adc_counts(data):
#### Convert A List Of Binary Data To an ADC Count ####
#### MSB 1ST ####
    adc_count = 0
    exponent = len(data)
    
    for bit in range(len(data)):
        print "Exponent : {}\tData : {}".format(exponent, data[bit])
        exponent -= 1
        if data[bit]:
           adc_count += 2 ** exponent

    return adc_count

if __name__ == '__main__':
    print_board_info()
    signal.signal(signal.SIGINT, clean_up)
    setup_spi_interface()
   
    it_count = 0
    adc_count = 0


    while 1:
        adc_select = int(it_count % 4 / 2)
        adc_channel = it_count % 2
        it_count += 1
        adc_count = read_adc(adc_select=adc_select, adc_channel=adc_channel)
        time.sleep(0.2)
        print "ADC COUNT : {}".format(adc_count)
