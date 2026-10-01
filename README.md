<table>
  <tr>
    <td><img src="images/hacktheplanet.gif" alt="Hack The Planet"></td>
    <td>
    ###################################### <br>
    ESPager                                <br>
    A GSC and POCSAG Encoder for ESP32 C3  <br>
    ###################################### <br>
    Written by Hardcore Corey Harding      <br>
    ###################################### <br>
    Licensed: GPL v3.0                     <br>
    ###################################### <br>
    </td>
  </tr>
</table>

![alt](images/ESPager_demo.gif)

* Note: The pager related instructions are specifically tailored to the 1st Generation Motorola Advisor but ESPager should be able to send pages to similar pagers using the same methods as described in the readme.  

--------------------
# What is it?  
--------------------
* ESPager is capable of sending either GSC or POCSAG encoded pages to a pager through ESPager's retro inspired web interface.  
* The radio module is removed from the pager and replaced with an ESP32 C3.  
* The ESP32 has a GPIO pin connected directly to the pager's data in pin that was formerly occupied by the pager's radio module.  
* ESPager effectively bit bangs the encoded message directly into the pager's data in pin, thus signalling a page on the pager.  
* You can send a page either through ESPager's web interface or by using URL GET Requests with the proper parameters.  Using the GET Request API you can now more easily integrate this project into an Asterisk PBX to send a message to your pager.  

<a href="images/web_root.png">
  <img src="images/web_root.png" width="100" alt="Full-size image">
</a>

<a href="images/web_pocsag_encoder.png">
  <img src="images/web_pocsag_encoder.png" width="100" alt="Full-size image">
</a>

<a href="images/web_pocsag_encoded.png">
  <img src="images/web_pocsag_encoded.png" width="100" alt="Full-size image">
</a>

<a href="images/web_gsc_encoder.png">
  <img src="images/web_gsc_encoder.png" width="100" alt="Full-size image">
</a>

<a href="images/web_gsc_encoded.png">
  <img src="images/web_gsc_encoded.png" width="100" alt="Full-size image">
</a>

--------------------
# BOM  
--------------------
** Note: Most of these items can only be found on AliExpress or through similar vendors  
* Pager that supports POCSAG or GSC (1st Generation Motorola Advisor)  
* ESP32 C3 (Generic ESP32-C3 SuperMini)  
* 3.7V 10440 Battery (Button-Top 3.7v Vapcell F4 450mAh or Generic USB C Rechargeable 3.7v 10440 750mWh 200mAh, maybe a 601230?)  
* x1 mini DC-DC Buck Converter (1.5v output voltage with 2.5v-6.0v input voltage)  
* x1 3.7v 1S 2.5A li-ion BMS Protection Module with Overdischarge protection (Round type that goes on end of 18650 is smallest I found)  
* Assorted Color Wire (I used solid core 22awg)  
* Wiring harness tape(Tesa cloth fabric tape)  

Optional:  
* Serial UART adapter for programming pager (Worked best in DOSBOX: RadioMaster ExpressLRS USB UART Flasher V2)  
* 3d printed TPU programming POGO adapter for Motorola Advisor (Included in 3d_prints folder)  
* Male to Female jumper wires for 3d prints  

--------------------
# Instructions  
--------------------

An ESP32 C3 running the code contained within this repo is installed inside a Motorola Advisor pager with slight hardware modifications.  

