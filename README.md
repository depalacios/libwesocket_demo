Secure Stream Echo Server Demo using Libwebsocket library for FRDM-IMX91 development board

# Compile
Ctrl+Shift+B
In order to compile and upload executable to targer you need to change variable values in settings.json:

- myRemoteIpAddr
- myRemoteFolder

# Debug
F5
In order to debug you need to start gdb server:

- gdbserver :2345 ./executable


# Repo structre

.
├── .vscode/            # VS Code config folder
├── include/            # Program libraries
│   ├── custom/         # Custom libraries
│   └── websockets/     # libwebsockets library
│
├── lib/                # .so / .a compiled libraries
│
├── main.c              # Main application
├── policy.json         # Secure Streams policy
└── readme.md           # Project documentation

# Run in target
In order to run in target you need in the same folder the next files

- libwebsockets.so
- policy.json
- main (Executable)