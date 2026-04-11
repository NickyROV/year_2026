**Operational process**  
This sequential workflow was designed to meet the 2026 MATE Floats!

**Mission requirements**  
Step 1: Initialization and HandshakeStartup: Both devices initialize ESP-NOW for wireless communication and the float initializes the MS5837 pressure sensor.Status Report: The float sends a "Ready" status message to the control station once the I2C sensor handshake is successful.Prompt: The control station displays STEP 1: Press 'Pre-dive Transmission' (Pin 17) to the user upon receiving the ready status.  

Step 2: Pre-Dive Transmission (Scoring Requirement)Request: The operator presses the button on GPIO 17, sending a predive command to the float.Verification: The float reads the current surface pressure and sends one data packet (ID, Time, Pressure, Depth) back to the station.Display: The control station displays this packet with units (kPa, m) for the judge to verify and confirms it is ready for deployment.  
  
Step 3: Mission Deployment and CalibrationDeployment: The operator presses the button on GPIO 15, sending the deploy command with target depths (2.5m and 0.4m) and hold times (30s).Mechanical Reset: The float resets its buoyancy engine by moving the piston to the forward limit switch.Calibration: The float averages 20 sensor readings to set the surface_pressure_kpa reference before starting the dive.  
    
Step 4: Vertical Profiling (Two Cycles)Data Logging: Once the mission begins, the float automatically logs data every 5 seconds to internal memory to ensure the required 7 packets per hold are captured.Profile 1:Descend: The buoyancy engine adjusts the float's density to reach 2.5 meters.Low Hold: Maintains depth for 30 seconds.Ascend: Moves to 40 cm without breaching the surface.High Hold: Maintains 40 cm for 30 seconds.Profile 2: The float repeats the exact descend, hold, ascend, and hold sequence for the second required profile.  
  
Step 5: Surfacing and Data RecoverySurfacing: The buoyancy engine returns the float to the surface (depth < 0.05m).Recovery: Once retrieved, the operator presses the button on GPIO 16 to send a send_now command.CSV Dump: The float transmits all stored log packets. The control station prints these in a comma-delimited format (company_id, timestamp, depth_m, pressure_kpa) ready for copy-pasting into LibreOffice for graphing.
