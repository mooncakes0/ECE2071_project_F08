import serial
import serial.tools.list_ports

devices = serial.tools.list_ports.comports()

print("Available serial ports:")
for device in devices:
    print(device)

stm32_port = "COM5" # To be modified each time
baud_rate = 115200

ser = serial.Serial(stm32_port, baud_rate, timeout=5)
print(f"Connected to {stm32_port} at {baud_rate} baud")

while True:
    cmd = input("Enter message: ").strip()
    if not cmd:
        continue

    msg = cmd + "\n"                # send full line
    ser.write(msg.encode())  # send to the stm32

    # wait for returned message from head STM
    reply = ser.readline().decode().strip()
    if reply:
        print("Returned from STM32:", reply)