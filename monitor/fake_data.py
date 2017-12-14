import paho.mqtt.client as mqtt
import time

broker_address = "35.188.242.1"
port = 1883
timeout = 60
username = "mbed"
password = "homework"
uuid = "2345"
# topic = "cis541/hw-mqt/26013f37-08009003ae2a90e552b1fc8ef5001e87/echo"
topic = "group8/data"
qos = 0


# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    print("Connected with result code " + str(rc))

    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    # client.subscribe(topic)


client = mqtt.Client(client_id=uuid, clean_session=True)
client.on_connect = on_connect

# Set username and password
client.username_pw_set(username, password)

client.connect(broker_address, port, 60)

# Blocking call that processes network traffic, dispatches callbacks and
# handles reconnecting.
# Other loop*() functions are available that give a threaded interface and a
# manual interface.

client.loop_start()

data = ""
while True:
    count = 0
    while count != 20:
        # data = float(count) / 10.0
        if count % 2 == 0:
            data = str(count) + "= The heart is beating slow"
        else:
            data = str(count) + "= The heart is beating fast"

        print("Publishing {0}".format(data))
        client.publish(topic, data, qos=0)
        count += 1
        time.sleep(2)


