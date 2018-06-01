import datetime
import fcntl
import io
import json
import jwt
import os
import paho.mqtt.client as mqtt
import Queue
import random
import ssl
import sys
import thread
import threading
import time

class AtlasI2C:
    # the timeout needed to query readings and calibrations
    long_timeout = .8
    short_timeout = .8
    current_addr = None

    def __init__(self, address, bus):
        # open two file streams, one for reading and one for writing
        # the specific I2C channel is selected with bus
        # it is usually 1, except for older revisions where its 0
        # wb and rb indicate binary read and write
        self.file_read = io.open("/dev/i2c-" + str(bus), "rb", buffering=0)
        self.file_write = io.open("/dev/i2c-" + str(bus), "wb", buffering=0)
        # initializes I2C to either a user specified or default address
        self.set_i2c_address(address)

    def set_i2c_address(self, addr):
        '''Set the I2C communications to the slave specified by the address.
        The commands for I2C dev using the ioctl functions are specified in
        the i2c-dev.h file from i2c-tools'''
        I2C_SLAVE = 0x703
        fcntl.ioctl(self.file_read, I2C_SLAVE, addr)
        fcntl.ioctl(self.file_write, I2C_SLAVE, addr)
        self.current_addr = addr

    def write(self, cmd):
        '''Appends the null character and sends the string over I2C'''
        cmd += "\00"
        self.file_write.write(cmd)

    def read(self, num_of_bytes=31):
        '''Reads a specified number of bytes from I2C, then parses and displays the result'''
        res = self.file_read.read(num_of_bytes)  # read from the board
        response = filter(lambda x: x != '\x00', res)  # remove the null characters to get the response
        if ord(response[0]) == 1:  # if the response isn't an error
            # change MSB to 0 for all received characters except the first and get a list of characters
            char_list = map(lambda x: chr(ord(x) & ~0x80), list(response[1:]))
            # NOTE: having to change the MSB to 0 is a glitch in the raspberry pi, and you shouldn't have to do this!
            return ''.join(char_list)  # convert the char list to a string and returns it
        else:
            return "Error " + str(ord(response[0]))

    def query(self, string):
        '''Write a command to the board, wait the correct timeout, and read the response'''
        self.write(string)
        # The read and calibration commands require a longer timeout
        if (string.upper().startswith("R")):
            time.sleep(self.long_timeout)
        elif (string.upper().startswith("CAL")):
            time.sleep(self.short_timeout)
        else:
            time.sleep(self.short_timeout)
        return self.read()

    def close(self):
        self.file_read.close()
        self.file_write.close()

def create_jwt(project_id, private_key_file, algorithm):
    """Create a JWT (https://jwt.io) to establish an MQTT connection."""
    token = {
        # The time that the token was issued.
        'iat': datetime.datetime.utcnow(),
        # The time that the token expires.
        'exp': datetime.datetime.utcnow() + datetime.timedelta(minutes=60),
        # The GCP project id.
        'aud': project_id
    }
    with open(private_key_file, 'r') as f:
        private_key = f.read()
    print 'Creating JWT using {} from private key file {}'.format(
        algorithm, private_key_file)
    return jwt.encode(token, private_key, algorithm=algorithm)

def error_str(rc):
    """Convert a Paho error to a human readable string."""
    return '{}: {}'.format(rc, mqtt.error_string(rc))

