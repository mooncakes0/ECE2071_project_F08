import csv
import time
import wave
import serial
import serial.tools.list_ports
import numpy as np
import matplotlib.pyplot as plt

STM32_PORT = "COM4"
BAUD_RATE = 921600
SAMPLE_RATE = 44138 # 32MHz / (724 + 1) = 44137.9  Hz
TEAM_ID = "F08"

# ===========Serial setup================
devices = serial.tools.list_ports.comports()

print("Available serial ports:")
for device in devices:
    print(device)

stm32_port = "COM4" # To be modified each time

ser = serial.Serial(stm32_port, BAUD_RATE, timeout=0.1)
print(f"Connected to {stm32_port} at {BAUD_RATE} baud")

# ============Save output functions=============
def save_wav(samples):
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    filename = f"{TEAM_ID}_{SAMPLE_RATE}Hz_{timestamp}.wav"

    data = np.array(samples, dtype=np.int32)

    # Convert unsigned 12-bit ADC data to signed audio
    data = data - 2048

    # Scale 12-bit range to 16-bit WAV range
    data = data * 16

    data = np.clip(data, -32768, 32767)
    data = data.astype(np.int16)

    with wave.open(filename, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(data.tobytes())

    print(f"Saved WAV: {filename}")

def save_csv(samples):
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    filename = f"{TEAM_ID}_{SAMPLE_RATE}Hz_{timestamp}.csv"

    with open(filename, "w", newline="") as file:
        writer = csv.writer(file)
        writer.writerow(["Sample Rate", SAMPLE_RATE])
        writer.writerow(["Sample Index", "Amplitude"])

        for i, value in enumerate(samples):
            writer.writerow([i, value])

    print(f"Saved CSV: {filename}")

def save_png(samples):
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    filename = f"{TEAM_ID}_{SAMPLE_RATE}Hz_{timestamp}.png"
    data = np.array(samples, dtype=np.int32)
    t = np.arange(len(data)) / SAMPLE_RATE

    plt.figure()
    plt.plot(t, data)
    plt.title(f"Audio waveform - {TEAM_ID} - {SAMPLE_RATE} Hz")
    plt.xlabel("Time (s)")
    plt.ylabel("Amplitude centred around 0")
    plt.grid(True)
    plt.savefig(filename)
    plt.close()

    print(f"Saved PNG: {filename}")

def choose_outputs(samples):
    if len(samples) == 0: # if the sample is empty
        print("No samples to save.")
        return

    print("\nChoose output format:")
    print("1. WAV")
    print("2. CSV")
    print("3. PNG")
    print("4. WAV + CSV + PNG")

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

# ============Recording functions=============
def unpack_12bit_packed(data):
    samples = []

    for i in range(0, len(data) - 2, 3):
        b0 = data[i]
        b1 = data[i + 1]
        b2 = data[i + 2]

        sample1 = b0 | ((b1 & 0x0F) << 8)
        sample2 = ((b1 >> 4) & 0x0F) | (b2 << 4)

        samples.append(sample1 & 0x0FFF)
        samples.append(sample2 & 0x0FFF)

    return samples

def manual_recording_mode():
    seconds = float(input("Enter recording length in seconds: "))
    num_samples = int(seconds * SAMPLE_RATE)

    # Packed format: 2 samples = 3 bytes
    num_bytes = ((num_samples + 1) // 2) * 3

    print("Manual recording started...")
    print(f"Need {num_samples} samples")
    print(f"Need {num_bytes} packed bytes")

    ser.reset_input_buffer()
    ser.write(b'M')
    ser.flush()
    time.sleep(0.05)

    raw_bytes = bytearray()
    start_time = time.time()
    next_print = 5000

    while len(raw_bytes) < num_bytes:
        remaining = num_bytes - len(raw_bytes)
        chunk = ser.read(min(remaining, 1024))

        if len(chunk) > 0:
            raw_bytes.extend(chunk)

            sample_count = (len(raw_bytes) // 3) * 2

            if sample_count >= next_print:
                print(f"Received {min(sample_count, num_samples)} / {num_samples}")
                next_print += 5000

    extra = len(raw_bytes) % 3
    if extra != 0:
        raw_bytes = raw_bytes[:-extra]

    samples = unpack_12bit_packed(raw_bytes)
    samples = samples[:num_samples]

    elapsed = time.time() - start_time
    actual_rate = len(samples) / elapsed

    print("Manual recording finished.")
    print(f"Elapsed time: {elapsed:.2f} s")
    print(f"Actual receive rate: {actual_rate:.1f} samples/s")

    ser.write(b'I') # stop the STM
    ser.flush()
    time.sleep(0.05)
    ser.reset_input_buffer()
    
    choose_outputs(samples)

def distance_trigger_mode():
    ser.reset_input_buffer()    # clear old memory
    ser.write(b'D') # send command 
    ser.flush() # make sure STM receive the command before proceeding
    time.sleep(0.05)

    print("Distance Trigger Mode")
    print("Waiting for object...")
    print("Press Ctrl+C to return to menu.")

    try:
        while True:
            raw_bytes = bytearray()

            while True:
                chunk = ser.read(1024)

                if len(chunk) > 0:
                    raw_bytes.extend(chunk)
                    print("Recording started.")
                    break

            last_data_time = time.time()
            next_print = 5000

            while True:
                chunk = ser.read(1024)

                if len(chunk) > 0:
                    raw_bytes.extend(chunk)
                    last_data_time = time.time()

                    sample_count = (len(raw_bytes) // 3) * 2

                    if sample_count >= next_print:
                        print(f"Recorded {sample_count} samples")
                        next_print += 5000
                else:
                    if time.time() - last_data_time > 1.0:
                        print("Recording stopped.")
                        break

            extra = len(raw_bytes) % 3
            if extra != 0:
                raw_bytes = raw_bytes[:-extra]

            samples = unpack_12bit_packed(raw_bytes)

            print(f"Captured {len(samples)} samples")
            print(f"Approx duration: {len(samples) / SAMPLE_RATE:.2f} seconds")

            choose_outputs(samples)

            print("Waiting for next trigger...")

    except KeyboardInterrupt:
        print("\nLeaving Distance Trigger Mode.")

# ==============Main CLI==============
while True:
    try:
        print("\n===== ECE2071 Task 2 CLI =====")
        print("1. Manual Recording Mode")
        print("2. Distance Trigger Mode")
        print("3. Exit")

        mode = input("Choose mode: ").strip()

        if mode == "1":
            manual_recording_mode()
        elif mode == "2":
            distance_trigger_mode()
        elif mode == "3":
            print("Exiting.")
            break
        else:
            print("Invalid option.")
    except KeyboardInterrupt:
        print("\nKeyboard Interrupt\nExiting.")
ser.close()
