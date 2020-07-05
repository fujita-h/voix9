require('supports-color')
require('heapdump');

const settings = {
    server: {
        port: +process.env.PORT || 3000,
        wsPath: +process.env.WS_PATH || '/api/websocket/stream'

    },
    redis: {
        host: process.env.REDIS_HOST || "127.0.0.1",
        port: +process.env.REDIS_PORT || 6379,
        db: +process.env.REDIS_DB || 0
    }
}

var Server = require('./server');
var server = new Server(settings.server, settings.redis)
server.start();

process.on('SIGINT', () => {
    server.stop()
    process.exit();
});
process.on('SIGTERM', () => {
    server.stop()
    process.exit();
});
