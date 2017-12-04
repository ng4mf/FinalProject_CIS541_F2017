from flask import Flask, render_template, make_response, redirect, url_for
from flask_bootstrap import Bootstrap
from flask_nav import Nav
from flask_nav.elements import *
import paho.mqtt.client as mqtt
import time
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib import style
from matplotlib.backends.backend_agg import FigureCanvasAgg as FigureCanvas
import StringIO


# Global variables for graphing
x = [1, 2, 3, 4, 5]
y = [5, 4, 3, 2, 1]

broker_address = "35.188.242.1"
port = 1883
timeout = 60
username = "mbed"
password = "homework"
uuid = "1234"
topic = "cis541/hw-mqt/26013f37-08009003ae2a90e552b1fc8ef5001e87/echo"
qos = 0


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

client = mqtt.Client(client_id=uuid)
client.on_connect = on_connect
client.on_message = on_message
client.on_disconnect = on_disconnect
client.username_pw_set(username, password)

client.connect(broker_address, port, 60)
client.loop_start()

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

# @app.route('/graph')
# def graph():
#     # Matplotlib setup
#     style.use('fivethirtyeight')  # Make the graphs look better
#
#     fig = plt.figure()
#     ax1 = fig.add_subplot(1, 1, 1)
#
#     ax1.clear()
#     ax1.plot(x, y)
#
#     # ani = animation.FuncAnimation(fig, animate, interval=1000)
#
#     plt.savefig('static/graphs/graph.png')
#
#     return '<img src=' + url_for('static/graphs', filename='graph.png') + '>'


    # canvas = FigureCanvas(fig)
    # png_output = StringIO.StringIO()
    # canvas.print_png(png_output)
    # response = make_response(png_output.getvalue())
    # response.headers['Content-Type' = image/png'']

if __name__ == "__main__":


    app.run(debug=True)