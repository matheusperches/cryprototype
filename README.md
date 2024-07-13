
[![linkedin](https://img.shields.io/badge/linkedin-0A66C2?style=for-the-badge&logo=linkedin&logoColor=white)](https://www.linkedin.com/in/matheus-perches/)

#### [Return to my Portfolio](https://github.com/matheusperches/matheusperches.github.io) 

## [CryEngine Flight Controller study ]([https://github.com/matheusperches/6DoF-TimeAttack](https://github.com/matheusperches/cryprototype))

#### A small CryEngine project with the goal to get experience using its tools and APIs.

## Features
 - First person character with a raycast system for entering the vehicle
 - A fully customizable flyable entity with six degrees of freedom control
 - Fully physically simulated, simplified by applying all math onto a symmetric cube. Can be hidden and replaced with a ship mesh. 
 - Tunable accelerations profiles for linear and angular motion, including jerk profiles
 - Acceleration control mode (Netwonian): The player's inputs are interpreted as a directional acceleration request.
 - Velocity control mode (Coupled): The player's inputs are interpreted as a directional velocity request.
 - Gravity assist: Can be toggled ON or OFF, and it is enforced in Coupled mode. Compensates for gravity by generating a proprotional counter force against gravity pull. 
 - Boost: a simple acceleration multiplier, applied at the generator of impulse (last stage of calculations)

## Media

- The ship in action

![Demo gif]()

- Input controller overview

![Flight Controller diagram](https://raw.githubusercontent.com/matheusperches/PlaygroundProj/main/Info/sfcs.jpg)