--------------------
### Programming the Motorola Advisor  
--------------------
* 3d print (TPU) and assemble the POGO adapter for the Motorola Advisor
* Remember to connect UARTs TX to Pagers RX and UARTs RX to Pagers TX
* Programming software is available at this repo (https://github.com/goshante/motorola-advisor-linguist) and various locations online.  
* For ESPager's default settings to work out of the box be sure to set all pager settings to exactly mirror the images below.  
<a href="images/motorola_advisor_programming_pins.jpg">
  <img src="images/motorola_advisor_programming_pins.jpg" width="100" alt="Full-size image">
</a>

<a href="images/advisor_prog_jig1.jpg">
  <img src="images/advisor_prog_jig1.jpg" width="100" alt="Full-size image">
</a>

<a href="images/advisor_prog_jig2.jpg">
  <img src="images/advisor_prog_jig2.jpg" width="100" alt="Full-size image">
</a>

<br>

<a href="images/pocsag1.png">
  <img src="images/pocsag1.png" width="100" alt="Full-size image">
</a>

<a href="images/pocsag2.png">
  <img src="images/pocsag2.png" width="100" alt="Full-size image">
</a>

<a href="images/pocsag3.png">
  <img src="images/pocsag3.png" width="100" alt="Full-size image">
</a>

<br>

<a href="images/gsc1.png">
  <img src="images/gsc1.png" width="100" alt="Full-size image">
</a>

<a href="images/gsc2.png">
  <img src="images/gsc2.png" width="100" alt="Full-size image">
</a>

<a href="images/gsc3.png">
  <img src="images/gsc3.png" width="100" alt="Full-size image">
</a>

--------------------
### Opening the Motorola Advisor  
--------------------
* Refer to photos below instructions as you follow along.
* Slide battery tray lock away from the compartment to unlock it.  Slide battery tray cover away from the pager to open it. Remove the battery if present.
* Locate the lanyard loop pin on bottom side of the pager.  Very lightly lift the locking tab closest to the lanyard loop pin.  Slide the locking tab from the bottom and up towards the lanyard loop and it should pop out.  
* Gently pry the bottom corner near the positive battery terminal using the battery tray as leverage and the body should pop open on that side.  Gently roll the body towards the still locked side to open it.  
* Locate the radio board and gently wiggle and lift up and away from the main pcb to free it from the header pins.  
<a href="images/motorola_advisor_open_1.jpg">
  <img src="images/motorola_advisor_open_1.jpg" width="100" alt="Full-size image">
</a>

<a href="images/motorola_advisor_open_2.jpg">
  <img src="images/motorola_advisor_open_2.jpg" width="100" alt="Full-size image">
</a>

<br>

<a href="images/motorola_advisor_open_3.jpg">
  <img src="images/motorola_advisor_open_3.jpg" width="100" alt="Full-size image">
</a>

<a href="images/motorola_advisor_open_4.jpg">
  <img src="images/motorola_advisor_open_4.jpg" width="100" alt="Full-size image">
</a>

<a href="images/motorola_advisor_open_5.jpg">
  <img src="images/motorola_advisor_open_5.jpg" width="100" alt="Full-size image">
</a>

<br>

<a href="images/motorola_advisor_open_6.jpg">
  <img src="images/motorola_advisor_open_6.jpg" width="100" alt="Full-size image">
</a>

<a href="images/motorola_advisor_open_7.jpg">
  <img src="images/motorola_advisor_open_7.jpg" width="100" alt="Full-size image">
</a>

<a href="images/motorola_advisor_open_8.jpg">
  <img src="images/motorola_advisor_open_8.jpg" width="100" alt="Full-size image">
</a>

--------------------
### Modifying the Motorola Advisor and Installing ESP32 C3  
--------------------

* Desolder the battery holder.  On the positive side clip the legs off the bottom that protrude through the pcb.  On the negative side, bend the leg towards the inside of the pager.  Solder you main battery wires to the battery holder.  Clip the corner of the pager's rear cover as pictured to make room for the battery wire to pass through.  Clip the nub off the bottom of the battery holder under the positive side to make room for the wiring underneat it.  
* Solder the 3.7v wires from the battery holder to the BMS input as seen in wiring diagram.  
* Solder the 3.7v wires from the BMS output directly to the ESP32-C3's 5v input and GND and to the 1.5v buck converters input as seen in wiring diagram.(ESP32-C3 5v input can be powered by 3.5v-6v and a fully charged 10440 is 4.2v)  
* Solder the 1.5v wires onto the pager's main pcb as pictured and solder those wires to the output of the buck converter as seen in wiring diagram.  Cover the wires on the pager's main pcb to prevent feeding the pager 3.7v if the battery trays positive somehow made contact.  
* Solder GPIO 3(Sleep) on the ESP32 C3 to header pin 7(pulse when pager is on) on the pager, solder GPIO 7(Data out) on ESP32-C3 to header pin 4(Data In) of pager.  
* Verify that the ESP32 and pager share a common ground plane by checking continuity between both grounds.  If not connect the ground from ESP32 to a ground on the pager, but I have a common ground plane through my power supply configuration.  
* Tape off the inside of the pager to prevent shorts, tape of the bms, buck converter, and ESP32-C3.  
* Assembly is reverse of disassembly.  

<a href="images/ESPager_wiring.png">
  <img src="images/ESPager_wiring.png" width="800" alt="Full-size image">
</a>

<br>

<a href="images/motorola_advisor_radio_header.jpg">
  <img src="images/motorola_advisor_radio_header.jpg" width="400" alt="Full-size image">
</a>

<br>

<a href="images/motorola_advisor_battery_tray_negative.jpg">
  <img src="images/motorola_advisor_battery_tray_negative.jpg" width="100" alt="Full-size image">
</a>

<a href="images/motorola_advisor_battery_tray_positive.jpg">
  <img src="images/motorola_advisor_battery_tray_positive.jpg" width="100" alt="Full-size image">
</a>

<a href="images/motorola_advisor_battery_tray_negative_mod.jpg">
  <img src="images/motorola_advisor_battery_tray_negative_mod.jpg" width="100" alt="Full-size image">
</a>

<a href="images/motorola_advisor_battery_tray_positive_mod.jpg">
  <img src="images/motorola_advisor_battery_tray_positive_mod.jpg" width="100" alt="Full-size image">
</a>

<br>

<a href="images/motorola_advisor_battery_tray_removed.jpg">
  <img src="images/motorola_advisor_battery_tray_removed.jpg" width="100" alt="Full-size image">
</a>

<a href="images/motorola_advisor_battery_tray_trim.jpg">
  <img src="images/motorola_advisor_battery_tray_trim.jpg" width="100" alt="Full-size image">
</a>

<a href="images/motorola_advisor_battery_tray_closeup.jpg">
  <img src="images/motorola_advisor_battery_tray_closeup.jpg" width="100" alt="Full-size image">
</a>

<a href="images/motorola_advisor_battery_tray_complete.jpg">
  <img src="images/motorola_advisor_battery_tray_complete.jpg" width="100" alt="Full-size image">
</a>

--------------------
### Accessing ESPocsag 
--------------------

* Via WiFi  
** STA MODE: http://ESPager  
** AP MODE: http://192.168.4.1  

* POCSAG API EXAMPLE  
** STA MODE: http://ESPager/pocsag?msg=TEST+MESSAGE&cap=1337331&baud=1200&function_code=3&data_type=0&repeat=1  
** AP MODE:  http://192.168.4.1/pocsag?msg=TEST+MESSAGE&cap=1337331&baud=1200&function_code=3&data_type=0&repeat=1  

* GSC API EXAMPLE  
** STA MODE: http://ESPager/gsc?msg=TEST+MESSAGE&cap=313371&function_code=3&data_type=1  
** AP MODE:  http://192.168.4.1/gsc?msg=TEST+MESSAGE&cap=313371&function_code=3&data_type=1  

--------------------
##### Bonus: Motorola Advisor Test Mode for Radio (Unrelated to this repo)  
--------------------
* Press power button
* Press red button to confirm power down
* Press green button
* Press up arrow
* Press left arrow
* Press red button
* Observe the red light on the radio is lit up to confirm radio is in test mode

--------------------
##### Bonus: Motorola Advisor CMOS Chip (Unrelated to this repo)  
--------------------
* Pinout for CMOS Chip U2 (https://github.com/goshante/motorola-advisor-linguist/blob/master/schematics/main/chematics.png)
* CMOS memory address 0x25
* Set bit 0 to 1 to enable a disabled pager
* Set bit 5 to 0 to disable the programming password requirement