class Device(object):
    """Represents the state of a single device."""
    def __init__(self):
        # Variable to check if the device is collecting data or not.  
        self.collect_data = False
        # Variable to check if device is connected or not
        self.connected = False
        # Prev_id to avoid running the same command twice in config. 
        self.prev_id = None
        # Client object to send and recieve messages.
        self.client = None    
        # Topic where you will send calibration value data stream.
        self.calibration_topic = '/devices/controller1/events/calibration_value'
        # The controller ID
        self.controller_id = 'C001'
        # Queue to hold the data collected from by bus 0 and bus 1 of I2C channel.
        self.result = Queue.Queue()
        # Device_1 | Hex - 97 | Dec - 61 | Type - PH (PH1) | Bus - 1
        self.device_1 = AtlasI2C(address=97, bus=1)
        # Device_2 | Hex - 101 | Dec - 65 | Type - EC (EC2) | Bus - 1
        self.device_2 = AtlasI2C(address=101, bus=1)
        # Device_3 | Hex - 102 | Dec - 66 | Type - EC (EC3) | Bus - 1
        self.device_3 = AtlasI2C(address=102, bus=1)
        # Device_4 | Hex - 101 | Dec - 62 | Type - PH (PH2) | Bus - 0
        self.device_4 = AtlasI2C(address=98, bus=0)
        # Device_2 | Hex - 99 | Dec - 63 | Type - PH (PH3) | Bus - 0
        self.device_5 = AtlasI2C(address=99, bus=0)
        # Device_2 | Hex - 100 | Dec - 64 | Type - EC (EC1) | Bus - 0
        self.device_6 = AtlasI2C(address=100, bus=0)
        
    def get_collect_data_state(self):
        '''Get the state of Collect_data.'''
        return self.collect_data

    def get_data_bus1(self, device1, device2, device3):
        '''Get the data from I2C Channel Bus 1.'''
        ph_data1 = device1.query("R")
        c_data2 = device2.query("R")
        c_data3 = device3.query("R")
        self.result.put({'ph_data1': ph_data1, 'c_data2': c_data2, 'c_data3': c_data3})

    def get_data_bus0(self, device1, device2, device3):
        '''Get the data from I2C Channel Bus 0.'''
        ph_data2 = device1.query("R")
        ph_data3 = device2.query("R")
        c_data1 = device3.query("R")
        self.result.put({'ph_data2': ph_data2, 'ph_data3': ph_data3, 'c_data1': c_data1})

    def prepare_json(self):
        '''Prepare data in json format.'''
        token = self.result.get()
        token.update(self.result.get())
        token.update(self.result.get())
        json_data = json.dumps(token)
        return json_data

    def prepare_payload(self):
        '''Main functioning function called in a loop.
           Put the current datatime and controller_id in queue.
           Then calls the two bus channel to load the data in the queue.
           Call the prepare_json to get all the data in json_format.'''
        current_time = datetime.datetime.now().isoformat()
        self.result.put({'controller_id': self.controller_id, 'current_time': current_time})
        thread1 = threading.Thread(target=self.get_data_bus1, args=(self.device_1, self.device_2, self.device_3))
        thread2 = threading.Thread(target=self.get_data_bus0, args=(self.device_4, self.device_5, self.device_6))
        thread1.start()
        thread2.start()
        thread1.join()
        thread2.join()
        payload = self.prepare_json()
        return payload

    def wait_for_connection(self, timeout):
        """Wait for the device to become connected."""
        total_time = 0
        while not self.connected and total_time < timeout:
            time.sleep(1)
            total_time += 1

        if not self.connected:
            raise RuntimeError('Could not connect to MQTT bridge.')

    def on_connect(self, unused_client, unused_userdata, unused_flags, rc):
        """Callback for when a device connects."""
        print 'Connection Result:', error_str(rc)
        self.connected = True

    def on_disconnect(self, unused_client, unused_userdata, rc):
        """Callback for when a device disconnects."""
        print 'Disconnected:', error_str(rc)
        self.connected = False

    def on_publish(self, unused_client, unused_userdata, unused_mid):
        """Callback when the device receives a PUBACK from the MQTT bridge."""
        print 'Published message acked.'

    def on_subscribe(self, unused_client, unused_userdata, unused_mid,
                     granted_qos):
        """Callback when the device receives a SUBACK from the MQTT bridge."""
        print 'Subscribed: ', granted_qos
        if granted_qos[0] == 128:
            print 'Subscription failed.'

    def on_message(self, unused_client, unused_userdata, message):
        """Callback when the device receives a message on a subscription."""
        payload = str(message.payload)
        print "Received message '{}' on topic '{}' with Qos {}".format(
            payload, message.topic, str(message.qos))
        # The device will receive its latest config when it subscribes to the config
        # topic. If there is no configuration for the device, the device will
        # receive an config with an empty payload.
        if not payload:
            return
        data = json.loads(payload)
        # Save the uuid to overcome atleast one delivery problem of the publish message. (QOS1) 
        if data['id'] == self.prev_id:
            print 'Message already processed. Ignore'
            return
        self.prev_id = data['id']
        # For calibration
        if data['message'] == 'Calibrate Data':
            # Start new thread for calibration since callback are thread blocking. 
            if data['type'] == 'PH':
                # Handle ph caliration
                value = data['value']
                if data['device'] == 'ph1':
                    # Send calibrate value to ph1
                    thread.start_new_thread(self.calibrate_ph, (self.device_1,value,))
                elif data['device'] == 'ph2':
                    # Send calibrate value to ph2
                    thread.start_new_thread(self.calibrate_ph, (self.device_4, value,))
                elif data['device'] == 'ph3':
                    # Send calibrate value to ph3
                    thread.start_new_thread(self.calibrate_ph, (self.device_5, value,))
                else:
                    print 'Illegal device id for ph'
            elif data['type'] == 'EC':
                # Handle ec calibration
                if data['device'] == 'ec1':
                    # Send calibrate value to ec1
                    thread.start_new_thread(self.calibrate_ec, (self.device_6, value,))
                elif data['device'] == 'ec2':
                    # Send calibrate value to ec2
                    thread.start_new_thread(self.calibrate_ec, (self.device_2, value,))
                elif data['device'] == 'ec3':
                    # Send calibrate value to ec3
                    thread.start_new_thread(self.calibrate_ec, (self.device_3, value,))
                else:
                    print 'Illegal device id for ec'
        elif data['message'] == 'Data Collection':
            print 'Data Collection is set to {}'.format(data['collect_data'])
            if data['collect_data'] == 'True':
                self.collect_data = True
            elif data['collect_data'] == 'False':
                self.collect_data = False
            else:
                print 'Illegal state for collect_data'
        else:
            print 'Illegal message'
        return

    def calibrate_ph(self, device, value):
        '''Handle the MID, LOW, HIGH based on value
           Send the reading of the probe to the calibrartion tube for the user to see the trend.
           Issue the command after 2 minutes since value usually stabilize after that time.'''
        command = 'CAL,'
        if value == '7.00':
            command = command + 'MID,' + value
        elif value == '4.00':
            command = command + 'LOW,' + value
        elif value == '10.00':
            command = command + 'HIGH,' + value
        else:
            print 'Illegal Value for ph'
            return
        print command
        intial_time = datetime.datetime.utcnow()
        while ((datetime.datetime.utcnow() - intial_time).seconds <= 120):
            payload = device.query("R")
            print payload
            self.client.publish(self.calibration_topic, payload, qos=1)
            # This will be send to pub sub.
        # 2 minutes have expired
        status = device.query(command)
        print status

    def calibrate_ec(self, device, value):
        '''Handle the MID, LOW, HIGH based on value
           Send the reading of the probe to the calibrartion tube for the user to see the trend.
           Issue the command after 2 minutes since value usually stabilize after that time.'''
        command = 'CAL,'
        if value == '12900':
            command = command + 'LOW,' + value
        elif value == '30100':
            command = command + 'HIGH,' + value
        else:
            print 'Illegal Value for ph'
            return
        print command
        intial_time = datetime.datetime.utcnow()
        while ((datetime.datetime.utcnow() - intial_time).seconds <= 120):
            payload = device.query("R")
            print payload
            self.client.publish(self.calibration_topic, payload, qos=1)
            # This will be send to pub sub.
        # 2 minutes have expired
        status = device.query(command)
        print status
        
    def get_client(
        self, project_id, cloud_region, registry_id, device_id, private_key_file,
        algorithm, ca_certs, mqtt_bridge_hostname, mqtt_bridge_port, device):
        """Create our MQTT client. The client_id is a unique string that identifies
        this device. For Google Cloud IoT Core, it must be in the format below."""
        client = mqtt.Client(
            client_id=('projects/{}/locations/{}/registries/{}/devices/{}'
                .format(
                project_id,
                cloud_region,
                registry_id,
                device_id)
                       )
            )
        # With Google Cloud IoT Core, the username field is ignored, and the
        # password field is used to transmit a JWT to authorize the device.
        client.username_pw_set(
            username='unused',
            password=create_jwt(
                project_id, private_key_file, algorithm))
        # Enable SSL/TLS support.
        client.tls_set(ca_certs=ca_certs, tls_version=ssl.PROTOCOL_TLSv1_2)
        # Register message callbacks. https://eclipse.org/paho/clients/python/docs/
        # describes additional callbacks that Paho supports. In this example, the
        # callbacks just print to standard out.
        client.on_connect = device.on_connect
        client.on_publish = device.on_publish
        client.on_disconnect = device.on_disconnect
        client.on_message = device.on_message
        # Connect to the Google MQTT bridge.
        client.connect(mqtt_bridge_hostname, mqtt_bridge_port)
        # This is the topic that the device will receive configuration updates on.
        mqtt_config_topic = '/devices/{}/config'.format(device_id)
        # Subscribe to the config topic.
        client.subscribe(mqtt_config_topic, qos=1)
        self.client = client
        return client

