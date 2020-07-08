var vueStreamList;
var vueAsideMessage;

var ws
var player = new PlayRtp(0, 8000, 2, false)

setInterval(() => {
    if (ws && ws.readyState == ws.OPEN) {
        ws.send('ping')
    }
}, 5000);

$(document).ready(function () {
    vueStreamList = new Vue({
        el: '#stream-list',
        data: {
            items: []
        }
    })
    vueAsideMessage = new Vue({
        el: '#aside-message',
        data: {
            key: '',
            items: [],
            pendInItem: '',
            pendOutItem: '',
        }
    })
    updateStreamList(vueStreamList);
});

const updateStreamList = (vueStreamList) => {
    getStreamList()
        .then((streamList) => {
            let items = []
            for (var i in streamList.data) {
                getStreamInfo(streamList.data[i].key)
                    .then((data) => { items.push(data.data) })
                    .catch((err) => { })
            }
            vueStreamList.items = items;
        })

}

const getStreamList = () => {
    return new Promise((resolve, reject) => {
        $.ajax({
            url: '/api/streams',
            type: 'GET'
        }).then((data) => {
            resolve(data)
        }).catch((err) => {
            reject(err)
        })
    })
}

const getStreamInfo = (key) => {
    return new Promise((resolve, reject) => {
        $.ajax({
            url: '/api/streams/' + key + '/info',
            type: 'GET'
        }).then((data) => {
            resolve(data)
        }).catch((err) => {
            reject(err)
        })
    })
}

const openMessageBox = (key) => {
    $('#aside-message').removeClass('hidden');
    $('.message-chat-area').outerHeight(window.innerHeight - 250)

    vueAsideMessage.key = key
    vueAsideMessage.items = []
    vueAsideMessage.pendInItem = ''
    vueAsideMessage.pendOutItem = ''

    if (ws) {
        ws.close()
    }
    ws = new WebSocket('ws://' + location.hostname + ':' + location.port + '/api/websocket/stream?key=' + key + '&parse=rtp,google-realtime-text&filter=layer_2,payload')
    ws.onopen = ((wsevent) => {
        console.log("onopen:", wsevent)
    })
    ws.onclose = ((wsevent) => {
        console.log("onclose:", wsevent)
    })
    ws.onerror = ((wserror) => {
        console.log("onerror:", wserror)
    })
    ws.onmessage = ((wsevent) => {
        if (wsevent.data === 'pong') return;
        try {
            var obj = JSON.parse(wsevent.data)
        } catch (err) {
            console.log(err);
            return;
        }
        let timestamp = obj.timestamp
        let eventType = obj.eventType
        let data = obj.data

        if (eventType == "NETWORK_PACKET" && data.type == "RTP") {
            if (data.rtp_payload && key == data.dst_addr) {
                player.playFromBase64(data.rtp_payload, 0);
            } else if (data.rtp_payload && key == data.src_addr) {
                player.playFromBase64(data.rtp_payload, 1);
            }
        } else if (eventType == "GOOGLE_SPEECH_IN" && data.speechEventType) {
            if (data.speechEventType == "SPEECH_EVENT_UNSPECIFIED") {
                if (data.results && data.results[0]) {
                    if (data.results[0].isFinal) {
                        vueAsideMessage.items.push({ direction: 'message-in', message: data.results[0].alternatives[0].transcript })
                        vueAsideMessage.pendInItem = ''
                    } else {
                        vueAsideMessage.pendInItem = data.results[0].alternatives[0].transcript
                    }
                    document.getElementById('message-chat-area').scrollTop = document.getElementById('message-chat-area').scrollHeight;

                }
            }
        } else if (eventType == "GOOGLE_SPEECH_OUT" && data.speechEventType) {
            if (data.speechEventType == "SPEECH_EVENT_UNSPECIFIED") {
                if (data.results && data.results[0]) {
                    if (data.results[0].isFinal) {
                        vueAsideMessage.items.push({ direction: 'message-out', message: data.results[0].alternatives[0].transcript })
                        vueAsideMessage.pendOutItem = ''
                    } else {
                        vueAsideMessage.pendOutItem = data.results[0].alternatives[0].transcript
                    }
                    document.getElementById('message-chat-area').scrollTop = document.getElementById('message-chat-area').scrollHeight;
                }
            }
        }
    })
}

const closeMessageBox = () => {
    $('#aside-message').addClass('hidden');
    if (ws) {
        ws.close()
    }
}

const startRtpPlay = () => {
    $('#message-start-rtp').addClass('hidden')
    $('#message-stop-rtp').removeClass('hidden')
    player.enable()
}

const stopRtpPlay = () => {
    $('#message-start-rtp').removeClass('hidden')
    $('#message-stop-rtp').addClass('hidden')
    player.disable()
}
