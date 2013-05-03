import nanotec.*;

class NanoJMotorControl {
	// for 35:1: 2, for 16:1 with old encoder settings: 0, for 16:1 with correct values: 1
	final static int ENCODER_SHIFT = 2;
	final static int POSITION_BIAS = 16384; // has to match in µC code
	
	// function to initialize the controller
	static void initializeController() {
		
		// pause register is used to communicate a state with the PC
		// 0 controller just started
		// 1 controller searching for middle position
		// 2 normal mode
		// 3 compliance mode
		// other, halt the motor
		drive.SetPause( 0 );

		// this sets the encoder resolution in relation to the motor resolution
		// the controller is adjusted to a value of 662 for tilting and 619 for rotating
		// IMPORTANT this has to be adjusted before programming!
		config.SetRotencInc( 133 ); // 4640 / 35
		
		// set the encoder direction
		config.SetEncoderDirection(0);
		
		// try and accelerate/decelerate smoothly
		drive.SetMinSpeed( 1 );
		
		// set acceleration ramp, 
		// parameter = ( 3000 / [Hz/ms] )^2
		drive.SetAcceleration( 300 );
		
		// set deceleration ramp, 0 means use the same as the acceleration ramp
		drive.SetDeceleration( 0 );
		
		// set acceleration/deceleration ramptype
		// 1 = sinus
		drive.SetRampType( 1 );
		
		// set the used current while driving and holding
		drive.SetCurrent( 50 );
		drive.SetCurrentReduction( 20 );

		// set to absolute-position mode, the controller will correct any tension in the cable
		drive.SetMode( 2 );

		// don't do anything after initializing
		drive.StopDrive( 1 );
		
		io.SetOutput1Selection(0);
		io.SetOutput2Selection(0);
		io.SetOutput3Selection(0);
	}
	
	
	// function to turn increasingly left/right to find the right and left encoder positions of the Hall Sensor. 
	// returns the microstep offset (0 or 1)
	static int findCenter() {

		drive.SetMode( 5 );

		// reduce the current so the motor doesn't power through a hardware limit
		drive.SetCurrent( 20 );

		int searchSpread = 200;
	
		drive.SetMaxSpeed( 500 );

		int startingTicks = drive.GetDemandPosition();
		
		// if the joint is already on the Hall-Sensor drive to the right side 
		if ( io.GetAnalogInput( 1 ) < 580 ) {
			drive.SetDirection( 1 );
			drive.StartDrive();
			while ( io.GetAnalogInput( 1 ) < 580 ) {
			}
			util.Sleep( 100 );
			drive.StopDrive( 1 );
		}
		
		drive.SetDirection( 0 );
		drive.StartDrive();
		// get first side of Hall-Sensor
		int analogIn = io.GetAnalogInput( 1 );
		while ( analogIn > 580 ) {
			if ( drive.GetDirection() == 0 ) {
				if ( startingTicks - drive.GetDemandPosition() > searchSpread ) {
					drive.SetDirection( 1 );
					searchSpread += 200;
				}
			} else {
				if ( drive.GetDemandPosition() - startingTicks > searchSpread ) {
					drive.SetDirection( 0 );
					searchSpread += 200;
				}
			}
			analogIn = io.GetAnalogInput( 1 );
		}
		
		// drive a bit further to be outside again
		util.Sleep( 250 );
		drive.SetDirection( ( drive.GetDirection() + 1 ) % 2 );
		drive.SetMaxSpeed( 100 );

		while ( io.GetAnalogInput( 1 ) > 580 ) {
		}
		int middlePositionMotor1 = drive.GetEncoderPosition();
		while ( io.GetAnalogInput( 1 ) < 580 ) {
		}
		middlePositionMotor1 += drive.GetEncoderPosition();
		
		// drive a bit further to be outside again
		util.Sleep( 250 );
		drive.SetDirection( ( drive.GetDirection() + 1 ) % 2 );
		
		// then find the other side of the Hall-Sensor
		while ( io.GetAnalogInput( 1 ) > 580 ) {
		}
		int middlePositionMotor2 = drive.GetEncoderPosition();
		while ( io.GetAnalogInput( 1 ) < 580 ) {
		}
		middlePositionMotor2 += drive.GetEncoderPosition();

		drive.StopDrive( 1 );
		int middlemiddlePosition = ( ( middlePositionMotor1 + middlePositionMotor2 ) >> 2 );

		// restore the current
		drive.SetCurrent( 50 );

		// drive to the calculated zero position
		drive.SetMode( 2 );
		
		int motorActualPosition = drive.GetDemandPosition();
		int encoderActualPosition = drive.GetEncoderPosition();
		
		drive.SetTargetPos( motorActualPosition - ( ( encoderActualPosition - middlemiddlePosition ) << (1-ENCODER_SHIFT) ) );
		drive.StartDrive();

		while ( (( encoderActualPosition - middlemiddlePosition ) >> ENCODER_SHIFT) != 0 ) {
			motorActualPosition = drive.GetDemandPosition();
			encoderActualPosition = drive.GetEncoderPosition();
			drive.SetTargetPos( motorActualPosition - ( ( encoderActualPosition - middlemiddlePosition ) << 1 ) );
			drive.StartDrive();
		}
		
		// reset the encoder and motor position to this position
		drive.SetPosition( 0 );
		drive.SetTargetPos( 0 );
		
		// set drive target for following position control mode
		drive.SetMaxSpeed2( POSITION_BIAS );
		
		// set to a moderate speed
		drive.SetMaxSpeed( 500 );
		
		return motorActualPosition & 3;
	}

