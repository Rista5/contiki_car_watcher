const amqp = require('amqplib');

const connectionStringCluster = "172.17.0.2"
const connectionStringLocal = "127.0.0.1"
const exchange = 'amq.topic';
const queueName = 'contiki-k8s';
const queueBindings = ['*.*'];

async function handleContikiMsg(msg) {
    const channel = await clusterChannel;
    const routingKey = msg.fields.routingKey;
    // console.log(msg);
    // console.log(`Routing key ${routingKey}`)

    channel.publish(exchange, routingKey, msg.content);
    (await localChanngel).ack(msg);
}

async function handleClusterMsg(msg) {
    const channel = await localChanngel;
    const routingKey = msg.routingKey;
    // console.log('handleClusterMsg', msg);

    channel.publish(exchange, routingKey, msg.content);
    (await clusterChannel).ack(msg);
}

async function setupRabbitMQConnection(connStr, queueName, exchange, queueBindings, consumeMessage, port) {
    try {
        const conn = await amqp.connect(`amqp://${connStr}:${port}`, {});
        const channel = await conn.createChannel();

        await channel.assertExchange(exchange, 'topic', {durable: true});

        await channel.assertQueue(queueName, { durable: true });
        queueBindings.forEach(bindStr => {
            channel.bindQueue(queueName, exchange, bindStr);
        });
        
        await channel.consume(queueName, consumeMessage);

        process.on('exit', (code) =>{
            channel.close();
        });

        console.log('Connected to RabbitMQ');
        return channel;

    } catch (error) {
        console.error(error);
    }
}


const localChanngel = setupRabbitMQConnection(connectionStringLocal, queueName, exchange, queueBindings, handleContikiMsg, 5672);
const clusterChannel = setupRabbitMQConnection(connectionStringCluster, queueName, exchange, queueBindings, handleClusterMsg, 30123);