import csv
import time
import wave
import serial
import serial.tools.list_ports as stlp
import matplotlib.pyplot as plt
import numpy as np

# Settings
# =========================
STM32_PORT = "COM4"     # To be modified each time
BAUD_RATE = 921600
SAMPLE_RATE = 44138     # 32MHz / (724 + 1) = 44137.9  Hz
TEAM_ID = "F08"


# Main CLI
# =========================
def main() -> None:
    """
    Main function loop:
    Initializes the serial connection to the STM32 device, displays available
    ports, and provides a menu for selecting recording modes.

    Modes:
        1. Manual recording for a fixed duration
        2. Distance-triggered recording
        3. Exit program

    The function runs continuously until the user exits or interrupts execution.
    """

    # Serial setup
    # =========================
    devices = stlp.comports()
    print("Available serial ports:")
    for device in devices:
        print(device)

    ser = serial.Serial(STM32_PORT, BAUD_RATE, timeout=0.1)
    print(f"Connected to {STM32_PORT} at {BAUD_RATE} baud")

    # Main loop
    # =========================
    while True:
        try:
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
        except KeyboardInterrupt:
            print("\nKeyboard Interrupt\nExiting.")
    ser.close()


# Recording functions
# =========================
def manual_recording_mode(ser: serial.Serial) -> None:
    """
    Records audio samples for a user-specified duration.

    Sends a command to the STM32 to start manual recording, receives packed
    12-bit sample data over UART, unpacks it, and calculates the actual
    sampling rate.

    The captured data is unpacked and optionally saved in user-selected formats.

    Args:
        ser (serial.Serial): Active serial connection to the STM32 device.

    Returns:
        None:
    """
    seconds = float(input("Enter recording length in seconds: "))
    num_samples = int(seconds * SAMPLE_RATE)

    # Packed format: 2 samples = 3 bytes
    num_bytes = ((num_samples + 1) // 2) * 3    # if num_sample is odd , add 1 before dividing so enough byte to read

    print("Manual recording started...")
    print(f"Need {num_samples} samples")
    print(f"Need {num_bytes} packed bytes")

    ser.reset_input_buffer()    # reset the serial input buffer from old memory
    ser.write(b'M')
    ser.flush()
    time.sleep(0.05)

    raw_bytes = []  # store raw UART byte before unpacking
    start_time = time.time()
    next_print = 5000

    while len(raw_bytes) < num_bytes:
        remaining = num_bytes - len(raw_bytes) # calculate how many more byte needed
        chunk = ser.read(min(remaining, 1024))  # use 1024 byte max to avoid reading too much

        if len(chunk) > 0:
            raw_bytes.extend(chunk)

            sample_count = (len(raw_bytes) // 3) * 2    # every 3 byte give two sample (estimate how many complete sample)

            if sample_count >= next_print:
                print(f"Received {min(sample_count, num_samples)} / {num_samples}")
                next_print += 5000

    extra = len(raw_bytes) % 3  # if the bytes count is not multiply of 3 
    if extra != 0:  # remove incomplete bytes
        raw_bytes = raw_bytes[0:-extra]

    samples = unpack_12bit_packed(raw_bytes)
    samples = samples[0:num_samples] # remove extra sample if have more

    elapsed = time.time() - start_time
    actual_rate = len(samples) / elapsed

    print("Manual recording finished.")
    print(f"Elapsed time: {elapsed:.2f} s")
    print(f"Actual receive rate: {actual_rate:.1f} samples/s")

    choose_outputs(samples)
    return


def distance_trigger_mode(ser: serial.Serial) -> None:
    """
    Records audio samples when triggered by proximity sensor within set range.
    
    Continuously listens for incoming data from the STM32. Recording starts when
    data begins arriving and stops when no data is received for a specified timeout.

    The captured data is unpacked and optionally saved in user-selected formats.

    Args:
        ser (Serial): Active serial connection to the STM32 device.

    Returns:
        None:
    """
    ser.reset_input_buffer()    # clear old memory
    ser.write(b'D') # send command 
    ser.flush() # make sure STM receive the command before proceeding
    time.sleep(0.05)

    print("Distance Trigger Mode")
    print("Waiting for object...")
    print("Press Ctrl+C to return to menu.")

    try:    # to go back to main CLI
        while True:
            raw_bytes = [] # store raw UART byte before unpacking

            while True:
                chunk = ser.read(1024)  # before detection, try to read up to 1024 byte from STM, just a safety measure, will timeout automatically

                if len(chunk) > 0:
                    raw_bytes.extend(chunk) # store the first receive byte
                    print("Recording started.")
                    break   # leave this loop and to the main loop

            last_data_time = time.time()
            next_print = 5000

            while True:
                chunk = ser.read(1024)  # read up to 1024 byte  

                if len(chunk) > 0:
                    raw_bytes.extend(chunk) # store byte 
                    last_data_time = time.time()

                    sample_count = (len(raw_bytes) // 3) * 2    # 3byte = 2 sample (need to calculate how many sample)
                    if sample_count >= next_print:
                        print(f"Recorded {sample_count} samples")
                        next_print += 5000

                else:
                    if time.time() - last_data_time > 1.0:  # if the last byte came over 1 second before
                        print("Recording stopped.")
                        break

            extra = len(raw_bytes) % 3  # remove incomplete packed byte
            if extra != 0:
                raw_bytes = raw_bytes[0:-extra]

            samples = unpack_12bit_packed(raw_bytes)
            print(f"Captured {len(samples)} samples")
            print(f"Approx duration: {len(samples) / SAMPLE_RATE:.2f} seconds")

            choose_outputs(samples)
            print("Waiting for next trigger...")

    except KeyboardInterrupt:
        print("\nLeaving Distance Trigger Mode.")
    return


# Save output functions
# =========================
def save_wav(samples: list[int]) -> None:
    """
    Saves audio samples to a WAV file.

    Converts unsigned 12-bit samples (0 - 4095) into signed 12-bit,
    scales the amplitude, and writes the data to a mono WAV file.

    Args:
        samples (list[int]): List of 12-bit audio samples.

    Returns:
        None:
    """
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    filename = f"{TEAM_ID}_{SAMPLE_RATE}Hz_{timestamp}.wav"

    # Convert unsigned 12-bit to signed
    data = np.array(samples, dtype=np.int32)
    data = data - 2048                          # 0 to +4095 --> -2048 to +2047
    data = data * 16                            # Scale 12-bit to 16 bit so volume louder

    data = np.clip(data, -32768, 32767)         # make sure final value stay in range
    data = data.astype(np.int16)                # convert to signed 16bit

    with wave.open(filename, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)                      # 2 bytes = 16 bit
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(data.tobytes())

    print(f"Saved WAV: {filename}")
    return


def save_csv(samples: list[int]) -> None:
    """
    Saves audio samples to a CSV file.

    The file includes sample index and amplitude values for each sample.

    Args:
        samples (list[int]): List of 12-bit audio samples.

    Returns:
        None:
    """
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    filename = f"{TEAM_ID}_{SAMPLE_RATE}Hz_{timestamp}.csv"

    with open(filename, "w", newline="") as file:
        writer = csv.writer(file)
        writer.writerow(["Sample Rate", SAMPLE_RATE])
        writer.writerow(["Sample Index", "Amplitude"])

        for i, value in enumerate(samples):
            writer.writerow([i, value])         # value 0 to 4095

    print(f"Saved CSV: {filename}")
    return


def save_png(samples: list[int]) -> None:
    """
    Saves a plot of the audio waveform as a PNG image.

    Converts samples to a NumPy array, generates a time axis based on the
    sampling rate, and plots amplitude versus time.

    Args:
        samples (list[int]): List of 12-bit audio samples.

    Returns:
        None:
    """
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


def choose_outputs(samples: list[int]) -> None:
    """
    Prompts the user to choose an output format and saves the samples accordingly.

    Args:
        samples (list[int]): List of sample values to be saved.

    Returns:
        None:
    """
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


# helper functions
# =========================
def unpack_12bit_packed(data: list[int]) -> list[int]:
    """
    Unpacks 12-bit samples from a packed byte stream.

    Each group of 3 bytes encodes 2 samples:
    - Sample 1: byte0 (low 8 bits) + lower nibble of byte1
    - Sample 2: upper nibble of byte1 + byte2

    Args:
        data (list[int]): List of byte values (0 - 255).

    Returns:
        samples (list[int]): Unpacked 12-bit samples (0 - 4095).
    """
    samples = []

    for i in range(0, len(data) - 2, 3):            # each packet have 3 bytes (len(data)-2 protect code from reading past the end of the packet)
        # stores value as int but python can perform bitwise operations on int 
        b0 = data[i]                                # lower 8 bit of first sample
        b1 = data[i + 1]                            # upper 4 bit of first sample and lower 4 bit of second sample
        b2 = data[i + 2]                            # upper 8 bit of second sample

        sample1 = b0 | ((b1 & 0x0F) << 8)           # rebuild the first sample
        sample2 = (b1 >> 4) | (b2 << 4)             # rebuild the second sample

        samples.append(sample1 & 0x0FFF)            # keep both sample 12 bit 
        samples.append(sample2 & 0x0FFF)

    return samples

if __name__ == "__main__":
    main()