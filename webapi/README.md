# webapi

## 必要なソフトウェア

- Node.js >= 12

## ライブラリのインストール

```Shell
npm install
```

## 起動

```Shell
node index.js
```

### 環境変数

- `PORT`: Listenするポート番号。デフォルト: 3000
- `WS_PATH`: WebSocketで使用するURLのパス。デフォルト: /api/websocket/stream
- `REDIS_HOST`: データソースとして使用するRedisのホスト。デフォルト: 127.0.0.1
- `REDIS_PORT`: データソースとして使用するRedisのポート番号。デフォルト: 6379
- `REDIS_DB`: データソースとして使用するRedisのDB番号。デフォルト: 0
- `GOOGLE_APPLICATION_CREDENTIALS`: GoogleのCredentialファイル

### Google Speech-to-Text を使用する場合

`GOOGLE_APPLICATION_CREDENTIALS` 環境変数を設定して実行します。

```Shell
GOOGLE_APPLICATION_CREDENTIALS=path/to/googlecredentials.json node index.js
```
