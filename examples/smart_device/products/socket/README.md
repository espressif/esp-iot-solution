# Product: Socket

* This is a demo of a Socket product.

* A socket is consist of a power meter, a main button, some states would be save in flash and some units.
* Every unit is consist of a button, a relay, an led and a touchpad, and all of them are not forcible.

* Allot the harware resource for socket in socket_init()
	* create a powermeter and a main button object if they are need, then add the handles to the socket object
	* create some unit objects which are consist of the handles of button, relay, led and touchpad, and any of them can be NULL if not needed
	* add the handles of unit objects to the socket object