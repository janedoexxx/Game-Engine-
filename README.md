## Gamedoe
A pure C game engine built from scratch without external dependencies (except X11 and ALSA for windowing and audio). Designed for performance and simplicity with a custom resource management system, scene management, and basic physics.

### Features
- **Pure C Implementation**: No external libraries except system-level dependencies

- **Custom Resource Management**: Handles sprites, sounds, and other assets

- **Scene System**: Easy management of different game states (menu, game, etc.)

- **Basic Physics**: Vector-based movement and collision detection

- **Audio System**: ALSA-based sound effects with waveform generation

- **Input Handling**: Keyboard input with configurable key bindings

- **Custom Memory Management**: Efficient resource allocation and cleanup

### Requirements
- Arch Linux (tested on Arch)

- X11 development libraries

- ALSA development libraries

- GCC compiler

- Make build system

### Installation
1. Install required dependencies:
```bash
sudo pacman -S libx11 alsa-lib
```

2. Clone the repo
```bash
git clone https://github.com/janedoexxx/Game-Engine-.git
```

3. Build the engine
```bash
make
```

4. Run the demo
```bash
./eng 
```

### Building
The project uses simple Makefile
```bash
make        # Build the engine
make clean  # Clean build artifacts
```

### Usage
- **SPACE**: Jump (in game) or Start game (in menu)

- **Arrow Keys**: Apply forces/movement

- **ESC**: Return to menu (in game) or Quit (in menu)

### Project Structure
```text
eng.h       - Engine header with type definitions and function declarations
eng.c       - Engine implementation with all systems
main.c      - Entry point and main loop
makefile    - Build configuration
```

### Contributing
PRs are accepted! This is a WIP and contributions are welcome:
1. Fork the repo
2. Create a feature branch
3. Make ur changes
4. Submit a pull request

### Current status(dc9bedc)
- [x] Basic window creation and management 

- [x] Graphics context and drawing

- [x] Frame timing and animation

- [x] Input handling 

- [x] Vector math and physics

- [x] Sprite system

- [x] Audio system

- [x] Resource management

- [x] Scene management

### Future Plans
- [x] Texture and image support

- [ ] Advanced collision detection

- [x] Particle systems

- [ ] Network multiplayer

- [ ] Scripting support

- [ ] 3D rendering capabilities

### License
This project is open source (do whatever u want)
