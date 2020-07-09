class PlayRtp {
    constructor(codec, frequency, sourceCount = 1, enabled = true) {
        this.context = new (window.AudioContext || window.webkitAudioContext)
        this.enabled = enabled
        this.codec = codec
        this.frequency = frequency
        this.initial_delay_sec = 0.125
        this.gain = Array(sourceCount).fill(this.context.createGain())
        this.gain.forEach(x => {
            x.gain.value = 1.0;
            x.connect(this.context.destination);
        });
        this.scheduled_time = Array(sourceCount).fill(0)
        console.log("PlayRtp initialized.")
    }

    enable() { this.enabled = true; }
    disable() { this.enabled = false; }
    setGain(sourceNum, gain) { if (this.gain.length > sourceNum) { this.gain[sourceNum] = gain } }

    playFromBase64(payload, sourceNum = 0) {
        if (this.enabled) {
            let uint8Array = new Uint8Array(this.base64ToArrayBuffer(payload));
            if (this.codec == 0) {
                this.play(alawmulaw.mulaw.decode(uint8Array), sourceNum)
            } else if (this.codec == 9) {
                this.play(alawmulaw.alaw.decode(uint8Array), sourceNum)
            }
        }
    }

    play(raw, sourceNum = 0) {
        if (raw && this.enabled) {
            let raw2 = new Float32Array(raw.length);
            for (let i = 0; i < raw.length; i++) {
                raw2[i] = (raw[i] / 32767)
            }
            this.playAudioStream(raw2, sourceNum);
        }
    }

    playAudioStream(audio_f32, sourceNum = 0) {
        var audio_buffer = this.context.createBuffer(1, audio_f32.length, this.frequency),
            audio_source = this.context.createBufferSource(),
            current_time = this.context.currentTime;

        audio_buffer.getChannelData(0).set(audio_f32);
        audio_source.buffer = audio_buffer;
        audio_source.connect(this.gain[sourceNum])

        if (current_time < this.scheduled_time[sourceNum]) {
            audio_source.start(this.scheduled_time[sourceNum]);
            this.scheduled_time[sourceNum] += audio_buffer.duration;
        } else {
            audio_source.start(current_time);
            this.scheduled_time[sourceNum] = current_time + audio_buffer.duration + this.initial_delay_sec;
        }
    }

    base64ToArrayBuffer(base64) {
        var binary_string = window.atob(base64);
        var len = binary_string.length;
        var bytes = new Uint8Array(len);
        for (var i = 0; i < len; i++) {
            bytes[i] = binary_string.charCodeAt(i);
        }
        return bytes.buffer;
    }

}
