'''import numpy as np
import wave
import serial
import serial.tools.list_ports

devices = serial.tools.list_ports.comports()

print("Available serial ports:")
for device in devices:
    print(device)

stm32_port = "COM4" # To be modified each time
baud_rate = 230400

ser = serial.Serial(stm32_port, baud_rate, timeout=0.1)
print(f"Connected to {stm32_port} at {baud_rate} baud")

SAMPLE_RATE = 10000

newEmptyList = []

for i in range(5*SAMPLE_RATE):
    reply = ser.read(1)
    if len(reply) == 1:
        data = reply[0]
        newEmptyList.append(data)
        print("raw and indexed:", reply, data)

newdata = np.array(newEmptyList)

newdata = (newdata - newdata.min()) / newdata.max()
newdata = newdata * 255
newdata = newdata.astype(np.uint8)  

with wave.open('filename.wav', 'wb') as wf:
    wf.setnchannels(1)
    wf.setsampwidth(1)
    wf.setframerate(SAMPLE_RATE)
    wf.writeframes(newdata.tobytes())

print("Done. Saved filename.wav")
'''

import csv
import time
import wave
import serial
import serial.tools.list_ports as stlp
import numpy as np
import matplotlib.pyplot as plt
from array import array


# Settings
# =========================
STM32_PORT = "COM3"
BAUD_RATE = 921600
SAMPLE_RATE = 48000
TEAM_ID = "F08"

# Main CLI
# =========================
def main():
    
    # Serial setup
    # =========================
    devices = stlp.comports()
    print("Available serial ports:")
    for device in devices:
        print(device)

    try:
        ser = serial.Serial(STM32_PORT, BAUD_RATE, timeout=0.1)
        print(f"Connected to {STM32_PORT} at {BAUD_RATE} baud")
    except Exception as error_string:
        print(f"connection to {STM32_PORT} failed, with error string: {error_string}")

        
    # Main loop
    # =========================
    while True:
        print("\n===== ECE2071 Task 2 CLI =====")
        print("1. Manual Recording Mode")
        print("2. Distance Trigger Mode")
        print("3. Exit")

        mode = input("Choose mode: ").strip()

        if mode == "1":
            manual_recording_mode(ser)
        elif mode == "2":
            distance_trigger_mode(ser)
        elif mode == "3":
            print("Exiting.")
            break
        else:
            print("Invalid option.")
    ser.close()
    return


# Recording functions
# =========================
def manual_recording_mode(ser: serial.Serial) -> None:
    seconds = float(input("Enter recording length in seconds: "))
    num_samples = int(seconds * SAMPLE_RATE)

    print("Manual recording started...")
    print(f"Need {num_samples} samples")

    ser.reset_input_buffer()
    ser.write(b'M')
    time.sleep(0.05)

    samples = bytearray()
    start_time = time.time()
    next_print = 5000

    # since we are sending 2 bytes we need to collect 2*num_samples of bytes 
    while len(samples) < 2*num_samples:
        remaining = 2*num_samples - len(samples)
        chunk = ser.read(min(remaining, 1024))

        if len(chunk) > 0:
            samples.extend(chunk)

            ## not sure what this does
            if len(samples) >= next_print:
                print(f"Received {min(next_print, num_samples)} / {num_samples}")
                next_print += 5000

    # the data is in single bytes where each byte represent 1 sample when it should be 2 bytes per sample
    # convert with helper function
    converted_data = to_16bit(samples)

    elapsed = time.time() - start_time
    actual_rate = len(samples) / elapsed

    print("Manual recording finished.")
    print(f"Elapsed time: {elapsed:.2f} s")
    print(f"Actual receive rate: {actual_rate:.1f} samples/s")

    choose_outputs(converted_data)
    return

