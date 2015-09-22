/* Data structure for storing FlightGear property tree variables */
typedef struct ap_data
{
  // Flight Controls
  float aileron; // /controls/flight/aileron
  float aileron_trim; // /controls/flight/aileron-trim
  float elevator; // /controls/flight/elevator
  float elevator_trim; // /controls/flight/elevator
  float rudder; // /controls/flight/rudder
  float rudder_trim; // /controls/flight/rudder-trim
  float flaps; // /controls/flight/flaps
  float slats; // /controls/flight/slats
  float speedbrake; // /controls/flight/speedbrake

  // Engines
  float throttle0; // /controls/engines/engine[0]/throttle 
  float throttle1; // /controls/engines/engine[1]/throttle
  float starter0; // /controls/engines/engine[0]/starter
  float starter1; // /controls/engines/engine[1]/starter
  float fuel_pump0; // /controls/engines/engine[0]/fuel-pump
  float fuel_pump1; // /controls/engines/engine[1]/fuel-pump
  float cutoff0; // /controls/engines/engine[0]/cutoff
  float cutoff1; // /controls/engines/engine[1]/cutoff
  float mixture0; // /controls/engines/engine[0]/mixture
  float mixture1; // /controls/engines/engine[1]/mixture
  float propeller_pitch0; // /controls/engines/engine[0]/propeller-pitch
  float propeller_pitch1; // /controls/engines/engine[1]/propeller-pitch
  float magnetos0; // /controls/engines/engine[0]/magnetos
  float magnetos1; // /controls/engines/engine[1]/magnetos
  float ignition0; // /controls/engines/engine[0]/ignition
  float ignition1; // /controls/engines/engine[1]/ignition

  // Gear
  float brake_left; // /controls/gear/brake-left
  float brake_right; // /controls/gear/brake-right
  float brake_parking; // /controls/gear/brake-parking
  float steering; // /controls/gear/steering
  float gear_down; // /controls/gear/gear_down
  float gear_position0; // /controls/gear/position-norm
  float gear_position1; // /controls/gear[1]/position-norm
  float gear_position2; // /controls/gear[2]/position-norm
  float gear_position3; // /controls/gear[3]/position-norm
  float gear_position4; // /controls/gear[4]/position-norm

  // Hydraulics
  float engine_pump0; // /controls/hydraulic/system[0]/engine-pump
  float engine_pump1; // /controls/hydraulic/system[1]/engine-pump
  float electric_pump0; // /controls/hydraulic/system[0]/electric-pump
  float electric_pump1; // /controls/hydraulic/system[1]/engine-pump

  // Electric
  float battery_switch; // /controls/electric/battery-switch
  float external_power; // /controls/electric/external-power
  float APU_generator; // /controls/electric/APU-generator

  // Autoflight
  float engage; // /controls/autoflight/autopilot[0]/engage
  float heading_select; // /controls/autoflight/heading-select
  float altitude_select; // /controls/autoflight/altitude-select
  float bank_angle_select; // /controls/autoflight/bank-angle-select
  float vertical_speed_select; // /controls/autoflight/vertical-speed-select
  float speed_select; // /controls/autoflight/speed_select

  // Position
  double latitude_deg; // /position/latitude-deg
  double longitude_deg; // /position/longitude-deg
  double altitude_ft; // /position/altitude-ft

  // Orientation
  float roll_deg; // /orientation/roll-deg
  float pitch_deg; // /orientation/pitch-deg
  float heading_deg; // /orientation/heading-deg
  float side_slip_deg; // /orientation/side-slip-deg

  // Velocities
  float airspeed_kt; // /velocities/airspeed-kt
  float glideslope; // /velocities/glideslope
  float mach; // /velocities/mach
  float speed_down_fps; // /velocities/speed-down-fps
  float speed_east_fps; // /velocities/speed-east-fps
  float speed_north_fps; // /velocities/speed-north-fps
  float uBody_fps; // /velocities/uBody-fps
  float vBody_fps; // /velocities/vBody-fps
  float wBody_fps; // /velocities/wBody-fps
  float vertical_speed_fps; // /velocities/vertical-speed-fps

  // Accelerations
  float nlf; // /accelerations/nlf
  float ned_down_accel_fps_sec; // /accelerations/ned/down-accel-fps_sec
  float ned_east_accel_fps_sec; // /accelerations/ned/east-accel-fps_sec
  float ned_north_accel_fps_sec; // /accelerations/ned/north-accel-fps_sec
  float pilot_x_accel_fps_sec; // /accelerations/pilot/x-accel-fps_sec
  float pilot_y_accel_fps_sec; // /accelerations/pilot/y-accel-fps_sec
  float pilot_z_accel_fps_sec; // /accelerations/pilot/z-accel-fps_sec

  // Surface Positions
  float elevator_pos_norm; // /surface-positions/elevator-pos-norm[0]
  float flap_pos_norm; // /surface-positions/flap-pos-norm[0]
  float left_aileron_pos_norm; // /surface-positions/left-aileron-pos-norm[0]
  float right_aileron_pos_norm; // /surface-positions/right-aileron-pos-norm[0]
  float rudder_pos_norm; // /surface-positions/rudder-pos-norm[0]
} ap_data;

