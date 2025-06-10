// DOM Elements
    const connectButton = document.getElementById('connectBleButton');
    const disconnectButton = document.getElementById('disconnectBleButton');

    const startWorkout = document.getElementById('startWorkout');
    const retrievedValue = document.getElementById('valueContainer');
    const bleStateContainer = document.getElementById('bleState');
    const timestampContainer = document.getElementById('timestamp');



    //Define BLE Device Specs
    var deviceName ='ESP32';
    var bleService = '19b10000-e8f2-537e-4f6c-d104768a1214';
    var ledCharacteristic = '19b10002-e8f2-537e-4f6c-d104768a1214';
    var sensorCharacteristic= '19b10001-e8f2-537e-4f6c-d104768a1214';

    //Global Variables to Handle Bluetooth
    var bleServer;
    var bleServiceFound;
    var sensorCharacteristicFound;

    // Connect Button (search for BLE Devices only if BLE is available)
    connectButton.addEventListener('click', (event) => {
        if (isWebBluetoothEnabled()){
            connectToDevice();
        }
    });

    // Disconnect Button
    disconnectButton.addEventListener('click', disconnectDevice);

    // Write to the ESP32 LED Characteristic
    startWorkout.addEventListener("click", () => {
        var Sets = document.getElementById('Sets').value  
        var WorkoutType = "ReactionHangs";
        var GoalWeight = document.getElementById('GoalWeight').value 

        if ([Sets, GoalWeight].some(value => value === "" || value === null)) {
                alert("Fill in the necessary fields");
        }
        else{
        var dataToSend = `${WorkoutType}, 0, 0,${Sets},${GoalWeight}`; 
        writeOnCharacteristic(dataToSend);
      }});

    // Check if BLE is available in your Browser
    function isWebBluetoothEnabled() {
        if (!navigator.bluetooth) {
            console.log("Web Bluetooth API is not available in this browser!");
            bleStateContainer.innerHTML = "Web Bluetooth API is not available in this browser!";
            return false
        }
        console.log('Web Bluetooth API supported in this browser.');
        return true
    }

    // Connect to BLE Device and Enable Notifications
    function connectToDevice(){
        console.log('Initializing Bluetooth...');
        navigator.bluetooth.requestDevice({
            filters: [{name: deviceName}],
            optionalServices: [bleService]
        })
        .then(device => {
            console.log('Device Selected:', device.name);
            bleStateContainer.innerHTML = 'Connected to device ' + device.name;
            bleStateContainer.style.color = "#24af37";
            device.addEventListener('gattservicedisconnected', onDisconnected);
            return device.gatt.connect();
        })
        .then(gattServer =>{
            bleServer = gattServer;
            console.log("Connected to GATT Server");
            return bleServer.getPrimaryService(bleService);
        })
        .then(service => {
            bleServiceFound = service;
            console.log("Service discovered:", service.uuid);
            return service.getCharacteristic(sensorCharacteristic);
        })
        .then(characteristic => {
            console.log("Characteristic discovered:", characteristic.uuid);
            sensorCharacteristicFound = characteristic;
            characteristic.addEventListener('characteristicvaluechanged', handleCharacteristicChange);
            characteristic.startNotifications();
            console.log("Notifications Started.");
            return characteristic.readValue();
        })
        .then(value => {
            console.log("Read value: ", value);
            const decodedValue = new TextDecoder().decode(value);
            console.log("Decoded value: ", decodedValue);
            let values = decodedValue.split(";");
            for (let i = 0; i <= 5; i++) {
                values[i] = (values[i] / 1000).toFixed(3);  // divide and format to 3 decimals
            }
            

            retrievedValue.innerHTML = 'Max weight: ' + values[4] + ' Average weight: ' + values[5];
        })
        .catch(error => {
            console.log('Error: ', error);
        })
    }

    function onDisconnected(event){
        console.log('Device Disconnected:', event.target.device.name);
        bleStateContainer.innerHTML = "Device disconnected";
        bleStateContainer.style.color = "#d13a30";

        connectToDevice();
    }

    function handleCharacteristicChange(event){
        const newValueReceived = new TextDecoder().decode(event.target.value);
        let values = newValueReceived.split(";");
        for (let i = 0; i <= 5; i++) {
            values[i] = (values[i] / 1000).toFixed(3);  // divide and format to 3 decimals
        }
        

        retrievedValue.innerHTML = 'Max weight: ' + values[4] + ' Average weight: ' + values[5];
        }


    function handleCharacteristicChange(event) {
        const newValueReceived = new TextDecoder().decode(event.target.value);
        let values = newValueReceived.split(";").filter(v => v !== '');

  let messageToSend = '';
  let displayText = '';

  if (values.length === 1) {
    // Single value received
    messageToSend = 'Reaction Time: ' + values[0];
    displayText = 'Reaction time: ' + values[0];
  } else if (values.length >= 6) {
    // Full data received, use max and average
    messageToSend = 'Max weight: ' + values[4] + ' Average weight: ' + values[5];
    displayText = messageToSend;
  } else {
    // Optional: handle unexpected cases
    displayText = 'Incomplete data received.';
  }

  // Update the page
  if (retrievedValue) {
    retrievedValue.innerHTML = displayText;
  }

  // Send message to server
  if (messageToSend) {
   // sendMessageToServer(messageToSend);
  }
}

    function writeOnCharacteristic(value){
        if (bleServer && bleServer.connected) {
            bleServiceFound.getCharacteristic(ledCharacteristic)
            .then(characteristic => {
                console.log("Found the LED characteristic: ", characteristic.uuid);
                let data;
                if (typeof value === "number") {
                    data = new Uint8Array([value]);
                } else if (typeof value === "string") {
                    data = new TextEncoder().encode(value);
                } else {
                    throw new Error("Unsupported value type");
                }
                return characteristic.writeValue(data);
            })
            .then(() => {
                console.log("Value written to LED characteristic:", value);
            })
            .catch(error => {
                console.error("Error writing to the LED characteristic: ", error);
            });
        } else {
            console.error ("Bluetooth is not connected. Cannot write to characteristic.");
            window.alert("Bluetooth is not connected. Cannot write to characteristic.\nConnect to BLE first!");
        }
    }

    function disconnectDevice() {
        console.log("Disconnect Device.");
        if (bleServer && bleServer.connected) {
            if (sensorCharacteristicFound) {
                sensorCharacteristicFound.stopNotifications()
                    .then(() => {
                        console.log("Notifications Stopped");
                        return bleServer.disconnect();
                    })
                    .then(() => {
                        console.log("Device Disconnected");
                        bleStateContainer.innerHTML = "Device Disconnected";
                        bleStateContainer.style.color = "#d13a30";

                    })
                    .catch(error => {
                        console.log("An error occurred:", error);
                    });
            } else {
                console.log("No characteristic found to disconnect.");
            }
        } else {
            // Throw an error if Bluetooth is not connected
            console.error("Bluetooth is not connected.");
            window.alert("Bluetooth is not connected.")
        }
    }

    function getDateTime() {
        var currentdate = new Date();
        var day = ("00" + currentdate.getDate()).slice(-2); // Convert day to string and slice
        var month = ("00" + (currentdate.getMonth() + 1)).slice(-2);
        var year = currentdate.getFullYear();
        var hours = ("00" + currentdate.getHours()).slice(-2);
        var minutes = ("00" + currentdate.getMinutes()).slice(-2);
        var seconds = ("00" + currentdate.getSeconds()).slice(-2);

        var datetime = day + "/" + month + "/" + year + " at " + hours + ":" + minutes + ":" + seconds;
        return datetime;
    }

    if (!navigator.bluetooth) {
        alert("Web Bluetooth not available. Please use Chrome, Edge, or Opera.");
    }