
[![linkedin](https://img.shields.io/badge/linkedin-0A66C2?style=for-the-badge&logo=linkedin&logoColor=white)](https://www.linkedin.com/in/matheus-perches/)

#### [Return to my Portfolio](https://github.com/matheusperches/matheusperches.github.io) 

## [CryEngine Flight Controller study ]([https://github.com/matheusperches/6DoF-TimeAttack](https://github.com/matheusperches/cryprototype))

#### A small CryEngine project with the goal to get experience using its tools and APIs.

## Build instructions
Make sure you have Visual Studio 2022 installed with the C++ game development add ons. 

1. Click on Code -> Download ZIP.
2. Extract the file in a folder.
3. Click on game.cryproject 
4. Choose "build solution"
5. Navigate to solutions -> Win64 -> Game.sln and open it in VS2022. 
6. Build the project with CTRL + Shift + B.
7. Inside Visual Studio on the side bar "Solution Explorer", right click "Game" and select "Set as startup project"
8. Press F5 to launch the project
9. Play.
10. To launch the editor and customize parameters -> Switch startup project to "Editor" -> F5 -> File - Open - Level -"Example"

## Controls 
WASD - Linear movement 
Q/E - Roll
Mouse X/Y - Pitch & Yaw

V - Netwonian toggle 
G - Gravity Assist 
Shift - Boost

## Features
 - First person character with a raycast system for entering the vehicle
 - A fully customizable flyable entity with six degrees of freedom control
 - Fully physically simulated, simplified by applying all math onto a symmetric cube. Can be hidden and replaced with a ship mesh. 
 - Tunable accelerations profiles for linear and angular motion, including jerk profiles
 - Acceleration control mode (Netwonian): The player's inputs are interpreted as a directional acceleration request.
 - Velocity control mode (Coupled): The player's inputs are interpreted as a directional velocity request.
 - Gravity assist: Can be toggled ON or OFF, and it is enforced in Coupled mode. Compensates for gravity by generating a proprotional counter force against gravity pull. 
 - Boost: a simple acceleration multiplier, applied at the generator of impulse (last stage of calculations)

## Code 

- To be added - 
