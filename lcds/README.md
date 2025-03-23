## Compatible LCDs

<table>
<tr>
  <th>Size</th>
  <th>Resolution</th>
  <th>Model</th>
  <th>Description</th>
  <th>Touch Driver</th>
  <th>Purchase Link</th>
  <th>Datasheets</th>
</tr>
<tr>
  <td title="Size">4.3"</td>
  <td title="Resolution">800×480</td>
  <td title="Model">ER-TFT043A1-7</td>
  <td title="Description">Good quality IPS display with optional capacitive touch panel. Used in the PSPi 6 project</td>
  <th title="Touch Driver">✅</th>
  <td title="Purchase Link"><a href="https://www.buydisplay.com/4-3-800x480-ips-tft-lcd-module-all-viewing-optl-touchscreen-display">Buy</a></td>
  <td title="Datasheet"><a href="/ER-TFT043A1-7">Datasheet</a></td>
</tr>
<!-- Template row for adding new entries:
<tr>
  <td title="Size">[SIZE]</td>
  <td title="Resolution">[RESOLUTION]</td>
  <td title="Model">[MODEL]</td>
  <td title="Description">[DESCRIPTION]</td>
  <th title="Touch Driver">[✅ or -]</th>
  <td title="Purchase Link"><a href="[PURCHASE URL]">Buy</a></td>
  <td title="Datasheet"><a href="[DATASHEET PATH]">Datasheet</a></td>
</tr>
-->
</table>

*Note: This table will be updated as more compatible displays are tested and confirmed. If you've successfully used other displays with this project, please submit a pull request with the relevant information.*

## Compatible Touch Panels

<table>
<tr>
  <td align="center"><h4>✅ CORRECT TOUCH PANEL</h4></td>
  <td align="center"><h4>❌ INCORRECT TOUCH PANEL</h4></td>
  <td align="center"><h4>❌ INCORRECT TOUCH PANEL</h4></td>
</tr>
  <tr>
    <td><img src="lcd with touch.jpg" alt="Correct Touch Panel"/></td>
    <td><img src="lcd with touch incorrect1.jpg" alt="Incorrect Touch Panel"/></td>
    <td><img src="lcd with touch incorrect2.jpg" alt="Incorrect Touch Panel"/></td>
  </tr>
</table>

*Note: The Topper is only compatible with I2C capacitive touch panels that are routed to the 40-pin FPC cable. It is not compatible with resistive touch panels and is not compatible with panels that use a separate smaller FPC cable.*

## LCD 40-Pin FPC Connector Pinout

The following table details the pinout of the 40-pin FPC connector used by compatible displays:

<table>
<tr>
  <th>Pin No</th>
  <th>Pin Name</th>
  <th>Description</th>
</tr>
<tr>
  <td title="Pin No">1</td>
  <td title="Pin Name">LEDK</td>
  <td title="Description">LED Cathode</td>
</tr>
<tr>
  <td title="Pin No">2</td>
  <td title="Pin Name">LEDA</td>
  <td title="Description">LED Anode</td>
</tr>
<tr>
  <td title="Pin No">3</td>
  <td title="Pin Name">GND</td>
  <td title="Description">Ground</td>
</tr>
<tr>
  <td title="Pin No">4</td>
  <td title="Pin Name">VDD</td>
  <td title="Description">Power Supply</td>
</tr>
<tr>
  <td title="Pin No">5~12</td>
  <td title="Pin Name">R0~R7</td>
  <td title="Description">Red Data</td>
</tr>
<tr>
  <td title="Pin No">13~20</td>
  <td title="Pin Name">G0~G7</td>
  <td title="Description">Green Data</td>
</tr>
<tr>
  <td title="Pin No">21~28</td>
  <td title="Pin Name">B0~B7</td>
  <td title="Description">Blue Data</td>
</tr>
<tr>
  <td title="Pin No">29</td>
  <td title="Pin Name">GND</td>
  <td title="Description">Ground</td>
</tr>
<tr>
  <td title="Pin No">30</td>
  <td title="Pin Name">CLK</td>
  <td title="Description">Clock</td>
</tr>
<tr>
  <td title="Pin No">31</td>
  <td title="Pin Name">DISP</td>
  <td title="Description">Display on/off</td>
</tr>
<tr>
  <td title="Pin No">32</td>
  <td title="Pin Name">HSYNC</td>
  <td title="Description">Horizontal sync input in RGB mode.</td>
</tr>
<tr>
  <td title="Pin No">33</td>
  <td title="Pin Name">VSYNC</td>
  <td title="Description">Vertical sync input in RGB mode.</td>
</tr>
<tr>
  <td title="Pin No">34</td>
  <td title="Pin Name">DEN</td>
  <td title="Description">Data Enable</td>
</tr>
<tr>
  <td title="Pin No">35</td>
  <td title="Pin Name">NC</td>
  <td title="Description">No Connection</td>
</tr>
<tr>
  <td title="Pin No">36</td>
  <td title="Pin Name">GND</td>
  <td title="Description">Ground</td>
</tr>
<tr>
  <td title="Pin No">37</td>
  <td title="Pin Name">XR</td>
  <td title="Description">Unused on this PCB</td>
</tr>
<tr>
  <td title="Pin No">38</td>
  <td title="Pin Name">YD</td>
  <td title="Description">Unused on this PCB</td>
</tr>
<tr>
  <td title="Pin No">39</td>
  <td title="Pin Name">XL</td>
  <td title="Description">This pin is used for capacitive touch communication</td>
</tr>
<tr>
  <td title="Pin No">40</td>
  <td title="Pin Name">YU</td>
  <td title="Description">This pin is used for capacitive touch communication</td>
</tr>
</table>

*Note: Pins 37-40 are used for only capacitive I2C touch panels, not resistive ones. Resistive panels will interfere with the I2C connection.*
