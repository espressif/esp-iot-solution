# Example

### To run the examples here, you need to set IOT_SOLUTION_PATH pointing to the root path of this project.


---
### Add IOT_SOLUTION_PATH to User Profile

To preserve setting of ``IOT_SOLUTION_PATH`` environment variable between system restarts, add it to the user profile, following instructions below.

---

### Windows

The user profile scripts are contained in ``C:/msys32/etc/profile.d/`` directory. They are executed every time you open an MSYS2 window.

* Create a new script file in ``C:/msys32/etc/profile.d/`` directory. Name it ``export_iot_path.sh``.

* Identify the path to ESP-IDF directory. It is specific to your system configuration and may look something like ``C:\msys32\home\user-name\esp\esp-iot-solution``

* Add the ``export`` command to the script file, e.g.

	```
export IOT_SOLUTION_PATH="C:/msys32/home/user-name/esp/esp-iot-solution"
```

   Remember to replace back-slashes with forward-slashes in the original Windows path.

* Save the script file.

* Close MSYS2 window and open it again. Check if ``IOT_SOLUTION_PATH`` is set, by typing

	```
printenv IOT_SOLUTION_PATH
```

   The path previusly entered in the script file should be printed out.

---
### Linux and MacOS

Set up ``IOT_SOLUTION_PATH`` by adding the following line to ``~/.profile`` file:

```
export IDF_PATH=~/esp/esp-iot-solution
```

Log off and log in back to make this change effective. 

   * If you have ``/bin/bash`` set as login shell, and both ``.bash_profile`` and ``.profile`` exist, then update ``.bash_profile`` instead.

Run the following command to check if ``IOT_SOLUTION_PATH`` is set::
    
```
printenv IOT_SOLUTION_PATH
```

The path previously entered in ``~/.profile`` file (or set manually) should be printed out.

If you do not like to have ``IOT_SOLUTION_PATH`` set up permanently, you should enter it manually in terminal window on each restart or logout:

```
    export IDF_PATH=~/esp/esp-iot-solution
```