def main():
    # Senstive data.
    project_id = ''
    cloud_region = ''
    registry_id = ''
    device_id = ''
    private_key_file = ''
    algorithm = ''
    ca_certs = ''
    mqtt_bridge_hostname = 'mqtt.googleapis.com'
    mqtt_bridge_port = 8883
    jwt_iat = datetime.datetime.utcnow()
    jwt_exp_mins = 50
    device = Device()
    client = device.get_client(
        project_id, cloud_region, registry_id, device_id,
        private_key_file, algorithm, ca_certs,
        mqtt_bridge_hostname, mqtt_bridge_port, device)
    client.loop_start()
    mqtt_telemetry_topic = '/devices/{}/events'.format(device_id)
    device.wait_for_connection(5)
    # Start control logic here.
    while (1):
        # This will be when the controller is not doing data recording.
        # [START iot_mqtt_jwt_refresh]
        seconds_since_issue = (datetime.datetime.utcnow() - jwt_iat).seconds
        if seconds_since_issue > 60 * jwt_exp_mins:
            print 'Refreshing token after {}s'.format(seconds_since_issue)
            jwt_iat = datetime.datetime.utcnow()
            client.loop_stop()
            client = device.get_client(
                project_id, cloud_region,
                registry_id, device_id, private_key_file,
                algorithm, ca_certs, mqtt_bridge_hostname,
                mqtt_bridge_port, device)
            client.loop_start()
            device.wait_for_connection(5)
            # [END iot_mqtt_jwt_refresh]
        while (device.get_collect_data_state() == True):
            start = time.time()
            payload = device.prepare_payload()
            print 'Publishing payload', payload
            #This will be when the controller is doing data recording.
            # [START iot_mqtt_jwt_refresh]
            seconds_since_issue = (datetime.datetime.utcnow() - jwt_iat).seconds
            if seconds_since_issue > 60 * jwt_exp_mins:
                print 'Refreshing token after {}s'.format(seconds_since_issue)
                jwt_iat = datetime.datetime.utcnow()
                client.loop_stop()
                client = device.get_client(
                    project_id, cloud_region,
                    registry_id, device_id, private_key_file,
                    algorithm, ca_certs, mqtt_bridge_hostname,
                    mqtt_bridge_port, device)
                client.loop_start()
                device.wait_for_connection(5)
            # [END iot_mqtt_jwt_refresh]
            # Publish "payload" to the MQTT topic. qos=1 means at least once
            # delivery. Cloud IoT Core also supports qos=0 for at most once
            # delivery.
            client.publish(mqtt_telemetry_topic, payload, qos=1)
            stop = time.time()
	    time.sleep(15 - (stop-start) - 0.01 )

if __name__ == '__main__':
    main()
