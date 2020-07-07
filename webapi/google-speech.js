const util = require("util");
const EventEmitter = require("events").EventEmitter;
const Speech = require('@google-cloud/speech').v1p1beta1;
const mulaw = require('alawmulaw').mulaw;

const SILENCE_LEVEL_THRESHOLD = 200
const SILENCE_COUNT_THRESHOLD = 50
const speechRequest = {
    config: {
        encoding: "MULAW",
        sampleRateHertz: 8000,
        languageCode: "ja-JP",
        enableAutomaticPunctuation: true,
    },
    singleUtterance: false,
    interimResults: true,
};

function GoogleSpeech() {
    this.speechClient = null
    this.recognizeStream = null
    this.voiceCache = []
    this.silence_counter = 0
    this.speechClient = new Speech.SpeechClient();
}


GoogleSpeech.prototype.sendChunk = function (rtp_payload) {
    if (!this.recognizeStream || !this.recognizeStream.writable || this.recognizeStream.destroyed) {
        console.log(Date.now(), "recognizeStream is null, ended or destroyed. create new one.")
        this.recognizeStream = this.speechClient
            .streamingRecognize(speechRequest)
            .on('error', err => {
                console.log(err.message)
                //this.emit('err', err.message) // 'error' にすると ERR_UNHANDLED_ERROR が出る
                this.recognizeStream.destroy()
            })
            .on('data', data => {
                this.emit('data', data)
            })
    }

    if (rtp_payload) {


        // decode mulaw
        let decoded = Object.values(mulaw.decode(rtp_payload))
        //let voice_min = decoded.reduce((a, b) => { return Math.min(Math.abs(a), Math.abs(b)) })
        //let voice_max = decoded.reduce((a, b) => { return Math.max(Math.abs(a), Math.abs(b)) })
        //let voice_avg = average(decoded.map(x => Math.abs(x)))
        let voice_med = median(decoded.map(x => Math.abs(x)))

        if (voice_med < SILENCE_LEVEL_THRESHOLD) {
            this.silence_counter++;
        } else {
            this.silence_counter = 0;
        }

        //console.log(voice_min, voice_max, voice_avg, voice_med, silence_counter)

        if (!this.recognizeStream.writable || this.recognizeStream.destroyed) {
            this.voiceCache.push(rtp_payload)
        } else {
            let cache;
            while ((cache = this.voiceCache.shift()) != undefined) {
                this.recognizeStream.write(cache);
            }
            if (this.silence_counter == SILENCE_COUNT_THRESHOLD) {
                this.recognizeStream.end()
            } else {
                this.recognizeStream.write(rtp_payload);
            }
        }
    }
}


util.inherits(GoogleSpeech, EventEmitter)
module.exports = GoogleSpeech;


var sum = function (arr, fn) {
    if (fn) {
        return sum(arr.map(fn));
    }
    else {
        return arr.reduce(function (prev, current, i, arr) {
            return prev + current;
        });
    }
};
var average = function (arr, fn) {
    return sum(arr, fn) / arr.length;
};
var median = function (arr, fn) {
    var half = (arr.length / 2) | 0;
    var temp = arr.sort(fn);

    if (temp.length % 2) {
        return temp[half];
    }
    return (temp[half - 1] + temp[half]) / 2;
};
