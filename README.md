# Introduction
This is *work-in-progress* project to build orienteering clock, currently in prototyping phase. I will put more information here when it will be completed.

# Contents
 * `src/` - Arduino prototype source for driving multiplexed LED display and time counting. It's best to link this folder to your Arduino projects folder:
  
  ```
    in Windows:
    > mklink /D c:\Users\$USER\Documents\Arduino\orienteering_clock c:\proj\orienteering_clock\src

    in Linux:
    $ ln -s src /home/$USER/sketchbook/orienteering_clock
  ```
 
 * `schematics/` - EAGLE and PDF schematics of prototype parts. It's best to link this folder to your Eagle library:

  ```
    in Windows:
    > mklink /D c:\Users\$USER\Documents\eagle\orienteering-clock C:\proj\orienteering_clock\schematics

    in Linux:
    $ ln -s schematics /home/$USER/eagle/orienteering-clock

  ```

 * `doc/` - documentation of parts, etc.