	public static void main() 
	{
	
		initializeController();
		
		// initialize used variables
		int state = 0;
		int motorActualPosition = 0;
		int encoderActualPosition = 0;
		int encoderTargetPosition = 0;
		int targetSpeed = 0;
		int driveTarget = 0;
		//int cableTension = 0;
		int motorOldPosition = 0;
		int encoderOldPosition = 0;
		int microstepOffset = 0;
		int holdingPosition = 0;
		int output = 0;

		// main control loop
		while ( true ) 
		{
			if(output == 0)
				output = 1;
			else
				output = 0;

			io.SetDigitalOutput(output);

			// The pause buffer is used to transfer a state command from the PC.
			state = drive.GetPause();
			
			if ( state == 0 ) 
			{
				// wait for PC to request initialise
			}
			
			else if ( state == 1 ) 
			{
				// initialise the link
				microstepOffset = findCenter();
				
				// Switch to state 2.
				drive.SetPause( 2 );
			}
			
			else if ( state == 2 ) 
			{
				// position control mode
				
				// decode target position, use MaxSpeed2 to communicate with the PC as it is not used in absolute position mode
				encoderTargetPosition = drive.GetMaxSpeed2()-POSITION_BIAS;
				targetSpeed = drive.GetMaxSpeed();

				// read the actual encoder position
				encoderActualPosition = drive.GetEncoderPosition();
				
				// read the actual motor position (GetDemandPosition is misleading)
				motorActualPosition = drive.GetDemandPosition();
				
				// calculate the next target to send
				int delta = (encoderTargetPosition - encoderActualPosition) >> ENCODER_SHIFT;
				int deltaAbs = (delta > 0) ? delta : (-delta);  //( delta ^ ( delta >> 31 ) ) + ( ( delta >> 31 ) & 1 );
				int farShift = ( ( ( targetSpeed >> 5 ) - deltaAbs ) >> 31 ) & 1;  // 1 if (targetSpeed >> 5) < deltaAbs
				int closeShift = ( ( deltaAbs - 4 ) >> 31 ) & 1;                   // 1 if deltaAbs < 4
				
				if(deltaAbs < 3)
				{
					if(holdingPosition == 0 && deltaAbs < 2)
					{
						holdingPosition = 1;
						driveTarget = motorActualPosition;
						
						if(delta < 0)
							driveTarget -= 4;
					}
				}
				else
				{
					holdingPosition = 0;
				}
				
				if(holdingPosition == 0)
				{
					driveTarget = delta << farShift; //( delta << farShift ) >> closeShift;
					
					if(driveTarget > 0 && driveTarget < 5)
						driveTarget = 5;
					else if(driveTarget < 0 && driveTarget > -5)
						driveTarget = -5;

					driveTarget += motorActualPosition;
				}

				// set target and start the motor
				drive.SetTargetPos( driveTarget );
				drive.StartDrive();
				encoderOldPosition = encoderActualPosition;
				motorOldPosition = motorActualPosition;
			} 

			/*else if ( state == 3 ) 
			{
				// compliance mode
				
				// read the actual encoder position
				encoderActualPosition = drive.GetEncoderPosition();
				
				// read the actual motor position
				motorActualPosition = drive.GetDemandPosition();
								
				// calculate the tension in the cable
				cableTension += ( ( encoderActualPosition  - encoderOldPosition ) << 1 ) - ( motorActualPosition - motorOldPosition );
				
				// set target and start the motor
				drive.SetTargetPos(motorActualPosition + cableTension);
				drive.StartDrive();
				encoderOldPosition = encoderActualPosition;
				motorOldPosition = motorActualPosition;
			} */
			
			else if ( state == 4 ) 
			{
				// Passive mode.
				// In this state the PC has full control of the motors. Only the motor start command is set.
				drive.StartDrive();
			}
			
			else 
			{
				// unknown state, stop the motor
				drive.StopDrive( 1 );
			}
		}
	}
}
