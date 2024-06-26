# Smart Shogi Board Project (2022)

## Introduction
Welcome to the Smart Shogi Board Project! This innovative project combines the traditional Japanese game of Shogi with advanced technology. By using an ESP32 as the main board, it allows players to engage in physical Shogi games against a computer-controlled AI. The AI communication is facilitated through a sample AI server utilizing Gikou, a sophisticated Shogi AI.


## Main Features
- **ESP32 Main Board:** Acts as the central unit of the smart Shogi board, managing game logic and communication.
- **Web Client:** A user-friendly interface provided by a React-based web client, communicating with the ESP32 via Bluetooth.
- **AI Server Integration:** Use Gikou for AI-based gameplay, offering a challenging Shogi opponent. But the app.py doesn't work well (super weak)
- **Physical Gameplay Experience:** Play against AI on an actual board, bringing the authentic feel of Shogi to life.

  ## Images
<!-- Container with horizontal layout -->
<div style="display: flex; justify-content: space-between;">

  <!-- First Image and Title -->
  <div style="flex: 1; padding: 5px; text-align: center;">
    <p>・kicad Main Board preview</p>
    <img src="https://github.com/miyashita-code/smart_shogi_board_esp/assets/116678712/6c27e11f-9f49-4cfd-99f6-25afe34ea3a7" style="width: 60%; max-width: 300px;" alt="kicad Main Board preview">
  </div>

  <!-- Second Image and Title -->
  <div style="flex: 1; padding: 5px; text-align: center;">
    <p>・Detailed View of the Real Board</p>
    <img src="https://github.com/miyashita-code/smart_shogi_board_esp/assets/116678712/503ab837-693b-45e0-b4bc-aa81f26e2c8e" style="width: 60%; max-width: 300px;" alt="Detailed View of the Real Board">
  </div>

  <!-- Third Image and Title -->
  <div style="flex: 1; padding: 5px; text-align: center;">
    <p>・new version machine looks</p>
    <img src="https://github.com/miyashita-code/smart_shogi_board_esp/assets/116678712/96bffbb3-9ec0-4035-8ef1-90ca2399b5cb" style="width: 60%; max-width: 300px;" alt="new version machine looks">
  </div>

</div>


## Hardware Components
- **Hall Sensors:** Detect the positions of Shogi pieces on the board.
- **Shift Registers and Multiplexers:** Efficiently manage signal inputs and outputs.
- **ESP32 (Main):** The primary controller for the board's functions.
- **DFPlayMini Audio Module:** Adds auditory elements to the gameplay.
- **Serial LEDs & Kifu Reading:** Visually indicate piece movements and game progress.
- **Load Cells (Sub):** Determine the type of piece being moved.
- **Magnetized Pieces:** Each piece is embedded with a magnet and precisely crafted using a 3D printer, with exact weight specifications.

...

## Development & Collaboration
- **GitHub Integration:** Initially not maintained on GitHub, but essential resources will now be provided. Sorry messey ...
- **Prototype Image Reference:** See Image 1 for the initial prototype design of the board.
- **Demonstration Video:**
(this is first demo version and gikou api server broken, so super weak AI ...)

  https://github.com/miyashita-code/smart_shogi_board_esp/assets/116678712/a6df7edc-be3c-4e71-8968-0c8eb9295571

## Note
This project is continually evolving, and some components may be incomplete or in the development phase. Our goal is to merge traditional Shogi with modern technology to provide a unique and interactive gaming experience. Watch out for more updates and improvements!

