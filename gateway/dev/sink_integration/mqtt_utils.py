import paho.mqtt.client as mqtt
import global_variables as g
import params as par
import json

def on_publish_cb(client, data, result):
    g.publish_callback_counter += 1
    g.net.addPrint("[MQTT] on_publish_cb. data: "+str(data)+", result: "+str(result))

def on_disconnect_cb(client, userdata, result):
    g.net.addPrint("[MQTT] on_disconnect. result: "+str(result))

    g.mqtt_connected=False

def on_connect_cb(client, userdata, flags, rc):
    g.net.addPrint("[MQTT] on_connect. result code: "+str(rc))

    if rc==0:
        g.mqtt_connected=True
    else:
        g.mqtt_connected=False

    #client.subscribe() #in case data needs to be received, subscribe here

def on_message_cb(client, userdata, msg):
    g.net.addPrint("[MQTT] on_message_cb.")

def connect_mqtt():
    g.net.addPrint("[MQTT] Connecting to mqtt broker...")

    client=mqtt.Client()
    client.on_publish=on_publish_cb
    client.on_connect=on_connect_cb
    client.on_disconnect=on_disconnect_cb
    client.on_message=on_message_cb

    client.username_pw_set(par.DEVICE_TOKEN, password=None)
    client.connect(par.MQTT_BROKER,par.MQTT_PORT)
    
    return client

def publish_mqtt(mqtt_client, data_to_publish, topic):
    if mqtt_client==None:
        g.net.addPrint("[MQTT] Not yet connected, cannot publish "+str(data_to_publish))    
    qos=1
    try:
        g.publish_function_counter += 1
        ret=mqtt_client.publish(topic, json.dumps(data_to_publish),qos)
        # g.net.addPrint("[GGG] counters: function {}, callback {}".format(g.publish_function_counter, g.publish_callback_counter))
        if ret.rc: #print only in case of error
            g.net.addPrint("[MQTT] publish return: "+str(ret.rc))
    except Exception:
        g.net.addPrint("[MQTT] Exception during publishing!!!!")
        
