# controllino-lights-control

With this sketch, you can use a [controllino unit](http://controllino.cc/) as a core unit to connect the buttons in your house with the lights/bel/port,... In extention, you can also use it as part of a full home automation system through a [pyGate](https://github.com/ATT-JBO/pyGate) gateway. 
The following configurations can be set, either through the [configuration tool](https://github.com/ATT-JBO/CLC-configurator) or by editing constants in the sketch:

- which inputs, outputs and relays are active. 
- how each input should be interpreted:
  - a push button
  - a toggle button
  - analog (knob/slider). Note: this setting is only useful in conjunction with other devices, either through [the cloud](https://maker.smartliving.io) or locally with a [pyGate](https://github.com/ATT-JBO/pyGate).
- which output/relay should be triggered by an input (connect a button to a light/bel, port,...)
 
This application can work completely stand-alone, but comes best to use in conjunction with the [Smartliving maker platform](https://maker.smartliving.io) and [pyGate](https://github.com/ATT-JBO/pyGate).