def distance_trigger_mode(ser: serial.Serial) -> None:
    ser.reset_input_buffer()
    ser.write(b'D')
    ser.flush()
    time.sleep(0.02)
    ser.reset_input_buffer()

    print("Distance Trigger Mode")
    print("Waiting for object...")
    print("Press Ctrl+C to return to menu.")

    try:
        while True:
            samples = bytearray()

            # Wait for STM to start sending audio
            while True:
                chunk = ser.read(1024)

                if len(chunk) > 0:
                    samples.extend(chunk)
                    print("Recording started.")
                    break

            last_data_time = time.time()
            next_print = 5000

            # Keep recording while STM keeps sending audio
            while True:
                chunk = ser.read(1024)

                if len(chunk) > 0:
                    samples.extend(chunk)
                    last_data_time = time.time()

                    if len(samples) >= next_print:
                        print(f"Recorded {len(samples)} samples")
                        next_print += 5000

                else:
                    # If no bytes for 1 second, assume STM stopped sending
                    if time.time() - last_data_time > 1.0:
                        print("Recording stopped.")
                        break

            converted_data = to_16bit(samples)           

            print(f"Captured {len(samples)} samples")
            print(f"Approx duration: {len(samples) / SAMPLE_RATE:.2f} seconds")

            choose_outputs(converted_data)

            print("Waiting for next trigger...")

    except KeyboardInterrupt:
        print("\nLeaving Distance Trigger Mode.")
    return


# Save output functions
# =========================
def save_wav(samples: array[int]) -> None:
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    filename = f"{TEAM_ID}_{SAMPLE_RATE}Hz_{timestamp}.wav"

    with wave.open(filename, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)       # 2 byte or 16-bit audio
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(samples.tobytes())

    print(f"Saved WAV: {filename}")
    return

def save_csv(samples: array[int]) -> None:
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    filename = f"{TEAM_ID}_{SAMPLE_RATE}Hz_{timestamp}.csv"

    with open(filename, "w", newline="") as file:
        writer = csv.writer(file)
        writer.writerow(["Sample Rate", SAMPLE_RATE])
        writer.writerow(["Sample Index", "Amplitude"])

        for i, value in enumerate(samples):
            writer.writerow([i, value])

    print(f"Saved CSV: {filename}")
    return

def save_png(samples: array[int]) -> None:
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    filename = f"{TEAM_ID}_{SAMPLE_RATE}Hz_{timestamp}.png"
    data = np.array(samples, dtype=np.uint16)
    t = np.arange(len(data)) / SAMPLE_RATE

    plt.figure()
    plt.plot(t, data)
    plt.title(f"Audio waveform - {TEAM_ID} - {SAMPLE_RATE} Hz")
    plt.xlabel("Time (s)")
    plt.ylabel("Amplitude")
    plt.grid(True)
    plt.savefig(filename)
    plt.close()

    print(f"Saved PNG: {filename}")
    return

def choose_outputs(samples: array[int]) -> None:
    if len(samples) == 0:
        print("No samples to save.")
        return

    print("output format:")
    print("="*50)
    print("1. WAV \n2. CSV \n3. PNG \n4. WAV + CSV + PNG")
    
    choice = input("Choice: ").strip()
    if choice == "1":
        save_wav(samples)
    elif choice == "2":
        save_csv(samples)
    elif choice == "3":
        save_png(samples)
    elif choice == "4":
        save_wav(samples)
        save_csv(samples)
        save_png(samples)
    else:
        print("Invalid choice. Saving WAV by default.")
        save_wav(samples)
    return


# this function is never used?? commented out for now
'''
def read_one_byte(ser):
    reply = ser.read(1)
    if len(reply) == 1:
        return reply[0]
    return None
'''


# helper functions
# =========================
def to_16bit(samples: bytearray) -> array[int]:
    converted_data = array('H')
    
    # if length of sample is not multiple of 2 concatonate with null byte: b'\x00'
    if len(samples)%2 != 0:
        samples_cpy = samples + b'\x00'

    for i in range(0, len(samples_cpy), 2):
        lo = samples_cpy[i]                         # lower 8 bits as an int
        hi = samples_cpy[i+1]                       # higher 8 bits as an int, only the first 4 bits should be occupied
        converted_data.append(lo | (hi<<8))         # bitwise or to combine by first shifting hi by 8 bits to the left to get a 16 bit value with 12bits of information
    return converted_data
    

if __name__ == "__main__":
    main()