class PlayRtp {
    constructor(codec, frequency, sourceCount = 1, enabled = true) {
        this.enabled = enabled
        this.context = new (window.AudioContext || window.webkitAudioContext)
        this.codec = codec
        this.frequency = frequency
        this.initial_delay_sec = 0.125
        this.scheduled_time = Array(sourceCount).fill(0)
    }

    enable() {
        this.enabled = true;
    }
    
    disable() {
        this.enabled = false;
    }

    playFromBase64(payload) {
        let uint8Array = new Uint8Array(this.base64ToArrayBuffer(payload));
        let raw = null;
        if (this.codec == 0) {
            raw = alawmulaw.mulaw.decode(uint8Array);
        } else if (this.codec == 9) {
            raw = alawmulaw.alaw.decode(uint8Array);
        }
        this.play(raw)
    }

    play(raw) {
        if (raw && this.enabled) {
            let raw2 = new Float32Array(raw.length);
            for (let i = 0; i < raw.length; i++) {
                raw2[i] = (raw[i] / 32767)
            }
            this.playAudioStream(raw2);
        }
    }

    playAudioStream(audio_f32, sourceNum = 0) {
        var audio_buffer = this.context.createBuffer(1, audio_f32.length, this.frequency),
            audio_source = this.context.createBufferSource(),
            current_time = this.context.currentTime;

        audio_buffer.getChannelData(0).set(audio_f32);
        audio_source.buffer = audio_buffer;
        audio_source.connect(this.context.destination);

        if (current_time < this.scheduled_time[sourceNum]) {
            this.playChunk(audio_source, this.scheduled_time[sourceNum]);
            this.scheduled_time[sourceNum] += audio_buffer.duration;
        } else {
            this.playChunk(audio_source, current_time);
            this.scheduled_time[sourceNum] = current_time + audio_buffer.duration + this.initial_delay_sec;
        }
    }

    playChunk(source, time) {
        if (source.start) {
            source.start(time);
        } else {
            source.noteOn(time);
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
