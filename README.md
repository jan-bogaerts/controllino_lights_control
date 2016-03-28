# controllino-lights-control

With this sketch, you can use a [controllino unit](http://controllino.cc/) as part of a home automation system with a [pyGate](https://github.com/ATT-JBO/pyGate) at it's center. 
The following configurations can be set, either by editing the sketch or by using the [configuration tool](https://github.com/ATT-JBO/CLC-configurator):

- which inputs, outputs and relays are active. 
- how each input should be interpreted:
  - a push button
  - a toggle button
  - analog (knob/slider). Note: this setting is only useful in conjunction with other devices, either through [the cloud](https://maker.smartliving.io) or locally with a [pyGate](https://github.com/ATT-JBO/pyGate).
- which output/relay should be triggered by an input (connect a button to a light/bel, port,...)
 
This application can work completely stand-alone, but comes best to use in conjunction with the [Smartliving maker platform](https://maker.smartliving.io).
