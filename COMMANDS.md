# UUV Simulation - Command Reference

## Build Commands

### Configure CMake
```bash
cmake -S . -B build
```
Configure the project and generate build files in the `build/` directory.

### Build Project
```bash
cmake --build build
```
Compile all source files and create the executable.

### Clean Build
```bash
Remove-Item -Recurse -Force build
cmake -S . -B build
cmake --build build
```
Clean everything and rebuild from scratch.

## Run Commands

### Run Simulation
```bash
.\build\Debug\uuv_sim.exe
```
Execute your agent-based simulation.

### Quick Build & Run
```bash
cmake --build build; .\build\Debug\uuv_sim.exe
```
Build and immediately run the simulation.

## VS Code Integration

### Build (Ctrl+Shift+P)
- "CMake: Build"
- "Tasks: Run Task" → "CMake: Build"

### Run/Debug
- **F5** - Debug with breakpoints
- **Ctrl+F5** - Run without debugging
- **Play button (▶️)** - Build and run

## File Structure
```
uuv_sim/
├── build/              # Build artifacts (ignored by git)
├── main.cpp           # Entry point
├── vec2.h/cpp         # 2D vector math
├── agent.h/cpp        # Agent intelligence
├── field.h/cpp        # Scalar field
├── event_controller.h/cpp  # Simulation coordinator
├── world_state.h      # Agent perception data
├── neighbor_info.h    # Neighbor data structure
├── constants.h        # Simulation constants
└── CMakeLists.txt     # Build configuration
```

## Useful Tips

- **Add new files**: Update `CMakeLists.txt` with new `.cpp` and `.h` files
- **Check errors**: Look at VS Code Problems panel
- **Clean start**: Delete `build/` folder if things get weird
- **Git ignore**: `build/` folder is already in `.gitignore`

## Your Simulation Parameters

- **Agents**: Autonomous entities with individual intelligence
- **Field**: Quadratic scalar field `f(x,y) = k1*x² + k2*xy + k3*y² + k4`
- **Behavior**: Avoidance + seek low field values
- **Time step**: `DELTA_T` (defined in constants.h)
- **End time**: `END_T` (defined in constants.h)

---
*Happy coding! 🚀*
