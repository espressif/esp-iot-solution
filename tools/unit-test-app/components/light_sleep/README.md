# light_sleep wake_up test

This test case provides the following tests:

- EXT0 wake up test, we use rtc_io 34 as trigger source. After entering light_sleep mode, set this io low will wake up system.
         
- EXT1 wake up test, we use rtc_io 34, 35, 36 and 39 as trigger source. After entering light_sleep mode, set them all low at the sametime will wake system up.

- Timer wakeup, 50 seconds later , system will waked up by timer after entering light_sleep mode.

- Touch_Pad  wake_up, touch pad_7 will wake up system during light_sleep mode.

- After system waked up, you can run `Deep_sleep get wake_up cause test` to get what waked the system up.    
