from flask import Flask, render_template, redirect, url_for
from flask_bootstrap import Bootstrap
from flask_nav import Nav
from flask_nav.elements import *
import paho.mqtt.client as mqtt
import time

broker_address = "35.188.242.1"
port = 1883
timeout = 60
username = "mbed"
password = "homework"
uuid = "26013f37-08009003ae2a90e552b1fc8ef5001e87"
topic = "cis541/hw-mqt/26013f37-08009003ae2a90e552b1fc8ef5001e87/echo"
qos = 0

# f = open("data.txt", "w")

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    # f.write("Connected with result code "+str(rc))
    print("Connected with result code " + str(rc))

    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe(topic)

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    # f.write(msg.topic+" "+str(msg.payload))
    print(msg.topic+" "+str(msg.payload) + '\n')

def on_disconnect(client, userdata, rc):
    print("Closing data file...")

client = mqtt.Client(client_id=uuid, clean_session=True)
client.on_connect = on_connect
client.on_message = on_message
client.on_disconnect = on_disconnect
client.username_pw_set(username, password)

client.connect(broker_address, port, 60)
client.loop_start()


# count = 0
# while count != 30:
#     client.publish(topic, count, qos=0)
#     count += 1
#     time.sleep(1)
#
# client.disconnect()





nav = Nav()

# @todo Apply theme to Nav Bar
@nav.navigation()
def mynavbar():
    return Navbar(
        'Pacemaker Project',
        View('Dashboard', 'index'),
        View('About', 'about')
    )

app = Flask(__name__)
Bootstrap(app)
nav.init_app(app)

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/about')
def about():
    return render_template('about.html', message='About page under construction')

if __name__ == "__main__":
    app.run(debug=True)