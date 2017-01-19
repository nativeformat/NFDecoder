package com.spotify.NFDecoder;

import android.media.MediaCodec;
import android.media.MediaCodec.BufferInfo;
import android.media.MediaDataSource;
import android.media.MediaExtractor;
import android.media.MediaFormat;

import java.nio.ByteBuffer;

public class Decoder extends MediaDataSource {
    public int samplerate = 0;
    public int numberOfChannels = 0;
    public long durationFrames = 0;
    private String lastError = null;
    private MediaExtractor extractor = null;
    private MediaCodec decoder = null;
    private BufferInfo bufferInfo = null;
    private boolean endOfInput = false;
    private int bytesToCutAfterSeek = 0;
    private int bytesPerFrame = 4;
    private long clientdata = 0;

    Decoder(long cd) {
        clientdata = cd;

        // Open the source.
        extractor = new MediaExtractor();
        try {
            extractor.setDataSource(this);
        } catch (Exception e) {
            lastError = e.getLocalizedMessage();
        }

        if (lastError == null) {
            // Check the source for valid audio content.
            int trackCount = extractor.getTrackCount();

            if (trackCount < 1) lastError = "Track count is " + trackCount + ".";
            else {
                for (int n = 0; n < trackCount; n++) {
                    MediaFormat format = extractor.getTrackFormat(n);

                    if (format.containsKey(MediaFormat.KEY_SAMPLE_RATE) && format.containsKey(MediaFormat.KEY_MIME) && format.containsKey(MediaFormat.KEY_DURATION)) {
                        String mime = format.getString(MediaFormat.KEY_MIME);
                        samplerate = format.getInteger(MediaFormat.KEY_SAMPLE_RATE);
                        durationFrames = format.getLong(MediaFormat.KEY_DURATION);

                        // This track looks promising, try to select and configure a decoder around it.
                        if ((mime != null) && (samplerate > 1000) && (durationFrames > 0)) {
                            extractor.selectTrack(n);
                            try {
                                decoder = MediaCodec.createDecoderByType(mime);
                                decoder.configure(format, null, null, 0);

                                MediaFormat outputFormat = decoder.getOutputFormat();
                                if (outputFormat.containsKey(MediaFormat.KEY_SAMPLE_RATE)) {
                                    int sr = outputFormat.getInteger(MediaFormat.KEY_SAMPLE_RATE);
                                    if (sr > 1000) samplerate = sr;
                                }
                                if (outputFormat.containsKey(MediaFormat.KEY_CHANNEL_COUNT)) {
                                    int ch = outputFormat.getInteger(MediaFormat.KEY_CHANNEL_COUNT);
                                    if (ch > 0) {
                                        bytesPerFrame = ch * 2;
                                        numberOfChannels = ch;
                                    }
                                }

                                durationFrames = (long)(((double)durationFrames / 1000000.0) * (double)samplerate);
                                decoder.start();
                                bufferInfo = new BufferInfo();
                                lastError = null;
                                break;
                            } catch (Exception e) {
                                lastError = e.getLocalizedMessage();
                            }
                        }
                    }
                }
            }
        }
    }

    // Returns false on error.
    public boolean seek(int frameIndex) {
        double samplerate = (double)this.samplerate;
        double timeUs = ((double)frameIndex / samplerate) * 1000000.0;
        extractor.seekTo((long)timeUs, MediaExtractor.SEEK_TO_PREVIOUS_SYNC);

        // Check how many bytes should be cut at the beginning for precise frame positioning.
        double timeToCutUs = timeUs - extractor.getSampleTime();
        if (timeToCutUs <= 0) bytesToCutAfterSeek = 0;
        else bytesToCutAfterSeek = (int)((timeToCutUs / 1000000.0) * samplerate) * bytesPerFrame;
        android.util.Log.d("", "cut after seek " + bytesToCutAfterSeek / bytesPerFrame);
        try {
            decoder.flush();
            endOfInput = false;
        } catch (Exception e) {
            lastError = e.getLocalizedMessage();
        }
        return (lastError == null);
    }

