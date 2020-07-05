const Rx = require('rxjs/Rx');
const Redis = require('ioredis');

function RedisStream(redis_hostname, redis_port, redis_database) {
    this.redis_config = {
        port: redis_port,
        host: redis_hostname,
        db: redis_database
    }

}

// キャッシュ
const cache = {}

RedisStream.prototype.observeNewRedisStreamEvent = function observeNewRedisStreamEvent(key) {

    // すでに作成済の Obervable に対するリクエストだったら、作ったものを返す。
    if (cache[key]) return cache[key]

    let nextId = '$'
    let redis = new Redis(this.redis_config)

    // キャッシュに入れつつ、Observable を返す
    return cache[key] = Rx.Observable.of(null)
        .expand(() => { // Empty() を返すまで再帰実行。
            // redis.xread の結果を返し続ける。結果、Empty() は返さないので、終了されるまで実行される
            if (nextId == '$') {
                return Rx.Observable.fromPromise(redis.xread('BLOCK', '0', 'COUNT', '100', 'STREAMS', key, nextId))
            } else {
                return Rx.Observable.fromPromise(redis.xread('BLOCK', '10', 'COUNT', '10000', 'STREAMS', key, nextId))
            }
        })
        .filter(streams => streams) // null 除去
        .flatMap(streams => streams) // 配列を平坦に
        .flatMap(stream => stream[1]) // 1つ目のみ抽出
        .do(streamEvent => { // do はストリームに影響しない副次的処理。流れてきた値はそのまま流れる。
            // 次に redisから検索するキー nextId をセットする
            let lastIds = streamEvent[0].split('-')
            let nextIds = nextId.replace('$', '0-0').split('-')
            if (!nextId ||
                (Number(lastIds[0]) > Number(nextIds[0])) ||
                (Number(lastIds[0]) == Number(nextIds[0]) && Number(lastIds[1]) > Number(nextIds[1]))
            ) {
                nextId = streamEvent[0];
            }
        })
        .finally(() => { // 終了手順
            redis.quit(); // redis を切断
            delete cache[key]; // キャッシュを削除
        })
        .publish() // Hot Observable を作成
        .refCount(); // 最初の subscriber が現れたら放流開始、subscriber が誰もいなくなったら放流停止
}

module.exports = RedisStream;
