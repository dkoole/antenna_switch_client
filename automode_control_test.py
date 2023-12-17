import serial

port = '/dev/ttyUSB1'
baudrate = 57600

expected_command = 'IF;'

response_20m = 'IF00014175000     0000000000030000000;'

with serial.Serial(port, baudrate, timeout=1) as ser:

    buffer = ''

    while True:
        x = ser.read(3)
        if x.decode() == expected_command:
            ser.write(response_20m.encode())
            ser.flush()


