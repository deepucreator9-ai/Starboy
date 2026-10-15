# Starboy
**A Hack Club's Half Life program's fun Project.**


I've now completed the project digitally all is left is i just get my items and build it physically!!
Let me introduce you to my project piece by piece:

The first thing you see is the schematic, what i build pretty much same as the guide.

<img width="542" height="377" alt="Schematic" src="https://github.com/user-attachments/assets/dd9647d4-ea28-4d95-b9b0-d0c4c3739fe6" />

First up with the buttons, connect one side with label GPIO2 and power the other side with GND same with the second button:

<img width="60" height="75" alt="Buttons" src="https://github.com/user-attachments/assets/a4dc5b0f-0f8c-4841-8aff-63441078a7f9" />


Then connect the generic 01x04 pin with oled display

then add a mpu6050 module to a 01x08 module pin 

then we'll need DHT11 sensor with a 10k resistor

and then power all of them according to the guide along with xiao esp32 c3

**and then we'll assign the footprints**

<img width="450" height="360" alt="Footprint" src="https://github.com/user-attachments/assets/ff21b432-c17e-4c77-9d72-d4ceeea80cc5" />


this phase was kind of hard and took more than half of my time, so i recommend to watch a quick youtube video about it before actually start building like i did, 


i faced so many issues like the silk symbols were on the soldering plate and then assigning the right 3d models, and wiring the footprints too!! like it was almost so messy, even after using back cu layer with the front too but still try to keep the wiring clean.

but you know what was cooler, MY STICKERS!! nailed em perfectly
those svg files wasn't hard to make and it was so efficient to just import with file -> Import -> graphics and then just select your file and then decide the size and properties and also the layer, which i recommend should be the front silkscreen for the front and back silkscreen for the back

here are my cool stickers:

<img width="378" height="340" alt="AB" src="https://github.com/user-attachments/assets/3f210716-b84c-4537-afd7-e317bf155596" />


<img width="454" height="454" alt="CoolStar" src="https://github.com/user-attachments/assets/3275a43f-bc8a-412b-90e5-e7218bac45fa" />

<img width="756" height="862" alt="starboy" src="https://github.com/user-attachments/assets/f83c9cd6-95b1-4337-8596-e04e6a7b8db7" />


<img width="454" height="378" alt="Saturn" src="https://github.com/user-attachments/assets/61d9b0bc-6a56-48f5-bc50-4ab20be06794" />


<img width="529" height="151" alt="Heart" src="https://github.com/user-attachments/assets/7a11355d-eadb-47e4-9ca5-b4230f83bd37" />

and then after tweaking around a bit, and running my DRC check with no serious errors, i finally made it to my favourite part, 3d render image!!


<img width="694" height="409" alt="Render Front" src="https://github.com/user-attachments/assets/27167ad0-e6b3-42fb-a256-7f65d45382cf" />


this is the front, you can see the esp 32, the display two buttons a mpu 6050 and 4 pins connecting to the display too, assigning them to the right place was one hell of a work!! but the star shape came out so cute!!


<img width="635" height="395" alt="Render Back" src="https://github.com/user-attachments/assets/0c498ac7-6742-4e5d-b333-fc85d0b8336d" />

this is the back of the render, you can see all the wiring and my stickers along with some pins which would be soldered.

now comes the coding part i've added some cool expressions in it along with some animations, like when the temperature drops to a certain level the starboy will 'shiver' and when the temperature rises, he starts to sweat!!
and he'll blink every few seconds too, he's got a happy face, a munching face, a dizzy face when you shake it too much, sleepy eyes in nap reaction, sad when joy score drops from a certain number, surprised eyes when you'll tilt it!! waddle walk, snack time, tippy-tap happy dance, tilt sliding, spontaneous idle quirks, and also some cute particles too, like hearts when he is loved, Zzz when he is sleepy, floaty musical notes, dizzy stars when he gets dizzy, a question mark bubble when he's curious, checkout the starboy.ino for the code