    // Returns:
    // - 0 on error.
    // - Positive bytes on successful read.
    // - Negative bytes on successful read with eof.
    // - INT_MIN on eof without any bytes read.
    public int decodeOnePacket(ByteBuffer output) {
        // Handle the next source packet.
        if (!endOfInput) {
            // Try to get an input buffer for the next packet from the decoder.
            int inputBufferIndex;
            try {
                inputBufferIndex = decoder.dequeueInputBuffer(-1);
            } catch (Exception e) {
                lastError = e.getLocalizedMessage();
                return 0;
            }
            if (inputBufferIndex < 0) {
                lastError = "InputBufferIndex is " + inputBufferIndex + ".";
                return 0;
            }
            ByteBuffer inputBuffer;
            try {
                inputBuffer = decoder.getInputBuffer(inputBufferIndex);
            } catch (Exception e) {
                lastError = e.getLocalizedMessage();
                return 0;
            }
            if (inputBuffer == null) {
                lastError = "InputBuffer is null.";
                return 0;
            }

            // Read one packet.
            int sampleSize = extractor.readSampleData(inputBuffer, 0);
            try {
                // Submit the packet to the decoder.
                if (sampleSize < 0) { // Submit with eof.
                    decoder.queueInputBuffer(inputBufferIndex, 0, 0, 0, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
                    endOfInput = true;
                } else { // Regular submit.
                    decoder.queueInputBuffer(inputBufferIndex, 0, sampleSize, extractor.getSampleTime(), 0);
                    extractor.advance();
                }
            } catch (Exception e) {
                lastError = e.getLocalizedMessage();
                return 0;
            }
        }

        // Try to get an output buffer from the decoder.
        int outputBufferIndex;
        try {
            outputBufferIndex = decoder.dequeueOutputBuffer(bufferInfo, -1);
        } catch (Exception e) {
            lastError = e.getLocalizedMessage();
            return 0;
        }

        switch (outputBufferIndex) {
            case MediaCodec.INFO_TRY_AGAIN_LATER: lastError = "Try again later."; return 0;
            case MediaCodec.INFO_OUTPUT_FORMAT_CHANGED:
                // This is not an error, but a notification.
                return decodeOnePacket(output);
            default:
                ByteBuffer outputBuffer;
                try {
                    // Try to get the actual PCM output bytes.
                    outputBuffer = decoder.getOutputBuffer(outputBufferIndex);
                    if (outputBuffer == null) {
                        lastError = "Output buffer is null.";
                        return 0;
                    }
                    int pcmBytesAvailable;
                    try {
                        pcmBytesAvailable = outputBuffer.remaining();
                    } catch (Exception e) {
                        lastError = e.getLocalizedMessage();
                        return 0;
                    }

                    int bytesRead = pcmBytesAvailable;
                    // Cut the frames after seek.
                    if (bytesToCutAfterSeek > 0) {
                        bytesRead -= bytesToCutAfterSeek;
                        if (bytesRead <= 0) { // The cut is larger than the actual bytes read.
                            bytesRead = 0;
                            bytesToCutAfterSeek -= pcmBytesAvailable;
                        } else { // Finish cutting.
                            output.put(outputBuffer.array(), bytesToCutAfterSeek, bytesRead);
                            bytesToCutAfterSeek = 0;
                        }
                    } else output.put(outputBuffer);

                    decoder.releaseOutputBuffer(outputBufferIndex, false);
                    if ((bufferInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                        if (bytesRead < 1) return Integer.MIN_VALUE; // Nothing read and eof.
                        else return -bytesRead; // Successful read and eof.
                    } else {
                        if (bytesRead < 1) return decodeOnePacket(output); // Nothing read, but everything was okay: try again.
                        else return bytesRead; // Successful read.
                    }
                } catch (Exception e) {
                    lastError = e.getLocalizedMessage();
                    return 0;
                }
        }
    }

    public String getLastError() {
        String e = lastError;
        lastError = null;
        return e;
    }

    // MediaDataSource implementation
    public void close() {}

    public long getSize() { return nativeGetSize(clientdata); }

    public int readAt(long position, byte[] buffer, int offset, int size) {
        return nativeReadAt(clientdata, position, buffer, offset, size);
    }

    // JNI methods
    public native int nativeReadAt(long clientdata, long position, byte[] buffer, int offset, int size);
    public native long nativeGetSize(long clientdata);
}
