import numpy as np
import wave
import serial
import serial.tools.list_ports

devices = serial.tools.list_ports.comports()

print("Available serial ports:")
for device in devices:
    print(device)

stm32_port = "COM3" # To be modified each time
baud_rate = 115200

ser = serial.Serial(stm32_port, baud_rate, timeout=5)
print(f"Connected to {stm32_port} at {baud_rate} baud")

SAMPLE_RATE = 9000
newEmptyList = []

for i in range(5*SAMPLE_RATE):
    reply = ser.read(1)
    data = reply[0]
    print("raw and indexed:", reply, data)
    
    newEmptyList.append(data)
    #print(data)

newdata = np.array(newEmptyList)

newdata = (newdata - newdata.min()) / newdata.max()
newdata = newdata * 255
newdata = newdata.astype(np.uint8)
print('hello')

with wave.open('filename.wav', 'wb') as wf:
    wf.setnchannels(1)
    wf.setsampwidth(1)
    wf.setframerate(SAMPLE_RATE)
    wf.writeframes(newdata.tobytes())