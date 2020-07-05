const Rx = require('rxjs/Rx')
const GoogleSpeech = require('./google-speech')
const RedisStream = require('./redis-stream');

const EVENT_TYPE_NETWPRK_PACKET = 'NETWORK_PACKET'
const EVENT_TYPE_GOOGLE_SPEECH_IN = 'GOOGLE_SPEECH_IN'
const EVENT_TYPE_GOOGLE_SPEECH_OUT = 'GOOGLE_SPEECH_OUT'

function StreamManager(redisSettings, enableGoogleSpeech = false) {
    this.enableGoogleSpeech = enableGoogleSpeech

    // Redis へ接続
    this.redisStream = new RedisStream(redisSettings.host, redisSettings.port, redisSettings.db);

    // Google Speech to Text を IN/OUT の2チャンネル分用意
    this.googleSpeech_IN = enableGoogleSpeech ? new GoogleSpeech() : null;
    this.googleSpeech_OUT = enableGoogleSpeech ? new GoogleSpeech() : null;

    // キャッシュ
    this.cache = {}
}


StreamManager.prototype.observeAllStream = function (key) {
    // すでに作成済の Obervable に対するリクエストだったら、作ったものを返す。
    if (this.cache[key]) return this.cache[key]

    return this.cache[key] = Rx.Observable.of(null)
        // ネットワークパケットのストリームをマージ
        .merge(this.observeNetworkPacketStream(key))
        // Google Speech の結果ストリームをマージ
        .merge(this.observeGoogleSpeechStream())
        .filter(message => message) // null 除去
        .finally(() => {
            delete this.cache[key]
        })
        .publish() // Hot Observable 化
        .refCount() // 最初の subscriber が現れたら放流開始、subscriber が誰もいなくなったら放流停止

}

StreamManager.prototype.observeNetworkPacketStream = function (key) {
    // Redis Stream から新規データを読み取り、パースする一連の処理
    return this.redisStream.observeNewRedisStreamEvent(key)
        .filter(message => message) // null 除去
        .map(message => {
            // message のフォーマットは Redis Stream に準ずる
            // [ key, [ filed_1, string_1, field_2, string_2, ... ] ]
            //
            // このままでは扱いにくいので 平坦なフォーマットを Object に変える
            let eventType = EVENT_TYPE_NETWPRK_PACKET
            let timestamp = message[0]
            let dataArray = message[1]
            let data = {}
            for (let i = 0; i < Math.floor(dataArray.length / 2); i++) {
                // filed_1, string_1, field_2, string_2, ...
                // というデータ構造なので、key, value として Object にする
                let k = dataArray[i * 2]
                let v = dataArray[i * 2 + 1]

                // Redisでの格納値は、元の形式が Number でも string　になるので、
                // Number である項目の値は Number に戻す
                if (k.endsWith('_port')) {
                    v = Number(v)
                } else if (k.startsWith('rtp_') && !k.endsWith('_payload')) {
                    v = Number(v)
                }

                // data に格納
                data[k] = v
            }

            // 次へデータを流す
            return { eventType, timestamp, data }
        })
        .do(message => {
            // RTPをパースして data に付け加える処理
            let eventType = message.eventType
            let timestamp = message.timestamp
            let data = message.data

            if (data.type == "RTP" && data.rtp_payload_type == 0) {
                // エンコードされた payload を Buffer に戻す
                let rtp_payload = Buffer.from(data.rtp_payload, 'base64')

                // Google Speech to Text に RTP ペイロードを送る
                if (this.enableGoogleSpeech && rtp_payload) {
                    // IN/OUT の向きを確認して、それぞれの Google Speech へ投げる
                    if (this.googleSpeech_IN && data.dst_addr && key.includes(data.dst_addr)) {
                        this.googleSpeech_IN.sendChunk(rtp_payload)
                    } else if (this.googleSpeech_OUT && data.src_addr && key.includes(data.src_addr)) {
                        this.googleSpeech_OUT.sendChunk(rtp_payload)
                    }
                }
            }
        })
}


StreamManager.prototype.observeGoogleSpeechStream = function () {
    // Google Speech が有効な場合
    if (this.googleSpeech_IN && this.googleSpeech_OUT) {
        return Rx.Observable.of(null)
            // IN 向きのテキスト化結果をマージ
            .merge(Rx.Observable.fromEvent(this.googleSpeech_IN, "data")
                .filter(message => message)
                .map(message => {
                    // オブジェクト整形
                    let eventType = EVENT_TYPE_GOOGLE_SPEECH_IN
                    let timestamp = Date.now().toString()
                    let data = message
                    return { eventType, timestamp, data }
                }))
            // OUT 向きのテキスト化結果をマージ
            .merge(Rx.Observable.fromEvent(this.googleSpeech_OUT, "data")
                .filter(message => message)
                .map(message => {
                    // オブジェクト整形
                    let eventType = EVENT_TYPE_GOOGLE_SPEECH_OUT
                    let timestamp = Date.now().toString()
                    let data = message
                    return { eventType, timestamp, data }
                }))
            .filter(message => message) // null 除去

    } else {
        // Google Speech が無効だったら空の Observable を渡す
        return Rx.Observable.of(null)
    }
}


module.exports = StreamManager;
