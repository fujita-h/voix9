require('supports-color')
const path = require('path')
const express = require('express')
const bodyParser = require('body-parser')
const Rx = require('rxjs/Rx');
const WebSocketServer = require('ws').Server
const StreamManager = require('./stream-manager')
const debug = require('debug')('streamserver');
const debug_performance = require('debug')('streamserver:performance')

function Server(serverSettings, redisSettings) {
    this.port = serverSettings.port
    this.webSocketPath = serverSettings.wsPath

    this.server = null
    this.webConnections = []
    this.websocketConnections = []
    if (process.env.GOOGLE_APPLICATION_CREDENTIALS) {
        this.streamManager = new StreamManager(redisSettings, true)
    } else {
        this.streamManager = new StreamManager(redisSettings, false)
    }
}

Server.prototype.start = function () {
    debug("start() called.")
    var app = express()
    app.use(bodyParser.json())

    // import router api
    app.use('/api', require('./router-api'));

    // Static Content
    app.use('/', express.static(path.join(process.cwd(), './content_root')))

    // Start Web Server
    this.server = app.listen(this.port)
    debug('Server Starting on port %s', this.port)
    this.server.on('connection', connection => {
        this.webConnections.push(connection);
        connection.on('close', () => connections = this.webConnections.filter(curr => curr !== connection));
    });

    // Start WebSocket Server
    var webSocketServer = new WebSocketServer({ server: this.server, path: this.webSocketPath })

    Rx.Observable.fromEvent(webSocketServer, 'connection')
        .filter(event => event)
        .flatMap(event => {
            let websocket = event[0]
            let request = event[1]

            const disconnect$ = Rx.Observable.fromEvent(websocket, 'close')
                .do(() => {
                    debug("socket disconnected.")

                    // remove socket
                    this.websocketConnections.splice(this.websocketConnections.indexOf(websocket), 1)
                });

            this.websocketConnections.push(websocket);

            // parse query
            let queryString = request.url.replace(new RegExp("^" + this.webSocketPath), "");
            let queries = [...new URLSearchParams(queryString).entries()].reduce((obj, e) => ({ ...obj, [e[0]]: e[1] }), {});
            let key = queries.key ? queries.key : null;
            let parse = queries.parse ? queries.parse.toLowerCase().split(',') : null
            let filter = queries.filter ? queries.filter.toLowerCase().split(',') : null

            if (!key) {
                websocket.close()
            } else {
                let remote = request.headers['x-forwarded-for'] ||
                    request.connection.remoteAddress ||
                    request.socket.remoteAddress ||
                    (request.connection.socket ? request.connection.socket.remoteAddress : null);
                debug('WebSocket connected:', 'remote=>', remote, ', key=>', key)
            }

            return this.streamManager.observeAllStream(key) // Hot Observable の購読開始
                .do(message => {
                    websocket.send(JSON.stringify(message));
                })
                .takeUntil(disconnect$); // read new message until socket disconnects

        }).finally(() => {
            // disconnect all sockets
            this.websocketConnections.forEach((websocket) => { websocket.close() })
        })
        .takeUntil(
            // listen until server stop signal
            Rx.Observable.merge(
                Rx.Observable.fromEvent(process, 'SIGINT'),
                Rx.Observable.fromEvent(process, 'SIGTERM')
            ))
        .subscribe(() => { });

    setInterval(() => {
        let heapUsed = process.memoryUsage().heapUsed;

        let wsQueueBytes = 0;
        let wsConnectionCount = 0;
        this.websocketConnections.forEach(element => {
            wsQueueBytes += element.bufferedAmount
            wsConnectionCount++;
        });

        debug_performance(
            "Heap:" + Math.round(heapUsed / 1024 / 1024 * 100) / 100 + "MB,",
            "WebSocket Conn:", wsConnectionCount, ",",
            "WebSocket Queue:", Math.round(wsQueueBytes / 1024 / 1024 * 100) / 100, "MB")

    }, 10 * 1000);

    debug('server started.')

}

Server.prototype.stop = function () {
    debug("stop() called.")
    try {
        this.webConnections.forEach(x => {
            try {
                x.destroy();
            } catch { }
        })
        this.websocketConnections.forEach(x => {
            try {
                x.close();
            } catch { }
        })
    } catch { }
    debug('server stopped.')
}


module.exports = Server;