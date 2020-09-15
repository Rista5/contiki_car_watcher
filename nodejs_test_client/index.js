var amqp = require('amqplib/callback_api');
const { exit } = require('process');
var readline = require('readline');
var mqMs = require('./mqtt-message');


const exchange = 'amq.topic';
const USERID = 'client1';


const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout
});

function getRoutingKey(userId) {
    return 'server.' + userId;
}

function sendBasicMessage(channel, userId, msgNum) {
    var msg = mqMs.createMessageObject(msgNum);
    var msgStr = JSON.stringify(msg);
    var buff = Buffer.from(msgStr)
    channel.publish(exchange, getRoutingKey(userId), buff);
}

var intervalCallback = null;
function setReadingInterval(channel, userId, interval) {
    var msg = mqMs.createSetIntervalObject(interval);
    var msgStr = JSON.stringify(msg);
    var buff = Buffer.from(msgStr)
    channel.publish(exchange, getRoutingKey(userId), buff);
}

function printInfo() {
    console.log('Press:')
    console.log("2 - to start sensor publisher");
    console.log("3 - to stop sensor publisher");
    console.log("4 - to send a warning");
    console.log('5 - to stop a warning');
    console.log('6 - to turn off a car');
    console.log('7 - to (enable/turn on) a car ');
    console.log('8 - to get sensor publish interval');
    console.log('9 - to set sensor publish interval');

    console.log('q/Q - to exit');
}

const process_input = (num, channel, userId) => {
    if(intervalCallback != null) {
        intervalCallback(channel, userId, num);
        intervalCallback = null;
        return;
    }

    switch (num) {
        case mqMs.msg_event_num.START_SENDING:
            sendBasicMessage(channel, userId, num);
            break;
        case mqMs.msg_event_num.STOP_SENDING:
            sendBasicMessage(channel, userId, num);
            break;
        case mqMs.msg_event_num.SEND_WARNING: 
            sendBasicMessage(channel, userId, num);
            break;
        case mqMs.msg_event_num.STOP_WARNING: 
            sendBasicMessage(channel, userId, num);
            break;
        case mqMs.msg_event_num.TURN_OFF_CAR:
            sendBasicMessage(channel, userId, num);
            break;
        case mqMs.msg_event_num.ENABLE_CAR_ENGINE:
            sendBasicMessage(channel, userId, num);
            break;
        case mqMs.msg_event_num.GET_SENSOR_PUBLISH_INTERVAL:
            sendBasicMessage(channel, userId, num);
            break;
        case mqMs.msg_event_num.SET_READING_INTERVAL:
            console.log('Inesrt new interval');
            intervalCallback = setReadingInterval;
            break;
        default: 
            console.log('Message number ' + num + ' not defined');
            break;
    }
}

const start = async (channel, userId) => {
    printInfo();
    for await (const line of rl) {
        console.log(line);
        if(!isNaN(Number(line))) {
            process_input(Number(line), channel, userId);
        } else if (line.startsWith("q") || line.startsWith("q")) {
            exit(0);
        }
        if(intervalCallback == null)
        {
            printInfo();
        }
    }    
}


amqp.connect('amqp://localhost', function(error0, connection){
    if (error0){
        console.error('Error connecting to rabbtimq\n');
        throw error0;
    }
    connection.createChannel(function(error1, channel){
        if (error1){
            console.error('Error while creating channel\n')
        }
        
        channel.assertQueue('', {autoDelete: true}, (err, q) => {
            if (err) {
                console.log('Error creating queue\n');
                throw err;
            }

            channel.bindQueue(q.queue, exchange, '*.server');
            console.log("Queue bound\n");

            channel.consume(q.queue, (msg) => {
                console.log("SENDER: ");
                var dotLoc = msg.fields.routingKey.indexOf(".");
                console.log(msg.fields.routingKey.substring(0, dotLoc));
                console.log('RECEIVED MESSAGE: ');
                let msgText = msg.content.toString();
                console.log(msgText)
                try {
                    console.log(JSON.parse(msgText));
                }
                catch(err) {
                    console.log(err);
                    console.log('DATA:');
                    console.log(msg.content);
                    console.log(msg.content.toString());
                }
            });
        });
        
        start(channel, USERID);
    });
});