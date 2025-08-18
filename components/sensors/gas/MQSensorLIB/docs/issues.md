# Issues y soluciones

Este documento resume los problemas reportados en el repositorio y las soluciones propuestas o implementadas.

## Abiertos

### #75 MQ-3 sensor and CH4 gas reading 'ovf' failure
**Estado:** resuelto en la rama `main`

El usuario reporta desbordamiento ("ovf") al utilizar valores muy altos en `setA` y `setB` para leer CH4 con un sensor MQ-3. La solución propuesta es revisar los valores utilizados en `setA` y `setB`, ya que `2*10^31` en C++ no corresponde a `2e31`. Se recomienda usar notación exponencial (`2e31`) o `pow(10,31)` y comprobar que los parámetros no excedan el rango de `float`.

**Actualización:** se añadieron validaciones de entrada, predicción de desbordamiento mediante logaritmos y verificación del resultado final para evitar valores "ovf" incluso con coeficientes muy altos. Las nuevas funciones limitan `setA` y `setB` y emplean cálculos en doble precisión.

### #74 possible error in the calculation formula for `_RS_Calc`
**Estado:** resuelto en la rama `work`

Se detectó que la resistencia del sensor se calculaba con `_VOLT_RESOLUTION` en lugar del voltaje de alimentación real. Se añadieron los métodos `setVCC` y `getVCC` y se modificaron las ecuaciones para usar `VCC`. Esta corrección se refleja en la versión 3.0.1 de la biblioteca.

### #70 Parameters to model temperature and humidity dependence
**Estado:** resuelto en la rama `work`

Se añadieron variables opcionales de "correction factor" en todos los ejemplos y se extendieron las funciones `calibrate` y `readSensorR0Rs` para aceptar este parámetro opcional. Así, el usuario puede ajustar las lecturas en función de temperatura y humedad cuando el datasheet lo permita. Los coeficientes deben consultarse para cada sensor.

### #67 Sensor won't finish the Calibration process if done in clean air
**Estado:** abierto

Se reporta que la calibración se detiene mostrando un mensaje de "Conection issue" cuando se intenta calibrar el sensor MQ-135 en aire limpio. La recomendación del mantenedor es revisar la conexión física y probar el sensor con un programa básico para asegurar su correcto funcionamiento antes de usar la librería. Aún no se ha implementado un cambio en el código.


## Cerrados destacados

Para obtener más información sobre todos los issues cerrados, consulte la página de [Issues en GitHub](https://github.com/miguel5612/MQSensorsLib/issues?q=is%3Aissue+is%3Aclosed).

