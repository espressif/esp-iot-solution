Sensor Hub 
===============
:link_to_translation:`zh_CN:[中文]`

Sensor Hub is a sensor management component that can realize hardware abstraction, device management and data distribution for sensor devices. When developing applications based on Sensor Hub, users do not have to deal with complex sensor implementations, but only need to make simple selections for sensor operation, acquisition interval, range, etc. and then register callback functions to the event messages of your interests. By doing so, users can receive notifications when sensor states are switched or when data is collected.

.. figure:: ../../_static/sensor_hub_diagram.png
    :align: center
    :width: 80%

    Sensor Hub Programming Model

Sensor Hub supports loading sensor drivers as components. Users only need to input the name of the sensor registered with the Sensor Hub to directly access the corresponding sensor driver. Additionally, as this component enables centralized management of sensors, it not only simplifies operations but also improves operational efficiency. It can serve as a foundational component for sensor applications and can be applied to various intelligent scenarios such as environmental sensing, motion sensing, health management, and more.

.. figure:: ../../_static/sensor_hub.png
    :align: center
    :width: 80%

    Sensor Hub Driver

Instructions
-------------------

``sensor_hub`` uses the `linker script generation mechanism <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/linker-script-generation.html>`_ to register sensor drivers into specific target file sections. For application developers, there is no need to focus on the specific implementation of the sensor drivers; simply adding the corresponding sensor component will load the respective driver.

Driver developer:
^^^^^^^^^^^^^^^^^^^

Taking the ``ShT3X`` temperature and humidity sensor as an example, driver developers need to populate the ``humiture_impl_t`` structure with operations related to the sensor and create a sensor detection function. ``sensor_hub`` provides a sensor registration interface: ``SENSOR_HUB_DETECT_FN``, where driver developers can directly insert the corresponding functions into the registration interface.

.. code-block:: c

        static humiture_impl_t sht3x_impl = {
            .init = humiture_sht3x_init,
            .deinit = humiture_sht3x_deinit,
            .test = humiture_sht3x_test,
            .acquire_humidity = humiture_sht3x_acquire_humidity,
            .acquire_temperature = humiture_sht3x_acquire_temperature,
        };

        SENSOR_HUB_DETECT_FN(HUMITURE_ID, sht3x, &sht3x_impl);

At the same time, add the interface dependency in the component's ``CMakeLists.txt``:

.. code-block:: cmake

        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u humiture_sht3x_init")

Application developer
^^^^^^^^^^^^^^^^^^^^^^^

1. Add ``sensor_hub`` and the required sensor components in the project's ``idf_component.yml``.

2. Create a sensor instance: Use :cpp:func:`iot_sensor_create` to create a sensor instance. The parameters include the sensor name, sensor configuration options, and a pointer to the sensor handle. The sensor name is used to locate and load the driver registered in the ``sensor_hub``. If the sensor supports configurable addresses, multiple instances of the same sensor name can be created. In the configuration options, ``bus`` specifies the bus to which the sensor is mounted; ``addr`` specifies the address corresponding to the sensor; ``type`` specifies the type of the sensor ; ``mode`` specifies the operating mode of the sensor; and ``min_delay`` specifies the sensor's data collection interval. Other options are optional. Upon successful creation, the sensor handle is obtained.

3. Register callback functions for sensor events: when a sensor event occurs, the callback functions will be called in sequence. There are two ways to register a callback function, and the instance handler of the event callback function will be returned after the registration succeed:

    - Use :cpp:func:`iot_sensor_handler_register` to register a callback function with the sensor handler
    - Use :cpp:func:`iot_sensor_handler_register_with_type` to register a callback function with the sensor type

4. Start a sensor: use :cpp:func:`iot_sensor_start` to start a specific sensor. After started, it will trigger a ``SENSOR_STARTED`` event, then it will collect the sensor data continuously with a set of period and trigger ``SENSOR_XXXX_DATA_READY`` event. The event callback function can obtain the specific data of each event via the ``event_data`` parameter;

5. Stop a sensor: use :cpp:func:`iot_sensor_stop` to stop a specified sensor temporarily. After stopped, the sensor will send out a ``SENSOR_STOPED`` event and then stop the data collecting work. If the driver of this sensor supports power management, the sensor will be set to sleep mode in this stage;

6. Unregister callback functions for sensor events: the user program can unregister an event at any time using the instance handler of this event callback function, and this callback function will not be called again when this event occurs afterwards. There are also two ways to do so:

    - Use :cpp:func:`iot_sensor_handler_unregister` to unregister the callback function with the sensor handler
    - Use :cpp:func:`iot_sensor_handler_unregister_with_type` to unregister the callback function with the sensor type

7. Delete sensors: use :cpp:func:`iot_sensor_delete` to delete the corresponding sensor to release the allocated memory and other resources.

Examples
--------------

1. Sensor control LED example: :example:`sensors/sensor_control_led`.
2. Sensor hub monitor example: :example:`sensors/sensor_hub_monitor`.

API Reference
------------------------

.. include-build-file:: inc/sensor_type.inc

.. include-build-file:: inc/iot_sensor_hub.inc
