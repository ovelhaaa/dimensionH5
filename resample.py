import os
import wave
import audioop

def convert_sample_rate(directory, target_rate=48000):
    for filename in os.listdir(directory):
        if filename.lower().endswith(".wav"):
            filepath = os.path.join(directory, filename)

            # Read the original wave file
            with wave.open(filepath, 'rb') as in_wav:
                params = in_wav.getparams()
                nchannels, sampwidth, framerate, nframes, comptype, compname = params

                if framerate == target_rate:
                    print(f"Skipping {filename}: Already at {target_rate} Hz")
                    continue

                print(f"Converting {filename}: {framerate} Hz -> {target_rate} Hz")

                # Read audio data
                data = in_wav.readframes(nframes)

                # Convert the sample rate
                state = None
                converted_data, _ = audioop.ratecv(data, sampwidth, nchannels, framerate, target_rate, state)

            # Save the converted data back to the same file
            with wave.open(filepath, 'wb') as out_wav:
                # Calculate new number of frames
                # The length of converted_data is in bytes. We need to divide by the number of bytes per frame.
                new_nframes = len(converted_data) // (sampwidth * nchannels)
                out_wav.setparams((nchannels, sampwidth, target_rate, new_nframes, comptype, compname))
                out_wav.writeframes(converted_data)

    print("Finished converting all WAV files in the directory.")

if __name__ == "__main__":
    samples_dir = os.path.join(os.path.dirname(__file__), "samples")
    convert_sample_rate(samples_dir)
