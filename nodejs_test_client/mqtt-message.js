
const msg_event_num = {
    UNDEFINED_MESSAGE: 0,
    SENSOR_READING: 1,
    START_SENDING: 2,
    STOP_SENDING: 3,
    SEND_WARNING: 4,
    STOP_WARNING: 5,
    TURN_OFF_CAR: 6,
    ENABLE_CAR_ENGINE: 7,
    GET_SENSOR_PUBLISH_INTERVAL: 8,
    SET_READING_INTERVAL: 9
}

function createMessageObject(msg_num) {
    return {
        event: msg_num
    };
}

function createSetIntervalObject(interval) {
    return {
        event: msg_event_num.SET_READING_INTERVAL,
        newInterval: interval
    }
}

module.exports = {
    msg_event_num, 
    createMessageObject,
    createSetIntervalObject    
}
