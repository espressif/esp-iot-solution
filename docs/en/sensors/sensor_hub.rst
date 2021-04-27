Sensor Hub 
===============
:link_to_translation:`zh_CN:[中文]`

Sensor Hub is a sensor management component that can realize hardware abstraction, device management and data distribution for sensor devices. When developing applications based on Sensor Hub, users do not have to deal with complex sensor implementations, but only need to make simple selections for sensor operation, acquisition interval, range, etc. and then register callback functions to the event messages of your interests. By doing so, users can receive notifications when sensor states are switched or when data is collected.

.. figure:: ../../_static/sensor_hub_diagram.png
    :align: center
    :width: 80%

    Sensor Hub Programming Model

The Sensor Hub provides hardware abstraction for common sensor categories, based on which users can switch sensor models without modifying the upper layer of the application. And it allows adding new sensors to the Sensor Hub by implementing their corresponding sensor interface at hardware abstraction layer. This component can be used as a basic component for sensor applications in various intelligent scenarios such as environment monitoring, motion detection, health management and etc. as it simplifies operation and improves operating efficiency by centralized management of sensors.

.. figure:: ../../_static/sensor_hub.png
    :align: center
    :width: 80%

    Sensor Hub Driver

Instructions
-------------------

1. Create a sensor instance: use :cpp:func:`iot_sensor_create` to create a sensor instance. The related parameters include the sensor ID defined in ``sensor_id_t``, configuration options for the sensor and its handler pointer. The sensor ID is used to find and load the corresponding driver, and each ID can only be used for one sensor instance. In configuration options, `bus` is used to specify the bus location on which the sensor is mounted; `mode` is used to specify the operating mode of the sensor; `min_delay` is used to specify the acquisition interval of the sensor, while other items inside are all non-required options. After the instance is created, the sensor handler is obtained;

2. Register callback functions for sensor events: when a sensor event occurs, the callback functions will be called in sequence. There are two ways to register a callback function, and the instance handler of the event callback function will be returned after the registration succeed:

    - Use :cpp:func:`iot_sensor_handler_register` to register a callback function with the sensor handler
    - Use :cpp:func:`iot_sensor_handler_register_with_type` to register a callback function with the sensor type

3. Start a sensor: use :cpp:func:`iot_sensor_start` to start a specific sensor. After started, it will trigger a ``SENSOR_STARTED`` event, then it will collect the sensor data continuously with a set of period and trigger ``SENSOR_XXXX_DATA_READY`` event. The event callback function can obtain the specific data of each event via the ``event_data`` parameter;

4. Stop a sensor: use :cpp:func:`iot_sensor_stop` to stop a specified sensor temporarily. After stopped, the sensor will send out a ``SENSOR_STOPED`` event and then stop the data collecting work. If the driver of this sensor supports power management, the sensor will be set to sleep mode in this stage;

5. Unregister callback functions for sensor events: the user program can unregister an event at any time using the instance handler of this event callback function, and this callback function will not be called again when this event occurs afterwards. There are also two ways to do so:

    - Use :cpp:func:`iot_sensor_handler_unregister` to unregister the callback function with the sensor handler
    - Use :cpp:func:`iot_sensor_handler_unregister_with_type` to unregister the callback function with the sensor type

6. Delete sensors: use :cpp:func:`iot_sensor_delete` to delete the corresponding sensor to release the allocated memory and other resources.

Examples
--------------

1. Sensor control LED example: :example:`sensors/sensor_control_led`.
2. Sensor hub monitor example: :example:`sensors/sensor_hub_monitor`.

API Reference
------------------------

.. include:: /_build/inc/sensor_type.inc

.. include:: /_build/inc/iot_sensor_hub.inc
